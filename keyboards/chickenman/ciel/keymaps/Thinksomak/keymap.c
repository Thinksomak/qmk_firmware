/* Copyright 2020 Koichi Katano
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
    TD_UP,
    TD_DOWN,
    TD_RIGHT,
    TD_LEFT,
    TD_COPY,
    TD_PASTE,
    TD_MEDIA
};

#define TPDN_L      TD(TD_LEFT)
#define TPDN_U      TD(TD_UP)
#define TPDN_R      TD(TD_RIGHT)
#define TPDN_D      TD(TD_DOWN)
#define TPDN_CP     TD(TD_COPY)
#define TPDN_PS     TD(TD_PASTE)
#define CK_MEDI     TD(TD_MEDIA)

#define CK_CSTB     C(S(KC_TAB))
#define CK_CTB      C(KC_TAB)
#define CK_AM01     LT(_FN1, KC_A)

#define ACTION_TAP_DANCE_FN_ADVANCED_USER(user_fn_on_each_tap, user_fn_on_dance_finished, user_fn_on_dance_reset, user_user_data) \
        { .fn = {user_fn_on_each_tap, user_fn_on_dance_finished, user_fn_on_dance_reset}, .user_data = (void*)user_user_data, }

td_state_t cur_dance(tap_dance_state_t *state);

// For the x tap dance. Put it here so it can be used in any keymap
void x_finished(tap_dance_state_t *state, void *user_data);
void x_reset(tap_dance_state_t *state, void *user_data);

enum layer_names {
    _BASE,
    _FN1,
    _FN2,
    _FN3
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT_60_ansi_split_bs_rshift(
        KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,   KC_BSPC,   KC_DEL,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC,             KC_BSLS,
        KC_CAPS, CK_AM01, KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,                      KC_ENT,
        KC_LSFT,          KC_Z,    KC_X,    TPDN_CP, TPDN_PS, KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,           TPDN_U,    CK_MEDI,
        KC_LCTL, KC_LGUI, KC_LALT,                            KC_SPC,                                      MO(_FN1),TPDN_L,   TPDN_D,    TPDN_R
    ),
    [_FN1] = LAYOUT_60_ansi_split_bs_rshift(
        QK_BOOT, KC_F1,      KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,   _______, _______,
        _______, _______,    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,           _______,
        _______, _______,    CK_CSTB, CK_CTB,  _______, _______, _______, _______, _______, _______, _______, _______,                    _______,
        _______,             _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,           _______, _______,
        _______, _______,    _______,                            _______,                                     _______, _______,  _______, _______
    ),
    [_FN2] = LAYOUT_60_ansi_split_bs_rshift(
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,  _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,           _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                    _______,
        _______,          _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,           _______, _______,
        _______, _______, _______,                            _______,                                     _______, _______,  _______, _______
    ),
    [_FN3] = LAYOUT_60_ansi_split_bs_rshift(
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,  _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,           _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                    _______,
        _______,          _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,           _______, _______,
        _______, _______, _______,                            _______,                                     _______, _______,  _______, _______
    )
};


//--------------------- TAP HOLD DANCE ----------------------

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
            if (tap_data->identifier == TD_DOWN         ||
                    tap_data->identifier == TD_RIGHT    ||
                    tap_data->identifier == TD_COPY     ||
                    tap_data->identifier == TD_PASTE){
                register_code(KC_LCTL);
                register_code(tap_data->keycode2);
            } else if (tap_data->identifier == TD_LEFT) {
                layer_on(_FN2);
            } else if (tap_data->identifier == TD_MEDIA) {
                layer_on(_FN3);
            }  else {
                register_code(tap_data->keycode2);
            }
            break;
        case TD_DOUBLE_TAP:
            register_code(tap_data->keycode3);
            break;
        case TD_DOUBLE_HOLD:
            if (tap_data->identifier == TD_UP ||
                    tap_data->identifier == TD_DOWN){
                register_code(KC_LCTL);
                register_code(tap_data->keycode4);
            }else if (tap_data->identifier == TD_LEFT ||
                    tap_data->identifier == TD_RIGHT) {
                register_code(KC_LSFT);
                register_code(tap_data->keycode4);
            }  else {
                register_code(tap_data->keycode4);
            }
            break;

        // Last case is for fast typing. Assuming your key is `f`:
        // For example, when typing the word `buffer`, and you want to make sure that you send `ff` and not `Esc`.
        // In order to type `ff` when typing fast, the next character will have to be hit within the `TAPPING_TERM`, which by default is 200ms.
        case TD_DOUBLE_SINGLE_TAP:
            tap_code(tap_data->keycode1);
            register_code(tap_data->keycode1);
            break;
        case TD_TRIPLE_TAP:
            if (tap_data->identifier == TD_UP ||
                    tap_data->identifier == TD_DOWN){
                register_code(KC_LSFT);
                register_code(tap_data->keycode5);
            } else {
                register_code(tap_data->keycode5);
            }
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
            if (tap_data->identifier == TD_DOWN         ||
                    tap_data->identifier == TD_RIGHT    ||
                    tap_data->identifier == TD_COPY     ||
                    tap_data->identifier == TD_PASTE){
                unregister_code(KC_LCTL);
                unregister_code(tap_data->keycode2);
            } else if (tap_data->identifier == TD_LEFT) {
                layer_off(_FN2);
            } else if (tap_data->identifier == TD_MEDIA) {
                layer_off(_FN3);
            } else {
                unregister_code(tap_data->keycode2);
            }
            break;
        case TD_DOUBLE_TAP:
            unregister_code(tap_data->keycode3);
            break;
        case TD_DOUBLE_HOLD:
            if (tap_data->identifier == TD_UP ||
                    tap_data->identifier == TD_DOWN){
                unregister_code(KC_LCTL);
                unregister_code(tap_data->keycode4);
            } else if (tap_data->identifier == TD_LEFT ||
                    tap_data->identifier == TD_RIGHT) {
                unregister_code(KC_LSFT);
                unregister_code(tap_data->keycode4);
                tap_code16(LCTL(KC_C));
            } else {
                unregister_code(tap_data->keycode4);
            }
            break;
        case TD_DOUBLE_SINGLE_TAP:
            unregister_code(tap_data->keycode1);
            break;
        case TD_TRIPLE_TAP:
            if (tap_data->identifier == TD_UP ||
                    tap_data->identifier == TD_DOWN){
                unregister_code(KC_LSFT);
                unregister_code(tap_data->keycode5);
            } else {
                unregister_code(tap_data->keycode5);
            }
            break;

        default: break;
    }
    xtap_state.state = TD_NONE;
}

// Tap Dance definitions
tap_dance_action_t tap_dance_actions[] = {
    [TD_UP]     = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_UP, KC_RSFT, KC_PGUP, KC_HOME, KC_HOME, KC_NO, TD_UP})), //1tap-1hld-2tap-2hld-3tap
    [TD_DOWN]   = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_DOWN, KC_DOWN, KC_PGDN, KC_END, KC_END, KC_NO, TD_DOWN})), //1tap-1hld-2tap-2hld-3tap
    [TD_LEFT]   = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_LEFT, KC_LEFT, KC_HOME, KC_HOME, KC_NO, KC_NO, TD_LEFT})), //1tap-1hld-2tap-2hld-3tap
    [TD_RIGHT]  = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_RIGHT, KC_RIGHT, KC_END, KC_END, KC_NO, KC_NO, TD_RIGHT})), //1tap-1hld-2tap-2hld-3tap
    [TD_COPY]   = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_C, KC_C, KC_NO, KC_NO, KC_NO, KC_NO, TD_COPY})), //1tap-1hld-2tap-2hld-3tap
    [TD_PASTE]  = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_V, KC_V, KC_NO, KC_NO, KC_NO, KC_NO, TD_PASTE})), //1tap-1hld-2tap-2hld-3tap
    [TD_MEDIA]  = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_VOLU, KC_NO, KC_MUTE, KC_VOLD, KC_NO, KC_NO, TD_MEDIA})), //1tap-1hld-2tap-2hld-3tap
};
//--------------------- Get Custom TAPPING TERM/KEY ---------------------
uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case TPDN_L:
            return TAPPING_TERM - 20;
        case TPDN_R:
            return TAPPING_TERM - 20;
        case TPDN_D:
            return TAPPING_TERM - 20;
        case TPDN_U:
            return TAPPING_TERM - 20;
        default:
            return TAPPING_TERM;
    }
}
