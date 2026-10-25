// Copyright 2024-2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Names for the 12 key LEDs of the Adafruit Macropad RP2040, so code can say
 * which key it lights. LED_Krc is the key in row r (0 = top) and column c
 * (0 = left); the knob has no LED. Used by keymap.c and ui_rgb.c.
 */
#pragma once

#ifdef RGB_MATRIX_ENABLE
// clang-format off
enum led_location_map {
    LED_K00, LED_K01, LED_K02,
    LED_K10, LED_K11, LED_K12,
    LED_K20, LED_K21, LED_K22,
    LED_K30, LED_K31, LED_K32
};
// clang-format on
#endif
