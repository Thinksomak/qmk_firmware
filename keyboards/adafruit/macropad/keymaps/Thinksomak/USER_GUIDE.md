# Thinksomak Macropad — User Guide

Firmware: QMK, keymap `Thinksomak`, for the Adafruit Macropad RP2040.
This guide covers every key, screen and setting of that firmware.

---

## Contents

1. [Overview](#1-overview)
2. [Quick start](#2-quick-start)
3. [Layer reference](#3-layer-reference)
4. [Pomodoro timer](#4-pomodoro-timer)
5. [PC stats (SYSINFO)](#5-pc-stats-sysinfo), and
   [Host Power: shut down or restart the PC](#55-host-power-shut-down-or-restart-the-pc)
6. [Numpad holds and the calculator](#6-numpad-holds-and-the-calculator)
7. [No-Sleep](#7-no-sleep)
8. [Lights (RGB)](#8-lights-rgb)
9. [Settings and what is saved](#9-settings-and-what-is-saved)
10. [Flashing and recovery](#10-flashing-and-recovery)
11. [Troubleshooting, FAQ and quick-reference card](#11-troubleshooting-faq-and-quick-reference-card)
12. [Specifications and glossary](#12-specifications-and-glossary)

---

## 1. Overview

The macropad is a small USB keypad that sits next to your keyboard. With
this firmware it is six keypads in one, chosen with the knob:

| Layer | What it is for |
|---|---|
| NUMPAD | a number pad; hold a digit for an operator; a built-in desk calculator |
| MACRO | typing shortcuts: `$`, `%`, a SQL template, two secure macro keys, RUN |
| MEDIA | play / pause, previous / next track, stop, volume |
| NAV | arrows, Page Up / Down, Home / End, Tab, Shift+Tab, Delete |
| SYSTEM | Task Manager, PC shutdown / restart, No-Sleep, PC stats, light controls, bootloader |
| POMODORO | a focus timer (25 / 5 / 15 minutes by default) with stats |

It also shows live PC stats (CPU, memory, disk, network, GPU) sent by a
small program on the PC, can keep the PC awake and, if you allow it on
the PC, shut it down or restart it (Host Power, §5.5).

### Hardware

| Part | Details |
|---|---|
| Keys | 12 mechanical keys in 4 rows of 3 |
| Knob | rotary encoder: twist (clicks) and press |
| Screen | 1.3" monochrome OLED, 128 × 64 pixels |
| Lights | one RGB LED under each key |
| Connection | USB-C, to any PC; no driver needed |
| Side button | reset (double-tap: bootloader, see §10) |
| Speaker | present on the board, not used by this firmware |

```
          ┌─────────────────────────┐
          │        ( knob )         │  twist = change layer, press = Mute
          │  ┌───────────────────┐  │
          │  │      screen       │  │
          │  └───────────────────┘  │
          │   [   ]  [   ]  [   ]   │  row 1
          │   [   ]  [   ]  [   ]   │  row 2
          │   [   ]  [   ]  [   ]   │  row 3
          │   [   ]  [   ]  [   ]   │  row 4
          │   left  middle  right   │
          └─────────────────────────┘
```
In this guide, keys are named by what the current layer prints on the
screen, or by their position (row, then left / middle / right).

---

## 2. Quick start

### Plugging in

1. Connect the macropad to the PC with a USB-C data cable.
2. The screen lights up on the **NUMPAD** layer. For the first 6 seconds
   the bottom line shows the firmware's build date and time
   (`firmware 10-25 14:32`), so you can tell which version is running.
3. The top-left key lights cyan: that is the NUMPAD layer's colour.

No software is needed on the PC for the keys. The PC stats (§5) need the
small agent program.

### The knob rule

* **On a layer, twisting the knob changes layer**: one click, one layer,
  round the ring:

  `NUMPAD → MACRO → MEDIA → NAV → SYSTEM → POMODORO → NUMPAD …`

  Clockwise goes right in that list, counter-clockwise goes left. From
  NUMPAD, one click counter-clockwise reaches POMODORO.
* **When a screen that needs a dial is open, the knob belongs to it**: it
  changes a value (settings, adjust time), turns pages (stats, SYSINFO), or
  scrolls (calculator). The moment that screen closes, the knob changes
  layer again.
* **Pressing the knob** is Mute on every layer except POMODORO, where it
  opens *Adjust time*.
* **The POWER menu (§5.5) ignores the knob**, twist and press, so nothing
  changes by accident while it is open.
* **While the screen is off or shows the screensaver, the first twist only
  wakes it**, so you never change layer without seeing it. Keys wake the
  screen and do their job at once.

### Entering and leaving sub-modes

| Sub-mode | Enter | Leave |
|---|---|---|
| Calculator | on NUMPAD, hold **ENT** for 0.4 s | hold **ENT** again for 0.4 s |
| Pomodoro settings | POMODORO → **EDIT** | **START** = save, **RESET** = cancel, 30 s untouched = cancel |
| Pomodoro stats | POMODORO → **STATS** | **STATS** again, or press the knob |
| Adjust time | POMODORO → **ADJUST** or knob press | knob press or **ADJUST**, any other key, or 10 s without a twist |
| PC stats view | SYSTEM → **SYSINF** | **SYSINF** again, or press the knob |
| PC stats setup | in the stats view, the top-right key | top-left = save, top-right = cancel, 30 s untouched = cancel |
| Bootloader confirm | SYSTEM → **BOOT** | bottom-right key = flash mode; anything else = cancel |
| Host Power menu | SYSTEM → **POWER** | top-right key = cancel, 10 s without a key = cancel; top-left / top-middle start a shutdown / restart countdown (§5.5) |
| Host Power countdown | Shutdown or Restart in the menu | top-right key on any layer = cancel (§5.5) |

### Reading the screen

Every screen has the same frame:

```
┌──────────────────────────────┐
│▓NUMPAD▓▓▓▓▓▓▓▓▓▓(●○○○○○)▓▓▓▓│  header: name · layer dots (filled = this layer)
│                              │  strip line (PC stats) / RGB line on SYSTEM
│  7 %       8 /       9 *     │
│  4         5 -       6 +     │  key legend: what each key does
│  1         2         3       │
│  ENT       0 .       BKSP    │
│                              │  pomodoro progress dots (while a phase runs)
│ ▣ ☕ ▭          FOCUS ▶12:40 │  bottom line: status icons · pomodoro time
└──────────────────────────────┘
```

**Header.** The name of the layer or open screen on the left. Main layers
show the layer dots on the right. A **±** before the name means the knob
changes a value there. Stats and SYSINFO show their page number, e.g.
`2/6`, where the knob turns pages.

**Bottom line**, left to right, each icon always in its own place:

| Icon | Meaning |
|---|---|
| **A** in a box | Caps Lock is on |
| steaming cup | No-Sleep is on |
| monitor, screen lit | the PC agent is sending stats |
| monitor, screen dark | the PC agent was sending but stopped |
| (no monitor) | the agent has not sent anything since power-up |

On the right, while a pomodoro phase is loaded: `FOCUS`, `BREAK` or
`LONG`, ▶ (running) or ‖ (paused), and the time left.

---

## 3. Layer reference

Each layer lights one key in its colour, so you can tell the layer from
the keys alone:

| Layer | Lit key | Colour |
|---|---|---|
| NUMPAD | row 1 left | cyan |
| MACRO | row 1 middle | blue |
| MEDIA | row 1 right | green |
| NAV | row 2 left | yellow |
| SYSTEM | row 2 middle | red |
| POMODORO | row 2 right | tomato red |

### 3.1 NUMPAD

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | 7 · hold `%` | 8 · hold `/` | 9 · hold `*` |
| **Row 2** | 4 | 5 · hold `-` | 6 · hold `+` |
| **Row 3** | 1 | 2 | 3 |
| **Row 4** | Enter · hold: **calculator** | 0 · hold `.` | Backspace |

* **Tap** types the key. **Hold** for a quarter of a second (0.25 s) types
  the operator shown after the digit, once.
* 4, 1, 2, 3 and Backspace have no operator: held, they repeat like a
  normal key.
* Enter and Backspace need a longer hold, 0.4 s, because Enter's hold
  switches mode.
* Holds are timed only: fast typing and rolling over keys always gives
  digits.
* Knob: twist = layer, press = Mute.
* Screen: the key legend in three straight columns; digits on the left of
  each column, the hold operator right of its digit.

### 3.2 MACRO

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | `$` | `%` | SQL |
| **Row 2** | — | — | — |
| **Row 3** | — | — | — |
| **Row 4** | TCS (secure macro) | MACE (secure macro) | RUN |

* **SQL** types a query template (Tab between words, Enter after lines):
  ```
  SELECT	*
  FROM	
  WHERE	
  AND 	
  ```
* **TCS** and **MACE** are *secure macro keys*: each types stored sign-in
  text. The text lives in the firmware's private settings file, never in
  this repository and never on the screen.

  | Taps | TCS | MACE |
  |---|---|---|
  | 1 | user name, then Tab | user name, then Enter |
  | 2 | password, then Enter | password, then Enter |
  | 3 | user name, Tab, password, Enter | user name, Tab, password, Enter |

  Tap the number of times you need, quickly, then wait a moment: the key
  acts when the taps stop. Anyone who can press the macropad can type this
  text, so do not leave it plugged into an unlocked PC you do not control.
* **RUN** (a tap dance):

  | Action | Sends |
  |---|---|
  | tap | Ctrl+F5 |
  | hold | Alt+Shift+F |
  | double tap | Ctrl+Shift+Delete |
  | double tap and hold, or triple tap | Ctrl |

* Knob: twist = layer, press = Mute.

### 3.3 MEDIA

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | — | — | — |
| **Row 2** | — | Stop | — |
| **Row 3** | Previous track | Play / Pause | Next track |
| **Row 4** | Volume − | Mute | Volume + |

Knob: twist = layer, press = Mute.

### 3.4 NAV

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | Page Up | ↑ | Page Down |
| **Row 2** | ← | ↓ | → |
| **Row 3** | Home | — | End |
| **Row 4** | Shift+Tab | Tab | Delete |

Knob: twist = layer, press = Mute.

### 3.5 SYSTEM

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | TSKMGR (Ctrl+Shift+Esc) | POWER (shut down / restart menu, §5.5) | BOOT (bootloader, §10) |
| **Row 2** | NS (No-Sleep, §7) | SYSINF (PC stats, §5) | RGB on / off |
| **Row 3** | MODE+ | BRI+ | SPD+ |
| **Row 4** | MODE− | BRI− | SPD− |

* The line under the header shows the lights: `RGB ON  MODE 7  58%`
  (on / off, effect number, brightness).
* The BOOT key blinks red; the NS key shows No-Sleep's state (§7).
* The NS key's label reads `NS ON` or `NS OFF`.
* POWER opens the Host Power menu (§5.5). It works only with the PC agent
  running and Host Power turned on in its settings.
* Knob: twist = layer, press = Mute.

### 3.6 POMODORO

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | START / PAUSE / RESUME | SKIP | RESET |
| **Row 2** | EDIT | ADJUST | STATS |
| **Row 3** | FOCUS | BREAK | LONG |
| **Row 4** | AUTO | UNDO | CYCLE |

* Knob: twist = layer, **press = Adjust time**.
* Screen: the clock (see §4). The other layers show the timer on their
  bottom line and as progress dots.

---

## 4. Pomodoro timer

A focus timer: 25 minutes of focus, a 5-minute break, and a 15-minute long
break after every 4 focus sessions. All four numbers can be changed.

### 4.1 Phases

```
READY ──START──▶ FOCUS ──▶ BREAK ──▶ FOCUS ──▶ BREAK ── … ──▶ FOCUS ──▶ LONG BREAK ──▶ FOCUS …
                 (25 min)  (5 min)                         (4th)      (15 min, new cycle)
```

* Press **START** on POMODORO to begin. The timer keeps running on every
  layer.
* When a phase ends, the next one is loaded and **waits for START** (or
  starts by itself with AUTO on).
* The clock screen:
  ```
  ┌──────────────────────────────┐
  │▓FOCUS▓▓▓▓▓▓▓▓▓▓▓(○○○○○●)▓▓▓▓│  phase, or PAUSED
  │ ■▣□□ 2/4          AUTO ☕ ▭  │  sessions: done ■, now ▣, to do □
  │        ██  ███   ███  ███    │
  │         █    █ ▪   █    █    │  time left (blinks while paused)
  │        ███  ███    █  ███    │
  │ (██████████──────────────)   │  progress bar
  │   next: SHORT BREAK 5m       │  hint line
  └──────────────────────────────┘
  ```

### 4.2 Keys

| Key | What it does |
|---|---|
| START / PAUSE / RESUME | start from READY, pause, resume, or start the waiting phase |
| SKIP | end this phase now and load the next (not counted in stats) |
| RESET | restart this phase from its full length, paused |
| EDIT | settings (§4.4) |
| ADJUST (or knob press) | adjust time (§4.3) |
| STATS | stats (§4.5) |
| FOCUS / BREAK / LONG | start that phase now |
| AUTO | auto start on / off (saved) |
| UNDO | undo the last SKIP, RESET, FOCUS/BREAK/LONG jump, re-open or CYCLE (one step) |
| CYCLE | new cycle: back to READY, session count 0 |

Short confirmations ("undone", "auto start ON", "new cycle"…) show on the
clock's bottom line for 2 seconds.

### 4.3 Adjust time

* Opens with **ADJUST** or the **knob press**. The header shows **± ADJUST
  TIME**.
* **Twist**: one click = one minute more or less. The time stays between
  1:00 and 99:59.
* **Done**: press the knob or ADJUST again. It also closes after 10 s
  without a twist. Any other key closes it and does its own job.
* **Re-open a phase that just ended**: while the next phase waits, ADJUST
  (or the knob press) gives the phase that just ended **5 more minutes**
  and runs it. It is not counted twice in the stats.

### 4.4 Settings (EDIT)

| Field | Range | Default |
|---|---|---|
| FOCUS | 1–90 min | 25 |
| SHORT BREAK | 1–30 min | 5 |
| LONG BREAK | 1–60 min | 15 |
| SESSIONS (focus sessions per long break) | 1–8 | 4 |

Keys while the settings are open (by position on the POMODORO layer):

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | **Save** | Field ↑ | **Cancel** |
| **Row 2** | Default (this field) | Field ↓ | — |
| **Row 3** | −1 | — | +1 |
| **Row 4** | −5 | — | +5 |

Knob: twist = −1 / +1, press = next field. A `*` marks a value that
differs from the saved one. **Save** writes to memory only if something
changed. Untouched for 30 seconds, the settings close without saving. A
phase that has not started yet takes the new length at once; a running
phase keeps its length.

### 4.5 Stats

Three pages; turn with the knob, or START (◀) and SKIP (▶):

| Page | Covers | Shows |
|---|---|---|
| 1/3 Current session | since power-up (not saved) | focus time, sessions done, breaks done |
| 2/3 Since stats reset | since you last cleared it (saved) | focus time, sessions done |
| 3/3 Lifetime | everything (saved, never cleared) | focus time, sessions done |

* **Focus time** counts every minute actually spent in focus, even if
  the phase was skipped or reset. **Sessions** and **breaks** count only
  phases that ran to their end.
* **Clear "since stats reset"**: hold **RESET** for 2 seconds (a bar fills;
  let go early to cancel). "cleared" confirms it. Lifetime is never
  cleared. Use this page as your own day or week.
* Close with **STATS** or the knob press. Other POMODORO keys keep working.
* There is no clock in the macropad, so there are no daily totals.

### 4.6 Alerts

When a phase ends, on whatever layer you are:

| | Focus finished | Break finished |
|---|---|---|
| Screen | `FOCUS COMPLETE`, flashing for 3 s | `BREAK OVER`, flashing for 3 s |
| Lights | calm green breathing on every key | amber double pulse sweeping down the rows |

* The alert wakes the screen and turns the lights on even if you had them
  off (back off afterwards). It stays up to 30 s.
* **Any key dismisses it** without doing anything else, except START (starts
  the waiting phase), ADJUST / knob press (re-opens the phase, §4.3),
  STATS, EDIT and FOCUS / BREAK / LONG, which act at once.
* While a phase is loaded, the **bottom-right key glows dimly**: red-orange
  for focus, green for a break, blue for a long break. It blinks slowly
  while the timer is paused or the next phase waits for START.

### 4.7 What is kept

Settings and AUTO are saved when you change them. Saved stats are written
when a phase ends and when you clear them. The running timer itself is not
saved: unplugging ends the phase (focus minutes since the last phase end
are lost).

---

## 5. PC stats (SYSINFO)

The macropad can show your PC's CPU, memory, disk, network and (NVIDIA)
GPU load. A small program on the PC, the **agent**, sends the numbers over
USB once a second. The same agent carries out Host Power (§5.5) when you
allow it; that is the only thing it ever reads from the keyboard.

### 5.1 Installing the agent

The agent's files are in the repository at
`keyboards/adafruit/macropad/keymaps/Thinksomak/host/`. Full details are in
`host/README.md`.

**Option A — the ready-made exe (everyday use)**

1. Build it once (needs Python, see option B, step 1):
   ```bat
   cd keyboards\adafruit\macropad\keymaps\Thinksomak\host
   build.bat
   ```
   This makes `dist\macropad-sysinfo.exe` (no window) and copies
   `macropad_sysinfo.ini` next to it.
2. Copy both files anywhere, e.g. `%LOCALAPPDATA%\Programs\MacropadSysinfo\`.
   The exe runs on other Windows PCs too; Python is not needed there.
3. Start it with a double-click. To start it at every login: Win+R →
   `shell:startup` → New → Shortcut → the exe.

**Option B — with Python**

1. Install Python 3.8 or newer from python.org (tick "Add python.exe to
   PATH"). The Python inside QMK MSYS cannot be used.
2. In the `host` folder:
   ```bat
   py -3 -m venv .venv
   .venv\Scripts\python -m pip install -r requirements.txt
   .venv\Scripts\python main.py --console
   ```
3. Look for `macropad connected`. Stop with Ctrl+C.

**Agent options**

| Option | What it does |
|---|---|
| `--console` | log every packet in the window |
| `--dry-run` | run without a macropad (logs what it would send) |
| `--list` | list the macropad's interfaces (checks it is found) |
| `--stop` | stop the running agent (the macropad shows NO HOST at once) |
| `--config FILE` | use another settings file |

**Settings file** `macropad_sysinfo.ini` (next to the exe or `main.py`):

| Setting | Default | Meaning |
|---|---|---|
| `interval` | `1.0` | seconds between updates (0.25–25) |
| `gpu` | `off` | `nvidia` adds GPU load (needs the NVIDIA driver's `nvidia-smi`) |
| `nic` | empty | one network adapter by its Windows name, e.g. `Wi-Fi`; empty = all |
| `log_level` | `INFO` | `DEBUG`, `INFO` or `WARNING` |
| `allow_power_commands` | `false` | `true` lets the POWER menu shut down or restart this PC (§5.5) |
| `power_delay` | `15` | Host Power countdown in seconds, 5–99 (never 0) |
| `vid`, `pid`, `usage_page`, `usage` | macropad values | only change if the USB IDs change |

The log is in `%LOCALAPPDATA%\MacropadSysinfo\agent.log`.

### 5.2 The SYSINFO view

Open it on SYSTEM with **SYSINF**; close with SYSINF again or the knob
press. Turn pages with the knob or the top-left (◀) / top-middle (▶) keys.

| Key (SYSTEM position) | In the view |
|---|---|
| TSKMGR | ◀ previous page |
| POWER | ▶ next page (close the view to open the Host Power menu) |
| BOOT | **Setup** (§5.3) |
| SYSINF | close |
| NS, RGB and the RGB keys | keep their normal job |

**Pages**, with everything shown (the default):

| Page | Shows |
|---|---|
| 1 Overview | CPU (a line of about the last minute), RAM and GPU (bars), each with its %; GPU only while the agent sends it |
| 2 Overview | DISK read (R) and write (W), NETWORK download (D) and upload (U), with units |
| CPU | the number large, a bar, and the last two minutes as a line |
| RAM, GPU | the number large and a bar |
| DISK, NETWORK | both rates large, e.g. `12.6 MB/s` |

```
┌──────────────────────────────┐   ┌──────────────────────────────┐
│▓SYSINFO▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓1/6▓│   │▓SYSINFO▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓2/6▓│
│                              │   │ DISK  R         12.3 MB/s    │
│ CPU    ~^~~^~~~~^~~~    37%  │   │       W          820 KB/s    │
│                              │   │                              │
│ RAM    ██████────────   64%  │   │ NET   D         1.21 MB/s    │
│                              │   │       U           96 KB/s    │
│ ▭                            │   │ ▭                            │
└──────────────────────────────┘   └──────────────────────────────┘
```

* A smaller choice that fits one page is shown on one overview page.
* Units: KB/s, MB/s, GB/s, picked to fit (1 KB = 1024 bytes).
* **NO HOST**: after 3 seconds without numbers (agent stopped, PC asleep,
  macropad not found), every page shows `NO HOST — start the PC agent`.

### 5.3 Setup

Open with the top-right key in the view. The header shows **± SYSINFO
SETUP**. Keys work like the pomodoro settings:

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | **Save** | Field ↑ | **Cancel** |
| **Row 2** | Default (this field) | Field ↓ | — |
| **Row 3** | −1 | — | +1 |
| **Row 4** | −5 | — | +5 |

Knob: twist = −1 / +1, press = next field. Untouched for 30 seconds, setup
closes without saving.

| Field | Choices | Default |
|---|---|---|
| SHOW | ALL, CPU, RAM, DISK, NET, GPU, CUSTOM | ALL (GPU only if the agent sends it) |
| CPU / RAM / DISK / NET / GPU (with CUSTOM) | on / off each | all on except GPU |
| STYLE | OFF, STRIP, SAVER (§5.4) | OFF |
| IDLE (SAVER) | 15 s, 30 s, 1, 2, 5, 10, 15 min | 1 min |
| SCREEN OFF (SAVER) | 1, 5, 10, 15, 30, 60 min, never | 10 min |
| EVERY (STRIP) | 10, 15, 20, 30, 45 s, 1, 2, 3, 5, 10, 15, 20, 30 min | 1 min |
| FOR (STRIP) | 2–30 s, shorter than EVERY | 5 s |

### 5.4 Display styles

| Style | What happens outside the SYSINFO view |
|---|---|
| **OFF** | nothing; the screen sleeps after 10 minutes without a key |
| **STRIP** | every EVERY, for FOR seconds, the line under the header on every layer shows the stats, one group after another: `CPU 37%  RAM 62%`, `GPU 18%`, `DISK R 12.3M  W 820K`, `NET D 1.21M  U 96K`. The groups share the FOR seconds. |
| **SAVER** | after IDLE without a key or twist, the overview fills the screen (a screensaver) until you touch the macropad; two overview pages take turns every 10 s. The screen turns off SCREEN OFF later, or never. |

Screensaver details: every 30 s the picture moves a pixel sideways and
every 2 minutes a row up or down, so no pixel stays lit in one place. The
bottom line keeps the pomodoro time and icons. A pomodoro alert always
wins, and a Host Power countdown (§5.5) shows over the saver. The saver
never covers an open screen (calculator, settings, stats).
Without the agent there is no saver or strip.

### 5.5 Host Power: shut down or restart the PC

SYSTEM → **POWER** opens a menu that shuts down or restarts the PC. A
keyboard cannot do that by itself, so the macropad asks the PC agent
(§5.1), which runs Windows' own `shutdown` command.

**It is off until you turn it on in the agent's settings** (see *Turning
it on*, below). Until then the menu answers `Disabled in agent config`.

> **Warning: open apps are closed without asking.** When the countdown
> ends, Windows closes every open app without offering to save (a
> `shutdown` with a delay always forces apps closed). Unsaved work is
> lost. Save first, or cancel. Try Restart for the first time with nothing
> important open.

**The menu**

```
┌──────────────────────────────┐
│▓HOST POWER▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│  header: no layer dots, the knob does nothing
│ [7] Shutdown                 │
│ [8] Restart                  │
│ [9] Cancel                   │
│                              │
│ PC agent connected           │  is the agent running?
│ Closes in 7 s                │  or the PC's answer
└──────────────────────────────┘
```

| Key | In the menu |
|---|---|
| Row 1 left (7) | **Shutdown**: asks the PC, then counts down |
| Row 1 middle (8) | **Restart**: asks the PC, then counts down |
| Row 1 right (9) | **Cancel**: closes the menu |
| every other key, the knob | nothing |

* The numbers are those of the NUMPAD keys in the same places.
* The menu closes by itself after **10 seconds** without a key.
* For the first half second Shutdown and Restart do nothing: POWER sits
  where Restart is, so a double tap on POWER can never restart the PC.
* Every key lights red, the cancel key green, at full brightness.
* The two bottom lines say whether the agent is running
  (`PC agent connected` / `PC agent not running`) and when the menu
  closes; after Shutdown or Restart they show the PC's answer:

| Message | Meaning |
|---|---|
| `Asking the PC...` | the request is out; the answer comes within 2 s |
| `Host agent not responding` | no answer within 2 s: the agent is not running, or it is older than Host Power (rebuild it, §5.1) |
| `Disabled in agent config` | Host Power is off in the agent's settings (the default) |
| `The PC refused it, see the agent log` | Windows said no, for example because a shutdown is already scheduled; the log says why |

**The countdown**

When the PC has accepted, the menu gives way to the countdown:

```
┌──────────────────────────────┐
│▓SHUTDOWN▓▓▓▓▓▓▓▓▓▓▓(●○○○○○)▓▓│  layer dots: the knob still changes layer
│                              │
│              12              │  seconds left, in large clock digits
│                              │
│    Shutting down in 12 s     │
│    Top-right key: cancel     │
└──────────────────────────────┘
```

* It shows over every layer, the SYSINFO view and the screensaver, and
  keeps the screen on. Open screens (pomodoro settings, stats, adjust,
  alerts, SYSINFO setup, the calculator) stay on top until you close them;
  the lights show the countdown all the same.
* Every key is red and the top-right key pulses green.
* **Cancel: the top-right key, on any layer** (even in the calculator,
  where it is 9 otherwise). The screen shows `Cancelling...`, then
  `Cancelled — The PC stays on.` for 3 seconds.
* Every other key and the knob work as usual meanwhile.
* The length is the agent's `power_delay` (default 15 s). The macropad
  counts from the moment it asked, so it reaches 0 just before the PC acts,
  never after.
* At 0 the screen shows `Shutting down now` / `Restarting now`. If the
  macropad still has power once the PC is off (some PCs keep USB powered),
  it goes back to normal after a minute.
* `! Cancel unconfirmed`: the agent did not confirm the cancel, so the PC
  may still go off. Press the top-right key again, or on the PC press
  Win+R, type `shutdown /a`, Enter. That command cancels from the PC side
  at any time during the countdown.

**Turning it on** (on the PC)

1. If your agent is older than this feature, rebuild and replace it
   (§5.1, and *Updating an installed exe* in `host/README.md`).
2. In `macropad_sysinfo.ini` next to the agent, in the `[power]` section:
   ```ini
   allow_power_commands = true
   power_delay = 15        ; seconds, 5 to 99
   ```
3. Restart the agent (`--stop`, then start it). Its log then says
   `Host Power: on, 15 s delay`.

The agent runs only these, each shutdown or restart with a note for
Windows' own message:

| Macropad | Command on the PC |
|---|---|
| Shutdown | `shutdown /s /t <power_delay>` |
| Restart | `shutdown /r /t <power_delay>` |
| Cancel | `shutdown /a`, always carried out, even with Host Power off |

The macropad asks for 15 s, but `power_delay` decides. Every request and
its outcome goes into the agent's log.

---

## 6. Numpad holds and the calculator

### 6.1 Operator holds

On NUMPAD, hold a key for 0.25 s to type its operator once:
`7 → %`, `8 → /`, `9 → *`, `5 → -`, `6 → +`, `0 → .`
(See §3.1. The calculator uses its own holds, below.)

### 6.2 Calculator mode

A desk calculator on the macropad's screen. Nothing reaches the PC unless
you send the result with TYPE.

* **Turn on**: on NUMPAD, hold **ENT** for 0.4 s. The header reads `CALC`
  and the keys light by role.
* **Turn off**: hold **ENT** again for 0.4 s. The calculation is kept
  until the macropad is unplugged, so turning the calculator on again
  shows it where you left it.

**Keys (tap / hold)**

| | Left | Middle | Right |
|---|---|---|---|
| **Row 1** | 7 / **%** | 8 / **÷** | 9 / **×** |
| **Row 2** | 4 / **±** | 5 / **−** | 6 / **+** |
| **Row 3** | 1 / **CE** | 2 | 3 / **TYPE** |
| **Row 4** | **=** / **OFF** (0.4 s) | 0 / **.** | ⌫ / **AC** (0.4 s) |

Key lights: amber = a digit with an operator on hold, dim white = the
other digits, green = `=`, red = ⌫ / AC.

| Key | Does |
|---|---|
| = | finishes the sum: the total is boxed. Again right away it repeats the last operation (`9 + 3 = =` → 15). Then a digit starts a new sum, and an operator goes on from the total. |
| ⌫ | removes the last character of the sum, see **Backspace** below |
| CE | clears the number being typed (it shows `0`) |
| AC | clears everything |
| ± | changes the sign of the number being typed or shown |
| % | Windows-calculator style: `200 + 10 %` → 220; `200 × 10 %` → 20; `50 %` → 0.5 |
| TYPE | types the current total into the PC as keystrokes |

* Sums go **left to right** with no operator precedence:
  `4 + 5 × 2 =` is 18.
* An operator pressed right after another replaces it.
* Up to 10 digits on screen, 6 decimals inside.

**The screen**

```
Typing  4 + 5 +                      "4 + 5 + 35", then =
┌──────────────────────────────┐     ┌──────────────────────────────┐
│▓CALC▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│     │▓CALC▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
│ 4 + 5 +                      │     │ 4 + 5 + 35                   │  the sum: small, left
│                              │     │                              │
│                              │     │                              │
│ ──────────────────────────── │     │ ──────────────────────────── │  divider
│ =                        9   │     │ =                    ▐ 44 ▌  │  "=" left, total LARGE, right
└──────────────────────────────┘     └──────────────────────────────┘

Then ⌫                               ⌫ until empty
┌──────────────────────────────┐     ┌──────────────────────────────┐
│▓CALC▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│     │▓CALC▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
│ 4 + 5 + 3                    │     │                              │
│                              │     │                              │
│                              │     │                              │
│ ──────────────────────────── │     │ ──────────────────────────── │
│ =                       12   │     │ =                        0   │
└──────────────────────────────┘     └──────────────────────────────┘

Long total: medium size              Long sum (4 lines, 3 in view)
┌──────────────────────────────┐     ┌──────────────────────────────┐
│▓CALC▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│     │▓CALC▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
│ 1000000000 / 3               │     │ - 99.5 / 2 + 77            ┆ │
│                              │     │ + 1000 x 2 - 12.5          ┆ │
│                              │     │ / 4                        █ │  scrollbar
│ ──────────────────────────── │     │ ──────────────────────────── │
│ ≈            ▐ 333333333.3 ▌ │     │ =              ▐ 1945.0625 ▌ │
└──────────────────────────────┘     └──────────────────────────────┘
```

* The whole screen is the calculator: no key help, no layer dots, no
  bottom line.
* **The sum** (top, small text, left-aligned) shows only what you typed:
  numbers and operators, never `=`. Up to **3 lines of 20 characters**; a
  long sum is broken like a paper tape, each new line starting with its
  operator (`+ 1000 x 2 - 12.5`). In this small text × and ÷ show as `x`
  and `/`.
* **Divider**: a thin line under the sum.
* **Total row**: `=` always at the left; the total in large digits at the
  right, live while you type and boxed once you press `=`. A total too wide
  to sit next to the `=` (11 or more characters, like `1428.571429`)
  steps down to a medium size of the same height, so it is never cut off.
* **`≈`** replaces the `=` when the total had to be rounded to fit 10
  digits (1,000,000,000 ÷ 3 shows `333333333.3`).
* **Scrolling**: when the sum has more than 3 lines, the last 3 show and a
  **scrollbar** appears at the right of the sum. Twist the knob
  counter-clockwise to see older lines, clockwise to come back. Any key
  jumps back to the newest line, and so does leaving the calculator.
* The sum memory holds about 96 characters; beyond that the oldest part is
  folded away and the top line starts with `..`.

**Backspace (⌫)** works from the end of the sum, one press at a time:

* Each press removes **one character**: the last digit, decimal point or
  `%` of the last number, or a whole **operator**. A number whose last
  digit goes disappears; it never turns into 0.
* Example: `4 + 12` → `4 + 1` → `4 +` → `4` → empty. The total updates
  after every press, ignoring an operator at the end (`4 +` shows 4).
* On an empty sum the total shows 0 and ⌫ does nothing.
* **After `=`**: ⌫ reopens the sum and removes its last character, so you
  can correct and go on. `4 + 5 + 35 =` (44), then ⌫ → `4 + 5 + 3` (12).
* A result you went on from (the `9` in `9 x 2` after `4 + 5 =`) is
  edited by the digits it shows.
* After an error, ⌫ works the same way: `7 / 0` → `7 /` (total 7).
* Hold ⌫ for 0.4 s to clear everything (AC).

**Errors**: the total reads `Err` and the divider gives the reason:
`! divide by 0`, or `! too big` (11 or more digits). ⌫ removes the last
character and the total comes back; a digit starts a new sum; CE or AC
clear everything.

**Worked examples**

| Keys | The sum on screen | Total row |
|---|---|---|
| `4 + 5 + 3` | `4 + 5 + 3` | `=` 12 |
| `4 + 5 × 2 =` | `4 + 5 x 2` | `=` 18 (boxed) |
| `1 ÷ 3 =` | `1 / 3` | `=` 0.333333 (boxed) |
| `200 + 10 % =` | `200 + 10%` | `=` 220 (boxed) |
| `5 ± × 3 =` | `-5 x 3` | `=` −15 (boxed) |
| `10000 ÷ 7 =` | `10000 / 7` | `=` 1428.571429 (boxed, medium size) |
| `1000000000 ÷ 3 =` | `1000000000 / 3` | `≈` 333333333.3 (boxed, medium size) |
| `1234.5 + 678.25 × 3 − 99.5 ÷ 2 + 77` | 2 lines: `1234.5 + 678.25 x 3` / `- 99.5 / 2 + 77` | `=` 2896.375 |
| `… + 1000 × 2 − 12.5 ÷ 4 =` (continuing) | 4 lines, last 3 in view, scrollbar | `=` 1945.0625 (boxed) |
| `4 + 12 ⌫` | `4 + 1` | `=` 5 |
| `4 + 5 + ⌫` | `4 + 5` | `=` 9 |
| `4 + 5 + 35 = ⌫` | `4 + 5 + 3` | `=` 12 |
| `3 . 7 5 ⌫` | `3.7` | `=` 3.7 |
| `7 ÷ 0` | `7 / 0` (divider: `! divide by 0`) | `=` Err |
| `9999999999 + 1` | `9999999999 + 1` (divider: `! too big`) | `=` Err |

---

## 7. No-Sleep

Keeps the PC from going to sleep or locking while you read or watch.

* **Toggle**: SYSTEM → **NS**. The label reads `NS ON` / `NS OFF`, and a
  steaming cup shows on the bottom line of every screen.
* **What it does**: every 60 seconds it moves the mouse pointer one pixel
  right and, 20 ms later, one pixel left.
* **Zero drift**: the move back is always sent once the first move went
  out, even if you switch No-Sleep off in between, so the pointer never
  wanders. It waits if you are using mouse keys, and does nothing while the
  PC is asleep.
* **Light**: the NS key glows dim blue with a soft blink every 2 s, and
  flashes cyan at each nudge. Dark when off.
* No-Sleep is **off after every power-up** (it is not saved).

---

## 8. Lights (RGB)

### 8.1 What the lights show

The firmware uses the key lights as indicators. On every layer all keys
are dark except:

| Light | When |
|---|---|
| the layer key in the layer's colour (§3) | always |
| BOOT key blinking red | on SYSTEM |
| NS key blue | on SYSTEM, while No-Sleep is on (§7) |
| calculator key colours | in calculator mode (§6) |
| bottom-right key, dim colour | while a pomodoro phase is loaded (§4.6) |
| green breathing / amber pulses on all keys | pomodoro alerts (§4.6) |
| all keys red, top-right key green | Host Power menu (§5.5) |
| all keys red, top-right key pulsing green | Host Power countdown (§5.5) |
| bottom-right key pulsing red, others dark | bootloader confirm (§10) |
| all keys red | flash mode (§10) |

### 8.2 Light keys (SYSTEM)

| Key | Does |
|---|---|
| RGB | all lights on / off |
| BRI+ / BRI− | brightness up / down |
| MODE+ / MODE− | next / previous effect |
| SPD+ / SPD− | effect speed |

The line under the SYSTEM header shows `RGB ON/OFF`, the effect number and
the brightness.

Notes:
* Because the keymap draws its indicators over every key, **the effect
  itself is not visible**; MODE and SPD change the stored effect only.
  Brightness sets how bright the alerts, the pomodoro hint, the No-Sleep
  key and the calculator colours are (with a minimum, so they never
  vanish). The layer key is always full colour.
* With RGB off, the keys stay dark except during a pomodoro alert or Host
  Power, which switch the lights on while they last.
* Host Power's red and green are always at full brightness.
* Light settings are saved automatically.

---

## 9. Settings and what is saved

| What | Saved? | When |
|---|---|---|
| Pomodoro settings (lengths, sessions) | yes | on Save, only if something changed |
| Pomodoro AUTO start | yes | when toggled |
| Pomodoro stats: since reset, lifetime | yes | when a phase ends, and on clear |
| Pomodoro stats: current session | no | lost at power-off |
| Running pomodoro timer | no | unplugging ends the phase |
| SYSINFO settings | yes | on Save in Setup, only if changed |
| Light on/off, brightness, effect, speed | yes | by QMK, when changed |
| Calculator | no | kept until power-off |
| No-Sleep | no | off at every power-up |
| Host Power | no | nothing on the macropad; on / off and the delay are in the agent's settings file (§5.5) |
| Key layout (VIA) | yes | see below |

* Nothing is written every second or per key press; writes happen only
  when something actually changed.
* Settings survive flashing new firmware.
* **Back to defaults**:
  * one pomodoro or SYSINFO setting: select it in its settings and press
    **Default**, then **Save**;
  * everything at once (settings, stats, lights, key layout): see §10.4.

**VIA.** The key layout can be changed in the VIA app (usevia.app or the
desktop app). Changes made there are kept until a firmware with a changed
built-in key layout is flashed; then the macropad reloads the built-in
layout once by itself.

---

## 10. Flashing and recovery

### 10.1 Entering the bootloader

**From the keys (normal way)**

1. Go to SYSTEM and press **BOOT** (the blinking red key).
2. The screen asks `ENTER BOOTLOADER?` and the bottom-right key pulses red.
   A bar shows the 10 seconds you have.
3. Press the **bottom-right key** to confirm. Any other key or the knob
   cancels.
4. The screen shows `FLASH MODE` and all keys turn red. A drive called
   **RPI-RP2** appears on the PC.

**Without working firmware**

* **Double-tap the reset button** on the side of the macropad, or
* hold the **knob press** down while pressing reset once.

Either way the RPI-RP2 drive appears (the screen may stay dark).

### 10.2 Flashing

Copy the `.uf2` file onto the RPI-RP2 drive. The macropad restarts by
itself with the new firmware. To cancel instead, unplug and replug.

After flashing, the first 6 seconds of the bottom line show the new build's
date and time.

### 10.3 Building the firmware

Needs QMK MSYS (Windows) with this repository:

```sh
qmk compile -kb adafruit/macropad -km Thinksomak
```

The file `adafruit_macropad_Thinksomak.uf2` appears in the `qmk_firmware`
folder. `qmk flash -kb adafruit/macropad -km Thinksomak` builds and waits
for the bootloader, then flashes.

The secure macro text comes from the keymap's private `config.h`, which is
not in the repository; a build needs that file.

### 10.4 Restoring and resetting stored settings

* **Normal flashing keeps everything** (settings, stats, lights). Older
  stored settings from earlier versions of this firmware are upgraded
  automatically.
* **Reset everything to factory defaults** (last resort): put the macropad
  in the bootloader, copy Raspberry Pi's `flash_nuke.uf2` (from the RP2040
  documentation) onto RPI-RP2 to erase it, then flash the firmware again.
  Settings, stats, light settings and the VIA layout all return to their
  defaults.

---

## 11. Troubleshooting, FAQ and quick-reference card

### 11.1 Troubleshooting

| Problem | Cause and fix |
|---|---|
| A digit came out as an operator | the key was held longer than 0.25 s; tap a little quicker |
| The knob did not change layer | a sub-mode is open (± or a page number in the header, CALC, or HOST POWER); close it. If the screen was dark or showing the screensaver, the first twist only woke it |
| Keys do the old thing after flashing | check the firmware date at power-up; the built-in layout reloads only when it changed in code; remaps made in VIA are replaced then |
| SYSINFO says NO HOST | start the agent; `main.py --list` must show an interface with usage `FF60/61`; after the PC wakes, the agent reconnects within seconds |
| No GPU value | set `gpu = nvidia` in `macropad_sysinfo.ini` (NVIDIA cards only) |
| No screensaver | needs STYLE = SAVER and a running agent (monitor icon lit) |
| No strip | needs STYLE = STRIP and a running agent; it shows only every EVERY for FOR seconds |
| The screen is dark | it sleeps after 10 minutes without a key (or the SAVER's SCREEN OFF time); press any key |
| The pointer moves by itself | No-Sleep is on (cup icon); switch it off on SYSTEM |
| Lights all off | RGB is off: SYSTEM → RGB |
| pip fails with CERTIFICATE_VERIFY_FAILED | an antivirus scans Python's HTTPS; see `host/README.md` |
| The PC does not see the macropad | try another cable (some are charge-only) or port |
| POWER menu: `Host agent not responding` | the agent is not running, or it is older than Host Power: rebuild and replace it (§5.1) |
| POWER menu: `Disabled in agent config` | set `allow_power_commands = true` in the agent's settings and restart the agent (§5.5) |
| POWER menu: `The PC refused it` | Windows said no (for example, a shutdown is already scheduled); the agent's log has the reason |
| Every key red, top-right green | a Host Power menu or countdown is open; the top-right key cancels (§5.5) |
| `Cancel unconfirmed` | the PC may still shut down: on the PC press Win+R, type `shutdown /a`, Enter |

### 11.2 FAQ

**Does it need software on the PC?** No, except the agent for PC stats.

**Does it work on Mac or Linux?** The keys do (TSKMGR's shortcut is
Windows-specific). The agent, and with it PC stats and Host Power, is
written for Windows.

**Can a stray key press turn my PC off?** Not unless you turned Host Power
on in the agent's settings. Even then it takes POWER and then Shutdown or
Restart (ignored for half a second after the menu opens), and the
countdown that follows can be cancelled with the top-right key.

**Can I change the keys?** Yes, in VIA, or in `keymap.c` and rebuild.

**Is the secure macro text safe?** It is stored in the firmware and typed
by the keys. It is not shown on screen or sent by the agent. Anyone who can
press the keys can type it.

**Does the pomodoro keep time while unplugged?** No. There is no clock or
battery.

**Why does the screen sleep?** To protect the OLED from burn-in. Use the
SAVER style if you want stats on screen while idle.

**Does No-Sleep move my mouse?** One pixel and back every minute; the
pointer ends where it was.

### 11.3 Quick-reference card

**Knob**

| Where | Twist | Press |
|---|---|---|
| Any layer | change layer | Mute (POMODORO: adjust time) |
| Calculator | scroll the expression | Mute |
| Pomodoro settings / SYSINFO setup | −1 / +1 | next field |
| Adjust time | −1 / +1 minute | done |
| Pomodoro stats / SYSINFO view | page | close |
| POWER menu | nothing | nothing |
| Screen off / screensaver | wakes it only | wakes it and mutes at once |

**Layers** (row 1 → row 4, left / middle / right)

| Layer | Row 1 | Row 2 | Row 3 | Row 4 |
|---|---|---|---|---|
| NUMPAD | 7% 8/ 9* | 4 5- 6+ | 1 2 3 | Ent(calc) 0. Bksp |
| MACRO | $ % SQL | — | — | TCS MACE RUN |
| MEDIA | — | · Stop · | Prev Play Next | Vol− Mute Vol+ |
| NAV | PgUp ↑ PgDn | ← ↓ → | Home · End | S-Tab Tab Del |
| SYSTEM | TaskMgr Power Boot | NS SysInfo RGB | Mode+ Bri+ Spd+ | Mode− Bri− Spd− |
| POMODORO | Start Skip Reset | Edit Adjust Stats | Focus Break Long | Auto Undo Cycle |

**Calculator** (hold ENT 0.4 s on NUMPAD)

| Row 1 | Row 2 | Row 3 | Row 4 |
|---|---|---|---|
| 7/% 8/÷ 9/× | 4/± 5/− 6/+ | 1/CE 2 3/TYPE | =/OFF 0/. ⌫/AC |

**Settings screens** (pomodoro EDIT, SYSINFO setup)

| Row 1 | Row 2 | Row 3 | Row 4 |
|---|---|---|---|
| Save · Field↑ · Cancel | Default · Field↓ · — | −1 · — · +1 | −5 · — · +5 |

**Bootloader**: SYSTEM → BOOT → bottom-right key. Or double-tap the side
reset button.

**Host Power** (agent with `allow_power_commands = true`): SYSTEM → POWER →
top-left = shut down, top-middle = restart, top-right = cancel. During the
countdown the top-right key cancels on any layer. Open apps are closed
without saving when it ends.

---

## 12. Specifications and glossary

### 12.1 Specifications

| Item | Value |
|---|---|
| Board | Adafruit Macropad RP2040 |
| Processor | Raspberry Pi RP2040, dual Arm Cortex-M0+ |
| Keys | 12, plus a rotary encoder with push switch |
| Display | 128 × 64 monochrome OLED (SH1106), SPI |
| Lights | 12 RGB LEDs, one per key |
| USB | USB-C; keyboard, media keys, mouse (No-Sleep), Raw HID (VIA, PC stats, Host Power) |
| USB IDs | VID 0x239A, PID 0x0108 |
| Firmware | QMK, keymap Thinksomak, VIA enabled, 7 VIA layers (6 used) |
| Saved data | 64-byte settings block (pomodoro, stats, SYSINFO) + QMK's own settings |
| Hold time | 0.25 s (Enter and Backspace 0.4 s) |
| Screen sleep | 10 min without input (SAVER style: its own times) |
| PC agent | Windows, Python 3.8+ or a single exe; 1 packet per second (32 bytes); Host Power answers within 2 s |
| Host Power | off by default (agent setting); countdown 15 s by default (5–99 s); menu closes after 10 s |

### 12.2 Glossary

| Term | Meaning |
|---|---|
| Layer | one of the six key sets, chosen with the knob |
| Sub-mode | a screen that takes over the keys or knob for a while (calculator, settings, stats, adjust, SYSINFO, POWER menu) |
| Tap / hold | a quick press / a press held past the hold time |
| Tap dance | a key that does different things for 1, 2 or 3 quick taps or a hold |
| Secure macro key | a key that types stored sign-in text |
| Agent | the PC program that sends stats to the macropad and carries out Host Power |
| Host Power | shutting down or restarting the PC from the macropad (§5.5), through the agent |
| NO HOST | the macropad gets no stats from the PC |
| Strip / screensaver | the two ways stats show outside the SYSINFO view |
| Phase | one focus, short break or long break of the pomodoro timer |
| Cycle | the focus sessions up to and including a long break |
| Extension | extra time given to a phase that already ended (ADJUST) |
| VIA | an app for changing the key layout without rebuilding |
| QMK | the open-source keyboard firmware this is built on |
| Bootloader | the mode in which new firmware can be copied on |
| UF2 | the firmware file format the RP2040 bootloader accepts |
| Raw HID | the USB channel VIA and the PC agent use |
| NKRO | n-key rollover: every key press is reported, however many at once |
