// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Two-step bootloader entry. The BOOT key only opens a confirm screen.
 * The confirm key (bottom-right, far from BOOT) enters the bootloader;
 * any other key or the knob cancels, and so does waiting
 * BOOT_CONFIRM_TIMEOUT_MS. Nothing pressed while it is open reaches the PC.
 */
#pragma once

#include "quantum.h"

#ifndef BOOT_CONFIRM_TIMEOUT_MS
#    define BOOT_CONFIRM_TIMEOUT_MS 10000
#endif

/* Matrix position of the confirm key: bottom-right. */
#ifndef BOOT_CONFIRM_ROW
#    define BOOT_CONFIRM_ROW 4
#endif
#ifndef BOOT_CONFIRM_COL
#    define BOOT_CONFIRM_COL 2
#endif

void     boot_confirm_begin(void);
bool     boot_confirm_active(void);
uint32_t boot_confirm_remaining_ms(void);
void     boot_confirm_task(void); // housekeeping_task_user

/* Call first in process_record_user. Returns false when the event was used
 * (confirm, cancel, or the release of a key that cancelled). */
bool boot_confirm_process_record(keyrecord_t *record);
