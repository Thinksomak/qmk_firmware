// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * The keymap's EEPROM record (see storage.h): a RAM copy, a CRC-8, upgrades
 * of older layouts, and writes only when something changed.
 */

#include "storage.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define STORAGE_MAGIC 0xB7
#define STORAGE_LAYOUT 4
#define OLD_SIZE 32 // layouts 2 and 3 were 32-byte blocks

/* Byte layout. Every field sits on its natural alignment, so the struct
 * needs no packing and its counters can be used in place on the
 * Cortex-M0+, which faults on unaligned 32-bit access.
 *
 *   0  magic          1  layout         2  option flags   3  checksum
 *   4  focus min      5  short min      6  long min       7  sessions per long break
 *   8  lifetime focus minutes (u32)     12 lifetime completed sessions (u32)
 *  16  since-reset focus minutes (u32)  20 since-reset completed sessions (u32)
 *  24  compiled-keymap fingerprint (u32)
 *  28  SYSINFO show   29 style   30 every   31 for          (layout 3: 29 = strip on)
 *  32  SYSINFO screensaver idle   33 screen off after        (layout 4)
 *  34  reserved, zero (30 bytes of headroom)
 *
 * Layouts 2 and 3 were 32 bytes, with the same meaning for bytes 0-31 (zero
 * there means defaults). Growing the block to 64 bytes moved VIA's EEPROM
 * area up, so bytes 32-63 of an old block hold what VIA left there: they
 * are cleared, and settings and stats carry over.
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
    storage_sysinfo_t   sysinfo;
    uint8_t             reserved[30];
} storage_block_t;

_Static_assert(sizeof(storage_block_t) == 64, "storage block must stay 64 bytes");
_Static_assert(offsetof(storage_block_t, settings) == 4, "storage layout changed");
_Static_assert(offsetof(storage_block_t, lifetime) == 8, "storage layout changed");
_Static_assert(offsetof(storage_block_t, since_reset) == 16, "storage layout changed");
_Static_assert(offsetof(storage_block_t, keymap_sig) == 24, "storage layout changed");
_Static_assert(offsetof(storage_block_t, sysinfo) == 28, "storage layout changed");

#if (EECONFIG_USER_DATA_SIZE) < 64
#    error "storage needs EECONFIG_USER_DATA_SIZE >= 64 (set in rules.mk)"
#endif

static storage_block_t ram;    // working copy
static storage_block_t stored; // what EEPROM holds

/* CRC-8 (polynomial 0x07) over the first len bytes except the checksum. */
static uint8_t block_crc(const storage_block_t *b, uint8_t len) {
    const uint8_t *p   = (const uint8_t *)b;
    uint8_t        crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        if (i == offsetof(storage_block_t, checksum)) continue;
        crc ^= p[i];
        for (uint8_t k = 0; k < 8; k++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* Reads the block and checks it: the current layout is used as it is, a
 * 32-byte layout 2 or 3 block is upgraded in RAM (written with the next
 * commit), anything else starts from zero (= defaults). */
void storage_init(void) {
    // QMK hands back zeros when the stored datablock version does not match
    // EECONFIG_USER_DATA_VERSION, so a wiped block fails the magic check.
    eeconfig_read_user_datablock(&stored, 0, sizeof stored);
    ram        = stored;
    bool magic = ram.magic == STORAGE_MAGIC;
    if (magic && ram.layout == STORAGE_LAYOUT && ram.checksum == block_crc(&ram, sizeof ram)) {
        return; // current layout
    }
    if (magic && (ram.layout == 2 || ram.layout == 3) && ram.checksum == block_crc(&ram, OLD_SIZE)) {
        // A 32-byte block from an older build: keep it, clear what follows.
        memset((uint8_t *)&ram + OLD_SIZE, 0, sizeof ram - OLD_SIZE);
        ram.layout = STORAGE_LAYOUT; // written with the next commit
        return;
    }
    memset(&ram, 0, sizeof ram); // zero settings = defaults, zero counters
    ram.magic  = STORAGE_MAGIC;
    ram.layout = STORAGE_LAYOUT;
}

/* ---- Accessors to the RAM copy (see storage.h) ---------------------- */

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

const storage_sysinfo_t *storage_sysinfo(void) {
    return &ram.sysinfo;
}

void storage_set_sysinfo(const storage_sysinfo_t *s) {
    ram.sysinfo = *s;
}

/* Writes the RAM copy to EEPROM if it differs from what is there. */
void storage_commit(void) {
    ram.checksum = block_crc(&ram, sizeof ram);
    if (memcmp(&ram, &stored, sizeof ram) == 0) return; // nothing new: no flash write
    eeconfig_update_user_datablock(&ram, 0, sizeof ram);
    stored = ram;
}
