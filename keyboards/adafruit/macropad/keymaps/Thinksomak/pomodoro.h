// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Pomodoro timer: state machine, timekeeping, and the edit, stats and
 * adjust-time views.
 *
 * The timer runs from housekeeping_task_user(), so it keeps going on every
 * layer. The keymap maps each of its pomodoro keycodes to a pomodoro_cmd_t
 * and hands every key event to pomodoro_process_record(). Display, LED and
 * stats code only see the timer through the getters and *_user hooks.
 */
#pragma once

#include "quantum.h"

/* ---- Tunables (override with OPT_DEFS += -D... in rules.mk) ---------- */

/* Defaults, used until settings are saved from edit mode. */
#ifndef POMODORO_FOCUS_MIN
#    define POMODORO_FOCUS_MIN 25
#endif
#ifndef POMODORO_SHORT_MIN
#    define POMODORO_SHORT_MIN 5
#endif
#ifndef POMODORO_LONG_MIN
#    define POMODORO_LONG_MIN 15
#endif
#ifndef POMODORO_SESSIONS
#    define POMODORO_SESSIONS 4
#endif

/* How long the phase-end alert (OLED and LEDs) stays up unless dismissed. */
#ifndef POMODORO_ALERT_MS
#    define POMODORO_ALERT_MS 30000
#endif

/* The edit view closes after this long without input, discarding edits. */
#ifndef POMODORO_EDIT_TIMEOUT_MS
#    define POMODORO_EDIT_TIMEOUT_MS 30000
#endif

/* The stats view stays open until closed, or until the display goes to
 * sleep (no input for OLED_TIMEOUT). 0 = never close by itself. */
#ifndef POMODORO_STATS_TIMEOUT_MS
#    if defined(OLED_TIMEOUT) && OLED_TIMEOUT > 0
#        define POMODORO_STATS_TIMEOUT_MS OLED_TIMEOUT
#    else
#        define POMODORO_STATS_TIMEOUT_MS 0
#    endif
#endif

/* Adjust-time mode ends after this long without a knob turn. */
#ifndef POMODORO_ADJUST_TIMEOUT_MS
#    define POMODORO_ADJUST_TIMEOUT_MS 10000
#endif

/* Minutes a phase that just reached its set time gets when ADJUST re-opens
 * it. The knob then changes it further. */
#ifndef POMODORO_EXTEND_MIN
#    define POMODORO_EXTEND_MIN 5
#endif

/* Stats view: how long CLEAR must be held, and how long "cleared" shows. */
#ifndef POMODORO_CLEAR_HOLD_MS
#    define POMODORO_CLEAR_HOLD_MS 2000
#endif
#ifndef POMODORO_CLEARED_MS
#    define POMODORO_CLEARED_MS 1500
#endif

/* How long a short confirmation ("undone", "auto start ON") stays on the
 * clock screen's bottom line. */
#ifndef POMODORO_TOAST_MS
#    define POMODORO_TOAST_MS 2000
#endif

/* Dim hint on one key while a phase is loaded: 1 = on, 0 = off. */
#ifndef POMODORO_LED_HINT
#    define POMODORO_LED_HINT 1
#endif

/* ---- Types ----------------------------------------------------------- */

/* Combined state, highest priority first: ALERT, EDIT, STATS, ADJUST,
 * PAUSED, then the phase. */
typedef enum {
    POMO_IDLE,
    POMO_FOCUS,
    POMO_SHORT_BREAK,
    POMO_LONG_BREAK,
    POMO_PAUSED,
    POMO_EDIT,
    POMO_STATS,
    POMO_ADJUST,
    POMO_ALERT,
} pomodoro_state_t;

/* The phase underneath PAUSED / EDIT / STATS / ADJUST / ALERT. */
typedef enum {
    POMO_PHASE_IDLE,
    POMO_PHASE_FOCUS,
    POMO_PHASE_SHORT_BREAK,
    POMO_PHASE_LONG_BREAK,
} pomodoro_phase_t;

typedef enum {
    POMO_ALERT_NONE,
    POMO_ALERT_FOCUS_DONE,
    POMO_ALERT_BREAK_DONE,
} pomodoro_alert_t;

typedef enum {
    POMO_FIELD_FOCUS,
    POMO_FIELD_SHORT,
    POMO_FIELD_LONG,
    POMO_FIELD_SESSIONS,
    POMO_FIELD_COUNT,
} pomodoro_field_t;

/* What a pomodoro key does. The keymap maps its keycodes to these. */
typedef enum {
    POMO_CMD_NONE,      // not a pomodoro key
    POMO_CMD_TOGGLE,    // start / pause / resume
    POMO_CMD_SKIP,      // go to the next phase now (it waits for START)
    POMO_CMD_RESET,     // restart the current phase, paused
    POMO_CMD_EDIT,      // open settings
    POMO_CMD_STATS,     // open stats
    POMO_CMD_ADJUST,    // adjust-time mode: the knob changes the time left
    POMO_CMD_FOCUS_NOW, // start a focus right away
    POMO_CMD_BREAK_NOW, // start a short break right away
    POMO_CMD_LONG_NOW,  // start a long break right away
    POMO_CMD_AUTO,      // auto start on / off (saved)
    POMO_CMD_UNDO,      // undo the last skip, reset, jump, re-open or new cycle
    POMO_CMD_NEW_CYCLE, // back to READY with no sessions done
} pomodoro_cmd_t;

/* Edit-mode actions, assigned to physical keys by pomodoro_edit_map. */
typedef enum {
    POMO_EDIT_NONE,
    POMO_EDIT_SAVE,
    POMO_EDIT_CANCEL,
    POMO_EDIT_PREV_FIELD,
    POMO_EDIT_NEXT_FIELD,
    POMO_EDIT_DEFAULT,
    POMO_EDIT_DEC1,
    POMO_EDIT_INC1,
    POMO_EDIT_DEC5,
    POMO_EDIT_INC5,
} pomodoro_edit_action_t;

/* Stats-view actions, assigned to physical keys by pomodoro_stats_map.
 * PASS lets the key do its normal job. */
typedef enum {
    POMO_STATS_NONE,
    POMO_STATS_PASS,
    POMO_STATS_BACK,
    POMO_STATS_PREV,
    POMO_STATS_NEXT,
    POMO_STATS_CLEAR,
} pomodoro_stats_action_t;

/* Stats view pages, in display order. */
enum {
    POMO_STATS_PAGE_SESSION,
    POMO_STATS_PAGE_SINCE_RESET,
    POMO_STATS_PAGE_LIFETIME,
    POMO_STATS_PAGES,
};

/* Short confirmations for the clock screen's bottom line. */
typedef enum {
    POMO_TOAST_NONE,
    POMO_TOAST_AUTO_ON,
    POMO_TOAST_AUTO_OFF,
    POMO_TOAST_UNDONE,
    POMO_TOAST_NO_UNDO,
    POMO_TOAST_NEW_CYCLE,
    POMO_TOAST_NOTHING_RUNNING,
} pomodoro_toast_t;

/* Minutes for focus / short / long, then sessions before a long break. */
typedef struct {
    uint8_t val[POMO_FIELD_COUNT];
} pomodoro_settings_t;

/* Defined by the keymap: view action for each matrix position. */
extern const uint8_t pomodoro_edit_map[MATRIX_ROWS][MATRIX_COLS];
extern const uint8_t pomodoro_stats_map[MATRIX_ROWS][MATRIX_COLS];

/* ---- Lifecycle ------------------------------------------------------- */

void pomodoro_init(void); // keyboard_post_init_user, after storage_init()
void pomodoro_task(void); // housekeeping_task_user: advance the timer

/* Call early in process_record_user for every key event, with the command
 * the keycode maps to (POMO_CMD_NONE for other keys). Returns false when
 * the event was used here and must go no further: pomodoro commands, alert
 * dismissal, and keys captured by the edit / stats / adjust views. */
bool pomodoro_process_record(pomodoro_cmd_t cmd, uint16_t keycode, keyrecord_t *record);

/* ---- Read-only state ------------------------------------------------- */

pomodoro_state_t           pomodoro_state(void);
pomodoro_phase_t           pomodoro_phase(void);
pomodoro_phase_t           pomodoro_next_phase(void);
bool                       pomodoro_is_paused(void);     // a phase is loaded and paused
bool                       pomodoro_is_ready(void);      // ...paused at full length, waiting for START
bool                       pomodoro_can_reopen(void);    // ADJUST would re-open the phase that just ended
int16_t                    pomodoro_delta_minutes(void); // time added (+) or taken (-) by ADJUST
bool                       pomodoro_auto_start(void);
uint32_t                   pomodoro_remaining_ms(void);
uint32_t                   pomodoro_total_ms(void);
uint8_t                    pomodoro_sessions_done(void); // focus sessions done this cycle
uint8_t                    pomodoro_phase_minutes(pomodoro_phase_t phase);
const pomodoro_settings_t *pomodoro_settings(void);
pomodoro_toast_t           pomodoro_toast(void);

pomodoro_alert_t pomodoro_alert(void);
uint32_t         pomodoro_alert_elapsed(void);

bool                       pomodoro_is_editing(void);
pomodoro_field_t           pomodoro_edit_field(void);
const pomodoro_settings_t *pomodoro_edit_values(void);

bool    pomodoro_is_viewing_stats(void);
uint8_t pomodoro_stats_page(void);
uint8_t pomodoro_stats_clear_progress(void); // 0 = not holding, 1..255 = hold progress
bool    pomodoro_stats_just_cleared(void);

bool pomodoro_is_adjusting(void);

/* ---- Optional keymap hooks ------------------------------------------- */

void pomodoro_alert_start_user(pomodoro_alert_t alert);
void pomodoro_alert_end_user(void);

/* Focus time used, reported in chunks of about one second and whenever a
 * focus phase is paused, skipped, reset or finished. */
void pomodoro_focus_time_user(uint32_t ms);

/* A phase ran to its natural end. Skips, resets and jumps do not call this.
 * counted is false when the phase was extra time on a phase that had
 * already completed, so it must not be counted twice. */
void pomodoro_phase_complete_user(pomodoro_phase_t phase, bool counted);

/* The stats CLEAR key was held for POMODORO_CLEAR_HOLD_MS. */
void pomodoro_stats_clear_user(void);
