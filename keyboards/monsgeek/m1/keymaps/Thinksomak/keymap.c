/* Copyright (C) 2025 Thinksomak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H

//---------------------- TAP DANCE --HOLD ---------------------------
typedef enum {
    TD_NONE,
    TD_UNKNOWN,
    TD_SINGLE_TAP,
    TD_SINGLE_HOLD,
    TD_DOUBLE_TAP,
    TD_DOUBLE_HOLD,
    TD_DOUBLE_SINGLE_TAP, // Send two single taps
    TD_TRIPLE_TAP,
    TD_TRIPLE_HOLD
} td_state_t;

typedef struct {
    bool is_press_action;
    td_state_t state;
} td_tap_t;

typedef struct {
    uint16_t keycode1;         // Keycode:
    uint16_t keycode2;
    uint16_t keycode3;
    uint16_t keycode4;
    uint16_t keycode5;
    uint16_t keycode6;
    uint16_t identifier;
} tap_data_user;

// Tap dance enums
enum {
    TD_COPY,
    TD_PASTE,
    TD_CUT,
    TD_BSLS
};

#define TPDN_CP     TD(TD_COPY)
#define TPDN_PS     TD(TD_PASTE)
#define TPDN_X      TD(TD_CUT)
#define CK_BSLS     TD(TD_BSLS)

#define CK_CSTB     C(S(KC_TAB))
#define CK_CTB      C(KC_TAB)
#define CK_AM01     LT(_FN1, KC_A)
#define CK_OSFT     OSM(MOD_LSFT)

#define CK_RSQL     C(S(KC_R))          // VSCode SQL RUN
#define CK_HIDE     C(KC_J)             // VSCode HIDE Output Window


#define ACTION_TAP_DANCE_FN_ADVANCED_USER(user_fn_on_each_tap, user_fn_on_dance_finished, user_fn_on_dance_reset, user_user_data) \
        { .fn = {user_fn_on_each_tap, user_fn_on_dance_finished, user_fn_on_dance_reset}, .user_data = (void*)user_user_data, }

td_state_t cur_dance(tap_dance_state_t *state);

// For the x tap dance. Put it here so it can be used in any keymap
void x_finished(tap_dance_state_t *state, void *user_data);
void x_reset(tap_dance_state_t *state, void *user_data);

//------------------- LAYER DEFINITIONS -------------------
enum layer_names {
    _BASE,
    _FN1,
    _FN2,
    _FN3,
    _FN4,
    _FN5
};


// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    [_BASE] = LAYOUT_all( /* Base */
        KC_ESC,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11, KC_F12,   KC_DEL,           KC_MUTE,
        KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS,KC_EQL,   KC_BSPC,          KC_HOME,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC,KC_RBRC,  CK_BSLS,          KC_END,
        KC_CAPS, CK_AM01, KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,KC_NUHS,  KC_ENT,           KC_PGUP,
        CK_OSFT, KC_NUBS, KC_Z,    TPDN_X, TPDN_CP,  TPDN_PS, KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,          KC_RSFT, KC_UP,   KC_PGDN,
        KC_LCTL, KC_LGUI, KC_LALT,                   KC_SPC,                             KC_RALT, MO(_FN2),KC_RCTL,          KC_LEFT, KC_DOWN, KC_RGHT),

    [_FN1] = LAYOUT_all( /* Base */
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, CK_HIDE,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, CK_CSTB, CK_CTB,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, CK_RSQL,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,                   KC_MS_BTN2,                         KC_TRNS, MO(_FN2),KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS),

    [_FN2] = LAYOUT_all( /* WASD/↑←↓→ */
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, CK_CSTB, CK_CTB,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,                   KC_TRNS,                            KC_TRNS, MO(_FN2),KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS),

    [_FN3] = LAYOUT_all( /* Base */
        KC_ESC,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11, KC_F12,   KC_DEL,           KC_HOME,
        KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS,KC_EQL,   KC_BSPC,          KC_END,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC,KC_RBRC,  KC_BSLS,          KC_PGUP,
        KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,KC_NUHS,  KC_ENT,           KC_PGDN,
        KC_LSFT, KC_NUBS, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,          KC_RSFT, KC_UP,   KC_END,
        KC_LCTL, KC_LGUI, KC_LALT,                   KC_SPC,                             KC_RALT, MO(_FN2),KC_RCTL,          KC_LEFT, KC_DOWN, KC_RGHT),

    [_FN4] = LAYOUT_all( /* WASD/↑←↓→ */
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,                   KC_TRNS,                            KC_TRNS, MO(_FN2),KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS),

    [_FN5] = LAYOUT_all( /* FN */
        _______, KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,  KC_F9,    KC_F10,  KC_F11,  KC_F12,  KC_DEL,            RM_NEXT,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, RM_SPDD, RM_SPDU, _______,           _______,
        _______, _______,TG(_FN4), _______, _______, _______, _______, _______, KC_INS,  DF(_BASE),KC_PSCR,_______, _______, RM_NEXT,           _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, RM_TOGG, _______, _______, RM_NEXT, RM_HUEU,           _______,
        _______, _______, _______, _______, KC_CALC, _______, _______, _______, KC_MUTE, KC_VOLD, KC_VOLU, _______,          _______,  RM_VALU, _______,
        _______, _______, _______,                   _______,                            _______, _______, _______,          RM_SATD,  RM_VALD, RM_SATU)
};


// clang-format off
#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [_BASE] = { ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
    [_FN1] = { ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
    [_FN2] = { ENCODER_CCW_CW(RM_VALU, RM_VALD) },
    [_FN3] = { ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
    [_FN4] = { ENCODER_CCW_CW(KC_VOLU, KC_VOLD) },
    [_FN5] = { ENCODER_CCW_CW(RM_VALU, RM_VALD) }
};
#endif


//----------------------------------------- TAP HOLD DANCE -----------------------------------------
td_state_t cur_dance(tap_dance_state_t *state) {
    if (state->count == 1) {
        if (state->interrupted || !state->pressed) return TD_SINGLE_TAP;
        // Key has not been interrupted, but the key is still held. Means you want to send a 'HOLD'.
        else return TD_SINGLE_HOLD;
    } else if (state->count == 2) {
        // TD_DOUBLE_SINGLE_TAP is to distinguish between typing "pepper", and actually wanting a double tap
        // action when hitting 'pp'. Suggested use case for this return value is when you want to send two
        // keystrokes of the key, and not the 'double tap' action/macro.
        if (state->interrupted) return TD_DOUBLE_SINGLE_TAP;
        else if (state->pressed) return TD_DOUBLE_HOLD;
        else return TD_DOUBLE_TAP;
    }

    // Assumes no one is trying to type the same letter three times (at least not quickly).
    // If your tap dance key is 'KC_W', and you want to type "www." quickly - then you will need to add
    // an exception here to return a 'TD_TRIPLE_SINGLE_TAP', and define that enum just like 'TD_DOUBLE_SINGLE_TAP'
    if (state->count == 3) {
        if (state->interrupted || !state->pressed) return TD_TRIPLE_TAP;
        else return TD_TRIPLE_HOLD;
    } else return TD_UNKNOWN;
}

// Create an instance of 'td_tap_t' for the 'x' tap dance.
static td_tap_t xtap_state = {
    .is_press_action = true,
    .state = TD_NONE
};

void x_finished(tap_dance_state_t *state, void *user_data) {
    // uint16_t keycode = ((tap_data_user*)user_data)->keycode;
    tap_data_user *tap_data = (tap_data_user *)user_data;

    xtap_state.state = cur_dance(state);
    switch (xtap_state.state) {
        case TD_SINGLE_TAP:
            register_code(tap_data->keycode1);
            break;
        case TD_SINGLE_HOLD:
            if (tap_data->identifier == TD_COPY     ||
                    tap_data->identifier == TD_CUT  ||
                    tap_data->identifier == TD_PASTE){
                register_code(KC_LCTL);
                register_code(tap_data->keycode2);
            } else if (tap_data->identifier == TD_BSLS) {
                layer_invert(_FN3);
            } else {
                register_code(tap_data->keycode2);
            }
            break;
        case TD_DOUBLE_TAP:
            register_code(tap_data->keycode3);
            break;
        case TD_DOUBLE_HOLD:
            register_code(tap_data->keycode4);
            break;

        // Last case is for fast typing. Assuming your key is `f`:
        // For example, when typing the word `buffer`, and you want to make sure that you send `ff` and not `Esc`.
        // In order to type `ff` when typing fast, the next character will have to be hit within the `TAPPING_TERM`, which by default is 200ms.
        case TD_DOUBLE_SINGLE_TAP:
            tap_code(tap_data->keycode1);
            register_code(tap_data->keycode1);
            break;
        case TD_TRIPLE_TAP:
            register_code(tap_data->keycode5);
            break;
        default: break;
    }
}

void x_reset(tap_dance_state_t *state, void *user_data) {
    // uint16_t keycode = ((tap_data_user*)user_data)->keycode;
    tap_data_user *tap_data = (tap_data_user *)user_data;

    switch (xtap_state.state) {
        case TD_SINGLE_TAP:
            unregister_code(tap_data->keycode1);
            break;
        case TD_SINGLE_HOLD:
            if (tap_data->identifier == TD_COPY     ||
                    tap_data->identifier == TD_CUT      ||
                    tap_data->identifier == TD_PASTE){
                unregister_code(KC_LCTL);
                unregister_code(tap_data->keycode2);
            } else {
                unregister_code(tap_data->keycode2);
            }
            break;
        case TD_DOUBLE_TAP:
            unregister_code(tap_data->keycode3);
            break;
        case TD_DOUBLE_HOLD:
            unregister_code(tap_data->keycode4);
            break;
        case TD_DOUBLE_SINGLE_TAP:
            unregister_code(tap_data->keycode1);
            break;
        case TD_TRIPLE_TAP:
            unregister_code(tap_data->keycode5);
            break;

        default: break;
    }
    xtap_state.state = TD_NONE;
}

// Tap Dance definitions
////1tap-1hld-2tap-2hld-3tap
tap_dance_action_t tap_dance_actions[] = {
    [TD_COPY]   = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_C, KC_C, KC_NO, KC_NO, KC_NO, KC_NO, TD_COPY})),
    [TD_PASTE]  = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_V, KC_V, KC_NO, KC_NO, KC_NO, KC_NO, TD_PASTE})),
    [TD_CUT]    = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_X, KC_X, KC_X, KC_NO, KC_X, KC_X, TD_CUT})),
    [TD_BSLS]   = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_BSLS, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, TD_BSLS})),

};
/*
qmk compile -kb monsgeek/m1 -km Thinksomak
qmk flash -kb monsgeek/m1 -km Thinksomak
*/
