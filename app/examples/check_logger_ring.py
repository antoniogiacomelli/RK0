#!/usr/bin/env python3
"""Check the public logger's emitted records, including order and drops."""
import argparse
from pathlib import Path
import re


def check(path: Path, capacity: int, length: int, disabled: bool) -> None:
    output = path.read_text(errors="replace")
    if disabled:
        assert "LR PASS disabled logger" in output, "missing disabled marker"
        assert "LROFF" not in output, "disabled logging emitted a record"
        return

    assert "LR PASS pointer ring" in output, "missing completion marker"
    records = re.findall(r"^\s*\d+ ms :: (.*?)\s*$", output, re.MULTILINE)
    expected = [f"LRREC {batch} {i}" for batch in range(4) for i in range(capacity)]
    expected += [
        f"LRMULTI {batch} {actor} {i}"
        for batch in range(4)
        for actor in range(2)
        for i in range(capacity // 2)
    ]
    expected += [f"LRORDER {i}" for i in range(3)]
    expected += [f"LRWAKE {i}" for i in range(8)]
    expected += [f"LRHIGH {actor} {i}" for actor in range(2) for i in range(2 * capacity)]
    expected += ["LRTRUNC:" + "X" * (length - 10) + "?"]
    assert records == expected, next(
        (f"record {i}: expected {a!r}, got {b!r}" for i, (a, b) in
         enumerate(zip(expected, records)) if a != b),
        f"expected {len(expected)} records, got {len(records)}",
    )
    assert not any("LRDROP" in record for record in records), "overflow was not dropped"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--capacity", type=int, default=12)
    parser.add_argument("--length", type=int, default=64)
    parser.add_argument("--disabled", action="store_true")
    args = parser.parse_args()
    try:
        check(args.log, args.capacity, args.length, args.disabled)
    except AssertionError as exc:
        raise SystemExit(f"logger output: FAIL: {exc}") from exc
    print("logger output: PASS")
