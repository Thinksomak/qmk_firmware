/* Copyright 2024 Somak Mondal <mondalsomak@gmail.com>
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
#include "config.h"
#include "rgb_matrix_map.h"

// Blinking LED Implementation.start
#include "timer.h"  // Include the timer functions


#define ARRAYSIZE(arr) sizeof(arr) / sizeof(arr[0])
// Screen saver
#define SCREENSAVE_DELAY 120000  //configure how long to wait after last activity. 1800000 = 30 mins
// Define your LED index, e.g., LED at position 3
#define LED_3 LED_K02
// Define the blink interval in milliseconds
#define BLINK_INTERVAL 300  // 500ms = half a second


// Declare variables for LED state and timing
static uint16_t blink_timer = 0;
static bool led_on = false;
// Blinking LED Implementation.end

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

#define CK_CSTB     S(KC_TAB)

// typedef struct{
// 	uint16_t keycode;
// } tap_data_user;

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
    TD_CRED_2,
    TD_RUN,
    TD_CRED_1
};

#define ACTION_TAP_DANCE_FN_ADVANCED_USER(user_fn_on_each_tap, user_fn_on_dance_finished, user_fn_on_dance_reset, user_user_data) \
        { .fn = {user_fn_on_each_tap, user_fn_on_dance_finished, user_fn_on_dance_reset}, .user_data = (void*)user_user_data, }

td_state_t cur_dance(tap_dance_state_t *state);

// For the x tap dance. Put it here so it can be used in any keymap
void x_finished(tap_dance_state_t *state, void *user_data);
void x_reset(tap_dance_state_t *state, void *user_data);
void rgb_blinker(uint16_t rVal, uint16_t gVal, uint16_t bVal);
//-------------------------------------------------------------------

// Custom Key Codes
enum custom_keycodes {
    KC_LUP = SAFE_RANGE,            //cycle layers in up direction
    KC_LDN,                         //cycle layers in down direction
    KC_CRDNTLS1,                    //Credentials1
    KC_CRDNTLS2,                    //Credentials2
    KC_SLCTQRY,                     //SQL Query
    NO_SLEEP,                       //No Sleep - Autolock off
    KC_PMDRO                        //Pomodoro Functionality
};

#define TSK_MGR C(S(KC_ESC))
#define MV_LEFT G(KC_LEFT)
#define MV_RIGT G(KC_RGHT)
#define MV_DOWN G(KC_DOWN)
#define MV_UP   G(KC_UP)

//------------------------ KEYMAP LAYOUT -------------------------
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT(                     // Base Layer [1] - NUMPAD
                                    KC_AUDIO_MUTE,
      KC_7,         KC_8,           KC_9,
      KC_4,         KC_5,           KC_6,
      KC_1,         KC_2,           KC_3,
      KC_ENT,       KC_0,           KC_BSPC
  ),
  [1] = LAYOUT(                     // Macro Layer [2] - OFFICE
                                    KC_AUDIO_MUTE,
      TSK_MGR,      _______,        NO_SLEEP,
      CK_CSTB,      KC_TAB,         KC_DEL,
      KC_VOLD,      KC_MUTE,        KC_MS_BTN2,
      TD(TD_CRED_1),TD(TD_CRED_2),  TD(TD_RUN)
  ),
  [2] = LAYOUT(                     // Macro Layer - II [3]
                                    KC_AUDIO_MUTE,
      KC_DOLLAR,    KC_PERCENT,     KC_SLCTQRY,
      KC_PAGE_UP,   KC_HOME,        KC_MYCM,
      KC_MPRV,      KC_MSTP,        KC_MNXT,
      KC_VOLD,      KC_MUTE,        KC_VOLU
  ),
  [3] = LAYOUT(                     // Media Layer  [4]
                                    KC_AUDIO_MUTE,
      KC_DOLLAR,    KC_PERCENT,     _______,
      KC_PAGE_UP,   KC_HOME,        KC_MYCM,
      KC_MPRV,      KC_MSTP,        KC_MNXT,
      KC_VOLD,      KC_MUTE,        KC_VOLU
  ),
  [4] = LAYOUT(                     // System Layer [5]
                                    KC_AUDIO_MUTE,
      RGB_TOG,      RGB_MOD,        QK_BOOTLOADER,
      RGB_VAI,      RGB_RMOD,       KC_MYCM,
      RGB_SPI,      RGB_SPD,        RGB_M_R,
      KC_VOLD,      KC_MUTE,        KC_VOLU
  ),
};

// RGB matrix
#ifdef RGB_MATRIX_ENABLE
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max)
{
  switch (get_highest_layer(layer_state))
  {
  case 0:
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_set_color(LED_LAYER[0], RGB_CYAN);
    break;
  case 1:
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_set_color(LED_LAYER[1], RGB_BLUE);
    rgb_blinker(0, 0, 128);
    break;
  case 2:
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_set_color(LED_LAYER[2], RGB_GREEN);
    break;
  case 3:
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_set_color(LED_LAYER[3], RGB_CHARTREUSE);
    break;
  case 4:
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_set_color(LED_LAYER[4], RGB_RED);
    rgb_blinker(255, 0, 0);
    break;

  default:
    break;
  }
  return false;
};
#endif


//--------------------------------------- TAP DANCE -------------------------------------
// CRED - Tap dance action function
void cred_send_string(tap_dance_state_t *state, void *user_data) {
    switch (state->count) {
        case 1:
            SEND_STRING(CRED_USER2 "\n");
            break;
        case 2:
            SEND_STRING(CRED_PASS2 "\n");
            break;
        case 3:
            SEND_STRING(CRED_USER2 "\t" CRED_PASS2 "\n");

            break;
    }
};
void cred_send_string_tcs(tap_dance_state_t *state, void *user_data) {
    switch (state->count) {
        case 1:
            SEND_STRING(CRED_USER1 "\t");
            break;
        case 2:
            SEND_STRING(CRED_PASS1 "\n");
            break;
        case 3:
            SEND_STRING(CRED_USER1 "\t" CRED_PASS1 "\n");

            break;
    }
};
//---------------------------------------------------------------------------------------

// Layer Switching
#define HIGHEST_LAYER 4  // Adjust for your highest layer

void matrix_init_user(void) {
    eeconfig_init();  // Reset the EEPROM
    // Your other initialization code here
    // set_single_persistent_default_layer(2);
}

void keyboard_post_init_user(void) {
    // rgb_matrix_mode(RGB_MATRIX_CYCLE_ALL);  // Set default RGB effect
    // rgb_matrix_mode_noeeprom(RGB_MATRIX_NONE);

}


bool stop_screensaver = false;     //screensaver mode status
uint32_t last_activity_timer = 0;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {

  if (record->event.pressed)
        stop_screensaver = false;  //turn off screensaver mode on any keypress

  switch (keycode) {
  case KC_LUP:
    if (record->event.pressed) {
      uint8_t current_layer = biton32(layer_state);  // Get current active layer
      if (current_layer < HIGHEST_LAYER) {
        layer_move(current_layer + 1);  // Move up a layer
      } else {
        layer_move(0);  // Wrap back to base layer
      }
    }
    return false;

  case KC_LDN:
    if (record->event.pressed) {
      uint8_t current_layer = biton32(layer_state);  // Get current active layer
      if (current_layer > 0) {
        layer_move(current_layer - 1);  // Move down a layer
      } else {
        layer_move(HIGHEST_LAYER);  // Wrap back to highest layer
      }
    }
    return false;
    // Username & Password
  case KC_CRDNTLS1:
    if (record->event.pressed) {
    SEND_STRING(CRED_USER1 "\t" CRED_PASS1 "\n");
    }
    return false;
  case KC_CRDNTLS2:
    if (record->event.pressed) {
      SEND_STRING(CRED_PASS2"\n");
    }
    return false;

    // Select Query
  case KC_SLCTQRY:
    if (record->event.pressed) {
      SEND_STRING("SELECT\t*\n"
                    "FROM\t\n"
                    "WHERE\t\n"
                    "AND \t\n");
    }
    return false;
  case NO_SLEEP:
    if (record->event.pressed) {               //if NO_SLEEP is pressed
        stop_screensaver = true;               //turn on screensaver mode
        last_activity_timer = timer_read32();  //reset timer
    }
    return false;

  default:
    return true;
  }
}

// Encoder
#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
  [0] = { ENCODER_CCW_CW(KC_LDN, KC_LUP) },
  [1] = { ENCODER_CCW_CW(_______, _______) },
  [2] = { ENCODER_CCW_CW(_______, _______) },
  [3] = { ENCODER_CCW_CW(_______, _______) },
  [4] = { ENCODER_CCW_CW(_______, _______) },
};
#endif

void matrix_scan_user(void) {
    // rgb_matrix_task();  // Keep RGB effects active
    if (stop_screensaver) {                                             //if screensaver mode is active
        if (timer_elapsed32(last_activity_timer) > SCREENSAVE_DELAY) {  //and no key has been pressed in more than SCREENSAVE_DELAY ms
            // tap_code16(KC_F13);                                      //  tap F13
            register_code(KC_MS_UP);                                    // Simulate a mouse movement
            unregister_code(KC_MS_UP);
            last_activity_timer = timer_read32();                       //  reset last_activity_timer
        }
    }
}


#ifdef OLED_ENABLE
bool oled_task_user(void) {
    oled_clear();
    switch (get_highest_layer(layer_state)) {
        case 0:
            //---------------("123456789012345678901")
            oled_write_P(PSTR("       NUMPAD        "), true);
            oled_advance_page(false);
            oled_advance_page(false);
            //---------------("123456789012345678901")
            oled_write_P(PSTR("     7     8     9"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("     4     5     6"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("     1     2     3"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("    ENT    0    BKSP"), false);
            break;
        case 1:
            //---------------("123456789012345678901")
            oled_write_P(PSTR("        OFFICE       "), true);
            oled_advance_page(false);
            oled_advance_page(false);
            //---------------("123456789012345678901")
            oled_write_P(PSTR("  TSKMGR   8     9"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("  WIN-Lay  5     6"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("  -VOL    MUTE  +VOL "), false);
            oled_advance_page(false);
            oled_write_P(PSTR("  TCS     MACE   -"), false);
            break;
        case 2:
            //---------------("123456789012345678901")
            oled_write_P(PSTR("       Layer-3       "), true);
            oled_advance_page(false);
            oled_advance_page(false);
            //---------------("123456789012345678901")
            oled_write_P(PSTR("     7     8     9"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("     4     5     6"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("     1     2     3"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("   -VOL   MUTE  +VOL"), false);
            break;
        case 3:
            //---------------("123456789012345678901")
            oled_write_P(PSTR("       Layer-4       "), true);
            oled_advance_page(false);
            oled_advance_page(false);
            //---------------("123456789012345678901")
            oled_write_P(PSTR("     7     8     9"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("     4     5     6"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("     1     2     3"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("    ENT    0    BKSP"), false);
            break;
        case 4:
            //---------------("123456789012345678901")
            oled_write_P(PSTR("       Layer-5       "), true);
            oled_advance_page(false);
            oled_advance_page(false);
            //---------------("123456789012345678901")
            oled_write_P(PSTR("     7     8     9"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("     4     5     6"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("     1     2     3"), false);
            oled_advance_page(false);
            oled_write_P(PSTR("    ENT    0    BKSP"), false);
            break;
        default:
            // Or use the write_ln shortcut over adding '\n' to the end of your string
            oled_write_ln_P(PSTR("Undefined"), false);
    }
    led_t led_state = host_keyboard_led_state();
    oled_set_cursor(0, 1);
    // oled_write_P(led_state.num_lock ? PSTR("NUM ") : PSTR("    "), false);
    oled_write_P(led_state.caps_lock ? PSTR("CAP ") : PSTR("    "), false);
    // oled_write_P(led_state.scroll_lock ? PSTR("SCR ") : PSTR("    "), false);

    return false;
}
#endif

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
            if (tap_data->identifier == TD_RUN) {
                register_code(tap_data->keycode1);
                register_code(tap_data->keycode2);
            }else {
                register_code(tap_data->keycode1);
            }
            break;
        case TD_SINGLE_HOLD:
            if (tap_data->identifier == TD_RUN) {
                register_code(tap_data->keycode5);
                register_code(tap_data->keycode3);
                register_code(tap_data->keycode6);
            } else {
                register_code(tap_data->keycode4);
            }
            break;
        case TD_DOUBLE_TAP:
            if (tap_data->identifier == TD_RUN) {
                register_code(tap_data->keycode1);
                register_code(tap_data->keycode3);
                register_code(tap_data->keycode4);
            }else {
                register_code(tap_data->keycode3);
            }
            break;
        case TD_DOUBLE_HOLD:
            register_code(tap_data->keycode1);
            break;

        // Last case is for fast typing. Assuming your key is `f`:
        // For example, when typing the word `buffer`, and you want to make sure that you send `ff` and not `Esc`.
        // In order to type `ff` when typing fast, the next character will have to be hit within the `TAPPING_TERM`, which by default is 200ms.
        case TD_DOUBLE_SINGLE_TAP:
            register_code(tap_data->keycode1);
            break;
        case TD_TRIPLE_TAP:
            register_code(tap_data->keycode1);
            break;
        default: break;
    }
}

void x_reset(tap_dance_state_t *state, void *user_data) {
    // uint16_t keycode = ((tap_data_user*)user_data)->keycode;
    tap_data_user *tap_data = (tap_data_user *)user_data;

    switch (xtap_state.state) {
        case TD_SINGLE_TAP:
            if (tap_data->identifier == TD_RUN) {
                unregister_code(tap_data->keycode1);
                unregister_code(tap_data->keycode2);
            } else if (tap_data->identifier == TD_CRED_1) {
                unregister_code(PSTR(tap_data->keycode1));
            } else {
                unregister_code(tap_data->keycode1);
            }
            break;
        case TD_SINGLE_HOLD:
            if (tap_data->identifier == TD_RUN) {
                unregister_code(tap_data->keycode5);
                unregister_code(tap_data->keycode3);
                unregister_code(tap_data->keycode6);
            } else {
                unregister_code(tap_data->keycode4);
            }
            break;
        case TD_DOUBLE_TAP:
           if (tap_data->identifier == TD_RUN) {
                unregister_code(tap_data->keycode1);
                unregister_code(tap_data->keycode3);
                unregister_code(tap_data->keycode4);
            } else {
                unregister_code(tap_data->keycode3);
            }
            break;
        case TD_DOUBLE_HOLD:
            unregister_code(tap_data->keycode1);
            break;
        case TD_DOUBLE_SINGLE_TAP:
            unregister_code(tap_data->keycode1);
            break;
        case TD_TRIPLE_TAP:
           unregister_code(tap_data->keycode1);
            break;

        default: break;
    }
    xtap_state.state = TD_NONE;
}

// Tap Dance definitions
tap_dance_action_t tap_dance_actions[] = {
    [TD_CRED_1] = ACTION_TAP_DANCE_FN(cred_send_string_tcs),
    [TD_CRED_2] = ACTION_TAP_DANCE_FN(cred_send_string),
    [TD_RUN]   = ACTION_TAP_DANCE_FN_ADVANCED_USER(NULL, x_finished, x_reset, \
                                                        &((tap_data_user){KC_LCTL, KC_F5, KC_LSFT, KC_DEL, KC_LALT, KC_F, TD_RUN})),


};
//---------------------- RGB Blinker Function ----------------------
#ifdef OLED_ENABLE
void rgb_blinker(uint16_t rVal, uint16_t gVal, uint16_t bVal)
{
    if (timer_elapsed(blink_timer) > BLINK_INTERVAL) {
        blink_timer = timer_read();  // Reset the timer
        led_on = !led_on;            // Toggle the LED state (on/off)
    }

    // If the LED is in the "on" state, light it up
    if (led_on) {
        rgb_matrix_set_color(LED_3, rVal, gVal, bVal);  // Set to red when on
    } else {
        rgb_matrix_set_color(LED_3, 0, 0, 0);    // Turn off when "off"
    }
}

#endif


/*
qmk compile -kb adafruit/macropad -km Thinksomak
qmk flash -kb adafruit/macropad -km Thinksomak
*/
