#!/usr/bin/env python3
"""Build, capture and validate NUCLEO-F103RB resource-sharing experiments.

Raw UART logs are the source of truth. Measurements are never synthesised.
PySerial is needed only for hardware capture; --input uses the standard library.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import re
import signal
import statistics
import subprocess
import sys
import time
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAKEFILE = ROOT / "benchmarks/resource_sharing/Makefile"
FIELDS = ("experiment", "case", "sample", "metric", "cycles", "operations")


def expected_groups(experiment: str, batch: int) -> set[tuple[str, str, int]]:
    """Schema 1 coverage: losing an entire case or metric must fail validation."""
    if experiment == "E1":
        metrics = ("h_release_to_first", "h_release_to_done", "h_operation",
                   "m_release_to_first", "m_release_to_done", "m_execution",
                   "l_resource_work_interval", "l_operation")
        cases = ("contended-pip", "contended-ceiling", "single-pip",
                 "single-ceiling", "single-invocation")
        groups = {(case, metric, 1) for case in cases for metric in metrics}
        groups.update((f"batch-{policy}", "transaction_batch", batch)
                      for policy in ("pip", "ceiling", "invocation"))
        return groups
    if experiment == "E2":
        metrics = ("work_cycles", "drain_cycles", "allocation_wait_cycles",
                   "receiver_dispatch_cycles", "medium_dispatch_cycles",
                   "medium_completion_cycles", "medium_execution_interval_cycles")
        return {(f"{mode}_n{n}_ceiling_{ceiling}", metric, 1)
                for mode in ("handoff", "timeout") for n in (1, 4)
                for ceiling in ("on", "off") for metric in metrics}
    if experiment == "E3":
        groups = {(f"composition_{mode}", "mutex_wait_with_coordination_cycles", 1)
                  for mode in ("handoff", "timeout")}
        for m in (0, 1, 4):
            for n in (0, 1, 4, 8):
                case = f"cost_m{m}_n{n}"
                groups.add((case, "stable_recalc_batch_cycles", batch))
                path = "changing" if n == 0 else "stable"
                groups.add((case, f"alloc_free_priority_{path}_batch_cycles", batch))
        return groups
    raise ValueError(f"unknown experiment: {experiment}")


def positive(text: str) -> int:
    value = int(text)
    if value < 1:
        raise argparse.ArgumentTypeError("must be positive")
    return value


def parse_log(text: str, expected: str | None = None) -> tuple[list[dict], dict]:
    """Fail closed on incomplete, rebooted, malformed or failed captures."""
    rows: list[dict] = []
    metadata: dict = {}
    keys: set[tuple] = set()
    pass_count = 0
    notes: list[str] = []
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith(("RS FAIL", "RS ERROR")):
            raise ValueError(line)
        if line.startswith("RS META "):
            if metadata:
                raise ValueError("multiple boot records: board restarted during capture")
            metadata = dict(re.findall(r"(\w+)=([^ ]+)", line[8:]))
            metadata["raw"] = line
        elif line.startswith("RS DATA "):
            if not metadata or pass_count:
                raise ValueError("data outside the boot-to-PASS interval")
            values = next(csv.reader([line[8:]]))
            if len(values) != len(FIELDS):
                raise ValueError(f"malformed data row: {line}")
            row = dict(zip(FIELDS, values))
            for name in ("sample", "cycles", "operations"):
                row[name] = int(row[name])
            if not (1 <= row["sample"] and 0 <= row["cycles"] < 2**32
                    and row["operations"] >= 1):
                raise ValueError(f"invalid measurement: {line}")
            key = tuple(row[name] for name in ("experiment", "case", "sample", "metric"))
            if key in keys:
                raise ValueError(f"duplicate measurement: {key}")
            keys.add(key)
            rows.append(row)
        elif line.startswith("RS PASS "):
            if not metadata or line != f"RS PASS experiment={metadata.get('experiment')}":
                raise ValueError(f"unexpected PASS marker: {line}")
            pass_count += 1
        elif line.startswith("RS ") and not line.startswith("RS CSV "):
            notes.append(line)
    if not metadata or not rows or pass_count != 1:
        raise ValueError("capture requires one boot record, data and one PASS marker")
    for field, value in (("schema", "1"), ("board", "NUCLEO-F103RB"),
                         ("timer", "DWT_CYCCNT"), ("unit", "cpu_cycles"),
                         ("idle_policy", "busy"), ("irq_policy", "enabled"),
                         ("overhead_subtracted", "0")):
        if metadata.get(field) != value:
            raise ValueError(f"invalid metadata {field}: {metadata.get(field)!r}")
    experiment = metadata["experiment"]
    if experiment not in ("E1", "E2", "E3") or (expected and experiment != expected):
        raise ValueError(f"unexpected experiment: {experiment}")
    if any(field not in metadata for field in ("samples", "batch", "core_hz")):
        raise ValueError("missing numeric run metadata")
    n = int(metadata["samples"])
    batch = int(metadata["batch"])
    if not (1 <= n <= 10000) or batch < 1 or int(metadata["core_hz"]) <= 0:
        raise ValueError("invalid sample count or clock metadata")
    samples: dict[tuple, set[int]] = defaultdict(set)
    for row in rows:
        if row["experiment"] != experiment:
            raise ValueError("mixed experiments in one capture")
        samples[(row["case"], row["metric"], row["operations"])].add(row["sample"])
    required = expected_groups(experiment, batch)
    if set(samples) != required:
        raise ValueError(f"incomplete/unknown metric matrix: missing={sorted(required - set(samples))}, "
                         f"extra={sorted(set(samples) - required)}")
    for key, observed in samples.items():
        if observed != set(range(1, n + 1)):
            raise ValueError(f"incomplete samples for {key}: {sorted(observed)}")
    metadata["notes"] = notes
    return rows, metadata


def write_results(output: Path, captures: list[tuple[Path, list[dict], dict]]) -> None:
    rows = [row for _, records, _ in captures for row in records]
    with (output / "results.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    grouped: dict[tuple, list[float]] = defaultdict(list)
    for row in rows:
        key = (row["experiment"], row["case"], row["metric"], row["operations"])
        grouped[key].append(row["cycles"] / row["operations"])
    with (output / "summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("experiment", "case", "metric", "operations", "samples",
                         "min_cycles_per_operation", "median_cycles_per_operation",
                         "mean_cycles_per_operation", "max_cycles_per_operation",
                         "stdev_cycles_per_operation"))
        for key, values in sorted(grouped.items()):
            writer.writerow((*key, len(values), min(values), statistics.median(values),
                             statistics.mean(values), max(values),
                             statistics.stdev(values) if len(values) > 1 else 0.0))
    (output / "capture_metadata.json").write_text(json.dumps([
        {"source": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
         "metadata": meta} for path, _, meta in captures
    ], indent=2) + "\n", encoding="utf-8")


def command_output(command: list[str]) -> str:
    try:
        return subprocess.check_output(command, cwd=ROOT, text=True,
                                       stderr=subprocess.STDOUT).strip()
    except (OSError, subprocess.CalledProcessError) as error:
        return f"unavailable: {error}"


def flash_image(elf: Path, tool: str) -> Path:
    return elf if tool == "openocd" else elf.with_suffix(".bin")


def build(args: argparse.Namespace, bench: str, output: Path) -> Path:
    command = ["make", "-f", str(MAKEFILE), f"build-{bench}", f"BUILD={args.build}",
               f"SAMPLES={args.samples}", f"BATCH={args.batch}", f"WORK={args.work}",
               f"MEDIUM_WORK={args.medium_work}", f"CORE_HZ={args.core_hz}",
               f"TICK_HZ={args.tick_hz}", f"RS_BUILD_ROOT={args.build_root}"]
    with (output / f"{bench}.build.log").open("w", encoding="utf-8") as stream:
        stream.write(json.dumps(command) + "\n")
        stream.flush()
        subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                       check=True)
    return args.build_root / bench / f"rk0_rs_{bench}.elf"


def stop_programmer(process: subprocess.Popen) -> None:
    """Stop only this runner's make/programmer process tree on failure."""
    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            return
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
    else:
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        process.wait(timeout=5)


def capture(args: argparse.Namespace, bench: str, image: Path, output: Path) -> Path:
    try:
        import serial
    except ImportError as error:
        raise RuntimeError("Install PySerial: python3 -m pip install pyserial") from error
    path = output / f"{bench}.serial.log"
    # Open and drain before reset/programming, otherwise short trials can be missed.
    with serial.Serial(args.port, args.baud, timeout=0.2) as port, \
            path.open("wb") as log, \
            (output / f"{bench}.flash.log").open("wb") as flash_log:
        port.reset_input_buffer()
        process = None
        deadline = time.monotonic() + args.timeout
        try:
            if not args.capture_only:
                programmed = flash_image(image, args.flash_tool)
                # A relative name also avoids embedding a spaced checkout path
                # in the top-level Makefile's OpenOCD/J-Link command language.
                try:
                    programmed = programmed.relative_to(ROOT)
                except ValueError:
                    pass
                command = ["make", "PLATFORM=stm32f103rb", f"FLASH_TOOL={args.flash_tool}",
                           f"IMAGE={programmed}", "flash"]
                group = {"start_new_session": True} if os.name == "posix" else {
                    "creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}
                process = subprocess.Popen(command, cwd=ROOT, stdout=flash_log,
                                           stderr=subprocess.STDOUT, **group)
            else:
                print(f"Serial ready for {bench.upper()}; reset the board now.", flush=True)
            buffer = b""
            while time.monotonic() < deadline:
                if process is not None and process.poll() not in (None, 0):
                    raise RuntimeError(f"{bench} flashing failed; see {bench}.flash.log")
                chunk = port.read(port.in_waiting or 1)
                if not chunk:
                    continue
                log.write(chunk)
                log.flush()
                buffer += chunk
                while b"\n" in buffer:
                    raw, buffer = buffer.split(b"\n", 1)
                    line = raw.decode("utf-8", errors="replace").strip()
                    if line.startswith(("RS FAIL", "RS ERROR")):
                        raise RuntimeError(line)
                    if line == f"RS PASS experiment={bench.upper()}":
                        if process is not None:
                            process.wait(timeout=max(1.0, deadline - time.monotonic()))
                            if process.returncode != 0:
                                raise RuntimeError("flashing command failed after target output")
                        return path
            raise TimeoutError(f"{bench}: no complete PASS in {args.timeout:g} s; raw log retained")
        finally:
            if process is not None and process.poll() is None:
                stop_programmer(process)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("benches", nargs="*", choices=("e1", "e2", "e3"))
    parser.add_argument("--port", help="e.g. /dev/ttyACM0, /dev/cu.usbmodem..., COM5")
    parser.add_argument("--baud", type=positive, default=115200)
    parser.add_argument("--flash-tool", choices=("openocd", "st-flash", "jlink", "stm32programmer"), default="openocd")
    parser.add_argument("--build", choices=("PROFILE", "DEBUG", "RELEASE"), default="PROFILE")
    parser.add_argument("--build-root", type=Path, help="directory containing e1/e2/e3 images")
    parser.add_argument("--samples", type=positive, default=5)
    parser.add_argument("--batch", type=positive, default=256)
    parser.add_argument("--work", type=positive, default=4000)
    parser.add_argument("--medium-work", type=positive, default=40000)
    parser.add_argument("--core-hz", type=positive, default=72000000)
    parser.add_argument("--tick-hz", type=positive, default=1000)
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--capture-only", action="store_true", help="do not build/flash; open serial then manually reset")
    parser.add_argument("--input", type=Path, nargs="+", help="validate existing serial logs without hardware")
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be finite and positive")
    if not args.input and not args.port:
        parser.error("--port is required for hardware capture")
    if not args.input and not args.capture_only and args.baud != 115200:
        parser.error("this firmware uses 115200 baud; --baud overrides require --capture-only")
    benches = args.benches or ["e1", "e2", "e3"]
    if len(set(benches)) != len(benches):
        parser.error("select each experiment only once")
    if args.capture_only and len(benches) != 1:
        parser.error("--capture-only requires exactly one experiment")
    args.build_root = args.build_root or Path("build/armv7m/stm32f103rb/resource_sharing") / args.build
    if not args.build_root.is_absolute():
        args.build_root = ROOT / args.build_root
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    output = args.output or ROOT / "build" / f"resource-sharing-{stamp}"
    output.mkdir(parents=True, exist_ok=False)
    manifest = {"created_utc": stamp, "argv": sys.argv[1:], "mode": "import" if args.input else "hardware",
                "repository_commit": command_output(["git", "rev-parse", "HEAD"]),
                "repository_status": command_output(["git", "status", "--porcelain"]),
                "compiler": command_output(["arm-none-eabi-gcc", "--version"])}
    captures: list[tuple[Path, list[dict], dict]] = []
    try:
        if args.input:
            for path in args.input:
                rows, metadata = parse_log(path.read_text(encoding="utf-8"))
                if any(meta["experiment"] == metadata["experiment"] for _, _, meta in captures):
                    raise ValueError("duplicate experiment logs; analyse independent runs separately")
                captures.append((path.resolve(), rows, metadata))
        else:
            for bench in benches:
                print(f"Preparing {bench.upper()}...", flush=True)
                image = Path() if args.capture_only else build(args, bench, output)
                if not args.capture_only:
                    programmed = flash_image(image, args.flash_tool)
                    manifest.setdefault("images", {})[bench] = {
                        "elf_path": str(image), "elf_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
                        "programmed_path": str(programmed),
                        "programmed_sha256": hashlib.sha256(programmed.read_bytes()).hexdigest()}
                path = capture(args, bench, image, output)
                rows, metadata = parse_log(path.read_text(encoding="utf-8"), bench.upper())
                if not args.capture_only and (int(metadata["samples"]) != args.samples or
                                             int(metadata["core_hz"]) != args.core_hz):
                    raise ValueError("firmware metadata differs from requested build")
                captures.append((path, rows, metadata))
                print(f"{bench.upper()}: PASS, {len(rows)} measurements", flush=True)
        write_results(output, captures)
        manifest["status"] = "PASS"
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        manifest["status"] = "FAIL"
        manifest["error"] = str(error)
        print(f"ERROR: {error}\nLogs retained in {output}", file=sys.stderr)
        return 1
    finally:
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Raw samples: {output / 'results.csv'}\nSummary: {output / 'summary.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
