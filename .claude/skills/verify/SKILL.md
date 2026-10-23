---
description: Compile all owner boards and lint touched board JSON. Use before commits, after editing keymaps, board files, keyboard.json, rules.mk or config.h, and after upstream merges.
---

Run from the repo root inside QMK MSYS. If `qmk` is not on PATH, wrap each command:
MSYSTEM=MINGW64 CHERE_INVOKING=1 /c/QMK_MSYS/usr/bin/bash.exe --login -c "<command>"

1. For each touched board, compile it. If core, shared or unsure, compile all:
   - qmk compile -kb adafruit/macropad -km Thinksomak
   - qmk compile -kb chickenman/ciel -km Thinksomak
   - qmk compile -kb monsgeek/m1 -km Thinksomak
   - qmk compile -kb neo/neo65/ansi -km Thinksomak
2. Run `qmk lint -kb <kb> -km Thinksomak` for any board whose keyboard.json changed.
3. Report a pass or fail line per board. For failures show the first error line only, not the whole log.
4. Never print or open keyboards/adafruit/macropad/keymaps/Thinksomak/config.h.
5. Do not commit or push. Only report results.