#!/usr/bin/env python3
# Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Macropad sysinfo agent.

Sends PC stats to the Adafruit Macropad (Thinksomak keymap) over Raw HID:
CPU %, RAM %, disk read/write and network download/upload rates, and
optionally NVIDIA GPU %. One 32-byte packet per interval (default 1 s).
The packet layout is in README.md and in ../sysinfo.h.

Host Power: between packets it listens for the macropad's requests to shut
down or restart this PC (SYSTEM layer, POWER key) and to cancel that. It
acts on them only with allow_power_commands = true in the settings (off by
default), through Windows' own shutdown.exe; a cancel is always carried
out. Layout in README.md and ../hostpower.h.

    python main.py              run (Ctrl+C to stop)
    python main.py --console    also log every packet to the console
    python main.py --dry-run    no device: print the packets instead
    python main.py --list       list the macropad's HID interfaces
    python main.py --stop       stop the agent that is already running
    python main.py --config F   use settings file F

Only numbers go to the keyboard. The only thing read back is a Host Power
request.
"""

import argparse
import configparser
import logging
import logging.handlers
import os
import struct
import subprocess
import sys
import threading
import time

try:
    import psutil
except ImportError:  # reported in main(), so the message reaches the log
    psutil = None

try:
    import hid  # the "hidapi" package
except ImportError:
    hid = None

APP_NAME = "MacropadSysinfo"
MUTEX_NAME = "Local\\MacropadSysinfoAgent"
STOP_EVENT_NAME = "Local\\MacropadSysinfoAgentStop"
CONFIG_NAME = "macropad_sysinfo.ini"

# Packet (see ../sysinfo.h)
CMD = 0xC5
MAGIC = 0x53
PROTOCOL = 1
FLAG_GPU_VALID = 0x01
FLAG_LEAVING = 0x02
PACKET_SIZE = 32

# Host Power packets (see ../hostpower.h)
POWER_REQUEST = 0xC6  # keyboard -> PC
POWER_REPLY = 0xC7  # PC -> keyboard
POWER_MAGIC = 0x50  # 'P'
POWER_PROTOCOL = 1
ACT_SHUTDOWN, ACT_RESTART, ACT_ABORT = 1, 2, 3
ST_STARTED, ST_ABORTED, ST_DISABLED, ST_ERROR = 1, 2, 3, 4
ACTION_NAMES = {ACT_SHUTDOWN: "shutdown", ACT_RESTART: "restart", ACT_ABORT: "cancel"}
STATUS_NAMES = {ST_STARTED: "started", ST_ABORTED: "cancelled", ST_DISABLED: "disabled", ST_ERROR: "error"}
POWER_DELAY_DEFAULT, POWER_DELAY_MIN, POWER_DELAY_MAX = 15, 5, 99  # seconds; never 0
ERROR_NO_SHUTDOWN_IN_PROGRESS = 1116  # shutdown /a when nothing is scheduled
READ_SLICE = 0.1  # s; how long one read waits, so stop and the next tick stay on time

DEFAULTS = {
    "device": {"vid": "0x239A", "pid": "0x0108", "usage_page": "0xFF60", "usage": "0x61"},
    "agent": {"interval": "1.0", "gpu": "off", "nic": "", "log_level": "INFO"},
    "power": {"allow_power_commands": "false", "power_delay": str(POWER_DELAY_DEFAULT)},
}

log = logging.getLogger("sysinfo")


# ---- Paths, settings and logging ------------------------------------------


def app_dir():
    """Folder of the exe (PyInstaller) or of this script."""
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


def data_dir():
    base = os.environ.get("LOCALAPPDATA") or os.path.join(os.path.expanduser("~"), ".local", "state")
    path = os.path.join(base, APP_NAME)
    os.makedirs(path, exist_ok=True)
    return path


class Settings:
    def __init__(self, path):
        cp = configparser.ConfigParser(inline_comment_prefixes=(";", "#"))
        cp.read_dict(DEFAULTS)
        self.path = path if path and os.path.isfile(path) else None
        if self.path:
            cp.read(self.path, encoding="utf-8")
        dev, agent = cp["device"], cp["agent"]
        self.vid = int(dev.get("vid"), 0)
        self.pid = int(dev.get("pid"), 0)
        self.usage_page = int(dev.get("usage_page"), 0)
        self.usage = int(dev.get("usage"), 0)
        self.interval = min(25.0, max(0.25, float(agent.get("interval"))))
        self.gpu = agent.get("gpu").strip().lower()
        self.nic = agent.get("nic").strip()
        self.log_level = agent.get("log_level").strip().upper()
        self.problems = []  # logged once logging is set up

        # Host Power stays off unless the setting clearly says true.
        power = cp["power"]
        allow = power.get("allow_power_commands").strip().lower()
        self.allow_power = allow in ("true", "yes", "on", "1")
        if allow not in ("true", "yes", "on", "1", "false", "no", "off", "0"):
            self.problems.append("allow_power_commands = %r is not true or false; Host Power stays off" % allow)
        try:
            delay = int(power.get("power_delay"))
        except ValueError:
            self.problems.append("power_delay = %r is not a whole number; using %d" % (power.get("power_delay"), POWER_DELAY_DEFAULT))
            delay = POWER_DELAY_DEFAULT
        self.power_delay = min(POWER_DELAY_MAX, max(POWER_DELAY_MIN, delay))
        if self.power_delay != delay:
            self.problems.append("power_delay = %d is outside %d-%d; using %d" % (delay, POWER_DELAY_MIN, POWER_DELAY_MAX, self.power_delay))


def setup_logging(level_name, console):
    log.setLevel(logging.DEBUG)
    fmt = logging.Formatter("%(asctime)s %(levelname)-7s %(message)s")
    file_handler = logging.handlers.RotatingFileHandler(
        os.path.join(data_dir(), "agent.log"), maxBytes=256 * 1024, backupCount=3, encoding="utf-8"
    )
    file_handler.setFormatter(fmt)
    file_handler.setLevel(getattr(logging, level_name, logging.INFO))
    log.addHandler(file_handler)
    if sys.stderr is not None:  # no console in the windowless exe
        stream = logging.StreamHandler(sys.stderr)
        stream.setFormatter(fmt)
        stream.setLevel(logging.DEBUG if console else logging.INFO)
        log.addHandler(stream)


# ---- Single instance and stop signal (Windows named kernel objects) --------


class Instance:
    """One agent at a time, and a way for `--stop` to end it cleanly."""

    def __init__(self):
        self._mutex = None
        self._stop = None
        self._lock_file = None
        self._event = threading.Event()  # used when not on Windows
        if os.name == "nt":
            import ctypes
            from ctypes import wintypes

            self._k32 = ctypes.WinDLL("kernel32", use_last_error=True)
            self._k32.CreateMutexW.restype = wintypes.HANDLE
            self._k32.CreateEventW.restype = wintypes.HANDLE
            self._k32.OpenEventW.restype = wintypes.HANDLE

    def acquire(self):
        if os.name == "nt":
            import ctypes

            self._mutex = self._k32.CreateMutexW(None, False, MUTEX_NAME)
            if not self._mutex or ctypes.get_last_error() == 183:  # ERROR_ALREADY_EXISTS
                return False
            self._stop = self._k32.CreateEventW(None, True, False, STOP_EVENT_NAME)
            return True
        import fcntl

        self._lock_file = open(os.path.join(data_dir(), "agent.lock"), "w")
        try:
            fcntl.flock(self._lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            return False
        return True

    def request_stop(self):
        self._event.set()
        if self._stop:
            self._k32.SetEvent(self._stop)

    def stopping(self):
        return self._event.is_set()

    def wait(self, seconds):
        """Sleeps up to `seconds`; True if a stop was requested."""
        if self._event.is_set():
            return True
        if self._stop:
            # Short slices keep Ctrl+C responsive.
            end = time.monotonic() + seconds
            while True:
                if self._k32.WaitForSingleObject(self._stop, int(max(0.0, min(end - time.monotonic(), 0.2)) * 1000)) == 0:
                    self._event.set()  # --stop from another process
                    return True
                if self._event.is_set():
                    return True
                if time.monotonic() >= end:
                    return False
        return self._event.wait(seconds)

    @staticmethod
    def signal_running():
        """`--stop`: ask the running agent to quit. True if one was found."""
        if os.name != "nt":
            return False
        import ctypes
        from ctypes import wintypes

        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        k32.OpenEventW.restype = wintypes.HANDLE
        handle = k32.OpenEventW(0x0002, False, STOP_EVENT_NAME)  # EVENT_MODIFY_STATE
        if not handle:
            return False
        k32.SetEvent(handle)
        k32.CloseHandle(handle)
        return True


# ---- Stats ----------------------------------------------------------------


class GpuPoller(threading.Thread):
    """NVIDIA GPU load from nvidia-smi every 2 s, in the background.
    Turns itself off for the session after the first failure."""

    def __init__(self, stop):
        super().__init__(daemon=True)
        self.value = None
        self._stop_check = stop

    def run(self):
        flags = 0x08000000 if os.name == "nt" else 0  # CREATE_NO_WINDOW: no console flash
        cmd = ["nvidia-smi", "--query-gpu=utilization.gpu", "--format=csv,noheader,nounits"]
        while not self._stop_check():
            try:
                out = subprocess.run(cmd, capture_output=True, text=True, timeout=3, creationflags=flags)
                if out.returncode != 0:
                    raise OSError(out.stderr.strip() or "exit code %d" % out.returncode)
                self.value = max(0, min(100, int(out.stdout.split()[0])))
            except (OSError, ValueError, IndexError, subprocess.SubprocessError) as e:
                log.warning("GPU load off: nvidia-smi failed (%s)", e)
                self.value = None
                return
            time.sleep(2)


class Sampler:
    """Rates are counter deltas over the real time between two samples."""

    def __init__(self, nic):
        self.nic = nic
        psutil.cpu_percent(None)  # first call only primes the counter
        self.t = time.monotonic()
        self.disk = self._disk()
        self.net = self._net()
        if nic and self.net is None:
            log.warning("network adapter %r not found; using all adapters", nic)
            self.nic = ""
            self.net = self._net()

    @staticmethod
    def _disk():
        try:
            c = psutil.disk_io_counters()
            return (c.read_bytes, c.write_bytes) if c else None
        except (OSError, RuntimeError):
            return None

    def _net(self):
        try:
            if self.nic:
                c = psutil.net_io_counters(pernic=True).get(self.nic)
            else:
                c = psutil.net_io_counters()
            return (c.bytes_recv, c.bytes_sent) if c else None
        except (OSError, RuntimeError):
            return None

    @staticmethod
    def _rates(new, old, dt):
        if new is None or old is None:
            return [0.0, 0.0]
        # A counter that went backwards (driver reset, sleep) counts as 0.
        return [max(0, n - o) / dt / 1024.0 for n, o in zip(new, old)]

    def sample(self):
        now = time.monotonic()
        dt = max(now - self.t, 0.001)
        disk, net = self._disk(), self._net()
        rates = self._rates(disk, self.disk, dt) + self._rates(net, self.net, dt)
        self.t, self.disk, self.net = now, disk, net
        cpu = psutil.cpu_percent(None)
        ram = psutil.virtual_memory().percent
        return cpu, ram, rates


# ---- Packet ---------------------------------------------------------------


def rate16(kbps):
    """KB/s in 16 bits: bit 15 set = the value is in MB/s."""
    kbps = max(0, int(round(kbps)))
    if kbps <= 0x7FFF:
        return kbps
    return 0x8000 | min(0x7FFF, int(round(kbps / 1024.0)))


def build_packet(seq, cpu, ram, gpu, rates, interval, leaving=False):
    def pct(v):
        return max(0, min(100, int(round(v or 0))))

    p = bytearray(PACKET_SIZE)
    p[0], p[1], p[2], p[3] = CMD, MAGIC, PROTOCOL, seq & 0xFF
    p[4] = (FLAG_GPU_VALID if gpu is not None else 0) | (FLAG_LEAVING if leaving else 0)
    p[5], p[6], p[7] = pct(cpu), pct(ram), pct(gpu)
    for i, r in enumerate(rates):
        struct.pack_into("<H", p, 8 + 2 * i, rate16(r))
    p[16] = max(1, min(255, int(round(interval * 10))))
    return bytes(p)


def describe(cpu, ram, gpu, rates):
    names = ("disk r", "disk w", "net d", "net u")
    parts = ["cpu %3.0f%%" % cpu, "ram %3.0f%%" % ram]
    if gpu is not None:
        parts.append("gpu %3d%%" % gpu)
    parts += ["%s %.0f KB/s" % (n, r) for n, r in zip(names, rates)]
    return "  ".join(parts)


def crc8(data):
    """CRC-8, polynomial 0x07, from 0: the same as the firmware's."""
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def parse_power_request(report):
    """(sequence, action, delay asked for) from a report the keyboard sent.
    None when it is not a Host Power request (VIA's replies come in here
    too); ValueError when it is one but damaged or of another protocol."""
    r = bytes(report)
    if len(r) == PACKET_SIZE + 1 and r[0] == 0:  # report ID left in front
        r = r[1:]
    if len(r) < 8 or r[0] != POWER_REQUEST:
        return None
    if r[1] != POWER_MAGIC or crc8(r[:7]) != r[7]:
        raise ValueError("damaged request: %s" % r[:8].hex(" "))
    if r[2] != POWER_PROTOCOL:
        raise ValueError("request of protocol %d, this agent knows %d: update the agent" % (r[2], POWER_PROTOCOL))
    return r[3], r[4], r[5] | r[6] << 8


def build_power_reply(seq, action, status, delay):
    """The reply to one request: sequence and action echoed, the status and
    the delay this PC uses."""
    p = bytearray(PACKET_SIZE)
    p[0], p[1], p[2], p[3], p[4], p[5] = POWER_REPLY, POWER_MAGIC, POWER_PROTOCOL, seq & 0xFF, action & 0xFF, status
    struct.pack_into("<H", p, 6, max(0, min(0xFFFF, delay)))
    p[8] = crc8(p[:8])
    return bytes(p)


# ---- Device ---------------------------------------------------------------


class Device:
    def __init__(self, s):
        self.s = s
        self.h = None

    def interfaces(self):
        return hid.enumerate(self.s.vid, self.s.pid)

    def find_path(self):
        for d in self.interfaces():
            if d.get("usage_page") == self.s.usage_page and d.get("usage") == self.s.usage:
                return d["path"]
        return None

    def open(self):
        path = self.find_path()
        if not path:
            return False
        if hasattr(hid, "device"):  # "hidapi" package (requirements.txt)
            h = hid.device()
            h.open_path(path)
        else:  # the ctypes "hid" package, as shipped with QMK MSYS
            h = hid.Device(path=path)
        self.h = h
        return True

    def write(self, packet):
        # Report ID 0 first: QMK's Raw HID interface has no numbered reports.
        try:
            n = self.h.write(b"\x00" + packet)
        except Exception as e:  # each hid package raises its own error type
            raise OSError(str(e)) from e
        if n is not None and n < 0:
            raise OSError("write failed")

    def read(self, timeout_ms):
        """One report from the keyboard, or None after timeout_ms. Never pass
        0: both hid packages then wait for ever."""
        try:
            data = self.h.read(64, max(1, int(timeout_ms)))
        except Exception as e:  # each hid package raises its own error type
            raise OSError(str(e)) from e
        return bytes(data) if data else None

    def close(self):
        if self.h is not None:
            try:
                self.h.close()
            except (OSError, ValueError):
                pass
            self.h = None


# ---- Host Power -----------------------------------------------------------


class PowerControl:
    """Shuts down or restarts this PC when the macropad asks, only if the
    settings allow it, with Windows' own shutdown.exe and nothing else:
        shutdown   /s /t <delay> /c <note>
        restart    /r /t <delay> /c <note>
        cancel     /a   (always allowed: it can only keep the PC on)
    With /t above 0, Windows implies /f: open apps are closed without
    asking, so unsaved work is lost. The delay is never under 5 s."""

    def __init__(self, allowed, delay, runner=None):
        self.allowed = allowed
        self.delay = min(POWER_DELAY_MAX, max(POWER_DELAY_MIN, int(delay)))
        self._run = runner or run_shutdown_exe  # tests pass a fake

    def handle(self, action):
        """Carries out one action: (status, delay in s) for the reply."""
        if action == ACT_ABORT:
            code = self._shutdown(["/a"])
            # 1116: nothing was scheduled, so nothing will happen either.
            return (ST_ABORTED if code in (0, ERROR_NO_SHUTDOWN_IN_PROGRESS) else ST_ERROR), 0
        if action not in (ACT_SHUTDOWN, ACT_RESTART):
            return ST_ERROR, 0
        if not self.allowed:
            return ST_DISABLED, 0
        what = "Shutdown" if action == ACT_SHUTDOWN else "Restart"
        note = "%s from the macropad in %d s. To cancel: its top-right key, or shutdown /a" % (what, self.delay)
        code = self._shutdown(["/s" if action == ACT_SHUTDOWN else "/r", "/t", str(self.delay), "/c", note])
        return (ST_STARTED, self.delay) if code == 0 else (ST_ERROR, 0)

    def _shutdown(self, args):
        """Exit code of shutdown.exe, or None if it could not run."""
        try:
            return self._run(args)
        except (OSError, subprocess.SubprocessError) as e:
            log.error("Host Power: shutdown %s failed to run (%s)", args[0], e)
            return None

    def answer(self, report):
        """The reply to a Host Power request, or None if the report is not
        one."""
        try:
            req = parse_power_request(report)
        except ValueError as e:
            log.warning("Host Power: %s", e)
            return None
        if req is None:
            return None
        seq, action, asked = req
        name = ACTION_NAMES.get(action, "unknown action %d" % action)
        log.debug("Host Power: request %d, %s, %d s asked for", seq, name, asked)
        status, delay = self.handle(action)
        if status == ST_STARTED:
            log.warning("Host Power: %s in %d s, asked by the macropad", name, delay)
        elif status == ST_DISABLED:
            log.warning("Host Power: %s asked, but allow_power_commands is off in the settings", name)
        elif status == ST_ERROR:
            log.warning("Host Power: %s asked -> error", name)
        else:
            log.info("Host Power: %s asked -> %s", name, STATUS_NAMES[status])
        return build_power_reply(seq, action, status, delay)


def run_shutdown_exe(args):
    """Runs Windows' shutdown.exe with args, as a list (never through a
    shell), from System32 (never from PATH). Returns its exit code."""
    if os.name != "nt":
        raise OSError("Host Power needs Windows")
    exe = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "shutdown.exe")
    # The timeout stays well under the keyboard's HOSTPOWER_LATE_MS (20 s).
    out = subprocess.run([exe] + list(args), capture_output=True, timeout=10, creationflags=0x08000000)  # CREATE_NO_WINDOW
    if out.returncode != 0:
        msg = (out.stderr or out.stdout).decode("oem", "replace").strip()
        quiet = out.returncode == ERROR_NO_SHUTDOWN_IN_PROGRESS  # nothing to cancel: fine
        log.log(logging.INFO if quiet else logging.WARNING, "Host Power: shutdown %s: exit code %d %s", args[0], out.returncode, msg)
    return out.returncode


def listen(inst, dev, power, seconds):
    """Waits like Instance.wait(), and meanwhile answers the macropad's
    Host Power requests. True if a stop was requested."""
    end = time.monotonic() + seconds
    while dev is not None and dev.h is not None:
        if inst.wait(0):
            return True
        left = end - time.monotonic()
        if left <= 0:
            return False
        try:
            report = dev.read(min(left, READ_SLICE) * 1000)
            reply = power.answer(report) if report else None
            if reply:
                dev.write(reply)
        except (OSError, ValueError) as e:
            log.warning("macropad lost (%s); reconnecting", e)
            dev.close()
        except Exception:  # keep running whatever happens
            log.exception("unexpected error; carrying on")
    return inst.wait(max(0.0, end - time.monotonic()))


# ---- Main loop ------------------------------------------------------------


def run(s, inst, dry_run):
    sampler = Sampler(s.nic)
    gpu = None
    if s.gpu == "nvidia":
        gpu = GpuPoller(inst.stopping)
        gpu.start()
    dev = None if dry_run else Device(s)
    power = PowerControl(s.allow_power, s.power_delay)
    seq = 0
    retry_at, backoff, waiting_logged = 0.0, 1.0, False
    next_tick = time.monotonic()

    log.info(
        "started: interval %.2f s, device %04X:%04X usage %04X/%02X%s",
        s.interval, s.vid, s.pid, s.usage_page, s.usage, " (dry run)" if dry_run else "",
    )
    if s.allow_power:
        log.info("Host Power: on, %d s delay (allow_power_commands = true)", power.delay)
    else:
        log.info("Host Power: off; the macropad's POWER menu gets \"disabled\" (allow_power_commands = false)")
    while True:
        try:
            cpu, ram, rates = sampler.sample()
            g = gpu.value if gpu else None
            packet = build_packet(seq, cpu, ram, g, rates, s.interval)
            log.debug("%s", describe(cpu, ram, g, rates))
            if dry_run:
                log.info("packet %s", packet.hex(" "))
            else:
                now = time.monotonic()
                if dev.h is None and now >= retry_at:
                    if dev.open():
                        log.info("macropad connected")
                        backoff, waiting_logged = 1.0, False
                    else:
                        if not waiting_logged:
                            log.info("waiting for the macropad (%04X:%04X)", s.vid, s.pid)
                            waiting_logged = True
                        retry_at, backoff = now + backoff, min(backoff * 2, 5.0)
                if dev.h is not None:
                    try:
                        dev.write(packet)
                    except (OSError, ValueError) as e:
                        log.warning("macropad lost (%s); reconnecting", e)
                        dev.close()
                        retry_at = time.monotonic() + 1.0
            seq += 1
        except Exception:  # keep running whatever happens in one tick
            log.exception("unexpected error; carrying on")

        next_tick += s.interval
        delay = next_tick - time.monotonic()
        if delay < -s.interval:  # fell far behind (PC slept): start again from now
            next_tick, delay = time.monotonic(), 0
        if listen(inst, dev, power, max(0.0, delay)):
            break

    if dev is not None and dev.h is not None:
        try:  # tell the macropad straight away instead of letting it time out
            dev.write(build_packet(seq, 0, 0, None, [0, 0, 0, 0], s.interval, leaving=True))
        except (OSError, ValueError):
            pass
        dev.close()
    log.info("stopped")


def main():
    # The windowless exe has no stdout/stderr; give argparse somewhere to write.
    if sys.stdout is None:
        sys.stdout = open(os.devnull, "w")
    ap = argparse.ArgumentParser(description="Send PC stats to the macropad over Raw HID.")
    ap.add_argument("--config", help="settings file (default: %s next to this program)" % CONFIG_NAME)
    ap.add_argument("--console", action="store_true", help="log every packet to the console")
    ap.add_argument("--dry-run", action="store_true", help="no device: log the packets instead")
    ap.add_argument("--list", action="store_true", help="list the macropad's HID interfaces and exit")
    ap.add_argument("--stop", action="store_true", help="stop the running agent and exit")
    args = ap.parse_args()

    if args.stop:
        found = Instance.signal_running()
        print("stop sent" if found else "no running agent found")
        return 0 if found else 1

    s = Settings(args.config or os.path.join(app_dir(), CONFIG_NAME))
    setup_logging(s.log_level, args.console)
    log.info("settings: %s", s.path or "defaults (no %s found)" % CONFIG_NAME)
    for problem in s.problems:
        log.warning("settings: %s", problem)
    if psutil is None:
        log.error("psutil is missing: pip install -r requirements.txt")
        return 2
    if hid is None and not args.dry_run:
        log.error("hidapi is missing: pip install -r requirements.txt")
        return 2

    if args.list:
        found = Device(s).interfaces()
        for d in found:
            log.info("usage %04X/%02X  interface %s  %s", d.get("usage_page", 0), d.get("usage", 0),
                     d.get("interface_number"), d.get("product_string"))
        if not found:
            log.info("no HID interface with %04X:%04X", s.vid, s.pid)
        return 0

    inst = Instance()
    if not inst.acquire():
        log.info("another agent is already running; exiting")
        return 0

    import signal

    def on_signal(signum, frame):
        inst.request_stop()

    for name in ("SIGINT", "SIGTERM", "SIGBREAK"):
        if hasattr(signal, name):
            signal.signal(getattr(signal, name), on_signal)

    run(s, inst, args.dry_run)
    return 0


if __name__ == "__main__":
    sys.exit(main())
