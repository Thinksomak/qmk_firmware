// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Pomodoro timer: phases, alerts, the EDIT / STATS / ADJUST views and
 * their keys. pomodoro.h lists the keys, settings and hooks.
 *
 * State machine. One phase at a time:
 *     IDLE (READY) --START--> FOCUS --ends--> SHORT BREAK --ends--> FOCUS ...
 *     every SESSIONS-th focus ends in a LONG BREAK, which starts a new cycle
 * Inside a phase: running <-> paused (START). A phase that reaches its set
 * time completes: stats count it, an alert shows, and the next phase is
 * loaded paused at full length ("ready"), or running when auto start is
 * on. SKIP ends a phase early without counting it; RESET restarts it
 * paused; FOCUS / BREAK / LONG jump straight to a phase; NEW CYCLE goes back
 * to IDLE. While the next phase waits, ADJUST re-opens the phase that just
 * ended with POMODORO_EXTEND_MIN more (an "extension", not counted twice).
 * UNDO puts back the timer as it was before the last skip, reset, jump,
 * re-open or new cycle (one level).
 *
 * Views over the clock (EDIT, STATS, ADJUST) and the alert read keys by
 * position; pomodoro_process_record() routes every key to the right one.
 *
 * Time is kept as "left_at_mark minus the time since mark", so nothing
 * depends on how often the task runs, and unsigned arithmetic survives the
 * 49-day wrap of the millisecond timer.
 *
 * EEPROM (storage.c): settings and the auto-start flag are written only
 * when SAVE changed a value or AUTO was pressed; stats only when a phase
 * completes or they are cleared. Never per second.
 */

#include "pomodoro.h"
#include <string.h>
#include "storage.h"

_Static_assert((MATRIX_ROWS) * (MATRIX_COLS) <= 16, "swallow_mask needs one bit per key");

#define MAX_LEFT_MS (99UL * 60000UL + 59000UL) // the clock shows up to 99:59
#define MIN_LEFT_MS 60000UL                    // turning the knob down stops at 1:00
#define OPT_AUTO_START 0x01                    // option bit, saved with the settings

/* Valid range and default for each settings field. */
static const uint8_t field_min[POMO_FIELD_COUNT] = {1, 1, 1, 1};
static const uint8_t field_max[POMO_FIELD_COUNT] = {90, 30, 60, 8};
static const uint8_t field_def[POMO_FIELD_COUNT] = {POMODORO_FOCUS_MIN, POMODORO_SHORT_MIN, POMODORO_LONG_MIN, POMODORO_SESSIONS};

static pomodoro_settings_t settings;
static uint8_t             options; // OPT_* bits

/* Timekeeping. While running, remaining = left_at_mark - elapsed(mark).
 * Unsigned 32-bit subtraction in timer_elapsed32() keeps this correct
 * across timer wrap-around; phases are far shorter than 2^31 ms. */
static pomodoro_phase_t phase = POMO_PHASE_IDLE;
static bool             paused;
static uint32_t         mark;
static uint32_t         left_at_mark;
static uint32_t         total_ms;
static uint32_t         banked; // focus ms of this phase already reported
static uint8_t          sessions_done;
static int32_t          delta_ms; // time added (+) or taken (-) from the set length

/* Re-open: a phase that reached its set time, while the next one waits. */
static bool             extension;       // loaded phase re-opens one that already completed
static bool             can_reopen;      // last phase ended by itself, next one not started
static pomodoro_phase_t reopen_phase;    // ...which phase that was
static uint8_t          reopen_sessions; // ...and sessions_done before it ended

/* Undo: the timer as it was before the last skip, reset, jump, re-open or
 * new cycle. One level, dropped when a phase is started or completes. */
typedef struct {
    bool             valid, paused, extension, can_reopen;
    pomodoro_phase_t phase, reopen_phase;
    uint8_t          sessions_done, reopen_sessions;
    uint32_t         left, total, taken_at;
    int32_t          delta_ms;
} snapshot_t;
static snapshot_t undo;

static pomodoro_alert_t alert = POMO_ALERT_NONE;
static uint32_t         alert_since;

/* The edit, stats and adjust views sit on top of the clock. */
typedef enum { VIEW_NONE, VIEW_EDIT, VIEW_STATS, VIEW_ADJUST } view_t;
static view_t              view = VIEW_NONE;
static uint32_t            view_touched;
static pomodoro_field_t    field;
static pomodoro_settings_t edit_vals;

/* Stats view: current page and the hold-to-clear confirmation. */
static uint8_t  stats_page;
static bool     clearing;  // CLEAR is held down
static uint16_t clear_bit; // matrix bit of the held CLEAR key
static uint32_t clear_since;
static bool     cleared_recently;
static uint32_t cleared_at;

static pomodoro_toast_t toast = POMO_TOAST_NONE;
static uint32_t         toast_at;

/* Matrix keys whose press was consumed here, so their release is too. */
static uint16_t swallow_mask;

/* Minutes to milliseconds. */
static uint32_t minutes_ms(uint8_t minutes) {
    return (uint32_t)minutes * 60000UL;
}

/* ---- Hooks (the keymap overrides the ones it needs) ------------------ */

__attribute__((weak)) void pomodoro_alert_start_user(pomodoro_alert_t a) {}
__attribute__((weak)) void pomodoro_alert_end_user(void) {}
__attribute__((weak)) void pomodoro_focus_time_user(uint32_t ms) {}
__attribute__((weak)) void pomodoro_phase_complete_user(pomodoro_phase_t p, bool counted) {}
__attribute__((weak)) void pomodoro_stats_clear_user(void) {}

/* ---- Settings (saved through storage.c) ------------------------------ */

/* The settings from pomodoro.h's defaults. */
static void settings_defaults(pomodoro_settings_t *s) {
    for (uint8_t f = 0; f < POMO_FIELD_COUNT; f++) {
        s->val[f] = field_def[f];
    }
}

/* Every field inside its range. Anything else (an empty or damaged block)
 * means defaults. */
static bool settings_valid(const pomodoro_settings_t *s) {
    for (uint8_t f = 0; f < POMO_FIELD_COUNT; f++) {
        if (s->val[f] < field_min[f] || s->val[f] > field_max[f]) return false;
    }
    return true;
}

/* The next phase starts by itself when one ends. */
static bool auto_start(void) {
    return options & OPT_AUTO_START;
}

/* A short confirmation on the clock's bottom line for POMODORO_TOAST_MS. */
static void show_toast(pomodoro_toast_t t) {
    toast    = t;
    toast_at = timer_read32();
}

/* ---- Alerts ---------------------------------------------------------- */

/* Starts a phase-end alert and tells the keymap (LEDs, wake-up). */
static void alert_start(pomodoro_alert_t a) {
    alert       = a;
    alert_since = timer_read32();
    pomodoro_alert_start_user(a);
}

/* Ends the alert, if one shows, and lets the keymap undo its effects. */
static void alert_end(void) {
    if (alert == POMO_ALERT_NONE) return;
    alert = POMO_ALERT_NONE;
    pomodoro_alert_end_user();
}

/* ---- Views ----------------------------------------------------------- */

/* Opens a view over the clock; its idle timeout counts from now. */
static void view_open(view_t v) {
    view         = v;
    view_touched = timer_read32();
}

/* Back to the clock; a CLEAR being held is dropped. */
static void view_close(void) {
    view     = VIEW_NONE;
    clearing = false;
}

/* ---- Phases ---------------------------------------------------------- */

/* Reports the focus time used in this phase that has not been reported yet.
 * Used time is total minus remaining, so pauses never count. */
static void bank_focus(void) {
    if (phase != POMO_PHASE_FOCUS) return;
    uint32_t used = total_ms - pomodoro_remaining_ms();
    if (used > banked) {
        pomodoro_focus_time_user(used - banked);
        banked = used;
    }
}

/* Makes p the phase at its set length, running or paused at full length. */
static void load_phase(pomodoro_phase_t p, bool run) {
    phase        = p;
    total_ms     = minutes_ms(pomodoro_phase_minutes(p));
    left_at_mark = total_ms;
    paused       = !run;
    mark         = timer_read32();
    banked       = 0;
    delta_ms     = 0;
    extension    = false;
}

/* Moves to the next phase. completed = the phase ran to its natural end;
 * false for a skip. Either way the focus time used so far is reported.
 * An extension is not counted again: its phase was counted when it first
 * reached its set time. */
static void finish_phase(bool completed) {
    pomodoro_phase_t ended           = phase;
    pomodoro_phase_t next            = pomodoro_next_phase();
    uint8_t          sessions_before = sessions_done;

    bank_focus();
    if (completed) {
        pomodoro_phase_complete_user(ended, !extension);
        undo.valid = false;
    }
    if (view == VIEW_ADJUST) view_close(); // the time being adjusted is gone

    // Cycle position (session pips, long-break cadence). A skipped focus
    // still moves the cycle on, but only completed phases count as stats.
    if (ended == POMO_PHASE_FOCUS) {
        if (sessions_done < 255) sessions_done++;
    } else if (ended == POMO_PHASE_LONG_BREAK) {
        sessions_done = 0;
    }

    load_phase(next, auto_start());

    // While the next phase waits, the one that just ended can be re-opened.
    can_reopen      = completed && paused;
    reopen_phase    = ended;
    reopen_sessions = sessions_before;

    if (completed) {
        alert_start(ended == POMO_PHASE_FOCUS ? POMO_ALERT_FOCUS_DONE : POMO_ALERT_BREAK_DONE);
    }
}

/* ---- Undo ------------------------------------------------------------ */

/* Remembers the timer before an action, for UNDO. */
static void snapshot_take(void) {
    undo = (snapshot_t){
        .valid           = true,
        .paused          = paused,
        .extension       = extension,
        .can_reopen      = can_reopen,
        .phase           = phase,
        .reopen_phase    = reopen_phase,
        .sessions_done   = sessions_done,
        .reopen_sessions = reopen_sessions,
        .left            = pomodoro_remaining_ms(),
        .total           = total_ms,
        .taken_at        = timer_read32(),
        .delta_ms        = delta_ms,
    };
}

/* Puts the timer back as if the last action had not happened: a phase
 * that was running has kept running in the meantime. */
static void cmd_undo(void) {
    if (!undo.valid) {
        show_toast(POMO_TOAST_NO_UNDO);
        return;
    }
    bank_focus();
    phase           = undo.phase;
    paused          = undo.paused;
    total_ms        = undo.total;
    sessions_done   = undo.sessions_done;
    extension       = undo.extension;
    delta_ms        = undo.delta_ms;
    can_reopen      = undo.can_reopen;
    reopen_phase    = undo.reopen_phase;
    reopen_sessions = undo.reopen_sessions;

    uint32_t left = undo.left;
    if (phase != POMO_PHASE_IDLE && !paused) {
        uint32_t gone = timer_elapsed32(undo.taken_at);
        left          = gone >= left ? 0 : left - gone;
    }
    left_at_mark = left;
    mark         = timer_read32();
    banked       = phase == POMO_PHASE_FOCUS ? undo.total - undo.left : 0;
    undo.valid   = false;
    show_toast(POMO_TOAST_UNDONE);
}

/* ---- Commands -------------------------------------------------------- */

/* START: begins a cycle from READY, resumes, or pauses. */
static void cmd_toggle(void) {
    if (phase == POMO_PHASE_IDLE) {
        sessions_done = 0;
        load_phase(POMO_PHASE_FOCUS, true);
        can_reopen = false;
        undo.valid = false;
    } else if (paused) {
        mark       = timer_read32();
        paused     = false;
        can_reopen = false; // the waiting phase has begun
        undo.valid = false;
    } else {
        left_at_mark = pomodoro_remaining_ms();
        paused       = true;
        bank_focus();
    }
}

/* SKIP: on to the next phase without counting this one. */
static void cmd_skip(void) {
    if (phase == POMO_PHASE_IDLE) return;
    snapshot_take();
    // Same rule as a phase that ends by itself: the next phase waits for
    // START unless auto start is on. A skipped phase cannot be re-opened.
    finish_phase(false);
}

/* Restarts the current phase at its full length, paused. */
static void cmd_reset(void) {
    if (phase == POMO_PHASE_IDLE || pomodoro_is_ready()) return;
    snapshot_take();
    bank_focus();
    left_at_mark = total_ms;
    paused       = true;
    banked       = 0;
    can_reopen   = false;
}

/* Starts the given phase right away. Leaving a focus moves the cycle on,
 * like SKIP; leaving a long break starts a new cycle. */
static void cmd_jump(pomodoro_phase_t to) {
    if (phase == to) {
        if (paused) cmd_toggle(); // same phase, waiting or paused: just start it
        return;
    }
    snapshot_take();
    bank_focus();
    if (view == VIEW_ADJUST) view_close();
    if (phase == POMO_PHASE_FOCUS) {
        if (sessions_done < 255) sessions_done++;
    } else if (phase == POMO_PHASE_LONG_BREAK) {
        sessions_done = 0;
    }
    load_phase(to, true);
    can_reopen = false;
}

/* NEW CYCLE: back to READY with no sessions done. */
static void cmd_new_cycle(void) {
    snapshot_take();
    bank_focus();
    if (view == VIEW_ADJUST) view_close();
    phase         = POMO_PHASE_IDLE;
    paused        = false;
    sessions_done = 0;
    banked        = 0;
    delta_ms      = 0;
    extension     = false;
    can_reopen    = false;
    show_toast(POMO_TOAST_NEW_CYCLE);
}

/* AUTO: auto start on / off, saved at once. */
static void cmd_auto(void) {
    options ^= OPT_AUTO_START;
    storage_set_flags(options);
    storage_commit(); // an explicit settings change, like saving in the editor
    show_toast(auto_start() ? POMO_TOAST_AUTO_ON : POMO_TOAST_AUTO_OFF);
}

/* ---- Adjust view ----------------------------------------------------- */

/* Changes the time left by whole minutes: never above 99:59, and never
 * below 1:00 when turning down. Total changes with it, so the focus time
 * already used stays the same. */
static void adjust_by(int8_t minutes) {
    uint32_t left = pomodoro_remaining_ms();
    if (minutes > 0) {
        uint32_t add = minutes_ms((uint8_t)minutes);
        if (left >= MAX_LEFT_MS) return;
        if (left + add > MAX_LEFT_MS) add = MAX_LEFT_MS - left;
        left_at_mark += add; // remaining = left_at_mark - elapsed, so this adds time either way
        total_ms += add;
        delta_ms += (int32_t)add;
    } else {
        uint32_t cut = minutes_ms((uint8_t)-minutes);
        if (left <= MIN_LEFT_MS) return;
        if (left - cut < MIN_LEFT_MS) cut = left - MIN_LEFT_MS;
        left_at_mark -= cut;
        total_ms -= cut;
        delta_ms -= (int32_t)cut;
    }
}

/* ADJUST: while the next phase waits after a phase reached its set time,
 * re-open that phase with POMODORO_EXTEND_MIN and run it; otherwise adjust
 * the loaded phase. Either way the knob then changes the time left. */
static void cmd_adjust(void) {
    if (can_reopen) {
        uint32_t add = minutes_ms(POMODORO_EXTEND_MIN);
        snapshot_take();
        can_reopen    = false;
        phase         = reopen_phase;
        sessions_done = reopen_sessions;
        total_ms      = add;
        left_at_mark  = add;
        mark          = timer_read32();
        paused        = false;
        banked        = 0;
        delta_ms      = (int32_t)add;
        extension     = true;
    } else if (phase == POMO_PHASE_IDLE) {
        show_toast(POMO_TOAST_NOTHING_RUNNING);
        return;
    }
    view_open(VIEW_ADJUST);
}

/* ---- Edit view ------------------------------------------------------- */

/* Changes the selected field by delta, kept inside its range. */
static void edit_adjust(int8_t delta) {
    int16_t v = (int16_t)edit_vals.val[field] + delta;
    if (v < field_min[field]) v = field_min[field];
    if (v > field_max[field]) v = field_max[field];
    edit_vals.val[field] = (uint8_t)v;
}

/* SAVE in the editor: keeps the new values and writes them, but only if
 * one changed. */
static void edit_save(void) {
    view_close();
    if (memcmp(&edit_vals, &settings, sizeof settings) == 0) return;
    settings = edit_vals;
    storage_set_settings(&settings);
    storage_commit(); // only when a value changed, to spare flash
    // A phase that has not started yet (paused at full length) takes the
    // new length now; a phase already under way, or adjusted time, keeps
    // its length.
    if (phase != POMO_PHASE_IDLE && paused && left_at_mark == total_ms && !extension && delta_ms == 0) {
        total_ms     = minutes_ms(pomodoro_phase_minutes(phase));
        left_at_mark = total_ms;
    }
    if (sessions_done > settings.val[POMO_FIELD_SESSIONS]) sessions_done = settings.val[POMO_FIELD_SESSIONS];
}

/* One editor key, by its job in pomodoro_edit_map. */
static void edit_action(pomodoro_edit_action_t action) {
    switch (action) {
        case POMO_EDIT_SAVE:
            edit_save();
            break;
        case POMO_EDIT_CANCEL:
            view_close();
            break;
        case POMO_EDIT_PREV_FIELD:
            field = field == 0 ? POMO_FIELD_COUNT - 1 : field - 1;
            break;
        case POMO_EDIT_NEXT_FIELD:
            field = field + 1 == POMO_FIELD_COUNT ? 0 : field + 1;
            break;
        case POMO_EDIT_DEFAULT:
            edit_vals.val[field] = field_def[field];
            break;
        case POMO_EDIT_DEC1:
            edit_adjust(-1);
            break;
        case POMO_EDIT_INC1:
            edit_adjust(1);
            break;
        case POMO_EDIT_DEC5:
            edit_adjust(-5);
            break;
        case POMO_EDIT_INC5:
            edit_adjust(5);
            break;
        default:
            break;
    }
}

/* Every key and the encoder belong to the editor while it is open. */
static void edit_key(keyevent_t ev) {
    if (ev.key.row == KEYLOC_ENCODER_CW) {
        edit_adjust(1);
    } else if (ev.key.row == KEYLOC_ENCODER_CCW) {
        edit_adjust(-1);
    } else if (ev.key.row < MATRIX_ROWS && ev.key.col < MATRIX_COLS) {
        edit_action((pomodoro_edit_action_t)pgm_read_byte(&pomodoro_edit_map[ev.key.row][ev.key.col]));
    }
}

/* EDIT: opens the editor on a copy of the settings, at the first field. */
static void cmd_edit(void) {
    edit_vals = settings;
    field     = POMO_FIELD_FOCUS;
    view_open(VIEW_EDIT);
}

/* ---- Stats view ------------------------------------------------------ */

/* A key or knob event's job in the stats view. */
static pomodoro_stats_action_t stats_key_action(keyevent_t ev) {
    if (ev.key.row == KEYLOC_ENCODER_CW) return POMO_STATS_NEXT;
    if (ev.key.row == KEYLOC_ENCODER_CCW) return POMO_STATS_PREV;
    if (ev.key.row < MATRIX_ROWS && ev.key.col < MATRIX_COLS) {
        return (pomodoro_stats_action_t)pgm_read_byte(&pomodoro_stats_map[ev.key.row][ev.key.col]);
    }
    return POMO_STATS_NONE;
}

/* One stats-view key, by its job in pomodoro_stats_map. bit is the key's
 * matrix bit, so letting go of CLEAR early can cancel the clear. */
static void stats_action(pomodoro_stats_action_t action, uint16_t bit) {
    switch (action) {
        case POMO_STATS_BACK:
            view_close();
            break;
        case POMO_STATS_PREV:
            if (clearing) break; // stay on the page being cleared
            stats_page = stats_page == 0 ? POMO_STATS_PAGES - 1 : stats_page - 1;
            break;
        case POMO_STATS_NEXT:
            if (clearing) break;
            stats_page = stats_page + 1 == POMO_STATS_PAGES ? 0 : stats_page + 1;
            break;
        case POMO_STATS_CLEAR:
            // Confirm by holding: nothing happens until the hold completes,
            // and releasing early cancels. Show what is about to be cleared.
            if (!bit) break;
            stats_page       = POMO_STATS_PAGE_SINCE_RESET;
            clearing         = true;
            clear_bit        = bit;
            clear_since      = timer_read32();
            cleared_recently = false;
            break;
        default:
            break;
    }
}

/* STATS: opens the stats view on the session page. */
static void cmd_stats(void) {
    stats_page       = POMO_STATS_PAGE_SESSION;
    clearing         = false;
    cleared_recently = false;
    view_open(VIEW_STATS);
}

/* ---- Dispatch -------------------------------------------------------- */

/* Runs a pomodoro key's command. */
static void run_cmd(pomodoro_cmd_t cmd) {
    switch (cmd) {
        case POMO_CMD_TOGGLE:
            cmd_toggle();
            break;
        case POMO_CMD_SKIP:
            cmd_skip();
            break;
        case POMO_CMD_RESET:
            cmd_reset();
            break;
        case POMO_CMD_EDIT:
            cmd_edit();
            break;
        case POMO_CMD_STATS:
            cmd_stats();
            break;
        case POMO_CMD_ADJUST:
            cmd_adjust();
            break;
        case POMO_CMD_FOCUS_NOW:
            cmd_jump(POMO_PHASE_FOCUS);
            break;
        case POMO_CMD_BREAK_NOW:
            cmd_jump(POMO_PHASE_SHORT_BREAK);
            break;
        case POMO_CMD_LONG_NOW:
            cmd_jump(POMO_PHASE_LONG_BREAK);
            break;
        case POMO_CMD_AUTO:
            cmd_auto();
            break;
        case POMO_CMD_UNDO:
            cmd_undo();
            break;
        case POMO_CMD_NEW_CYCLE:
            cmd_new_cycle();
            break;
        default:
            break;
    }
}

/* At a phase-end alert these commands act on the first press instead of
 * only dismissing it. START only begins a waiting phase, never pauses one. */
static bool acts_at_alert(pomodoro_cmd_t cmd) {
    switch (cmd) {
        case POMO_CMD_TOGGLE:
            return paused;
        case POMO_CMD_ADJUST:
        case POMO_CMD_STATS:
        case POMO_CMD_EDIT:
        case POMO_CMD_FOCUS_NOW:
        case POMO_CMD_BREAK_NOW:
        case POMO_CMD_LONG_NOW:
            return true;
        default:
            return false;
    }
}

/* ---- Lifecycle ------------------------------------------------------- */

/* Settings and the auto-start flag from storage; defaults if none valid. */
void pomodoro_init(void) {
    settings = *storage_settings();
    if (!settings_valid(&settings)) {
        // Nothing stored yet, or out of range: defaults, saved with the next commit.
        settings_defaults(&settings);
        storage_set_settings(&settings);
    }
    options = storage_flags() & OPT_AUTO_START;
}

/* Every housekeeping tick: ends a phase at zero, keeps the focus time in
 * the stats live, and runs out the alert, toast, stats clear and views. */
void pomodoro_task(void) {
    if (phase != POMO_PHASE_IDLE && !paused) {
        uint32_t left = pomodoro_remaining_ms();
        if (left == 0) {
            finish_phase(true);
        } else if (phase == POMO_PHASE_FOCUS && total_ms - left - banked >= 1000) {
            bank_focus(); // keep stats live, about once a second
        }
    }
    if (alert != POMO_ALERT_NONE && timer_elapsed32(alert_since) >= POMODORO_ALERT_MS) {
        alert_end();
    }
    // Stats CLEAR held long enough: clear "since stats reset" once.
    if (clearing && timer_elapsed32(clear_since) >= POMODORO_CLEAR_HOLD_MS) {
        clearing = false;
        pomodoro_stats_clear_user();
        cleared_recently = true;
        cleared_at       = timer_read32();
        view_touched     = cleared_at;
    }
    if (cleared_recently && timer_elapsed32(cleared_at) >= POMODORO_CLEARED_MS) {
        cleared_recently = false;
    }
    if (toast != POMO_TOAST_NONE && timer_elapsed32(toast_at) >= POMODORO_TOAST_MS) {
        toast = POMO_TOAST_NONE;
    }
    // Nobody touched the editor for a while: close it, discarding edits.
    if (view == VIEW_EDIT && timer_elapsed32(view_touched) >= POMODORO_EDIT_TIMEOUT_MS) {
        view_close();
    }
    if (view == VIEW_ADJUST && timer_elapsed32(view_touched) >= POMODORO_ADJUST_TIMEOUT_MS) {
        view_close(); // the new time is already in effect
    }
#if POMODORO_STATS_TIMEOUT_MS > 0
    // The stats page stays until closed, or until the display goes to sleep,
    // so keys are never captured by a screen you cannot see.
    if (view == VIEW_STATS && !clearing && timer_elapsed32(view_touched) >= POMODORO_STATS_TIMEOUT_MS) {
        view_close();
    }
#endif
}

/* Key routing, in order: releases of keys used here are eaten; an alert
 * is dismissed (and a few keys also act); an open view takes the key; then
 * a pomodoro key runs its command. */
bool pomodoro_process_record(pomodoro_cmd_t cmd, uint16_t keycode, keyrecord_t *record) {
    keyevent_t ev  = record->event;
    uint16_t   bit = 0;
    if (!IS_ENCODEREVENT(ev) && ev.key.row < MATRIX_ROWS && ev.key.col < MATRIX_COLS) {
        bit = (uint16_t)1 << (ev.key.row * MATRIX_COLS + ev.key.col);
    }

    if (!ev.pressed) {
        if (clearing && bit && bit == clear_bit) clearing = false; // let go early: no clear
        if (swallow_mask & bit) {
            swallow_mask &= ~bit;
            return false;
        }
        return cmd == POMO_CMD_NONE; // pomodoro keys never reach the PC
    }

    if (alert != POMO_ALERT_NONE) {
        bool act = view == VIEW_NONE && acts_at_alert(cmd);
        alert_end();
        if (!act) {
            // Let tap-dance keys through: eating one tap would change a
            // credential macro's tap count and type the wrong thing.
            if (IS_QK_TAP_DANCE(keycode)) return true;
            swallow_mask |= bit;
            return false;
        }
    }

    switch (view) {
        case VIEW_EDIT:
            view_touched = timer_read32();
            edit_key(ev);
            swallow_mask |= bit;
            return false;
        case VIEW_STATS: {
            view_touched                   = timer_read32();
            pomodoro_stats_action_t action = stats_key_action(ev);
            if (action != POMO_STATS_PASS) {
                stats_action(action, bit);
                swallow_mask |= bit;
                return false;
            }
            break; // PASS: the key does its normal job below
        }
        case VIEW_ADJUST:
            if (ev.key.row == KEYLOC_ENCODER_CW || ev.key.row == KEYLOC_ENCODER_CCW) {
                view_touched = timer_read32();
                adjust_by(ev.key.row == KEYLOC_ENCODER_CW ? 1 : -1);
                return false;
            }
            view_close(); // any key ends adjust mode
            if (cmd == POMO_CMD_ADJUST) {
                swallow_mask |= bit; // ADJUST or the knob press: just done
                return false;
            }
            break; // other keys go on to do their normal job
        default:
            break;
    }

    if (cmd == POMO_CMD_NONE) return true;
    run_cmd(cmd);
    return false;
}

/* ---- Getters --------------------------------------------------------- */

pomodoro_state_t pomodoro_state(void) {
    if (alert != POMO_ALERT_NONE) return POMO_ALERT;
    if (view == VIEW_EDIT) return POMO_EDIT;
    if (view == VIEW_STATS) return POMO_STATS;
    if (view == VIEW_ADJUST) return POMO_ADJUST;
    if (phase != POMO_PHASE_IDLE && paused) return POMO_PAUSED;
    switch (phase) {
        case POMO_PHASE_FOCUS:
            return POMO_FOCUS;
        case POMO_PHASE_SHORT_BREAK:
            return POMO_SHORT_BREAK;
        case POMO_PHASE_LONG_BREAK:
            return POMO_LONG_BREAK;
        default:
            return POMO_IDLE;
    }
}

pomodoro_phase_t pomodoro_phase(void) {
    return phase;
}

pomodoro_phase_t pomodoro_next_phase(void) {
    switch (phase) {
        case POMO_PHASE_FOCUS:
            return (sessions_done + 1 >= settings.val[POMO_FIELD_SESSIONS]) ? POMO_PHASE_LONG_BREAK : POMO_PHASE_SHORT_BREAK;
        default:
            return POMO_PHASE_FOCUS;
    }
}

bool pomodoro_is_paused(void) {
    return phase != POMO_PHASE_IDLE && paused;
}

bool pomodoro_is_ready(void) {
    return phase != POMO_PHASE_IDLE && paused && left_at_mark == total_ms;
}

bool pomodoro_can_reopen(void) {
    return can_reopen;
}

int16_t pomodoro_delta_minutes(void) {
    int32_t m = delta_ms >= 0 ? (delta_ms + 30000) / 60000 : -((-delta_ms + 30000) / 60000);
    return (int16_t)m;
}

bool pomodoro_auto_start(void) {
    return auto_start();
}

uint32_t pomodoro_remaining_ms(void) {
    if (phase == POMO_PHASE_IDLE) return minutes_ms(settings.val[POMO_FIELD_FOCUS]);
    if (paused) return left_at_mark;
    uint32_t elapsed = timer_elapsed32(mark);
    return elapsed >= left_at_mark ? 0 : left_at_mark - elapsed;
}

uint32_t pomodoro_total_ms(void) {
    return phase == POMO_PHASE_IDLE ? minutes_ms(settings.val[POMO_FIELD_FOCUS]) : total_ms;
}

uint8_t pomodoro_sessions_done(void) {
    uint8_t target = settings.val[POMO_FIELD_SESSIONS];
    return sessions_done > target ? target : sessions_done;
}

uint8_t pomodoro_phase_minutes(pomodoro_phase_t p) {
    switch (p) {
        case POMO_PHASE_SHORT_BREAK:
            return settings.val[POMO_FIELD_SHORT];
        case POMO_PHASE_LONG_BREAK:
            return settings.val[POMO_FIELD_LONG];
        default:
            return settings.val[POMO_FIELD_FOCUS];
    }
}

const pomodoro_settings_t *pomodoro_settings(void) {
    return &settings;
}

pomodoro_toast_t pomodoro_toast(void) {
    return toast;
}

pomodoro_alert_t pomodoro_alert(void) {
    return alert;
}

uint32_t pomodoro_alert_elapsed(void) {
    return timer_elapsed32(alert_since);
}

bool pomodoro_is_editing(void) {
    return view == VIEW_EDIT;
}

pomodoro_field_t pomodoro_edit_field(void) {
    return field;
}

const pomodoro_settings_t *pomodoro_edit_values(void) {
    return &edit_vals;
}

bool pomodoro_is_viewing_stats(void) {
    return view == VIEW_STATS;
}

uint8_t pomodoro_stats_page(void) {
    return stats_page;
}

uint8_t pomodoro_stats_clear_progress(void) {
    if (!clearing) return 0;
    uint32_t held = timer_elapsed32(clear_since);
    if (held >= POMODORO_CLEAR_HOLD_MS) return 255;
    return (uint8_t)(1 + held * 254 / POMODORO_CLEAR_HOLD_MS);
}

bool pomodoro_stats_just_cleared(void) {
    return cleared_recently;
}

bool pomodoro_is_adjusting(void) {
    return view == VIEW_ADJUST;
}
