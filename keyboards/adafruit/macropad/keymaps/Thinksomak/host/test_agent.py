#!/usr/bin/env python3
# Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Tests for the agent's Host Power part: packets, settings and what
shutdown.exe is asked to do. They never shut anything down: every test uses
a fake instead of shutdown.exe, and subprocess.run fails the test if it is
reached at all.

    python test_agent.py      (no macropad, psutil or hidapi needed)
"""

import os
import tempfile
import unittest
from unittest import mock

import main

# What the firmware (hostpower.c) sends for its first request, a shutdown
# asking for 15 s, and the reply it accepts ("started", 20 s); taken from
# the firmware itself.
FIRMWARE_REQUEST = bytes.fromhex("c6500101010f0037") + bytes(24)
FIRMWARE_ACCEPTS = bytes.fromhex("c750010101011400b9") + bytes(23)


def request(seq, action, delay=15, protocol=1, magic=0x50):
    p = bytearray(32)
    p[0:7] = bytes([0xC6, magic, protocol, seq, action, delay & 0xFF, delay >> 8])
    p[7] = main.crc8(p[:7])
    return bytes(p)


class FakeShutdown:
    """Stands in for shutdown.exe: records the arguments, returns `code`."""

    def __init__(self, code=0):
        self.code = code
        self.calls = []

    def __call__(self, args):
        self.calls.append(list(args))
        if isinstance(self.code, Exception):
            raise self.code
        return self.code


class Base(unittest.TestCase):
    def setUp(self):
        guard = mock.patch.object(main.subprocess, "run", side_effect=AssertionError("subprocess.run reached in a test"))
        guard.start()
        self.addCleanup(guard.stop)
        quiet = mock.patch.object(main, "log")
        quiet.start()
        self.addCleanup(quiet.stop)


class Packets(Base):
    def test_crc8_check_value(self):
        self.assertEqual(main.crc8(b"123456789"), 0xF4)  # CRC-8/SMBUS

    def test_reads_the_firmware_request(self):
        self.assertEqual(main.parse_power_request(FIRMWARE_REQUEST), (1, main.ACT_SHUTDOWN, 15))

    def test_report_id_in_front(self):
        self.assertEqual(main.parse_power_request(b"\x00" + FIRMWARE_REQUEST), (1, main.ACT_SHUTDOWN, 15))

    def test_other_reports_are_not_ours(self):
        self.assertIsNone(main.parse_power_request(bytes([0x01, 0x00, 0x0C]) + bytes(29)))  # a VIA reply
        self.assertIsNone(main.parse_power_request(b""))
        self.assertIsNone(main.parse_power_request(b"\xc6\x50\x01"))

    def test_damaged_request(self):
        bad = bytearray(FIRMWARE_REQUEST)
        bad[4] = main.ACT_RESTART  # CRC no longer matches
        with self.assertRaises(ValueError):
            main.parse_power_request(bytes(bad))
        with self.assertRaises(ValueError):
            main.parse_power_request(request(1, 1, magic=0x51))

    def test_other_protocol(self):
        with self.assertRaises(ValueError):
            main.parse_power_request(request(1, 1, protocol=2))

    def test_reply_the_firmware_accepts(self):
        self.assertEqual(main.build_power_reply(1, main.ACT_SHUTDOWN, main.ST_STARTED, 20), FIRMWARE_ACCEPTS)

    def test_reply_layout(self):
        r = main.build_power_reply(0x1FF, main.ACT_ABORT, main.ST_ABORTED, 300)
        self.assertEqual(len(r), 32)
        self.assertEqual(r[:8], bytes([0xC7, 0x50, 1, 0xFF, 3, 2, 300 & 0xFF, 300 >> 8]))
        self.assertEqual(r[8], main.crc8(r[:8]))
        self.assertEqual(r[9:], bytes(23))


class Power(Base):
    def test_off_by_default_says_disabled_and_runs_nothing(self):
        fake = FakeShutdown()
        for action in (main.ACT_SHUTDOWN, main.ACT_RESTART):
            self.assertEqual(main.PowerControl(False, 15, fake).handle(action), (main.ST_DISABLED, 0))
        self.assertEqual(fake.calls, [])

    def test_shutdown_and_restart_arguments(self):
        fake = FakeShutdown()
        pc = main.PowerControl(True, 15, fake)
        self.assertEqual(pc.handle(main.ACT_SHUTDOWN), (main.ST_STARTED, 15))
        self.assertEqual(pc.handle(main.ACT_RESTART), (main.ST_STARTED, 15))
        self.assertEqual([c[:3] for c in fake.calls], [["/s", "/t", "15"], ["/r", "/t", "15"]])
        for c in fake.calls:
            self.assertEqual(c[3], "/c")  # a note for Windows' message, nothing else
            self.assertEqual(len(c), 5)
            self.assertNotIn("/f", c)

    def test_cancel_always_runs(self):
        for allowed in (True, False):
            fake = FakeShutdown()
            self.assertEqual(main.PowerControl(allowed, 15, fake).handle(main.ACT_ABORT), (main.ST_ABORTED, 0))
            self.assertEqual(fake.calls, [["/a"]])

    def test_cancel_with_nothing_scheduled(self):
        fake = FakeShutdown(main.ERROR_NO_SHUTDOWN_IN_PROGRESS)
        self.assertEqual(main.PowerControl(True, 15, fake).handle(main.ACT_ABORT), (main.ST_ABORTED, 0))

    def test_cancel_refused(self):
        fake = FakeShutdown(1115)  # a shutdown is already under way
        self.assertEqual(main.PowerControl(True, 15, fake).handle(main.ACT_ABORT), (main.ST_ERROR, 0))

    def test_windows_refuses(self):
        fake = FakeShutdown(1190)  # a shutdown is already scheduled
        self.assertEqual(main.PowerControl(True, 15, fake).handle(main.ACT_SHUTDOWN), (main.ST_ERROR, 0))

    def test_shutdown_exe_missing(self):
        fake = FakeShutdown(FileNotFoundError("shutdown.exe"))
        self.assertEqual(main.PowerControl(True, 15, fake).handle(main.ACT_RESTART), (main.ST_ERROR, 0))

    def test_unknown_action_runs_nothing(self):
        fake = FakeShutdown()
        for action in (0, 4, 255):
            self.assertEqual(main.PowerControl(True, 15, fake).handle(action), (main.ST_ERROR, 0))
        self.assertEqual(fake.calls, [])

    def test_delay_never_under_5(self):
        for asked, used in ((0, 5), (-3, 5), (4, 5), (5, 5), (60, 60), (99, 99), (1000, 99)):
            fake = FakeShutdown()
            main.PowerControl(True, asked, fake).handle(main.ACT_SHUTDOWN)
            self.assertEqual(fake.calls[0][2], str(used))

    def test_answer_round_trip(self):
        pc = main.PowerControl(True, 20, FakeShutdown())
        self.assertEqual(pc.answer(FIRMWARE_REQUEST), FIRMWARE_ACCEPTS)
        self.assertIsNone(pc.answer(bytes([0x02]) + bytes(31)))  # not ours: no reply

    def test_answer_ignores_damaged(self):
        fake = FakeShutdown()
        bad = bytearray(FIRMWARE_REQUEST)
        bad[7] ^= 1
        self.assertIsNone(main.PowerControl(True, 20, fake).answer(bytes(bad)))
        self.assertEqual(fake.calls, [])

    def test_disabled_reply(self):
        reply = main.PowerControl(False, 20, FakeShutdown()).answer(FIRMWARE_REQUEST)
        self.assertEqual(reply[5], main.ST_DISABLED)
        self.assertEqual(reply[8], main.crc8(reply[:8]))


class SettingsFile(Base):
    def settings(self, power_lines):
        fd, path = tempfile.mkstemp(suffix=".ini")
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write("[power]\n" + power_lines)
        self.addCleanup(os.remove, path)
        return main.Settings(path)

    def test_defaults(self):
        s = main.Settings(None)
        self.assertFalse(s.allow_power)
        self.assertEqual(s.power_delay, 15)
        self.assertEqual(s.problems, [])

    def test_shipped_file_keeps_it_off(self):
        s = main.Settings(os.path.join(os.path.dirname(os.path.abspath(__file__)), main.CONFIG_NAME))
        self.assertIsNotNone(s.path)
        self.assertFalse(s.allow_power)
        self.assertEqual(s.power_delay, 15)

    def test_on(self):
        s = self.settings("allow_power_commands = true ; a comment\npower_delay = 30\n")
        self.assertTrue(s.allow_power)
        self.assertEqual(s.power_delay, 30)

    def test_unclear_value_stays_off(self):
        s = self.settings("allow_power_commands = ture\n")
        self.assertFalse(s.allow_power)
        self.assertEqual(len(s.problems), 1)

    def test_delay_limits(self):
        self.assertEqual(self.settings("power_delay = 0\n").power_delay, 5)
        self.assertEqual(self.settings("power_delay = 500\n").power_delay, 99)
        s = self.settings("power_delay = soon\n")
        self.assertEqual(s.power_delay, 15)
        self.assertEqual(len(s.problems), 1)


class Listening(Base):
    class FakeDevice:
        def __init__(self, reports):
            self.h = object()
            self.reports = list(reports)
            self.written = []

        def read(self, timeout_ms):
            assert timeout_ms > 0
            return self.reports.pop(0) if self.reports else None

        def write(self, packet):
            self.written.append(packet)

        def close(self):
            self.h = None

    class NoStop:
        def wait(self, seconds):
            return False

    def test_answers_while_waiting(self):
        dev = self.FakeDevice([bytes([0x01]) + bytes(31), FIRMWARE_REQUEST])
        pc = main.PowerControl(True, 20, FakeShutdown())
        self.assertFalse(main.listen(self.NoStop(), dev, pc, 0.3))
        self.assertEqual(dev.written, [FIRMWARE_ACCEPTS])

    def test_lost_device_is_closed(self):
        dev = self.FakeDevice([])

        def broken(timeout_ms):
            raise OSError("read error")

        dev.read = broken
        self.assertFalse(main.listen(self.NoStop(), dev, main.PowerControl(False, 15, FakeShutdown()), 0.1))
        self.assertIsNone(dev.h)


if __name__ == "__main__":
    unittest.main()
