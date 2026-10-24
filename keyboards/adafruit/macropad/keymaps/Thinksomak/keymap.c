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
#include "boot_confirm.h"
#include "nosleep.h"
#include "pomodoro.h"
#include "stats.h"
#include "storage.h"
#include "ui_oled.h"
#include "ui_rgb.h"

// Blinking LED Implementation.start
#include "timer.h"  // Include the timer functions


#define ARRAYSIZE(arr) sizeof(arr) / sizeof(arr[0])
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
    NO_SLEEP,                       //No-Sleep keep-awake toggle (off at power-up), see nosleep.c
    KC_PMDRO,                       //Pomodoro: start / pause / resume
    PM_SKIP,                        //Pomodoro: skip to next phase
    PM_RST,                         //Pomodoro: reset phase, press again to reset cycle
    PM_EDIT,                        //Pomodoro: edit settings
    PM_STAT,                        //Pomodoro: stats view (press again to close)
    PM_ADJ,                         //Pomodoro: adjust time with the knob (re-opens a phase that just ended)
    PM_FOCUS,                       //Pomodoro: start a focus now
    PM_BREAK,                       //Pomodoro: start a short break now
    PM_LONG,                        //Pomodoro: start a long break now
    PM_AUTO,                        //Pomodoro: auto start on / off
    PM_UNDO,                        //Pomodoro: undo the last skip / reset / jump / new cycle
    PM_CYCLE,                       //Pomodoro: new cycle (back to READY)
    SYS_BOOT                        //Bootloader, after a confirm screen (boot_confirm.c)
};

#define PM_TOGG KC_PMDRO
#define POMO_LAYER 5

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
      TSK_MGR,      PM_TOGG,        NO_SLEEP,
      CK_CSTB,      KC_TAB,         KC_DEL,
      KC_VOLD,      KC_MUTE,        MS_BTN2,
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
      KC_DOLLAR,    KC_PERCENT,     KC_MPLY,
      KC_PAGE_UP,   KC_HOME,        KC_MYCM,
      KC_MPRV,      KC_MSTP,        KC_MNXT,
      KC_VOLD,      KC_MUTE,        KC_VOLU
  ),
  [4] = LAYOUT(                     // System Layer [5]
                                    KC_AUDIO_MUTE,
      RM_TOGG,      RM_NEXT,        SYS_BOOT,
      RM_VALU,      RM_PREV,       KC_MYCM,
      RM_SPDU,      RM_SPDD,        RM_VALD,
      KC_VOLD,      KC_MUTE,        KC_VOLU
  ),
  [POMO_LAYER] = LAYOUT(            // Pomodoro Layer [6]: pomodoro keys only
                                    PM_ADJ,          // knob press: adjust time
      PM_TOGG,      PM_SKIP,        PM_RST,
      PM_EDIT,      PM_ADJ,         PM_STAT,
      PM_FOCUS,     PM_BREAK,       PM_LONG,
      PM_AUTO,      PM_UNDO,        PM_CYCLE
  ),
};

// Pomodoro EDIT mode, by physical position. While editing, every key and the
// encoder are captured (nothing reaches the PC) and the layer stays put.
// Encoder: turn = -1/+1, press = next field.
#define P_(a) POMO_EDIT_##a
const uint8_t PROGMEM pomodoro_edit_map[MATRIX_ROWS][MATRIX_COLS] = {
  { P_(NONE),       P_(NONE),       P_(NEXT_FIELD) },   // encoder press (col 2)
  { P_(SAVE),       P_(PREV_FIELD), P_(CANCEL) },       // START   SKIP   RESET
  { P_(DEFAULT),    P_(NEXT_FIELD), P_(NONE) },         // EDIT    ADJUST STATS
  { P_(DEC1),       P_(NONE),       P_(INC1) },         // FOCUS   BREAK  LONG
  { P_(DEC5),       P_(NONE),       P_(INC5) },         // AUTO    UNDO   CYCLE
};
#undef P_

// Pomodoro STATS view, by physical position. STATS (the key that opened it)
// or the encoder press closes it; encoder turn = page -/+. CLEAR must be held
// (POMODORO_CLEAR_HOLD_MS) and only clears "since stats reset". PASS keys
// keep their normal pomodoro job (EDIT opens settings, ADJUST, jumps, ...).
#define S_(a) POMO_STATS_##a
const uint8_t PROGMEM pomodoro_stats_map[MATRIX_ROWS][MATRIX_COLS] = {
  { S_(NONE),       S_(NONE),       S_(BACK) },         // encoder press (col 2)
  { S_(PREV),       S_(NEXT),       S_(CLEAR) },        // START   SKIP   RESET
  { S_(PASS),       S_(PASS),       S_(BACK) },         // EDIT    ADJUST STATS
  { S_(PASS),       S_(PASS),       S_(PASS) },         // FOCUS   BREAK  LONG
  { S_(PASS),       S_(PASS),       S_(PASS) },         // AUTO    UNDO   CYCLE
};
#undef S_

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
    ui_rgb_nosleep_key(LED_K02);  // the No-Sleep key shows its own state
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
  case POMO_LAYER:
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_set_color(LED_LAYER[POMO_LAYER], 0xFF, 0x3C, 0x14);  // tomato
    break;

  default:
    break;
  }
  ui_rgb_pomodoro_overlay();  // pomodoro hint / alert on top of everything
  if (boot_confirm_active()) ui_rgb_boot_confirm();  // ...and the boot confirm above that
  return false;
};
#endif

// Bootloader jump (confirmed on the boot screen): leave the OLED and LEDs
// showing flash mode, so it is obvious why the keyboard stopped.
bool shutdown_user(bool jump_to_bootloader) {
  if (jump_to_bootloader) {
#ifdef RGB_MATRIX_ENABLE
    ui_rgb_flash_mode();
#endif
#ifdef OLED_ENABLE
    ui_oled_flash_mode();
#endif
  }
  return true;
}

// Pomodoro alert: count it as input activity so RGB_MATRIX_TIMEOUT and the
// OLED sleep both wake up, and force RGB on for the alert if it was off.
void pomodoro_alert_start_user(pomodoro_alert_t alert) {
    set_activity_timestamps(sync_timer_read32(), last_encoder_activity_time(), last_pointing_device_activity_time());
#ifdef RGB_MATRIX_ENABLE
    ui_rgb_alert_begin();
#endif
}

void pomodoro_alert_end_user(void) {
#ifdef RGB_MATRIX_ENABLE
    ui_rgb_alert_end();
#endif
}

// Pomodoro -> stats. Stats saves to EEPROM only when a phase completes.
void pomodoro_focus_time_user(uint32_t ms) {
    stats_add_focus_ms(ms);
}

void pomodoro_phase_complete_user(pomodoro_phase_t phase, bool counted) {
    stats_phase_complete(phase, counted);
}

void pomodoro_stats_clear_user(void) {
    stats_clear_since_reset();  // lifetime is never touched
}

// Which pomodoro command a keycode is. pomodoro.c does the rest, including
// which commands act straight away at a phase-end alert.
static pomodoro_cmd_t pomodoro_cmd(uint16_t keycode) {
    switch (keycode) {
    case PM_TOGG:  return POMO_CMD_TOGGLE;
    case PM_SKIP:  return POMO_CMD_SKIP;
    case PM_RST:   return POMO_CMD_RESET;
    case PM_EDIT:  return POMO_CMD_EDIT;
    case PM_STAT:  return POMO_CMD_STATS;
    case PM_ADJ:   return POMO_CMD_ADJUST;
    case PM_FOCUS: return POMO_CMD_FOCUS_NOW;
    case PM_BREAK: return POMO_CMD_BREAK_NOW;
    case PM_LONG:  return POMO_CMD_LONG_NOW;
    case PM_AUTO:  return POMO_CMD_AUTO;
    case PM_UNDO:  return POMO_CMD_UNDO;
    case PM_CYCLE: return POMO_CMD_NEW_CYCLE;
    default:       return POMO_CMD_NONE;
    }
}


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
#define HIGHEST_LAYER POMO_LAYER  // Adjust for your highest layer

// No EEPROM wipe at boot any more: it erased RGB settings and the saved
// pomodoro settings and stats on every power-up. It also used to reload the
// compiled keymap into VIA's EEPROM copy each time, which VIA alone only does
// when the build DATE changes. load_compiled_keymap_if_changed() takes that
// job over. To wipe everything by hand, use QK_CLEAR_EEPROM / EE_CLR.
static void load_compiled_keymap_if_changed(void);

void keyboard_post_init_user(void) {
    // rgb_matrix_mode(RGB_MATRIX_CYCLE_ALL);  // Set default RGB effect
    // rgb_matrix_mode_noeeprom(RGB_MATRIX_NONE);
    storage_init();   // settings + stats from EEPROM (or defaults)
    pomodoro_init();
    load_compiled_keymap_if_changed();
}

// Background ticks: run on every layer, never block.
void housekeeping_task_user(void) {
    pomodoro_task();
    nosleep_task();
    boot_confirm_task();
}


bool process_record_user(uint16_t keycode, keyrecord_t *record) {

  // The bootloader confirm screen owns every key while it is open.
  if (!boot_confirm_process_record(record)) return false;

  // Pomodoro keys, alert dismissal and the edit / stats / adjust views.
  if (!pomodoro_process_record(pomodoro_cmd(keycode), keycode, record)) return false;

  switch (keycode) {
  case SYS_BOOT:
    if (record->event.pressed) boot_confirm_begin();
    return false;

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
    if (record->event.pressed) nosleep_toggle();
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
  [POMO_LAYER] = { ENCODER_CCW_CW(_______, _______) },
};
#endif

// VIA keeps its own copy of the keymap in EEPROM, and the keyboard types from
// that copy, not from keymaps[] above. VIA refreshes the copy only when the
// build date changes, so a second flash on the same day kept the old keys.
// Here the copy is reloaded whenever the compiled keymap (keys and knob) is
// different from the one loaded last time, which a fingerprint in the
// settings block remembers. Settings and stats are not touched. Remaps made
// in the VIA app survive power cycles until the next keymap change in code.
static uint32_t fnv1a(uint32_t h, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619UL;
    }
    return h;
}

static void load_compiled_keymap_if_changed(void) {
    uint32_t sig = fnv1a(2166136261UL, keymaps, sizeof(keymaps));
#ifdef ENCODER_MAP_ENABLE
    sig = fnv1a(sig, encoder_map, sizeof(encoder_map));
#endif
    if (storage_keymap_sig() == sig) return;
    dynamic_keymap_reset();  // VIA's keys and knob = the compiled ones
    storage_set_keymap_sig(sig);
    storage_commit();
}

#ifdef OLED_ENABLE
// OLED legend per layer: header name, then the 12 keys left to right, top to
// bottom (max 6 chars). Keep in sync with keymaps[] above. Credential keys
// show only their labels, never anything from the macros.
static const ui_layer_legend_t layer_legends[] = {
    [0] = {"NUMPAD", {"7",      "8",     "9",
                      "4",      "5",     "6",
                      "1",      "2",     "3",
                      "ENT",    "0",     "BKSP"}},
    [1] = {"OFFICE", {"TSKMGR", UI_LEGEND_POMO_TOGGLE, UI_LEGEND_NOSLEEP,
                      "S-TAB",  "TAB",   "DEL",
                      "VOL-",   "MUTE",  "RCLICK",
                      "TCS",    "MACE",  "RUN"}},
    [2] = {"MACRO",  {"$",      "%",     "SQL",
                      "PGUP",   "HOME",  "MYPC",
                      "PREV",   "STOP",  "NEXT",
                      "VOL-",   "MUTE",  "VOL+"}},
    [3] = {"MEDIA",  {"$",      "%",     "PLAY",
                      "PGUP",   "HOME",  "MYPC",
                      "PREV",   "STOP",  "NEXT",
                      "VOL-",   "MUTE",  "VOL+"}},
    [4] = {"SYSTEM", {"RGB",    "MODE+", "BOOT",
                      "BRI+",   "MODE-", "MYPC",
                      "SPD+",   "SPD-",  "BRI-",
                      "VOL-",   "MUTE",  "VOL+"}},
    [POMO_LAYER] = {"POMODORO", {UI_LEGEND_POMO_TOGGLE, "SKIP", "RESET",
                      "EDIT",   "ADJUST", "STATS",
                      "FOCUS",  "BREAK", "LONG",
                      "AUTO",   "UNDO",  "CYCLE"}},
};

bool oled_task_user(void) {
    uint8_t layer  = get_highest_layer(layer_state);
    uint8_t status = nosleep_is_on() ? UI_STATUS_NOSLEEP : 0;
    if (layer == 4) status |= UI_STATUS_RGB_INFO;
    ui_oled_render(layer, HIGHEST_LAYER + 1, layer < ARRAYSIZE(layer_legends) ? &layer_legends[layer] : NULL,
                   layer == POMO_LAYER, status);
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
