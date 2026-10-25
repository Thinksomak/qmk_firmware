#!/usr/bin/env python3
# Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Macropad sysinfo agent.

Sends PC stats to the Adafruit Macropad (Thinksomak keymap) over Raw HID:
CPU %, RAM %, disk read/write and network download/upload rates, and
optionally NVIDIA GPU %. One 32-byte packet per interval (default 1 s).
The packet layout is in README.md and in ../sysinfo.h.

    python main.py              run (Ctrl+C to stop)
    python main.py --console    also log every packet to the console
    python main.py --dry-run    no device: print the packets instead
    python main.py --list       list the macropad's HID interfaces
    python main.py --stop       stop the agent that is already running
    python main.py --config F   use settings file F

Only numbers are sent. Nothing is read back from the keyboard.
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

DEFAULTS = {
    "device": {"vid": "0x239A", "pid": "0x0108", "usage_page": "0xFF60", "usage": "0x61"},
    "agent": {"interval": "1.0", "gpu": "off", "nic": "", "log_level": "INFO"},
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

    def close(self):
        if self.h is not None:
            try:
                self.h.close()
            except (OSError, ValueError):
                pass
            self.h = None


# ---- Main loop ------------------------------------------------------------


def run(s, inst, dry_run):
    sampler = Sampler(s.nic)
    gpu = None
    if s.gpu == "nvidia":
        gpu = GpuPoller(inst.stopping)
        gpu.start()
    dev = None if dry_run else Device(s)
    seq = 0
    retry_at, backoff, waiting_logged = 0.0, 1.0, False
    next_tick = time.monotonic()

    log.info(
        "started: interval %.2f s, device %04X:%04X usage %04X/%02X%s",
        s.interval, s.vid, s.pid, s.usage_page, s.usage, " (dry run)" if dry_run else "",
    )
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
        if inst.wait(max(0.0, delay)):
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
