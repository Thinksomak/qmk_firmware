// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "storage.h"
#include <stddef.h>
#include <string.h>

#define STORAGE_MAGIC 0xB7
#define STORAGE_LAYOUT 2

/* Byte layout. Every field sits on its natural alignment, so the struct
 * needs no packing and its counters can be used in place on the
 * Cortex-M0+, which faults on unaligned 32-bit access.
 *
 *   0  magic          1  layout         2  option flags   3  checksum
 *   4  focus min      5  short min      6  long min       7  sessions per long break
 *   8  lifetime focus minutes (u32)     12 lifetime completed sessions (u32)
 *  16  since-reset focus minutes (u32)  20 since-reset completed sessions (u32)
 *  24  compiled-keymap fingerprint (u32)
 *  28  reserved, zero (4 bytes of headroom)
 *
 * Fields added later sit in bytes that were reserved and always zero, so
 * blocks saved by older builds stay valid without a version bump.
 */
typedef struct {
    uint8_t             magic;
    uint8_t             layout;
    uint8_t             flags; // option bits
    uint8_t             checksum;
    pomodoro_settings_t settings;
    storage_counters_t  lifetime;
    storage_counters_t  since_reset;
    uint32_t            keymap_sig;
    uint8_t             reserved[4];
} storage_block_t;

_Static_assert(sizeof(storage_block_t) == 32, "storage block must stay 32 bytes");
_Static_assert(offsetof(storage_block_t, settings) == 4, "storage layout changed");
_Static_assert(offsetof(storage_block_t, lifetime) == 8, "storage layout changed");
_Static_assert(offsetof(storage_block_t, since_reset) == 16, "storage layout changed");
_Static_assert(offsetof(storage_block_t, keymap_sig) == 24, "storage layout changed");

#if (EECONFIG_USER_DATA_SIZE) < 32
#    error "storage needs EECONFIG_USER_DATA_SIZE >= 32 (set in rules.mk)"
#endif

static storage_block_t ram;    // working copy
static storage_block_t stored; // what EEPROM holds

/* CRC-8 (polynomial 0x07) over every byte except the checksum itself. */
static uint8_t block_crc(const storage_block_t *b) {
    const uint8_t *p   = (const uint8_t *)b;
    uint8_t        crc = 0;
    for (uint8_t i = 0; i < sizeof *b; i++) {
        if (i == offsetof(storage_block_t, checksum)) continue;
        crc ^= p[i];
        for (uint8_t k = 0; k < 8; k++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

void storage_init(void) {
    // QMK hands back zeros when the stored datablock version does not match
    // EECONFIG_USER_DATA_VERSION, so an old layout fails the magic check.
    eeconfig_read_user_datablock(&stored, 0, sizeof stored);
    ram = stored;
    if (ram.magic != STORAGE_MAGIC || ram.layout != STORAGE_LAYOUT || ram.checksum != block_crc(&ram)) {
        memset(&ram, 0, sizeof ram); // zero settings = defaults, zero counters
        ram.magic  = STORAGE_MAGIC;
        ram.layout = STORAGE_LAYOUT;
    }
}

const pomodoro_settings_t *storage_settings(void) {
    return &ram.settings;
}

void storage_set_settings(const pomodoro_settings_t *s) {
    ram.settings = *s;
}

uint8_t storage_flags(void) {
    return ram.flags;
}

void storage_set_flags(uint8_t flags) {
    ram.flags = flags;
}

uint32_t storage_keymap_sig(void) {
    return ram.keymap_sig;
}

void storage_set_keymap_sig(uint32_t sig) {
    ram.keymap_sig = sig;
}

storage_counters_t *storage_lifetime(void) {
    return &ram.lifetime;
}

storage_counters_t *storage_since_reset(void) {
    return &ram.since_reset;
}

void storage_commit(void) {
    ram.checksum = block_crc(&ram);
    if (memcmp(&ram, &stored, sizeof ram) == 0) return; // nothing new: no flash write
    eeconfig_update_user_datablock(&ram, 0, sizeof ram);
    stored = ram;
}
