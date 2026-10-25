# Thinksomak keymap for adafruit/macropad: build features and sources.
# The board (keyboard.json, rules.mk) already turns on the encoder, OLED
# over SPI, RGB matrix, extra keys and NKRO; the lines below only change
# what this keymap needs differently.

VIA_ENABLE = yes            # remap keys in the VIA app; also brings Raw HID (sysinfo.c)
ENCODER_MAP_ENABLE = yes    # the knob's twist comes from encoder_map[] in keymap.c
TAP_DANCE_ENABLE = yes      # MACRO layer: secure macro keys and RUN
MOUSEKEY_ENABLE = yes       # off on the board; No-Sleep sends its nudges through it
BOOTMAGIC_ENABLE = no       # on for the board; SYSTEM's BOOT key does this job
AUDIO_ENABLE = no           # on for the board; the speaker is not used
OPT_DEFS += -DNO_DEBUG      # no console, so drop the debug code

# Pomodoro timer, stats, EEPROM storage, No-Sleep, boot confirm, OLED and RGB
SRC += pomodoro.c storage.c stats.c nosleep.c boot_confirm.c ui_oled.c ui_rgb.c
# Numpad operators on hold, and the calculator (calc.c is pure C, see tests/)
SRC += numpad_ops.c calc.c
OPT_DEFS += -DQUICK_TAP_TERM_PER_KEY
# PC stats over Raw HID, and Host Power (shut down / restart through the
# same PC agent, which is in host/)
SRC += sysinfo.c hostpower.c

# EEPROM user datablock: pomodoro settings + stats + SYSINFO settings, 64
# bytes (see storage.c; it was 32 up to layout 3). Bumping the version makes
# QMK wipe the block (settings and stats): don't. Layout changes go through
# storage.c's own layout number, which upgrades old blocks instead. A size
# change moves VIA's EEPROM area, so VIA reloads its keymap once.
OPT_DEFS += -DEECONFIG_USER_DATA_SIZE=64 -DEECONFIG_USER_DATA_VERSION=0x504D0002
