// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Pomodoro stats in three scopes. There is no real-time clock, so scopes
 * are "since power-up", "since stats reset" and "lifetime", never days.
 *
 * Focus minutes count every focus millisecond actually used, whether the
 * phase finished, was skipped, reset or paused (whole minutes; the rest
 * carries over in RAM). Sessions and breaks count only when the phase runs
 * to its natural end. Persistent scopes are saved at each phase completion
 * and on a stats reset, so focus time since the last save is lost if the
 * board is unplugged.
 */
#pragma once

#include <stdint.h>
#include "pomodoro.h"

typedef enum {
    STATS_SESSION,     // since power-up, RAM only
    STATS_SINCE_RESET, // persisted, cleared by the stats CLEAR key
    STATS_LIFETIME,    // persisted, never cleared by the keymap
} stats_scope_t;

typedef struct {
    uint32_t focus_min;
    uint32_t sessions;
    uint32_t breaks; // tracked for the current session only
} stats_values_t;

void           stats_add_focus_ms(uint32_t ms);
void           stats_phase_complete(pomodoro_phase_t phase, bool counted); // counts if counted, then saves
void           stats_clear_since_reset(void);                              // clears, then saves
stats_values_t stats_get(stats_scope_t scope);
