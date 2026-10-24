// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "boot_confirm.h"

_Static_assert((MATRIX_ROWS) * (MATRIX_COLS) <= 16, "swallow needs one bit per key");

static bool     active;
static uint32_t since;
static uint16_t swallow; // keys whose press cancelled: their release is eaten too

void boot_confirm_begin(void) {
    active = true;
    since  = timer_read32();
}

bool boot_confirm_active(void) {
    return active;
}

uint32_t boot_confirm_remaining_ms(void) {
    if (!active) return 0;
    uint32_t e = timer_elapsed32(since);
    return e >= BOOT_CONFIRM_TIMEOUT_MS ? 0 : BOOT_CONFIRM_TIMEOUT_MS - e;
}

void boot_confirm_task(void) {
    if (active && timer_elapsed32(since) >= BOOT_CONFIRM_TIMEOUT_MS) active = false;
}

bool boot_confirm_process_record(keyrecord_t *record) {
    keyevent_t ev  = record->event;
    uint16_t   bit = 0;
    if (!IS_ENCODEREVENT(ev) && ev.key.row < MATRIX_ROWS && ev.key.col < MATRIX_COLS) {
        bit = (uint16_t)1 << (ev.key.row * MATRIX_COLS + ev.key.col);
    }

    if (!ev.pressed) {
        if (swallow & bit) {
            swallow &= ~bit;
            return false;
        }
        return true;
    }
    if (!active) return true;

    if (bit && ev.key.row == BOOT_CONFIRM_ROW && ev.key.col == BOOT_CONFIRM_COL) {
        active = false;
        reset_keyboard(); // jumps to the bootloader; shutdown_user() runs first
        return false;
    }
    active = false; // anything else cancels
    swallow |= bit;
    return false;
}
