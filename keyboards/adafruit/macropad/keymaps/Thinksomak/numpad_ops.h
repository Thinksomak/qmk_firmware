// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Numpad extras: operators on hold, and a calculator mode.
 *
 * Numpad keys are NP(kc) = LT(0, kc): tap types the key, hold does this:
 *     7 %     8 /     9 *
 *     4       5 -     6 +
 *     1       2       3
 *     ENT calculator on   0 .    BKSP
 * Keys without a hold job (1-4, BKSP) act as held down, so they still
 * auto-repeat (after the hold time).
 *
 * Calculator mode turns the numpad into a desk calculator shown on the
 * OLED (calc.c): the expression as typed, and the live value below it.
 * Keys are read by position, tap / hold:
 *     7 / %      8 / divide   9 / multiply
 *     4 / +-     5 / minus    6 / plus
 *     1 / CE     2            3 / TYPE (types the value)
 *     = / OFF    0 / .        BKSP / AC
 * The screen shows the last three display lines of the expression; the knob
 * twist scrolls through the rest a line at a time (back = older text). Any
 * key, and leaving the mode, jumps back to the newest line. Nothing
 * reaches the PC except TYPE and the knob press (Mute). Hold = to leave;
 * the calculation is kept until power-off.
 *
 * Holds are decided by time only (no permissive hold, no hold on other
 * key press), so fast typing and rolls stay digits.
 */
#pragma once

#include "quantum.h"
#include "calc.h"

#ifndef NUMPAD_LAYER
#    define NUMPAD_LAYER 0
#endif

/* Hold times. A tap on a numpad lasts 60-120 ms. ENT and BKSP hold
 * longer because their holds switch modes or clear everything. */
#ifndef NUMPAD_HOLD_TERM
#    define NUMPAD_HOLD_TERM 250
#endif
#ifndef NUMPAD_LONG_TERM
#    define NUMPAD_LONG_TERM 400
#endif

/* A numpad key: tap = kc, hold = its operator (see above). */
#define NP(kc) LT(0, kc)

/* Call from process_record_user after the pomodoro and before the keymap's
 * own keys. Returns false when the event was used. */
bool numpad_ops_process_record(uint16_t keycode, keyrecord_t *record);

/* Call from housekeeping_task_user: ends calculator mode off the numpad. */
void numpad_ops_task(void);

bool          numpad_calc_active(void);
const calc_t *numpad_calc(void);
uint8_t       numpad_calc_scroll(void); // display lines scrolled back from the newest
