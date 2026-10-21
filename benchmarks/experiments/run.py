#!/usr/bin/env python3
"""Build native RK0 experiment variants, require PASS, and collect raw results."""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
MAKEFILE = ROOT / "benchmarks/experiments/Makefile"


def positive(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def qemu_run(command, log_path, marker, timeout):
    output = bytearray()
    marker_line = re.compile(rb"(?:^|[\r\n])" + re.escape(marker.encode()) + rb"\r?\n")
    deadline = time.monotonic() + timeout
    with subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT) as process:
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                while marker_line.search(output) is None:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise RuntimeError(f"QEMU timeout after {timeout}s")
                    if not selector.select(min(remaining, 0.5)):
                        continue
                    chunk = os.read(process.stdout.fileno(), 65536)
                    if not chunk:
                        raise RuntimeError(f"QEMU exited before {marker}")
                    output.extend(chunk)
                    if re.search(rb"EXP FAIL|HardFault|ASSERT|FAULT", output):
                        raise RuntimeError("QEMU reported a failure")
        finally:
            if process.poll() is None:
                process.terminate()
            try:
                rest, _ = process.communicate(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                rest, _ = process.communicate()
            output.extend(rest)
            log_path.write_bytes(output)
    decoded = output.decode("utf-8", errors="replace")
    if re.search(r"EXP FAIL|HardFault|ASSERT|FAULT", decoded):
        raise RuntimeError("QEMU reported a failure")
    return decoded


def parse_results(output, experiment, expected_rows):
    rows = []
    metadata = []
    for line in output.splitlines():
        if line.startswith("EXP RESULT "):
            row = dict(item.split("=", 1) for item in line.split()[2:])
            if row.get("experiment") != experiment:
                raise RuntimeError("unexpected experiment in result")
            rows.append(row)
        elif line.startswith("EXP META "):
            metadata.append(dict(item.split("=", 1) for item in line.split()[2:]))
    if len(rows) != expected_rows or len(metadata) != 1:
        raise RuntimeError(f"incomplete results: got {len(rows)} rows, "
                           f"expected {expected_rows}; metadata={len(metadata)}")
    return rows, metadata[0]


def footprint(elf):
    result = subprocess.check_output(["arm-none-eabi-size", str(elf)], text=True)
    text_bytes, data_bytes, bss_bytes = map(int, result.splitlines()[-1].split()[:3])
    return {"text_bytes": text_bytes, "data_bytes": data_bytes,
            "bss_bytes": bss_bytes, "flash_bytes": text_bytes + data_bytes,
            "static_ram_bytes": data_bytes + bss_bytes}


def write_tables(destination, rows, footprints, manifest):
    for name, table in (("results.csv", rows), ("footprints.csv", footprints)):
        if table:
            columns = list(dict.fromkeys(key for row in table for key in row))
            with (destination / name).open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=columns)
                writer.writeheader()
                writer.writerows(table)
        else:
            (destination / name).write_text("")
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("experiments", nargs="*", default=["E1", "E2", "E3"])
    parser.add_argument("--arch", choices=["armv7m", "armv6m"], default="armv7m")
    parser.add_argument("--build", choices=["DEBUG", "PROFILE", "RELEASE"], default="PROFILE")
    parser.add_argument("--samples", type=positive, default=5)
    parser.add_argument("--work", type=positive, default=4000)
    parser.add_argument("--interference", type=positive, default=40000)
    parser.add_argument("--iterations", type=positive, default=256)
    parser.add_argument("--capacities", type=positive, nargs="+", default=[1, 4])
    parser.add_argument("--chain-depths", type=positive, nargs="+", default=[1, 2, 3])
    parser.add_argument("--timeout", type=positive, default=20)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--inversion-control", action="store_true")
    args = parser.parse_args()
    if set(args.experiments) - {"E1", "E2", "E3"}:
        parser.error("experiments must be E1, E2, E3")
    if any(capacity > 8 for capacity in args.capacities):
        parser.error("capacities must be 1..8")
    if any(depth > 3 for depth in args.chain_depths):
        parser.error("chain depths must be 1..3")
    for tool in ("make", "arm-none-eabi-gcc", "arm-none-eabi-size", "qemu-system-arm"):
        if not shutil.which(tool):
            parser.error(f"required tool unavailable: {tool}")
    destination = args.output or Path("build/experiments") / args.arch / args.build
    if not destination.is_absolute():
        destination = ROOT / destination
    destination.mkdir(parents=True, exist_ok=True)
    variants = []
    if "E1" in args.experiments:
        modes = ["pip", "ceiling", "invocation"]
        if args.inversion_control:
            modes.append("unprotected")
        for mode in modes:
            variants.append(("E1", f"e1_{mode}", [f"MODE={mode}"],
                             args.samples * (2 if mode == "invocation" else 3)))
    if "E2" in args.experiments:
        for capacity in dict.fromkeys(args.capacities):
            for ceiling in (0, 3):
                variants.append(("E2", f"e2_n{capacity}_c{ceiling}",
                                 [f"CAPACITY={capacity}", f"CEILING={ceiling}"],
                                 args.samples * 2))
    if "E3" in args.experiments:
        variants.append(("E3", "e3_composition", [], args.samples * 14 + 2))
        for depth in dict.fromkeys(args.chain_depths):
            for ceiling in (0, 3):
                variants.append(("E3", f"e3_chain_d{depth}_c{ceiling}",
                                 ["E3_MODE=chain", f"CHAIN_DEPTH={depth}",
                                  f"CHAIN_CEILING={ceiling}"], args.samples))
    machine = "microbit" if args.arch == "armv6m" else "lm3s6965evb"
    manifest = {
        "parameters": {key: str(value) if isinstance(value, Path) else value
                       for key, value in vars(args).items()},
        "revision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "worktree_status": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
        "compiler": subprocess.check_output(["arm-none-eabi-gcc", "--version"], text=True).splitlines()[0],
        "qemu": subprocess.check_output(["qemu-system-arm", "--version"], text=True).splitlines()[0],
        "machine": machine,
        "timing": "QEMU virtual instruction counter, shift=0, align=off, sleep=off",
        "status": "running", "variants": [],
    }
    all_rows, footprints = [], []
    write_tables(destination, all_rows, footprints, manifest)
    for experiment, variant, definitions, expected_rows in variants:
        variant_dir = destination / variant
        variant_dir.mkdir(exist_ok=True)
        (variant_dir / "qemu.log").unlink(missing_ok=True)
        command = ["make", "-f", str(MAKEFILE), "build", f"ARCH={args.arch}",
                   "PLATFORM=qemu", f"BUILD={args.build}", f"SAMPLES={args.samples}",
                   f"WORK={args.work}", f"INTERFERENCE={args.interference}",
                   f"ITERATIONS={args.iterations}", f"EXPERIMENT={experiment}",
                   f"EXP_OUTPUT={destination}", *definitions]
        print(f"Build/run {variant} ({args.arch}, {args.build})", flush=True)
        elf = variant_dir / f"{variant}.elf"
        qemu_command = ["qemu-system-arm", "-machine", machine, "-nographic",
                        "-monitor", "none", "-icount", "shift=0,align=off,sleep=off",
                        "-kernel", str(elf)]
        entry = {"variant": variant, "build_command": command,
                 "qemu_command": qemu_command, "status": "running"}
        manifest["variants"].append(entry)
        try:
            with (variant_dir / "build.log").open("w") as log:
                subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
            output = qemu_run(qemu_command, variant_dir / "qemu.log",
                              f"EXP PASS {experiment}", args.timeout)
            rows, metadata = parse_results(output, experiment, expected_rows)
        except (RuntimeError, subprocess.CalledProcessError) as error:
            entry.update(status="failed", error=str(error))
            manifest["status"] = "failed"
            write_tables(destination, all_rows, footprints, manifest)
            raise
        common = {"variant": variant, "arch": args.arch, "build": args.build}
        all_rows.extend({**common, **metadata, **row} for row in rows)
        footprints.append({**common, **footprint(elf)})
        entry.update(metadata=metadata, status="PASS")
        write_tables(destination, all_rows, footprints, manifest)
        print(f"  PASS: {len(rows)} result rows", flush=True)
    manifest["status"] = "PASS"
    write_tables(destination, all_rows, footprints, manifest)
    print(f"Results: {destination / 'results.csv'}")
    print(f"Footprints: {destination / 'footprints.csv'}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Experiment run failed: {error}; inspect build.log/qemu.log", file=sys.stderr)
        sys.exit(1)
