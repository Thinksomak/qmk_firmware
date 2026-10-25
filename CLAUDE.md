# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A personal fork of QMK Firmware (`origin` = Thinksomak/qmk_firmware, `upstream` = qmk/qmk_firmware). Nearly all of the tree is stock upstream QMK. The owner's own work sits on branch `qmk-2026` as a few commits on top of upstream `master`, mostly `Thinksomak` keymaps plus small board-level tweaks. Treat everything else as upstream code. Don't refactor it unless asked.

## The owner's boards

| Keyboard (build target) | MCU / bootloader | Custom files |
|---|---|---|
| `adafruit/macropad` | RP2040 / rp2040 (UF2) | `keymaps/Thinksomak/` (OLED over SPI, encoder map, RGB matrix, tap dance); board `config.h` edited: PWM audio disabled, `SPEAKER_SHUTDOWN GP14`, `DYNAMIC_KEYMAP_LAYER_COUNT 7` |
| `chickenman/ciel` | atmega32u2 / atmel-dfu | `keymaps/Thinksomak/` (LTO on; flash space is tight) |
| `monsgeek/m1` | WB32FQ95 / wb32-dfu | `keymaps/Thinksomak/`; board `m1.c` edited: persistent default layers and RGB toggle handling |
| `neo/neo65/ansi` | STM32F103 / stm32duino (app at `0x08006000`, custom linker script in `ld/`) | Whole board definition is custom, brought over from an older tree (not in upstream). `keyboard.json` holds all data-driven config (old `info.json` was merged into it on 2026-10-23). Also has a `shigure` keymap. |

All `Thinksomak` keymaps enable VIA, tap dance, and mouse keys. Keycodes were migrated to current QMK names (`MS_*` for mouse, `RM_*` for RGB matrix). Use the current names, not legacy `KC_MS_*` / `RGB_*`.

### Secrets: important

`keyboards/adafruit/macropad/keymaps/Thinksomak/config.h` defines `CRED_USER*` / `CRED_PASS*` macros that `keymap.c` types out through tap dance (`TD_CRED_*`). The file is gitignored on purpose, and `remove_sensitive_lines.py` (also ignored) is a git-filter-repo callback used to scrub it from history. Never commit, `git add -f`, print, or copy the contents of either file. `keyboards/wireless/`, `CLAUDE.local.md` and `.claude/settings.local.json` are also local-only (see the bottom of `.gitignore`). Never write tokens, passwords or credentials into any file in this repo.

## Building

The `qmk` CLI and `make` live in QMK MSYS (`C:\QMK_MSYS`). The VS Code integrated terminal defaults to the `QMK_MSYS` profile, so `qmk` works there. If a different shell is used (Git Bash / PowerShell), wrap the command (untested, verify once):

```sh
MSYSTEM=MINGW64 CHERE_INVOKING=1 /c/QMK_MSYS/usr/bin/bash.exe --login -c "qmk compile -kb neo/neo65/ansi -km Thinksomak"
```

Run commands from the repo root:

```sh
qmk compile -kb adafruit/macropad -km Thinksomak
qmk compile -kb chickenman/ciel   -km Thinksomak
qmk compile -kb monsgeek/m1       -km Thinksomak
qmk compile -kb neo/neo65/ansi    -km Thinksomak

qmk flash   -kb <kb> -km <km>      # compile + flash (put the board in bootloader first)
make <kb>:<km>                     # legacy equivalent, e.g. make neo/neo65/ansi:Thinksomak
```

Firmware lands in the repo root as `<kb_with_underscores>_<km>.{bin,hex,uf2}` (gitignored). Intermediate objects go to `.build/`. The `milc.set_metadata` deprecation warning is harmless noise from the QMK CLI.

Validation and formatting:

```sh
qmk lint -kb <kb> -km <km>              # data-driven config / keymap checks
qmk format-json -i <path>/keyboard.json
qmk format-c <files>                    # clang-format with the repo's .clang-format
qmk format-python
```

Tests (core features, not keyboards; need a host GCC):

```sh
qmk test-c                      # all C unit/integration tests under tests/
qmk test-c -t tap_dance         # one suite (dir name under tests/), also: make test:tap_dance
qmk pytest                      # Python tests for lib/python (qmk CLI)
```

## Macropad Thinksomak keymap (adafruit/macropad)

Modular files in `keyboards/adafruit/macropad/keymaps/Thinksomak/`, each listed in its `rules.mk` `SRC`:

| File | Job |
|---|---|
| `keymap.c` | 6 main layers in knob order: NUMPAD, MACRO, MEDIA, NAV, SYSTEM (with RGB), POMODORO (no mouse layer; `MOUSEKEY_ENABLE` stays for No-Sleep); encoder map `KC_LDN`/`KC_LUP` on every layer; OLED legends (`numpad` flag = digit / hold-operator columns); positional maps for pomodoro EDIT/STATS and SYSINFO view/SETUP; MACRO tap dances (secure macro keys TD_CRED_1/2, RUN); layer LEDs; the `process_record_user` chain |
| `numpad_ops.c` | numpad keys are `NP(kc)` = `LT(0, kc)`; the hold is intercepted (7 %, 8 /, 9 *, 5 -, 6 +, 0 ., ENT = calculator); per-key tapping terms; `#error` checks on tap-hold options; in calculator mode the twist scrolls the expression by display lines |
| `calc.c` | desk calculator, pure C (int64 fixed point, 6 decimals): the expression is a list of numbers and operators (`calc_tok_t`, exact values; the oldest folded into `base` past 96 characters) and the value is replayed from it after every key, so Backspace can step back one character or operator at a time; the expression never shows `=`; `calc_display_lines()` breaks the expression into the OLED's 20-character small-font lines (tape style, a new line before each operator); `calc_value_rounded()` drives the `≈` mark, so screen, knob scroll limit and tests share one rule; unit test `tests/run_calc_test.sh` (needs `/mingw64/bin/gcc` in PATH) |
| `sysinfo.c` | Raw HID receive (`via_command_kb` in keymap.c passes command 0xC5 here), history, SYSINFO view/SETUP, display style OFF / STRIP / SAVER (screensaver) |
| `pomodoro.c`, `stats.c`, `nosleep.c`, `boot_confirm.c` | pomodoro timer, stats, keep-awake, two-step bootloader |
| `storage.c` | the 64-byte EEPROM user datablock, layout 4 (30 bytes free); upgrades the 32-byte layouts 2 and 3 in place, clearing what VIA left in bytes 32-63 |
| `ui_oled.c`, `ui_rgb.c` | every OLED screen and LED overlay; the screen-off gate and screensaver. Key legends, the calculator and SYSINFO are drawn in an 8-page canvas (`cv_*`) at exact pixel positions; legends sit on three fixed columns centred at x 21 / 64 / 107 |
| `host/` | PC agent (`main.py`, `macropad_sysinfo.ini`, `build.bat`, README) that sends PC stats over Raw HID |
| `USER_GUIDE.md` | the user manual: every layer, key, mode, setting, flashing and recovery |

`process_record_user` order matters: boot confirm, the "first twist only wakes" rule, pomodoro, sysinfo, numpad, then the keymap's own keycodes.

Knob convention: **the twist always changes the main layer (wrapping); a mode that is open takes it for its own values** (pomodoro edit / stats / adjust / alert, SYSINFO view and SETUP, calculator scroll) and lets go the moment it closes. The mode is worked out from what is open, never stored. The OLED header marks only the exceptions, with a plain glyph at its left: ± where the twist changes a value; stats and SYSINFO show their page number `k/N` on the right instead; the calculator shows a scrollbar while its expression has more than three lines (small expression on top, a divider, then `=` at the left and the value large and right-aligned, stepping down to a medium size when too wide). Main layers show no glyph. While the screen is off or the screensaver shows, the first twist only wakes it; keys wake it and act.

Host agent: needs Python 3.8+ from python.org (QMK MSYS's Python cannot build psutil). `cd host && py -3 -m venv .venv && .venv\Scripts\pip install -r requirements.txt && .venv\Scripts\python main.py --console`; `build.bat` makes `dist\macropad-sysinfo.exe` (`build.bat --console` for a debug build). `main.py --stop` ends a running agent cleanly; `--dry-run` needs no device.

## Repo-specific architecture notes

- Config layering (later wins): core defaults, keyboard JSON, keyboard `config.h`/`rules.mk`, keymap `config.h`/`rules.mk`. Keymap `rules.mk` toggles features per keymap (`VIA_ENABLE`, `TAP_DANCE_ENABLE`, `RGB_MATRIX_ENABLE`, ...).
- Data-driven config: `keyboard.json` (schema in `data/schemas/`, mappings in `data/mappings/`) is converted to `config.h`/`rules.mk` at build time. Prefer JSON settings over raw `#define`s when a mapping exists.
- Callback chain: board `<board>.c` files (e.g. `m1.c`, `ansi.c`) implement `*_kb()` hooks and must call the matching `*_user()` hooks defined by the keymap. When editing them, call `_user` first and honor its return value.
- The neo65 overrides the linker script (`MCU_LDSCRIPT`) because the stm32duino bootloader occupies the first 24 KB.
- ChibiOS boards (STM32, RP2040, WB32) may add `halconf.h`, `mcuconf.h`, `chconf.h` next to `keyboard.json`.

## Lessons from the 2026-10 upstream merge

- `url` in `keyboard.json` must be non-empty or validation fails. Errors like "No LAYOUTs defined", "Bootmagic row/col must be in range" and "RGB_MATRIX_DRIVER= is not a valid matrix type" were knock-on effects of the rejected JSON, not the root cause.
- Never keep `info.json` and `keyboard.json` in the same board folder ("Invalid info.json location").
- Prefer `keyboard.json` for every leaf board. Do not assume `info.json`-only leaves still validate.
- neo65/ansi: two matrix entries (the Backspace and `\` keys, around lines 91 and 107 of `keyboard.json`) differed between the owner's old `info.json` and the upstream-style file. The owner's values are kept. If those keys misbehave on hardware, the values are swapped.
- `features.rgb_matrix` in the JSON must be true when the keymap sets `RGB_MATRIX_ENABLE = yes`.

## Lessons from the macropad features (2026-10-24)

- VIA keeps its EEPROM keymap across same-day reflashes. The macropad keymap fingerprints `keymaps[]` + `encoder_map[]` and reloads them when they change; other VIA boards do not.
- The keymap `config.h` is force-included (`-include`) into every source file, so a module can check its settings with `#ifdef`/`#error` without anyone opening that file.
- Raw HID next to VIA: define `via_command_kb()` and return true only for your own command byte; VIA handles everything else.
- The OLED driver's font is `static`; to draw glyphs yourself, `#include OLED_FONT_H` again in your file.
- Never search the macropad keymap folder with a glob (`*.h`, `**`): it would include the private `config.h`. Name the files.
- QMK checks the user datablock by its version only, not its size. Growing `EECONFIG_USER_DATA_SIZE` keeps the old bytes but moves VIA's area, so the new bytes hold VIA leftovers and must be cleared on upgrade (storage.c does).
- The OLED screens can be rendered on the PC by compiling `ui_oled.c` and friends with stub `oled_*` functions; that is how the field guide's screen pictures are made.
- Screens that need exact pixel positions (key legends, calculator, SYSINFO and its screensaver) are composed in an 8-page canvas in `ui_oled.c` (`cv_*` helpers); the rest draw straight onto the 8-px page grid. SYSINFO overview pages come from `sysinfo_overview_items()`, which decides the split.
- The keymap blanks every LED each frame (`rgb_matrix_indicators_advanced_user`) and lights only indicators, so the RGB effect itself is never visible; MODE / SPD keys only change the stored effect.
- Never call `eeconfig_update_user()` in the macropad keymap: the datablock version lives in that word. Inside the 32 bytes use storage.c's layout number, not `EECONFIG_USER_DATA_VERSION` (bumping that wipes settings and stats).
- From Git Bash, `timeout -s INT` does not reach a native Windows process; stop the agent with `main.py --stop`. `bash --login` in QMK MSYS changes to the home folder, so pass absolute paths.

## Workflow rules

- Update `keyboards/adafruit/macropad/keymaps/Thinksomak/USER_GUIDE.md` whenever key layouts, modes or settings change.
- Work on `qmk-2026` or a feature branch off it. As of 2026-10-23 it contains `upstream/master` (tag 0.34.7), 6 commits ahead and 0 behind.
- To update: `git fetch upstream && git merge upstream/master`, then compile ALL five targets above and run `qmk lint` on any board JSON touched.
- Do not push, force-push, rebase published history, or run git-filter-repo without asking first. Pushing needs a GitHub personal access token entered by the owner.
- Stage specific files, not `git add -A` at the repo root.
- After any board or keymap change, run the matching `qmk compile` and report the result before saying the work is done.
- Keep commits small, with messages like `neo65: <what changed>`.

## Upstream conventions (only if upstreaming)

- New keyboards go to `master`; changes to existing keyboards and core go to `develop`. One keyboard per PR.
- Lowercase file and directory names; `*.c`/`*.h` need a GPL-compatible header (`// SPDX-License-Identifier: GPL-2.0-or-later`).
- `keyboard.json` needs `url`, `maintainer`, `usb.vid/pid/device_version`, processor, bootloader, and a `matrix` on every layout key. Layout macros go in JSON, not `.h`.
- Upstream would not accept the `Thinksomak` keymaps (VIA-enabled, personal). They stay in this fork.
