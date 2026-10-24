// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * All OLED screens: layer view with status strip, pomodoro clock, edit
 * mode and the phase-end alert. Redraws only when the shown content changes.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* One layer's legend: header name and 12 key labels (max 6 chars each),
 * left to right, top to bottom. */
typedef struct {
    const char *name;
    const char *keys[12];
} ui_layer_legend_t;

/* Use as a key label to show START / PAUSE / RESUME for the timer key. */
#define UI_LEGEND_POMO_TOGGLE "\x01"

/* Use as a key label to show NS ON / NS OFF for the No-Sleep key. */
#define UI_LEGEND_NOSLEEP "\x02"

/* Extra status bits passed by the keymap. */
enum {
    UI_STATUS_NOSLEEP  = 1 << 0, // No-Sleep mouse jiggler is armed
    UI_STATUS_RGB_INFO = 1 << 1, // show RGB on/off, mode and brightness
};

/* Call from oled_task_user(). legend may be NULL. */
void ui_oled_render(uint8_t layer, uint8_t layer_count, const ui_layer_legend_t *legend, bool pomodoro_layer, uint8_t status);

/* Call from shutdown_user() before the bootloader jump: shows how to flash. */
void ui_oled_flash_mode(void);
