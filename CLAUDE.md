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
| `neo/neo65/solder` | STM32F103 / stm32duino | Custom board from the same old tree; still uses `info.json` only. NOT yet migrated to `keyboard.json` like `ansi`. Likely needs the same fix (see Lessons below). |

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
# qmk compile -kb neo/neo65/solder  -km <keymap>   # check which keymap exists
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

## Workflow rules

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
