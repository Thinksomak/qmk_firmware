// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * No-Sleep (see nosleep.h). State machine, driven by nosleep_task():
 *     off  --toggle-->  waiting  --NOSLEEP_INTERVAL_MS-->  +1 sent ("owed")
 *     +1 sent  --NOSLEEP_STEP_MS-->  -1 sent  -->  waiting
 * The -1 is always sent once the +1 went out, even if No-Sleep is switched
 * off in between: that is the zero-drift guarantee. A PC that is asleep, or
 * mouse keys that are moving, make it wait and look again after
 * NOSLEEP_RETRY_MS.
 */

#include QMK_KEYBOARD_H
#include "nosleep.h"
#include "usb_device_state.h"

#define NOSLEEP_RETRY_MS 1000 // host busy or asleep: look again this soon

_Static_assert(NOSLEEP_INTERVAL_MS > NOSLEEP_RETRY_MS, "NOSLEEP_INTERVAL_MS must be longer than 1 s");
_Static_assert(NOSLEEP_STEP_MS >= 2, "the two halves of a nudge need separate USB reports");

static bool     on;   // RAM only, so No-Sleep is off after every power-up
static bool     owed; // the +1 went out and the -1 is still owed
static uint32_t mark; // start of the wait, or when the +1 went out
static bool     nudged;
static uint32_t last_nudge;

/* Configured and not suspended. A PC that is already asleep is left alone. */
static bool host_awake(void) {
    return usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED;
}

/* How a nudge is sent. pad_busy(): mouse keys are moving, so wait.
 * nudge(true) sends the first half, nudge(false) the second. */
#if NOSLEEP_USE_F15
static bool pad_busy(void) {
    return false;
}

static void nudge(bool first) {
    if (first) {
        register_code(KC_F15);
    } else {
        unregister_code(KC_F15);
    }
}
#else
/* Mouse keys on this pad are moving or scrolling: wait, so a nudge never
 * mixes with mouse-key motion or its acceleration. */
static bool pad_busy(void) {
    report_mouse_t r = mousekey_get_report();
    return r.x || r.y || r.v || r.h;
}

/* One raw report: x = +1 first, x = -1 second, nothing else moves. The
 * buttons mouse keys currently hold are copied, so a held click stays held. */
static void nudge(bool first) {
    report_mouse_t r = mousekey_get_report();
    r.x              = first ? 1 : -1;
    r.y              = 0;
    r.v              = 0;
    r.h              = 0;
    host_mouse_send(&r);
}
#endif

/* The state machine above, one step per housekeeping tick. */
void nosleep_task(void) {
    if (owed) {
        // Always pay back the -1, even if No-Sleep was switched off meanwhile.
        if (timer_elapsed32(mark) >= NOSLEEP_STEP_MS) {
            nudge(false);
            owed = false;
            mark = timer_read32();
        }
        return;
    }
    if (!on || timer_elapsed32(mark) < NOSLEEP_INTERVAL_MS) return;
    if (!host_awake() || pad_busy()) {
        mark = timer_read32() - (NOSLEEP_INTERVAL_MS - NOSLEEP_RETRY_MS);
        return;
    }
    nudge(true);
    owed       = true;
    mark       = timer_read32();
    last_nudge = mark;
    nudged     = true;
}

/* NO_SLEEP key. The first nudge comes one interval after switching on. */
void nosleep_toggle(void) {
    on = !on;
    if (on && !owed) mark = timer_read32(); // first nudge one interval from now
}

/* ---- Getters (see nosleep.h) ----------------------------------------- */

bool nosleep_is_on(void) {
    return on;
}

uint32_t nosleep_since_nudge(void) {
    return nudged ? timer_elapsed32(last_nudge) : UINT32_MAX;
}
