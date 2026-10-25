// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Pomodoro stats in three scopes (see stats.h). The session scope lives in
 * RAM; the two persistent scopes live in storage.c's block and are written
 * by storage_commit() when a phase completes or on a stats clear.
 */

#include "stats.h"
#include "storage.h"

#define MS_PER_MIN 60000UL

static stats_values_t session;    // since power-up
static uint32_t       pending_ms; // focus time not yet a whole minute

/* Adds focus time. Whole minutes go to all three scopes; the rest waits
 * in pending_ms. */
void stats_add_focus_ms(uint32_t ms) {
    pending_ms += ms;
    if (pending_ms < MS_PER_MIN) return;
    uint32_t minutes = pending_ms / MS_PER_MIN;
    pending_ms %= MS_PER_MIN;

    // RAM only here; the next storage_commit() saves the persistent scopes.
    session.focus_min += minutes;
    storage_lifetime()->focus_min += minutes;
    storage_since_reset()->focus_min += minutes;
}

/* counted = false for extra time on a phase that was already counted when
 * it first reached its set time: only its focus minutes are new. */
void stats_phase_complete(pomodoro_phase_t phase, bool counted) {
    if (counted && phase == POMO_PHASE_FOCUS) {
        session.sessions++;
        storage_lifetime()->sessions++;
        storage_since_reset()->sessions++;
    } else if (counted) {
        session.breaks++;
    }
    storage_commit();
}

/* Clears the "since stats reset" scope and saves at once. */
void stats_clear_since_reset(void) {
    storage_counters_t *c = storage_since_reset();
    c->focus_min          = 0;
    c->sessions           = 0;
    storage_commit();
}

/* One scope's numbers for the stats view (breaks: the session only). */
stats_values_t stats_get(stats_scope_t scope) {
    stats_values_t            v = {0};
    const storage_counters_t *c;
    switch (scope) {
        case STATS_SESSION:
            return session;
        case STATS_SINCE_RESET:
            c = storage_since_reset();
            break;
        default:
            c = storage_lifetime();
            break;
    }
    v.focus_min = c->focus_min;
    v.sessions  = c->sessions;
    return v;
}
