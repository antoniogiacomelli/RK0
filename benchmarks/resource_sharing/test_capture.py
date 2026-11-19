"""Integrity tests for the result collector; fixtures are NOT measurements."""
import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("rs_run", Path(__file__).with_name("run.py"))
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


def synthetic_log(experiment="E1", samples=2):
    lines = [f"RS META schema=1 experiment={experiment} board=NUCLEO-F103RB "
             f"timer=DWT_CYCCNT unit=cpu_cycles core_hz=72000000 tick_hz=1000 "
             f"samples={samples} batch=256 idle_policy=busy irq_policy=enabled "
             "overhead_subtracted=0 revision=SYNTHETIC_FIXTURE"]
    for case, metric, operations in sorted(runner.expected_groups(experiment, 256)):
        for sample in range(1, samples + 1):
            lines.append(f"RS DATA {experiment},{case},{sample},{metric},100,{operations}")
    lines.append(f"RS PASS experiment={experiment}")
    return "\r\n".join(lines) + "\r\n"


class CaptureIntegrity(unittest.TestCase):
    def test_complete_matrices(self):
        for experiment, groups in (("E1", 43), ("E2", 56), ("E3", 26)):
            with self.subTest(experiment=experiment):
                rows, meta = runner.parse_log(synthetic_log(experiment))
                self.assertEqual(len(rows), groups * 2)
                self.assertEqual(meta["revision"], "SYNTHETIC_FIXTURE")

    def test_missing_sample(self):
        lines = synthetic_log().splitlines()
        del lines[1]
        with self.assertRaisesRegex(ValueError, "incomplete samples"):
            runner.parse_log("\n".join(lines))

    def test_missing_whole_metric(self):
        lines = synthetic_log().splitlines()
        del lines[1:3]
        with self.assertRaisesRegex(ValueError, "metric matrix"):
            runner.parse_log("\n".join(lines))

    def test_duplicate_sample(self):
        lines = synthetic_log().splitlines()
        lines.insert(2, lines[1])
        with self.assertRaisesRegex(ValueError, "duplicate"):
            runner.parse_log("\n".join(lines))

    def test_reboot(self):
        with self.assertRaisesRegex(ValueError, "multiple boot"):
            runner.parse_log(synthetic_log() + synthetic_log())

    def test_timeout_without_pass(self):
        with self.assertRaisesRegex(ValueError, "PASS"):
            runner.parse_log(synthetic_log().replace("RS PASS experiment=E1", ""))

    def test_wrong_timer_rejected(self):
        with self.assertRaisesRegex(ValueError, "timer"):
            runner.parse_log(synthetic_log().replace("DWT_CYCCNT", "SYSTICK"))

    def test_missing_numeric_metadata(self):
        with self.assertRaisesRegex(ValueError, "missing numeric"):
            runner.parse_log(synthetic_log().replace("batch=256 ", ""))

    def test_failure_after_pass_rejected(self):
        with self.assertRaisesRegex(ValueError, "RS FAIL"):
            runner.parse_log(synthetic_log() + "RS FAIL experiment=E1 reason=test\n")

    def test_wrong_experiment_rejected(self):
        with self.assertRaisesRegex(ValueError, "unexpected experiment"):
            runner.parse_log(synthetic_log("E2"), "E1")

    def test_programmer_image_types(self):
        elf = Path("build/rk0_rs_e1.elf")
        self.assertEqual(runner.flash_image(elf, "openocd"), elf)
        for tool in ("st-flash", "jlink", "stm32programmer"):
            self.assertEqual(runner.flash_image(elf, tool).suffix, ".bin")


if __name__ == "__main__":
    unittest.main()
