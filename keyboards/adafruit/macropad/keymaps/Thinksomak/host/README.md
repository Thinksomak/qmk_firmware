# Macropad sysinfo agent

A small program for the PC that sends live system stats to the Adafruit
Macropad (Thinksomak keymap). The macropad shows them in its SYSINFO view
(SYSTEM layer, SYSINFO key) and, if you turn it on, in a short strip on the
other layers.

It also carries out **Host Power**: shutting down or restarting the PC from
the macropad's POWER menu. That is **off** until you turn it on in the
settings; see [Host Power](#host-power-shut-down-or-restart-from-the-macropad).

What it sends, once a second:

| Value | Source (psutil) |
|---|---|
| CPU load, % | `cpu_percent()` |
| RAM in use, % | `virtual_memory().percent` |
| Disk read and write, KB/s | change in `disk_io_counters()` over the elapsed time |
| Network download and upload, KB/s | change in `net_io_counters()` over the elapsed time |
| GPU load, % (optional, NVIDIA only) | `nvidia-smi`, every 2 s |

Only these numbers go to the keyboard. The only thing the agent reads back
is a Host Power request (shut down, restart or cancel), and only when you
press those keys on the macropad.

## Files

| File | What it is |
|---|---|
| `main.py` | the agent (one script) |
| `macropad_sysinfo.ini` | settings; keep it next to `main.py` or the exe |
| `requirements.txt` | `psutil` and `hidapi` |
| `build.bat` | builds a single `.exe` with PyInstaller |
| `test_agent.py` | tests of the Host Power part (`python test_agent.py`); they never shut anything down |

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

**Updating an installed exe** (for example to get Host Power): stop the old
one (`macropad-sysinfo.exe --stop`), run `build.bat`, copy the new
`dist\macropad-sysinfo.exe` over the old one, and add the `[power]`
section from `macropad_sysinfo.ini` to the settings file next to it (keep
your other settings). Then start it again. An old agent never answers the
POWER menu: the macropad shows "Host agent not responding".

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
| `[power] allow_power_commands` | `false` | `true` lets the macropad shut down or restart this PC (see [Host Power](#host-power-shut-down-or-restart-from-the-macropad)); anything other than `true`/`false` keeps it off |
| `[power] power_delay` | `15` | seconds from the key press to the shutdown or restart, 5 to 99; never 0. Used instead of the 15 s the macropad asks for |

Without a settings file the defaults above are used.

## Host Power (shut down or restart from the macropad)

The macropad's SYSTEM layer has a POWER key. It opens a menu: top-left
Shutdown, top-middle Restart, top-right Cancel. A keyboard cannot shut down
Windows by itself, so the macropad asks this agent, and the agent runs
Windows' own `shutdown.exe`. The macropad then counts down, with every key
red and the cancel key (top-right) green; pressing it cancels on any
layer.

> **Warning: open apps are closed without asking.** The agent runs
> `shutdown` with a delay (`/t`), and for any delay above 0 Windows implies
> `/f`: when the countdown ends, open apps are closed without a chance to
> save. Unsaved work is lost. Save first, or cancel.

**It is off by default.** While off, the agent answers "disabled" and the
macropad shows "Disabled in agent config". To turn it on:

1. Open `macropad_sysinfo.ini`, the one next to `macropad-sysinfo.exe` (or
   next to `main.py`). The first line of the log names the file in use.
2. In `[power]`, set `allow_power_commands = true`. If you like, set
   `power_delay` (seconds, 5 to 99, default 15).
3. Restart the agent: `macropad-sysinfo.exe --stop`, then start it again.
   The log now says `Host Power: on, 15 s delay`.

What the agent runs, and nothing else (as an argument list, never through
a shell, from `%SystemRoot%\System32`):

| Macropad | Command |
|---|---|
| Shutdown | `shutdown /s /t <power_delay> /c "<note>"` |
| Restart | `shutdown /r /t <power_delay> /c "<note>"` |
| Cancel | `shutdown /a` (always carried out, even with Host Power off: it can only keep the PC on) |

The note shows in Windows' own "about to shut down" message and says how to
cancel. You can also cancel from the PC: Win+R, `shutdown /a`, Enter.

The macropad's delay setting (`HOSTPOWER_DELAY_S`, 15 s) is only a request:
`power_delay` here decides, and the macropad counts down from what the
agent reports back.

### Host Power troubleshooting

| The macropad shows | What to do |
|---|---|
| "PC agent not running" in the menu | Start the agent. The menu still opens, but nothing will answer it. |
| "Host agent not responding" | No reply within 2 s: the agent is not running, or it is an old version without Host Power. Rebuild and replace the exe (see [Build the exe](#build-the-exe)). |
| "Disabled in agent config" | `allow_power_commands` is not `true` in the settings file the agent uses; set it and restart the agent. |
| "The PC refused it / see the agent log" | Windows refused. The log has `shutdown.exe`'s exit code and message, for example 1190: a shutdown is already scheduled (run `shutdown /a`, or wait for it). |
| "! Cancel unconfirmed" during a countdown | The agent did not confirm the cancel, so the PC may still shut down. Run `shutdown /a` on the PC at once (Win+R). |
| The countdown stays at "now" | The PC is shutting down but the macropad still has USB power. It goes back to normal by itself after a minute. |

## Log

`%LOCALAPPDATA%\MacropadSysinfo\agent.log`, at most 256 KB, plus 3 older
files. It logs start, stop, connect and disconnect, not every packet
(unless `log_level = DEBUG`), whether Host Power is on, and every Host
Power request with its outcome (a shutdown or restart that starts, and
anything refused, as WARNING).

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
4. The VIA app can stay open at the same time. The keyboard does not
   answer the stats packets. Its Host Power requests reach VIA too, which
   ignores them, unless VIA is busy talking to the keyboard at that very
   moment; then that one VIA action may fail. Just do it again.
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

## Host Power packets (protocol 1)

32 bytes each, little-endian, through the same Raw HID interface. The
firmware side is `../hostpower.c`; `0xC6` and `0xC7` are outside VIA's
commands too. CRC-8 is polynomial 0x07, starting from 0 (CRC-8/SMBUS).

Request, keyboard to PC, sent only when a POWER menu key or the cancel key
is pressed:

| Byte | Field |
|---|---|
| 0 | command `0xC6` |
| 1 | magic `0x50` (`P`) |
| 2 | protocol version, `1` |
| 3 | sequence number, 0-255, rolling |
| 4 | action: 1 shutdown, 2 restart, 3 cancel |
| 5-6 | delay the keyboard asks for, in s (15) |
| 7 | CRC-8 of bytes 0-6 |
| 8-31 | 0 |

Reply, PC to keyboard (the agent writes a report ID of 0 in front):

| Byte | Field |
|---|---|
| 0 | command `0xC7` |
| 1 | magic `0x50` |
| 2 | protocol version, `1` |
| 3 | sequence number, echoed |
| 4 | action, echoed |
| 5 | status: 1 started, 2 cancelled, 3 disabled, 4 error |
| 6-7 | delay the PC uses, in s (0 unless started) |
| 8 | CRC-8 of bytes 0-7 |
| 9-31 | 0 |

The keyboard ignores a reply whose sequence, action or CRC does not match
the request it is waiting for, and gives up waiting after 2 s. The agent
ignores damaged requests and those of another protocol (and logs them).
