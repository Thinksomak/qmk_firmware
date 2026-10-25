// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * LED overlays: the pomodoro alerts and its dim "running" hint, the No-Sleep
 * key, the calculator keys, the Host Power menu and countdown, the boot
 * confirm and flash mode. Drawn from the RGB matrix indicator callback, so
 * they sit on top of whatever is shown and go by themselves. Nothing is
 * saved to EEPROM.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Call last in rgb_matrix_indicators_advanced_user(). */
void ui_rgb_pomodoro_overlay(void);

/* Alert start / end: switch RGB on for the alert if the user had it off,
 * without saving, and switch it back off afterwards. */
void ui_rgb_alert_begin(void);
void ui_rgb_alert_end(void);

/* Host Power: the same for its menu and countdown (call every tick with
 * whether one shows), and their lights: every key vivid red, the cancel
 * key (top-right) green, pulsing while counting. Call after the pomodoro
 * overlay. */
void ui_rgb_hold_for_host_power(bool on);
void ui_rgb_host_power(bool counting);

/* No-Sleep key light, for the layer that has the No-Sleep key. Dark while
 * No-Sleep is off. While on: steady dim blue, a soft blue blink every 2 s,
 * and a short cyan flash each time a keep-awake nudge goes out. */
void ui_rgb_nosleep_key(uint8_t led);

/* Calculator mode on the numpad: dim white digits, amber keys that hold an
 * operator, green "=", red BKSP. Call before the pomodoro overlay. */
void ui_rgb_calc(void);

/* Boot confirm screen: everything dark except the confirm key, pulsing
 * red. Call after the other overlays so it wins. */
void ui_rgb_boot_confirm(void);

/* Entering the bootloader: every key red, sent to the LEDs at once. They
 * keep that colour while the board waits to be flashed. */
void ui_rgb_flash_mode(void);
