// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * The one EEPROM record of this keymap: pomodoro settings plus the
 * persistent stats, in the QMK user datablock.
 *
 * Everything works on a RAM copy. Only storage_commit() writes, and only
 * when the copy differs from what EEPROM holds. Callers commit on a phase
 * completion, an edit-mode save or a stats reset, never per second or per
 * key press.
 *
 * The datablock version lives in QMK's 32-bit user config word, so this
 * keymap must never call eeconfig_update_user().
 */
#pragma once

#include <stdint.h>
#include "pomodoro.h"

typedef struct {
    uint32_t focus_min;
    uint32_t sessions;
} storage_counters_t;

void storage_init(void); // keyboard_post_init_user, before pomodoro_init()

/* All-zero settings mean "nothing stored yet": the caller uses defaults. */
const pomodoro_settings_t *storage_settings(void);
void                       storage_set_settings(const pomodoro_settings_t *s);

/* Option bits (auto start). Zero in blocks saved before they existed. */
uint8_t storage_flags(void);
void    storage_set_flags(uint8_t flags);

/* Fingerprint of the compiled keymap last copied into VIA's EEPROM keymap.
 * Zero in blocks saved before it existed. */
uint32_t storage_keymap_sig(void);
void     storage_set_keymap_sig(uint32_t sig);

storage_counters_t *storage_lifetime(void);
storage_counters_t *storage_since_reset(void);

void storage_commit(void);
