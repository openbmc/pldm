import itertools
import json
import os
import subprocess
import sys
import unittest

BINARY = sys.argv.pop(1)
COMMAND = [BINARY, "platform", "SetStateEffecterEnables"]


class StateEffecterEnablesCLI(unittest.TestCase):
    def invoke(self, args, mode="success", instance=10, command=COMMAND):
        result = subprocess.run(
            [*command, *args],
            env={
                **os.environ,
                "PLDM_TEST_RESPONSE": mode,
                "PLDM_TEST_INSTANCE_ID": str(instance),
            },
            text=True,
            capture_output=True,
            timeout=10,
            check=False,
        )
        self.assertGreaterEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stderr.count("TEST_ALLOC="),
            result.stderr.count("TEST_FREE="),
            result.stderr,
        )
        return result

    def assert_request(self, result, effecter_id, fields, instance=10, eid=9):
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {"Response": "SUCCESS"})
        packet = bytes(
            [0x80 | instance, 2, 0x38, effecter_id & 255, effecter_id >> 8]
            + [len(fields) // 2]
            + fields
        )
        self.assertIn(f"TEST_REQUEST={packet.hex()} EID={eid}", result.stderr)

    def test_valid_settings_and_boundaries(self):
        for effecter_id, state, event in itertools.product(
            (1, 0x1234, 65534), (0, 2, 3), (0, 1, 255)
        ):
            with self.subTest(
                effecter_id=effecter_id, state=state, event=event
            ):
                fields = [state, event]
                result = self.invoke(
                    ["-m", "9", "-i", str(effecter_id), "-c", "1", "-d"]
                    + list(map(str, fields))
                )
                self.assert_request(result, effecter_id, fields)

    def test_composite_counts_and_instance_ids(self):
        all_fields = [0, 0, 2, 1, 3, 255, 0, 1, 2, 255, 3, 0, 2, 0, 3, 1]
        for count, instance in itertools.product(range(1, 9), (0, 10, 31)):
            with self.subTest(count=count, instance=instance):
                fields = all_fields[: 2 * count]
                result = self.invoke(
                    ["-m", "9", "-i", "4660", "-c", str(count), "-d"]
                    + list(map(str, fields)),
                    instance=instance,
                )
                self.assert_request(result, 4660, fields, instance=instance)

    def test_long_options_and_hex(self):
        result = self.invoke(
            [
                "--mctp_eid",
                "9",
                "--effecter_id",
                "0x1234",
                "--count",
                "2",
                "--data",
                "0",
                "0",
                "2",
                "0xff",
            ]
        )
        self.assert_request(result, 4660, [0, 0, 2, 255])

    def test_required_and_out_of_range_arguments(self):
        valid = ["-i", "1", "-c", "1", "-d", "0", "0"]
        cases = [[], valid[2:], valid[:2] + valid[4:], valid[:4]]
        for value in ("0", "65535", "65536", "-1", "abc", "1.5"):
            cases.append(["-i", value] + valid[2:])
        for value in ("0", "9", "255", "256", "-1", "abc", "1.5"):
            cases.append(valid[:2] + ["-c", value] + valid[4:])
        for value in ("256", "-1", "abc", "1.5"):
            cases.append(valid[:5] + [value, "0"])
        for args in cases:
            with self.subTest(args=args):
                result = self.invoke(args)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("TEST_REQUEST=", result.stderr)
                self.assertNotIn('"SUCCESS"', result.stdout)

    def test_count_data_mismatch(self):
        for count, data_size in (
            (1, 1),
            (1, 4),
            (2, 2),
            (2, 3),
            (8, 14),
            (8, 17),
        ):
            with self.subTest(count=count, data_size=data_size):
                result = self.invoke(
                    ["-i", "1", "-c", str(count), "-d"] + ["0"] * data_size
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("TEST_REQUEST=", result.stderr)
                self.assertNotIn('"SUCCESS"', result.stdout)

    def test_reject_reserved_field_values(self):
        for operational in (False, True):
            allowed = {0, 2, 3} if operational else {0, 1, 255}
            for value in set(range(256)) - allowed:
                with self.subTest(operational=operational, value=value):
                    fields = ["0"] * 16
                    fields[14 if operational else 15] = str(value)
                    result = self.invoke(["-i", "1", "-c", "8", "-d"] + fields)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertIn("Failed to encode request", result.stderr)
                    self.assertNotIn("TEST_REQUEST=", result.stderr)
                    self.assertNotIn('"SUCCESS"', result.stdout)

    def test_response_errors(self):
        for mode, diagnostic in (
            ("device-error", "cc=128"),
            ("empty", "invalid payload length"),
            ("extra", "invalid payload length"),
            ("short-header", "invalid payload length"),
            ("transport-error", "Failed to receive"),
        ):
            with self.subTest(mode=mode):
                result = self.invoke(
                    ["-i", "1", "-c", "1", "-d", "0", "0"], mode
                )
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn(diagnostic, result.stderr)
                self.assertNotIn('"SUCCESS"', result.stdout)

    def test_retries(self):
        args = ["-m", "9", "-i", "1", "-c", "1", "-d", "0", "0", "-n", "1"]
        result = self.invoke(args, "retry-once")
        self.assert_request(result, 1, [0, 0])
        self.assertEqual(result.stderr.count("TEST_REQUEST="), 2)
        self.assertEqual(result.stderr.count("TEST_ALLOC="), 1)
        result = self.invoke(args, "transport-error")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(result.stderr.count("TEST_REQUEST="), 2)

    def test_help_and_existing_commands(self):
        for command, expected in (
            ([BINARY], ("platform", "base", "bios", "fru")),
            (
                [BINARY, "platform"],
                ("SetStateEffecterEnables", "SetStateEffecterStates"),
            ),
            (COMMAND, ("--effecter_id", "--count", "--data", "255=NO_CHANGE")),
            (
                [BINARY, "platform", "GetStateSensorReadings"],
                ("--sensor_id", "--rearm", "--state_set_id"),
            ),
        ):
            result = self.invoke(["--help"], command=command)
            self.assertEqual(result.returncode, 0, result.stderr)
            for option in expected:
                self.assertIn(option, result.stdout)
            self.assertNotIn("TEST_REQUEST=", result.stderr)
        result = self.invoke(
            ["-i", "1", "-c", "1", "-d", "1", "2"],
            command=[BINARY, "platform", "SetStateEffecterStates"],
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {"Response": "SUCCESS"})
        self.assertIn("TEST_REQUEST=8a02390100010102", result.stderr)

    def test_verbose_output(self):
        result = self.invoke(["-i", "1", "-c", "1", "-d", "0", "0", "-v"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('"SUCCESS"', result.stdout)
        self.assertIn("TEST_REQUEST=8a02380100010000 EID=8", result.stderr)
        self.assertRegex(result.stdout, r"(?i)tx")
        self.assertRegex(result.stdout, r"(?i)rx")


if __name__ == "__main__":
    unittest.main(verbosity=2)
