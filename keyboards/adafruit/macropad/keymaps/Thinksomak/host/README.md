# Macropad sysinfo agent

A small program for the PC that sends live system stats to the Adafruit
Macropad (Thinksomak keymap). The macropad shows them in its SYSINFO view
(SYSTEM layer, SYSINFO key) and, if you turn it on, in a short strip on the
other layers.

What it sends, once a second:

| Value | Source (psutil) |
|---|---|
| CPU load, % | `cpu_percent()` |
| RAM in use, % | `virtual_memory().percent` |
| Disk read and write, KB/s | change in `disk_io_counters()` over the elapsed time |
| Network download and upload, KB/s | change in `net_io_counters()` over the elapsed time |
| GPU load, % (optional, NVIDIA only) | `nvidia-smi`, every 2 s |

Only these numbers go to the keyboard, and nothing is read back from it.

## Files

| File | What it is |
|---|---|
| `main.py` | the agent (one script) |
| `macropad_sysinfo.ini` | settings; keep it next to `main.py` or the exe |
| `requirements.txt` | `psutil` and `hidapi` |
| `build.bat` | builds a single `.exe` with PyInstaller |

## Run it with Python

You need Python 3.8 or newer from python.org (tick "Add python.exe to PATH"
when installing). The Python inside QMK MSYS does not work for this:
psutil does not build there.

```bat
cd keyboards\adafruit\macropad\keymaps\Thinksomak\host
py -3 -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
.venv\Scripts\python main.py --console
```

`--console` prints every packet. Stop with Ctrl+C. Other options:

| Option | What it does |
|---|---|
| `--console` | log every packet to the console (debug level) |
| `--dry-run` | no keyboard needed: log the packets instead of sending them |
| `--list` | list the macropad's HID interfaces, to check it is found |
| `--stop` | stop the agent that is running (also the windowless exe) |
| `--config FILE` | use another settings file |

## Build the exe

```bat
build.bat
```

This makes `.venv` (if needed), installs the requirements and PyInstaller,
and builds `dist\macropad-sysinfo.exe`: one file, no window. It copies
`macropad_sysinfo.ini` next to it. Copy both files wherever you like, for
example `%LOCALAPPDATA%\Programs\MacropadSysinfo\`.

`build.bat --console` builds `dist\macropad-sysinfo-console.exe` instead,
which opens a window and logs to it. Use it when something does not work.

Some antivirus tools distrust new one-file PyInstaller exes. If yours
blocks it, allow the file, or run `main.py` with Python instead.

## Start it at login

Either of these works. Use one, not both: a second copy exits by itself
anyway (only one agent runs at a time).

**Startup folder (simplest).** Press Win+R, type `shell:startup`, Enter.
Right-click in the folder, New, Shortcut, and point it at
`macropad-sysinfo.exe`.

**Task Scheduler (starts even if Explorer is slow, restarts on failure).**
1. Open Task Scheduler, Create Task.
2. General: name "Macropad sysinfo", "Run only when user is logged on".
3. Triggers: New, "At log on", your user.
4. Actions: New, Start a program, `macropad-sysinfo.exe`. Set "Start in"
   to its folder, so it finds `macropad_sysinfo.ini`.
5. Settings: tick "If the task fails, restart every 1 minute". Untick
   "Stop the task if it runs longer than".

To stop it: `macropad-sysinfo.exe --stop` (or `python main.py --stop`), or
end it in Task Manager. With `--stop` the macropad shows "no host" at once.
Otherwise it shows it after 3 seconds.

## Settings (`macropad_sysinfo.ini`)

| Key | Default | Meaning |
|---|---|---|
| `[device] vid`, `pid` | `0x239A`, `0x0108` | USB IDs of the macropad, from `keyboards/adafruit/macropad/keyboard.json` |
| `[device] usage_page`, `usage` | `0xFF60`, `0x61` | QMK's Raw HID interface |
| `[agent] interval` | `1.0` | seconds between updates, 0.25 to 25 |
| `[agent] gpu` | `off` | `nvidia` adds GPU load from `nvidia-smi` |
| `[agent] nic` | empty | one network adapter by its Windows name (e.g. `Wi-Fi`); empty = all |
| `[agent] log_level` | `INFO` | `DEBUG`, `INFO` or `WARNING` |

Without a settings file the defaults above are used.

## Log

`%LOCALAPPDATA%\MacropadSysinfo\agent.log`, at most 256 KB, plus 3 older
files. It logs start, stop, connect and disconnect, not every packet
(unless `log_level = DEBUG`).

## pip fails with CERTIFICATE_VERIFY_FAILED

`self signed certificate in certificate chain` means a security program on
the PC (here Kaspersky's "scan encrypted connections") re-signs Python's
HTTPS traffic with its own root certificate. Windows trusts that root, pip
does not. Give this venv's pip a bundle with that root added, from the
`host` folder in PowerShell (change the issuer text for another product):

```powershell
$c = Get-ChildItem Cert:\LocalMachine\Root, Cert:\CurrentUser\Root | Where-Object Subject -like "*Kaspersky*" | Select-Object -First 1
$pem = "-----BEGIN CERTIFICATE-----`n" + [Convert]::ToBase64String($c.RawData, 'InsertLineBreaks') + "`n-----END CERTIFICATE-----`n"
$certifi = .venv\Scripts\python -c "import pip._vendor.certifi as c; print(c.where())"
(Get-Content -Raw $certifi) + "`n" + $pem | Set-Content -NoNewline -Encoding ascii .venv\ca-bundle.pem
.venv\Scripts\python -m pip config --site set global.cert "$PWD\.venv\ca-bundle.pem"
```

This only changes `.venv\pip.ini`. Only building needs it: the finished
exe never goes on the internet.

## It does not show anything

1. `python main.py --list` should list an interface with usage `FF60/61`.
   If nothing is listed, the macropad is not plugged in or the IDs in the
   settings are wrong.
2. Run with `--console` and look for "macropad connected".
3. The macropad needs firmware with the SYSINFO feature (built from this
   keymap after 2026-10-24).
4. The VIA app can stay open at the same time. The agent never reads
   replies, and the keyboard sends none for these packets.
5. After the PC sleeps, the agent reconnects by itself within a few seconds.

## Packet (protocol 1)

32 bytes, little-endian. The agent writes a report ID of 0 in front, so it
sends 33 bytes. The firmware side is `../sysinfo.c`.

| Byte | Field |
|---|---|
| 0 | command `0xC5` (VIA uses 0x01-0x15 and 0xFF, so they never clash) |
| 1 | magic `0x53` (`S`) |
| 2 | protocol version, `1` |
| 3 | sequence number, 0-255, rolling |
| 4 | flags: bit 0 GPU value valid, bit 1 host is leaving |
| 5 | CPU %, 0-100 |
| 6 | RAM %, 0-100 |
| 7 | GPU %, 0-100 |
| 8-9 | disk read, rate16 |
| 10-11 | disk write, rate16 |
| 12-13 | network download, rate16 |
| 14-15 | network upload, rate16 |
| 16 | send interval in 100 ms units (10 = 1 s); the keyboard waits 3 intervals (at least 3 s) before it shows "no host" |
| 17-31 | reserved, 0 |

**rate16**: bit 15 is the unit (0 = KB/s, 1 = MB/s), bits 0-14 the value.
Up to 32767 KB/s goes as KB/s; above that as MB/s, capped at 32767 MB/s.
KB means 1024 bytes.

A change to the layout needs a new protocol number in both `main.py` and
`../sysinfo.h`. The firmware ignores packets with a protocol it does not
know.
