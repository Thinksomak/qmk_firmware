// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "numpad_ops.h"

/* The keymap's config.h is part of every file's build, so these checks
 * see its settings without anyone opening it. */
#ifndef TAPPING_TERM_PER_KEY
#    error "numpad holds need TAPPING_TERM_PER_KEY (keymap config.h)"
#endif
#ifdef PERMISSIVE_HOLD
#    error "PERMISSIVE_HOLD is on for every key: fast numpad typing would turn digits into operators"
#endif
#ifdef HOLD_ON_OTHER_KEY_PRESS
#    error "HOLD_ON_OTHER_KEY_PRESS is on for every key: rolls on the numpad would turn digits into operators"
#endif
#ifdef FLOW_TAP_TERM
#    error "FLOW_TAP_TERM is on: it would block quick operator holds on the numpad"
#endif

#define C_NONE 0xFF
#define C_OFF 0xFE  // leave calculator mode
#define C_TYPE 0xFD // type the shown number

/* Calculator keys by position (rows 1-4 of the matrix): {tap, hold}. */
// clang-format off
static const uint8_t PROGMEM calc_map[4][3][2] = {
    {{CALC_KEY_7,  CALC_KEY_PCT}, {CALC_KEY_8, CALC_KEY_DIV}, {CALC_KEY_9,    CALC_KEY_MUL}},
    {{CALC_KEY_4,  CALC_KEY_NEG}, {CALC_KEY_5, CALC_KEY_SUB}, {CALC_KEY_6,    CALC_KEY_ADD}},
    {{CALC_KEY_1,  CALC_KEY_CE},  {CALC_KEY_2, C_NONE},       {CALC_KEY_3,    C_TYPE}},
    {{CALC_KEY_EQ, C_OFF},        {CALC_KEY_0, CALC_KEY_DOT}, {CALC_KEY_BKSP, CALC_KEY_AC}},
};
// clang-format on

static calc_t   calc;
static bool     active;
static uint8_t  scroll;       // line 1: characters scrolled back from the newest
static uint16_t held_mask;    // keys held down by a hold, for auto-repeat
static uint16_t swallow_mask; // releases that must not reach the PC

/* An NP() key: a layer-tap on layer 0, which is how holds are told apart. */
static bool is_numpad_key(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) && QK_LAYER_TAP_GET_LAYER(keycode) == 0;
}

/* A key's bit in the press masks (0 for the knob). */
static uint16_t pos_bit(keyevent_t ev) {
    if (IS_ENCODEREVENT(ev) || ev.key.row >= MATRIX_ROWS || ev.key.col >= MATRIX_COLS) return 0;
    return (uint16_t)1 << (ev.key.row * MATRIX_COLS + ev.key.col);
}

/* What holding a numpad key types. KC_NO: hold it down like a normal key. */
static uint16_t hold_keycode(uint16_t tap) {
    switch (tap) {
        case KC_7:
            return KC_PERC;
        case KC_8:
            return KC_PSLS;
        case KC_9:
            return KC_PAST;
        case KC_5:
            return KC_PMNS;
        case KC_6:
            return KC_PPLS;
        case KC_0:
            return KC_DOT;
        default:
            return KC_NO;
    }
}

/* TYPE: the value on line 2, as keystrokes. Nothing on Err. */
static void type_result(void) {
    char text[CALC_TEXT_LEN];
    if (calc_value(&calc, text) == CALC_ERR_NONE) send_string(text);
}

/* How far line 1 can scroll back, in display lines: the screen shows the
 * last CALC_VIEW_LINES, so up to the line count minus that. */
static uint8_t scroll_max(void) {
    char    text[CALC_DISPLAY_LEN];
    uint8_t starts[CALC_WRAP_MAX + 1];
    uint8_t n = calc_display_lines(&calc, text, starts);
    return n > CALC_VIEW_LINES ? n - CALC_VIEW_LINES : 0;
}

/* Leaves calculator mode. The calculation is kept until power-off; the
 * view goes back to the newest line for next time. */
static void leave(void) {
    active = false;
    scroll = 0;
}

/* A key or knob event in calculator mode: keys by position (calc_map),
 * the twist scrolls. Nothing goes to the PC except TYPE and the knob
 * press. */
static bool calc_key(keyevent_t ev, bool hold, uint16_t bit) {
    if (IS_ENCODEREVENT(ev)) { // the twist scrolls line 1 by a display line: back = older
        if (ev.pressed) {
            if (ev.key.row == KEYLOC_ENCODER_CCW) {
                if (scroll < scroll_max()) scroll++;
            } else if (scroll) {
                scroll--;
            }
        }
        return false;
    }
    if (!bit || ev.key.row < 1) return true; // the knob press keeps its job (Mute)
    if (ev.pressed) {
        scroll       = 0; // any key (typing, CE, AC...): back to the newest line
        uint8_t code = pgm_read_byte(&calc_map[ev.key.row - 1][ev.key.col][hold ? 1 : 0]);
        if (code == C_OFF) {
            leave();
        } else if (code == C_TYPE) {
            type_result();
        } else if (code != C_NONE) {
            calc_input(&calc, (calc_key_t)code);
        }
        swallow_mask |= bit;
    }
    return false;
}

/* State machine, per key event:
 *   calculator mode   every numpad key goes to calc_key()
 *   numpad, tap       QMK types the key
 *   numpad, hold      7 8 9 5 6 0 tap their operator once; ENT turns
 *                     calculator mode on; 1-4 and BKSP stay held down
 *                     (auto-repeat) until let go
 * A release whose press was used here is eaten too (swallow_mask). */
bool numpad_ops_process_record(uint16_t keycode, keyrecord_t *record) {
    keyevent_t ev  = record->event;
    uint16_t   bit = pos_bit(ev);
    bool       np  = is_numpad_key(keycode);

    if (active && get_highest_layer(layer_state) != NUMPAD_LAYER) leave();

    if (!ev.pressed && (swallow_mask & bit)) {
        swallow_mask &= ~bit;
        return false;
    }

    // tap.count is only meaningful on tap-hold keys; anything else is a tap.
    if (active) return calc_key(ev, np && record->tap.count == 0, bit);

    if (!np) return true;
    if (record->tap.count) return true; // tap: QMK types the key

    uint16_t tap = QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
    if (ev.pressed) {
        uint16_t op = hold_keycode(tap);
        if (tap == KC_ENT) {
            active = true;
            swallow_mask |= bit;
        } else if (op != KC_NO) {
            tap_code16(op);
            swallow_mask |= bit;
        } else {
            register_code(tap);
            held_mask |= bit;
        }
    } else if (held_mask & bit) {
        held_mask &= ~bit;
        unregister_code(tap);
    }
    return false;
}

/* Ends calculator mode once the layer is no longer NUMPAD. */
void numpad_ops_task(void) {
    if (active && get_highest_layer(layer_state) != NUMPAD_LAYER) leave();
}

/* ---- For the screen and LEDs (see numpad_ops.h) --------------------- */

bool numpad_calc_active(void) {
    return active;
}

uint8_t numpad_calc_scroll(void) {
    return scroll;
}

const calc_t *numpad_calc(void) {
    return &calc;
}

/* ---- Tap-hold timing ---------------------------------------------------- */

/* QMK asks how long a key must be held to count as a hold. */
uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
    if (is_numpad_key(keycode)) {
        uint16_t tap = QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
        return (tap == KC_ENT || tap == KC_BSPC) ? NUMPAD_LONG_TERM : NUMPAD_HOLD_TERM;
    }
    return TAPPING_TERM;
}

#ifdef QUICK_TAP_TERM_PER_KEY
/* 0 on the numpad: "5, then hold 5" gives "5-", not a repeating 5. */
uint16_t get_quick_tap_term(uint16_t keycode, keyrecord_t *record) {
    return is_numpad_key(keycode) ? 0 : QUICK_TAP_TERM;
}
#endif
