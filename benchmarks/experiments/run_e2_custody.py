#!/usr/bin/env python3
"""Run E2's composed/native/static custody comparison and replay every event."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from run import ROOT, MAKEFILE, footprint, positive, qemu_run

VARIANTS = ("COMPOSED", "RK0_NOC", "RK0_CEIL", "STATIC_PRIORITY")
NO_BLOCK = 65535
MASK = 0xFFFFFFFF


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def elapsed(end, start):
    return (end - start) & MASK


def parse_output(output, samples):
    metadata, cases = [], []
    for line in output.splitlines():
        if line.startswith("EXP META "):
            metadata.append(dict(item.split("=", 1) for item in line.split()[2:]))
        elif line.startswith("EXP RESULT "):
            cases.append({"result": dict(item.split("=", 1) for item in line.split()[2:]), "events": []})
        elif line.startswith("EXP EVENT "):
            require(bool(cases), "event before result header")
            cases[-1]["events"].append(dict(item.split("=", 1) for item in line.split()[2:]))
    require(len(metadata) == 1 and len(cases) == 3 * samples, "incomplete E2 output")
    require({(case["result"]["path"], int(case["result"]["sample"])) for case in cases}
            == {(path, sample) for path in ("handoff", "timeout", "direct") for sample in range(1, samples + 1)},
            "missing or duplicate E2 case")
    return metadata[0], cases


def replay(case):
    result, events = case["result"], case["events"]
    n, items = int(result["capacity"]), int(result["items"])
    require(int(result["faults"]) == 0, "target reported observer fault")
    require(len(events) == int(result["events"]), "event log truncated")
    states = ["free"] * n
    custody, received = {}, {}
    request = None
    pending_release = None
    waits, custody_costs, pre_costs, return_costs, tails, release_calls = [], [], [], [], [], []
    grant_return_lags, handoff_after_release = [], []
    sent, consumed = [], []
    grants = releases = full = timeouts = handoffs = direct = 0
    medium = {}
    background_ends = {}
    for index, event in enumerate(events):
        require(int(event["index"]) == index, "event sequence gap/overflow")
        require(all(event[key] == result[key] for key in ("variant", "capacity", "path", "sample")), "event/case identity mismatch")
        time, block = int(event["time"]), int(event["block"])
        kind, value = event["kind"], int(event["value"])
        require(block == NO_BLOCK or 0 <= block < n, "block outside fixed pool")
        contribution = int(event["contribution"])
        require(contribution in (0, 3), "invalid priority contribution")
        if int(event["subject"]) in (1, 2):
            expected_priority = min(int(event["nominal"]), contribution or 255)
            require(int(event["priority"]) == expected_priority, "effective priority/contribution mismatch")
            if result["variant"] != "RK0_CEIL":
                require(contribution == 0, "non-ceiling variant claims dynamic protection")
        if kind == "request":
            require(request is None, "overlapping allocation calls")
            request = {"time": time, "timeout": value, "block": None}
        elif kind == "grant":
            require(request is not None and request["block"] is None, "grant without allocation request")
            require(states[block] in ("free", "releasing"), "duplicated grant or custody")
            states[block] = "producer"
            request["block"] = block
            request["grant_time"] = time
            waits.append(elapsed(time, request["time"]))
            grants += 1
            handoffs += int(int(event["actor"]) == 2)
            if int(event["actor"]) == 2:
                handoff_after_release.append(elapsed(time, int(result["release"])))
        elif kind == "alloc_return":
            require(request is not None and request["block"] == block and value == 0, "allocation return without matching grant")
            grant_return_lags.append(elapsed(time, request["grant_time"]))
            request = None
        elif kind == "reject":
            require(request is not None and request["block"] is None, "rejected granted allocation")
            if request["timeout"] == 0:
                require(value == int(result["full_code"]), "wrong full-pool policy")
                full += 1
            else:
                require(value == int(result["timeout_code"]), "wrong timeout policy")
                timeouts += 1
            request = None
        elif kind == "send":
            require(states[block] == "producer", "send without unique producer custody")
            states[block] = "receiver"
            custody[block] = time
            sent.append(value)
        elif kind == "receive":
            require(states[block] == "receiver", "receive without receiver custody")
            consumed.append(value)
            received[block] = time
        elif kind == "pre_release":
            require(pending_release is None and states[block] == "receiver" and block in received,
                    "release without processing or overlapping release")
            pending_release = {"block": block, "start": custody[block], "pre": time, "release": None}
        elif kind == "release":
            require(pending_release is not None and pending_release["block"] == block
                    and states[block] == "receiver", "release without receiver custody")
            states[block] = "releasing"
            pending_release["release"] = time
            releases += 1
        elif kind == "available":
            require(states[block] == "releasing", "availability without release")
            states[block] = "free"
        elif kind == "release_return":
            require(pending_release is not None and pending_release["block"] == block
                    and pending_release["release"] is not None and value == 0, "API return without release commit")
            previous = pending_release
            custody_costs.append(elapsed(previous["release"], previous["start"]))
            pre_costs.append(elapsed(previous["pre"], previous["start"]))
            return_costs.append(elapsed(time, previous["start"]))
            tails.append(elapsed(time, previous["release"]))
            release_calls.append(elapsed(time, previous["pre"]))
            pending_release = None
        elif kind in ("medium_dispatch", "medium_complete"):
            medium[(value, kind)] = time
        elif kind == "background_complete":
            require(contribution == 0, "custody leaked into background work")
            background_ends[int(event["subject"])] = time
        elif kind == "background_dispatch":
            require(contribution == 0, "background has an active custody contribution")
        elif kind == "receive_available":
            require(states[block] == "receiver", "direct delivery without custody")
            direct += 1
        elif kind == "priority":
            require(value != int(event["priority"]), "recorded stable priority as a transition")
        else:
            raise RuntimeError(f"unknown event: {kind}")
    require(all(state == "free" for state in states), "pool not conserved")
    require(request is None and pending_release is None, "unfinished API call")
    require(grants == releases == items and sent == consumed == list(range(items)), "lost, duplicate, or reordered data")
    require(int(result["checksum"]) == items * (items + 1) // 2, "payload checksum mismatch")
    require(full == (0 if result["path"] == "direct" else 1), "full-pool rejection differs")
    require(timeouts == (1 if result["path"] == "timeout" else 0), "timeout rejection differs")
    require(result["path"] != "handoff" or handoffs > 0, "no direct grant to blocked allocator")
    require(result["path"] != "direct" or direct > 0, "no direct delivery to waiting receiver")
    require(set(medium) == {(phase, kind) for phase in (0, 1) for kind in ("medium_dispatch", "medium_complete")}, "missing medium events")
    require(set(background_ends) == {1, 2}, "missing background work")
    release = int(result["release"])
    bg_release = int(result["background_release"])
    bg_dispatch = elapsed(medium[(1, "medium_dispatch")], bg_release)
    background_before_medium = sum(elapsed(time, bg_release) < bg_dispatch for time in background_ends.values())
    require(background_before_medium == (2 if result["variant"] == "STATIC_PRIORITY" else 0),
            "outside-custody static/dynamic interference contrast absent")
    metrics = {
        "W_alloc_max": max(waits), "W_alloc_median": statistics.median(waits),
        "T_grant_to_alloc_return_max": max(grant_return_lags),
        "first_handoff_after_release": handoff_after_release[0] if handoff_after_release else "",
        "T_custody_max": max(custody_costs), "T_pre_release_max": max(pre_costs),
        "T_api_return_max": max(return_costs), "T_post_release_max": max(tails),
        "T_release_call_max": max(release_calls), "handoff_grants": handoffs,
        "direct_deliveries": direct, "full_rejects": full, "timeout_rejects": timeouts,
        "medium_dispatch": elapsed(medium[(0, "medium_dispatch")], release),
        "medium_complete": elapsed(medium[(0, "medium_complete")], release),
        "background_medium_dispatch": bg_dispatch,
        "background_medium_complete": elapsed(medium[(1, "medium_complete")], bg_release),
        "background_tasks_before_medium": background_before_medium,
    }
    signature = {key: result[key] for key in ("capacity", "items", "checksum", "payload_bytes", "block_bytes")}
    signature.update(grants=grants, releases=releases, sent=sent, received=consumed, full=full, timeouts=timeouts)
    return metrics, signature


def table(path, rows):
    if not rows:
        path.write_text("")
        return
    columns = list(dict.fromkeys(key for row in rows for key in row))
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=["armv7m", "armv6m"], default="armv7m")
    parser.add_argument("--build", choices=["PROFILE", "DEBUG", "RELEASE"], default="PROFILE")
    parser.add_argument("--samples", type=positive, default=3)
    parser.add_argument("--work", type=positive, default=4000)
    parser.add_argument("--ratios", type=positive, nargs="+", default=[1, 10, 40])
    parser.add_argument("--capacities", type=positive, nargs="+", default=[1, 4, 8])
    parser.add_argument("--variants", choices=VARIANTS, nargs="+", default=list(VARIANTS))
    parser.add_argument("--timeout", type=positive, default=30)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    require(all(n in (1, 4, 8) for n in args.capacities), "capacities must be 1,4,8")
    destination = args.output or ROOT / "build/experiments" / args.arch / args.build / "e2-custody"
    if not destination.is_absolute():
        destination = ROOT / destination
    destination.mkdir(parents=True, exist_ok=True)
    machine = "microbit" if args.arch == "armv6m" else "lm3s6965evb"
    manifest = {
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "dirty_state": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
        "arch": args.arch, "build": args.build, "machine": machine,
        "parameters": {key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
        "compiler": subprocess.check_output(["arm-none-eabi-gcc", "--version"], text=True).splitlines()[0],
        "qemu": subprocess.check_output(["qemu-system-arm", "--version"], text=True).splitlines()[0],
        "timing": "instrumented SysTick virtual counts; QEMU -icount shift=0,align=off,sleep=off",
        "source_hashes": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in sorted((ROOT / "benchmarks/experiments").glob("*")) if path.is_file()},
        "missing_variants": {"COMPOSED_PROTECTED": "No public generic API transfers a custody-scoped ceiling to another blocked/queued task; mutex ownership cannot be borrowed."},
        "paired_comparison_complete": set(args.variants) == set(VARIANTS),
        "status": "running", "variants": [],
    }
    results, footprints, signatures = [], [], {}

    def save():
        table(destination / "results.csv", results)
        table(destination / "footprints.csv", footprints)
        (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    save()
    for n in dict.fromkeys(args.capacities):
        for ratio in dict.fromkeys(args.ratios):
            interference = args.work * ratio
            for variant in dict.fromkeys(args.variants):
                name = f"e2_custody_{variant}_n{n}_w{args.work}_i{interference}"
                variant_dir = destination / name
                variant_dir.mkdir(exist_ok=True)
                command = ["make", "-f", str(MAKEFILE), "build", "EXPERIMENT=E2", "E2_MODE=custody",
                           f"CUSTODY_VARIANT={variant}", f"CAPACITY={n}", f"ARCH={args.arch}", "PLATFORM=qemu",
                           f"BUILD={args.build}", f"SAMPLES={args.samples}", f"WORK={args.work}",
                           f"INTERFERENCE={interference}", f"EXP_OUTPUT={destination}"]
                qemu = ["qemu-system-arm", "-machine", machine, "-nographic", "-monitor", "none",
                        "-icount", "shift=0,align=off,sleep=off", "-kernel", str(variant_dir / f"{name}.elf")]
                entry = {"variant": variant, "capacity": n, "ratio": ratio, "build_command": command,
                         "qemu_command": qemu, "status": "running"}
                manifest["variants"].append(entry)
                print(f"Build/run {name} ({args.arch})", flush=True)
                try:
                    with (variant_dir / "build.log").open("w") as log:
                        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
                    output = qemu_run(qemu, variant_dir / "qemu.log", "EXP PASS E2", args.timeout)
                    metadata, cases = parse_output(output, args.samples)
                    all_events = []
                    for case in cases:
                        metrics, signature = replay(case)
                        raw = case["result"]
                        key = (n, ratio, raw["path"], raw["sample"])
                        if key in signatures:
                            require(signatures[key] == signature, f"functional contract differs: {name} {key}")
                        else:
                            signatures[key] = signature
                        results.append({"arch": args.arch, "build": args.build, "ratio": ratio,
                                        **metadata, **raw, **metrics})
                        all_events.extend(case["events"])
                    table(variant_dir / "events.csv", all_events)
                    footprints.append({"arch": args.arch, "build": args.build, "variant": variant,
                                       "capacity": n, "ratio": ratio, **footprint(variant_dir / f"{name}.elf")})
                    entry["metadata"] = metadata
                    compile_line = next(line for line in (variant_dir / "build.log").read_text().splitlines()
                                        if line.startswith("arm-none-eabi-gcc ") and " -c " in line)
                    entry["compile_command_example"] = compile_line
                    tokens = shlex.split(compile_line)
                    entry["compiler_flags"] = tokens[1:tokens.index("-c")]
                    entry["observers"] = json.loads((ROOT / "build/e2-observers" / args.arch / args.build / name / "observers.json").read_text())
                    entry["status"] = "PASS"
                    print(f"  PASS {len(cases)} cases; all events replayed and contracts matched", flush=True)
                except (RuntimeError, ValueError, KeyError, subprocess.CalledProcessError) as error:
                    entry.update(status="failed", error=str(error))
                    manifest["status"] = "failed"
                    save()
                    raise
                save()
    manifest["status"] = "PASS"
    save()
    print(f"Results: {destination / 'results.csv'}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"E2 comparison failed: {error}; inspect per-variant logs", file=sys.stderr)
        sys.exit(1)
