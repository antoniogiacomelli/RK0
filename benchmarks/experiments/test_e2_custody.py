#!/usr/bin/env python3
"""Verify the acceptance gate using a real native handoff/timeout/direct trace."""
import copy
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
from observe_e2 import ROOT, generate, replace_exact
from run_e2_custody import parse_output, replay


class CustodyAcceptanceTests(unittest.TestCase):
    def setUp(self):
        trace = Path(__file__).with_name("fixtures") / "e2-custody-ceiling-n1.log"
        _, self.cases = parse_output(trace.read_text(), 1)

    def test_real_trace_passes_all_scenarios(self):
        for case in self.cases:
            metrics, signature = replay(case)
            self.assertGreater(metrics["T_post_release_max"], 0)
            self.assertEqual(signature["grants"], int(case["result"]["items"]))

    def test_truncated_log_rejected(self):
        case = copy.deepcopy(self.cases[0])
        case["events"].pop()
        with self.assertRaises(RuntimeError):
            replay(case)

    def test_double_grant_rejected(self):
        case = copy.deepcopy(self.cases[0])
        next(event for event in case["events"] if event["kind"] == "send")["kind"] = "grant"
        with self.assertRaises(RuntimeError):
            replay(case)

    def test_corrupt_payload_order_rejected(self):
        case = copy.deepcopy(self.cases[0])
        next(event for event in case["events"] if event["kind"] == "receive")["value"] = "999"
        with self.assertRaises(RuntimeError):
            replay(case)

    def test_background_protection_leak_rejected(self):
        case = copy.deepcopy(self.cases[0])
        next(event for event in case["events"] if event["kind"] == "background_complete")["contribution"] = "3"
        with self.assertRaises(RuntimeError):
            replay(case)

    def test_regrant_before_previous_release_return_is_accepted(self):
        case = self.cases[0]
        kinds = [event["kind"] for event in case["events"]]
        release = kinds.index("release")
        grant = kinds.index("grant", release)
        returned = kinds.index("release_return", release)
        self.assertLess(grant, returned)
        replay(case)

    def test_generated_observers_preserve_every_original_line(self):
        with tempfile.TemporaryDirectory(prefix="rk0-e2-observers-") as directory:
            generated = Path(directory)
            generate(generated)
            for name in ("kmesg.c", "kmesgq.c", "ksch.c"):
                original = (ROOT / "core/src" / name).read_text()
                stripped = "".join(line for line in (generated / name).read_text().splitlines(keepends=True)
                                   if "E2Observe" not in line and '"e2_observe.h"' not in line)
                self.assertEqual(stripped, original)

    def test_insertion_site_drift_fails_closed(self):
        with self.assertRaises(RuntimeError):
            replace_exact("changed upstream implementation", "expected site", "hook")


if __name__ == "__main__":
    unittest.main()
