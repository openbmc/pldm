import itertools
import json
import os
import subprocess
import sys
import unittest

BINARY = sys.argv.pop(1)
COMMAND = [BINARY, "platform", "SetNumericSensorEnable"]
VALID = ["-m", "9", "-i", "4660", "-o", "0", "-e", "2"]


class NumericSensorEnableCLI(unittest.TestCase):
    def invoke(
        self,
        args,
        mode="success",
        instance=10,
        completion=0,
        command=COMMAND,
        alloc_fail=False,
    ):
        environment = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith("PLDM_TEST_")
        }
        environment.update(
            {
                "PLDM_TEST_RESPONSE": mode,
                "PLDM_TEST_INSTANCE_ID": str(instance),
                "PLDM_TEST_COMPLETION_CODE": str(completion),
            }
        )
        if alloc_fail:
            environment["PLDM_TEST_ALLOC_FAIL"] = "1"
        result = subprocess.run(
            [*command, *args],
            env=environment,
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
        self.assertNotIn("runtime error:", result.stderr)
        self.assertNotIn("ERROR: AddressSanitizer", result.stderr)
        return result

    def assert_request(
        self, result, sensor_id=4660, state=0, event=2, instance=10, eid=9
    ):
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {"Response": "SUCCESS"})
        packet = bytes(
            [
                0x80 | instance,
                2,
                0x10,
                sensor_id & 255,
                sensor_id >> 8,
                state,
                event,
            ]
        )
        self.assertIn(f"TEST_REQUEST={packet.hex()} EID={eid}", result.stderr)

    def test_valid_settings_and_instance_ids(self):
        for sensor_id, state, event, instance in itertools.product(
            (1, 0x1234, 65534), range(3), range(5), (0, 10, 31)
        ):
            with self.subTest(
                sensor_id=sensor_id,
                state=state,
                event=event,
                instance=instance,
            ):
                result = self.invoke(
                    [
                        "-m",
                        "9",
                        "-i",
                        str(sensor_id),
                        "-o",
                        str(state),
                        "-e",
                        str(event),
                    ],
                    instance=instance,
                )
                self.assert_request(result, sensor_id, state, event, instance)

    def test_long_options_and_hex(self):
        result = self.invoke(
            [
                "--mctp_eid",
                "9",
                "--sensor_id",
                "0x1234",
                "--op_state",
                "0x0",
                "--event_enable",
                "0x2",
            ]
        )
        self.assert_request(result)

    def test_missing_arguments(self):
        for args in (
            [],
            VALID[:2] + VALID[4:],
            VALID[:4] + VALID[6:],
            VALID[:6],
        ):
            with self.subTest(args=args):
                result = self.invoke(args)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("TEST_REQUEST=", result.stderr)
                self.assertNotIn("SUCCESS", result.stdout)

    def test_invalid_argument_types_and_ranges(self):
        for option, values in (
            ("-i", ("-1", "65536", "abc", "1.5")),
            ("-o", ("-1", "256", "abc", "1.5")),
            ("-e", ("-1", "256", "abc", "1.5")),
            ("-m", ("-1", "256", "abc", "1.5")),
        ):
            for value in values:
                with self.subTest(option=option, value=value):
                    args = VALID.copy()
                    args[args.index(option) + 1] = value
                    result = self.invoke(args)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertNotIn("TEST_REQUEST=", result.stderr)
                    self.assertNotIn("SUCCESS", result.stdout)

    def test_reserved_state_values(self):
        for option, first_reserved in (("-o", 3), ("-e", 5)):
            for value in range(first_reserved, 256):
                with self.subTest(option=option, value=value):
                    args = VALID.copy()
                    args[args.index(option) + 1] = str(value)
                    result = self.invoke(args)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertIn("Failed to encode request", result.stderr)
                    self.assertNotIn("TEST_REQUEST=", result.stderr)
                    self.assertNotIn("SUCCESS", result.stdout)

    def test_completion_codes(self):
        for completion in range(1, 256):
            with self.subTest(completion=completion):
                result = self.invoke(VALID, completion=completion)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn(f"cc={completion}", result.stderr)
                self.assertNotIn("SUCCESS", result.stdout)

    def test_malformed_responses_and_transport_error(self):
        for mode in (
            "null-response",
            "zero-packet",
            "one-byte-header",
            "short-header",
            "empty",
            "extra",
            "transport-error",
        ):
            with self.subTest(mode=mode):
                result = self.invoke(VALID, mode)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertRegex(
                    result.stderr, "invalid payload length|Failed to receive"
                )
                self.assertNotIn("SUCCESS", result.stdout)

    def test_retries(self):
        result = self.invoke(VALID + ["-n", "1"], "retry-once")
        self.assert_request(result)
        self.assertEqual(result.stderr.count("TEST_REQUEST="), 2)
        self.assertEqual(result.stderr.count("TEST_ALLOC="), 1)
        for retries in (0, 1, 255):
            with self.subTest(retries=retries):
                result = self.invoke(
                    VALID + ["-n", str(retries)], "transport-error"
                )
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertEqual(
                    result.stderr.count("TEST_REQUEST="), retries + 1
                )

    def test_instance_allocation_failure(self):
        result = self.invoke(VALID, alloc_fail=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertNotIn("TEST_REQUEST=", result.stderr)
        self.assertNotIn("SUCCESS", result.stdout)

    def test_help_and_existing_command(self):
        for command, expected in (
            ([BINARY], ("platform", "base", "bios", "fru")),
            (
                [BINARY, "platform"],
                ("SetNumericSensorEnable", "SetStateEffecterStates"),
            ),
            (COMMAND, ("--sensor_id", "--op_state", "--event_enable")),
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

    def test_shared_transport_regression(self):
        command = [BINARY, "platform", "SetStateEffecterStates"]
        args = ["-i", "1", "-c", "1", "-d", "1", "2"]
        for mode in (
            "null-response",
            "zero-packet",
            "one-byte-header",
            "short-header",
            "transport-error",
        ):
            with self.subTest(mode=mode):
                result = self.invoke(args, mode, command=command)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn("Failed to receive", result.stderr)
                self.assertNotIn("SUCCESS", result.stdout)
        result = self.invoke(
            args + ["-n", "255"], "transport-error", command=command
        )
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(result.stderr.count("TEST_REQUEST="), 256)

    def test_verbose_output(self):
        result = self.invoke(VALID + ["-v"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('"SUCCESS"', result.stdout)
        self.assertIn("TEST_REQUEST=8a021034120002 EID=9", result.stderr)
        self.assertRegex(result.stdout, r"(?i)tx")
        self.assertRegex(result.stdout, r"(?i)rx")


if __name__ == "__main__":
    unittest.main(verbosity=2)
