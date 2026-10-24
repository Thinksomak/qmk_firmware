// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * No-Sleep: keeps the PC awake while switched on.
 *
 * Every NOSLEEP_INTERVAL_MS it sends one mouse report with x = +1 and,
 * NOSLEEP_STEP_MS later, one with x = -1. The pair always completes, so
 * the net cursor movement is exactly zero. Mouse keys (and their
 * acceleration) are bypassed. Timer driven from housekeeping, never
 * blocking. Off after every power-up.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Time between keep-awake nudges. */
#ifndef NOSLEEP_INTERVAL_MS
#    define NOSLEEP_INTERVAL_MS 60000
#endif

/* Gap between the +1 and the -1 report of one nudge. */
#ifndef NOSLEEP_STEP_MS
#    define NOSLEEP_STEP_MS 20
#endif

/* 1 = tap F15 instead of moving the mouse (fallback if the mouse nudge
 * does not keep a particular PC awake). */
#ifndef NOSLEEP_USE_F15
#    define NOSLEEP_USE_F15 0
#endif

void     nosleep_toggle(void);
bool     nosleep_is_on(void);
void     nosleep_task(void);        // housekeeping_task_user
uint32_t nosleep_since_nudge(void); // ms since the last nudge, UINT32_MAX before the first
