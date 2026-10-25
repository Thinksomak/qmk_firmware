// Copyright 2024-2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Thinksomak keymap for the Adafruit Macropad RP2040: 12 keys, a knob with
 * a press, a 128x64 OLED and one RGB LED per key.
 *
 * This file holds what is specific to the layout and wires the modules
 * together:
 *   - the six main layers and the knob's encoder map (twist = change layer)
 *   - the positional key maps of the modes that take over the keys
 *     (pomodoro EDIT / STATS, SYSINFO view / SETUP)
 *   - the OLED legend of each layer, and the layer colour on the LEDs
 *   - the tap dances of the MACRO layer (secure macro keys and RUN)
 *   - process_record_user, which hands every key event to the modules in
 *     a fixed order (see there)
 *   - the reload of VIA's keymap copy whenever the keymap in code changes
 * The modules themselves: pomodoro.c, stats.c, storage.c, nosleep.c,
 * boot_confirm.c, numpad_ops.c + calc.c, sysinfo.c, ui_oled.c, ui_rgb.c.
 */

#include QMK_KEYBOARD_H
#include "rgb_matrix_map.h"
#include "boot_confirm.h"
#include "nosleep.h"
#include "numpad_ops.h"
#include "pomodoro.h"
#include "stats.h"
#include "storage.h"
#include "sysinfo.h"
#include "ui_oled.h"
#include "ui_rgb.h"

/* ---- Keycodes and layers ------------------------------------------------ */

enum custom_keycodes {
    KC_LUP = SAFE_RANGE, // next main layer (the knob's clockwise twist)
    KC_LDN,              // previous main layer (counter-clockwise)
    KC_SLCTQRY,          // types a SQL SELECT skeleton
    NO_SLEEP,            // No-Sleep keep-awake on / off (off at power-up), nosleep.c
    KC_PMDRO,            // pomodoro: start / pause / resume
    PM_SKIP,             // pomodoro: skip to the next phase
    PM_RST,              // pomodoro: reset the phase, again to reset the cycle
    PM_EDIT,             // pomodoro: settings
    PM_STAT,             // pomodoro: stats view (again to close)
    PM_ADJ,              // pomodoro: adjust the time with the knob
    PM_FOCUS,            // pomodoro: start a focus now
    PM_BREAK,            // pomodoro: start a short break now
    PM_LONG,             // pomodoro: start a long break now
    PM_AUTO,             // pomodoro: auto start on / off
    PM_UNDO,             // pomodoro: undo the last skip / reset / jump / new cycle
    PM_CYCLE,            // pomodoro: new cycle (back to READY)
    SYS_BOOT,            // bootloader, after a confirm screen (boot_confirm.c)
    SYS_INFO             // SYSINFO view: PC stats from the host agent (sysinfo.c)
};
#define PM_TOGG KC_PMDRO

/* Tap dances (MACRO layer bottom row). */
enum { TD_CRED_2, TD_RUN, TD_CRED_1 };

/* Main layers, in knob order: a twist steps through them and wraps round.
 * Modes on top of a layer (pomodoro settings / stats / adjust, SYSINFO, the
 * calculator) are never part of this cycle. */
enum layers { L_NUMPAD, L_MACRO, L_MEDIA, L_NAV, L_SYSTEM, L_POMO, LAYER_COUNT };
_Static_assert(LAYER_COUNT <= DYNAMIC_KEYMAP_LAYER_COUNT, "VIA has fewer layers than the keymap");

#define CK_CSTB S(KC_TAB)
#define TSK_MGR C(S(KC_ESC))

/* ---- Keymap --------------------------------------------------------------
 * The single key on top of each layout is the knob press. Each function
 * sits on its own category's layer. */
// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [L_NUMPAD] = LAYOUT(              // NP(): hold for an operator / the calculator (numpad_ops.h)
                                      KC_MUTE,
        NP(KC_7),     NP(KC_8),       NP(KC_9),
        NP(KC_4),     NP(KC_5),       NP(KC_6),
        NP(KC_1),     NP(KC_2),       NP(KC_3),
        NP(KC_ENT),   NP(KC_0),       NP(KC_BSPC)
    ),
    [L_MACRO] = LAYOUT(
                                      KC_MUTE,
        KC_DOLLAR,    KC_PERCENT,     KC_SLCTQRY,
        KC_NO,        KC_NO,          KC_NO,
        KC_NO,        KC_NO,          KC_NO,
        TD(TD_CRED_1),TD(TD_CRED_2),  TD(TD_RUN)
    ),
    [L_MEDIA] = LAYOUT(
                                      KC_MUTE,
        KC_NO,        KC_NO,          KC_NO,
        KC_NO,        KC_MSTP,        KC_NO,
        KC_MPRV,      KC_MPLY,        KC_MNXT,
        KC_VOLD,      KC_MUTE,        KC_VOLU
    ),
    [L_NAV] = LAYOUT(
                                      KC_MUTE,
        KC_PGUP,      KC_UP,          KC_PGDN,
        KC_LEFT,      KC_DOWN,        KC_RGHT,
        KC_HOME,      KC_NO,          KC_END,
        CK_CSTB,      KC_TAB,         KC_DEL
    ),
    [L_SYSTEM] = LAYOUT(              // system on top, RGB below: + on row 3, - on row 4
                                      KC_MUTE,
        TSK_MGR,      KC_MYCM,        SYS_BOOT,
        NO_SLEEP,     SYS_INFO,       RM_TOGG,
        RM_NEXT,      RM_VALU,        RM_SPDU,
        RM_PREV,      RM_VALD,        RM_SPDD
    ),
    [L_POMO] = LAYOUT(                // pomodoro keys only
                                      PM_ADJ,          // knob press: adjust time
        PM_TOGG,      PM_SKIP,        PM_RST,
        PM_EDIT,      PM_ADJ,         PM_STAT,
        PM_FOCUS,     PM_BREAK,       PM_LONG,
        PM_AUTO,      PM_UNDO,        PM_CYCLE
    ),
};

/* The knob's twist changes the main layer on every layer: clockwise = next.
 * Open modes take the twist before it gets here (process_record_user). */
#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [L_NUMPAD] = { ENCODER_CCW_CW(KC_LDN, KC_LUP) },
    [L_MACRO]  = { ENCODER_CCW_CW(KC_LDN, KC_LUP) },
    [L_MEDIA]  = { ENCODER_CCW_CW(KC_LDN, KC_LUP) },
    [L_NAV]    = { ENCODER_CCW_CW(KC_LDN, KC_LUP) },
    [L_SYSTEM] = { ENCODER_CCW_CW(KC_LDN, KC_LUP) },
    [L_POMO]   = { ENCODER_CCW_CW(KC_LDN, KC_LUP) },
};
#endif

/* ---- Positional maps of the modes ----------------------------------------
 * While a mode is open it reads keys by physical position, not keycode, so
 * the maps below say what each position does. Row 0 is the knob press
 * (column 2). PASS keys keep their normal job. */

/* Pomodoro EDIT: every key and the knob are captured (nothing reaches the
 * PC) and the layer stays put. Knob: turn = -1 / +1, press = next field. */
#define P_(a) POMO_EDIT_##a
const uint8_t PROGMEM pomodoro_edit_map[MATRIX_ROWS][MATRIX_COLS] = {
    { P_(NONE),       P_(NONE),       P_(NEXT_FIELD) },   // knob press
    { P_(SAVE),       P_(PREV_FIELD), P_(CANCEL) },       // START   SKIP   RESET
    { P_(DEFAULT),    P_(NEXT_FIELD), P_(NONE) },         // EDIT    ADJUST STATS
    { P_(DEC1),       P_(NONE),       P_(INC1) },         // FOCUS   BREAK  LONG
    { P_(DEC5),       P_(NONE),       P_(INC5) },         // AUTO    UNDO   CYCLE
};
#undef P_

/* Pomodoro STATS: STATS (the key that opened it) or the knob press closes
 * it; turn = page -/+. CLEAR must be held (POMODORO_CLEAR_HOLD_MS) and only
 * clears "since stats reset". */
#define S_(a) POMO_STATS_##a
const uint8_t PROGMEM pomodoro_stats_map[MATRIX_ROWS][MATRIX_COLS] = {
    { S_(NONE),       S_(NONE),       S_(BACK) },         // knob press
    { S_(PREV),       S_(NEXT),       S_(CLEAR) },        // START   SKIP   RESET
    { S_(PASS),       S_(PASS),       S_(BACK) },         // EDIT    ADJUST STATS
    { S_(PASS),       S_(PASS),       S_(PASS) },         // FOCUS   BREAK  LONG
    { S_(PASS),       S_(PASS),       S_(PASS) },         // AUTO    UNDO   CYCLE
};
#undef S_

/* SYSINFO view (SYSTEM layer): turn = page -/+, press = back. */
#define I_(a) SI_KEY_##a
const uint8_t PROGMEM sysinfo_view_map[MATRIX_ROWS][MATRIX_COLS] = {
    { I_(NONE),       I_(NONE),       I_(BACK) },         // knob press
    { I_(PREV),       I_(NEXT),       I_(SETUP) },        // TSKMGR  MYPC    BOOT
    { I_(PASS),       I_(BACK),       I_(PASS) },         // NS      SYSINF  RGB
    { I_(PASS),       I_(PASS),       I_(PASS) },         // MODE+   BRI+    SPD+
    { I_(PASS),       I_(PASS),       I_(PASS) },         // MODE-   BRI-    SPD-
};

/* SYSINFO SETUP: the same places as the pomodoro EDIT map above.
 * Knob: turn = -1 / +1, press = next field. */
const uint8_t PROGMEM sysinfo_setup_map[MATRIX_ROWS][MATRIX_COLS] = {
    { I_(NONE),       I_(NONE),       I_(NEXT_FIELD) },   // knob press
    { I_(SAVE),       I_(PREV_FIELD), I_(CANCEL) },       // TSKMGR  MYPC    BOOT
    { I_(DEFAULT),    I_(NEXT_FIELD), I_(NONE) },         // NS      SYSINF  RGB
    { I_(DEC1),       I_(NONE),       I_(INC1) },         // MODE+   BRI+    SPD+
    { I_(DEC5),       I_(NONE),       I_(INC5) },         // MODE-   BRI-    SPD-
};
#undef I_
// clang-format on

/* ---- LEDs ---------------------------------------------------------------- */

#ifdef RGB_MATRIX_ENABLE
/* The key that lights in each layer's colour, so the layer shows at a
 * glance: the first six keys, left to right, top to bottom. */
static const uint8_t LED_LAYER[LAYER_COUNT] = {LED_K00, LED_K01, LED_K02, LED_K10, LED_K11, LED_K12};

#    define BOOT_LED LED_K02  // SYSTEM's BOOT key blinks red
#    define BOOT_BLINK_MS 300 // on 300 ms, off 300 ms

/* Blinks the BOOT key red. Runs from the indicator callback, so the state
 * only changes on a redraw that comes after the interval. */
static void blink_boot_key(void) {
    static uint16_t since;
    static bool     on;
    if (timer_elapsed(since) > BOOT_BLINK_MS) {
        since = timer_read();
        on    = !on;
    }
    if (on) {
        rgb_matrix_set_color(BOOT_LED, 255, 0, 0);
    } else {
        rgb_matrix_set_color(BOOT_LED, 0, 0, 0);
    }
}

/* Every frame: all keys dark, then the layer key in the layer's colour
 * (or the calculator's key colours), then the overlays on top: No-Sleep
 * state on SYSTEM, the pomodoro hint or alert, the boot confirm. Returning
 * false stops the board's own indicator code. */
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t layer = get_highest_layer(layer_state);
    rgb_matrix_set_color_all(0, 0, 0);
    switch (layer) {
        case L_NUMPAD:
            if (numpad_calc_active()) {
                ui_rgb_calc();
            } else {
                rgb_matrix_set_color(LED_LAYER[L_NUMPAD], RGB_CYAN);
            }
            break;
        case L_MACRO:
            rgb_matrix_set_color(LED_LAYER[L_MACRO], RGB_BLUE);
            break;
        case L_MEDIA:
            rgb_matrix_set_color(LED_LAYER[L_MEDIA], RGB_GREEN);
            break;
        case L_NAV:
            rgb_matrix_set_color(LED_LAYER[L_NAV], RGB_YELLOW);
            break;
        case L_SYSTEM:
            rgb_matrix_set_color(LED_LAYER[L_SYSTEM], RGB_RED);
            ui_rgb_nosleep_key(LED_K10); // the No-Sleep key shows its own state
            blink_boot_key();
            break;
        case L_POMO:
            rgb_matrix_set_color(LED_LAYER[L_POMO], 0xFF, 0x3C, 0x14); // tomato
            break;
        default:
            break;
    }
    ui_rgb_pomodoro_overlay();                        // pomodoro hint / alert on top of everything
    if (boot_confirm_active()) ui_rgb_boot_confirm(); // ...and the boot confirm above that
    return false;
}
#endif

/* Just before the jump to the bootloader (after the confirm screen): leave
 * the OLED and LEDs showing flash mode, so it is clear why the keys
 * stopped. */
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

/* ---- Pomodoro hooks ------------------------------------------------------ */

/* A pomodoro alert counts as input activity, so the RGB and OLED timeouts
 * wake up, and RGB is forced on for the alert if it was off. */
void pomodoro_alert_start_user(pomodoro_alert_t alert) {
    set_activity_timestamps(sync_timer_read32(), last_encoder_activity_time(), last_pointing_device_activity_time());
#ifdef RGB_MATRIX_ENABLE
    ui_rgb_alert_begin();
#endif
}

/* The alert was dismissed or timed out: RGB back to how it was. */
void pomodoro_alert_end_user(void) {
#ifdef RGB_MATRIX_ENABLE
    ui_rgb_alert_end();
#endif
}

/* Pomodoro -> stats. Stats are saved to EEPROM only when a phase
 * completes, never per minute, to spare the flash. */
void pomodoro_focus_time_user(uint32_t ms) {
    stats_add_focus_ms(ms);
}

/* A phase reached its end: count it (unless it was an extension). */
void pomodoro_phase_complete_user(pomodoro_phase_t phase, bool counted) {
    stats_phase_complete(phase, counted);
}

/* CLEAR held on the stats view. */
void pomodoro_stats_clear_user(void) {
    stats_clear_since_reset(); // lifetime is never touched
}

/* Which pomodoro command a keycode is. pomodoro.c does the rest, including
 * which commands act straight away while a phase-end alert shows. */
static pomodoro_cmd_t pomodoro_cmd(uint16_t keycode) {
    switch (keycode) {
        case PM_TOGG:
            return POMO_CMD_TOGGLE;
        case PM_SKIP:
            return POMO_CMD_SKIP;
        case PM_RST:
            return POMO_CMD_RESET;
        case PM_EDIT:
            return POMO_CMD_EDIT;
        case PM_STAT:
            return POMO_CMD_STATS;
        case PM_ADJ:
            return POMO_CMD_ADJUST;
        case PM_FOCUS:
            return POMO_CMD_FOCUS_NOW;
        case PM_BREAK:
            return POMO_CMD_BREAK_NOW;
        case PM_LONG:
            return POMO_CMD_LONG_NOW;
        case PM_AUTO:
            return POMO_CMD_AUTO;
        case PM_UNDO:
            return POMO_CMD_UNDO;
        case PM_CYCLE:
            return POMO_CMD_NEW_CYCLE;
        default:
            return POMO_CMD_NONE;
    }
}

/* ---- Tap dances (MACRO layer, bottom row) ---------------------------------
 * The two secure macro keys type stored sign-in text, which is defined in
 * the keymap's private config.h (never in this file):
 *   TCS   tap = user name + Tab,   2 taps = password + Enter,
 *         3 taps = user name, Tab, password, Enter
 *   MACE  tap = user name + Enter, 2 taps = password + Enter,
 *         3 taps = user name, Tab, password, Enter
 * The OLED shows only the key labels. */

/* MACE. */
static void cred_send_string(tap_dance_state_t *state, void *user_data) {
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
}

/* TCS. */
static void cred_send_string_tcs(tap_dance_state_t *state, void *user_data) {
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
}

/* How a tap dance ended, decided when it finishes (tapping term over, key
 * held, or another key pressed). */
typedef enum {
    TD_NONE,
    TD_UNKNOWN,
    TD_SINGLE_TAP,
    TD_SINGLE_HOLD,
    TD_DOUBLE_TAP,
    TD_DOUBLE_HOLD,
    TD_DOUBLE_SINGLE_TAP, // two taps, interrupted by another key: typing, not a double tap
    TD_TRIPLE_TAP,
    TD_TRIPLE_HOLD
} td_state_t;

/* Reads a finished dance: how many taps, held or not, cut short or not. */
static td_state_t cur_dance(tap_dance_state_t *state) {
    if (state->count == 1) {
        return (state->interrupted || !state->pressed) ? TD_SINGLE_TAP : TD_SINGLE_HOLD;
    }
    if (state->count == 2) {
        if (state->interrupted) return TD_DOUBLE_SINGLE_TAP;
        return state->pressed ? TD_DOUBLE_HOLD : TD_DOUBLE_TAP;
    }
    if (state->count == 3) {
        return (state->interrupted || !state->pressed) ? TD_TRIPLE_TAP : TD_TRIPLE_HOLD;
    }
    return TD_UNKNOWN;
}

/* RUN key:
 *   tap          Ctrl+F5
 *   hold         Alt+Shift+F
 *   double tap   Ctrl+Shift+Del
 *   double hold, two taps cut short by another key, triple tap: Ctrl
 * The keys go down when the dance is decided and up when it resets (the
 * key is let go), in the same order. */
static td_state_t run_state;

/* Presses (register_code) or releases (unregister_code) the keys of a RUN
 * outcome. */
static void run_keys(td_state_t st, void (*key)(uint8_t)) {
    switch (st) {
        case TD_SINGLE_TAP:
            key(KC_LCTL);
            key(KC_F5);
            break;
        case TD_SINGLE_HOLD:
            key(KC_LALT);
            key(KC_LSFT);
            key(KC_F);
            break;
        case TD_DOUBLE_TAP:
            key(KC_LCTL);
            key(KC_LSFT);
            key(KC_DEL);
            break;
        case TD_DOUBLE_HOLD:
        case TD_DOUBLE_SINGLE_TAP:
        case TD_TRIPLE_TAP:
            key(KC_LCTL);
            break;
        default:
            break;
    }
}

/* The dance is decided: its keys go down. */
static void run_finished(tap_dance_state_t *state, void *user_data) {
    run_state = cur_dance(state);
    run_keys(run_state, register_code);
}

/* The key was let go: its keys go up. */
static void run_reset(tap_dance_state_t *state, void *user_data) {
    run_keys(run_state, unregister_code);
    run_state = TD_NONE;
}

tap_dance_action_t tap_dance_actions[] = {
    [TD_CRED_1] = ACTION_TAP_DANCE_FN(cred_send_string_tcs),
    [TD_CRED_2] = ACTION_TAP_DANCE_FN(cred_send_string),
    [TD_RUN]    = ACTION_TAP_DANCE_FN_ADVANCED(NULL, run_finished, run_reset),
};

/* ---- Start-up, background tasks, Raw HID ---------------------------------- */

/* One main layer up (+1) or down (-1), wrapping round. */
static void step_layer(int8_t dir) {
    uint8_t cur  = get_highest_layer(layer_state);
    uint8_t next = dir > 0 ? (cur + 1 < LAYER_COUNT ? cur + 1 : 0) : (cur > 0 ? cur - 1 : LAYER_COUNT - 1);
    layer_move(next);
}

static void load_compiled_keymap_if_changed(void);

/* Settings and stats from EEPROM first, then the modules that use them.
 * Nothing is wiped at boot: settings, stats and RGB survive power-ups. To
 * wipe everything by hand, use QK_CLEAR_EEPROM / EE_CLR. */
void keyboard_post_init_user(void) {
    storage_init();
    pomodoro_init();
    sysinfo_init();
    load_compiled_keymap_if_changed();
}

/* Background ticks: run on every layer, never block. */
void housekeeping_task_user(void) {
    pomodoro_task();
    nosleep_task();
    boot_confirm_task();
    numpad_ops_task();
    sysinfo_task();
}

/* Raw HID: VIA hands every packet here first. Only the SYSINFO command
 * (0xC5) is taken; everything else goes on to VIA as usual. */
bool via_command_kb(uint8_t *data, uint8_t length) {
    return sysinfo_receive(data, length);
}

/* ---- Key events -------------------------------------------------------------
 * Every key and knob event passes the modules in this order; the first one
 * that uses it returns false and it goes no further:
 *   1. boot confirm      owns every key while its screen is open
 *   2. sleeping screen   a twist only wakes it (a twist you cannot see would
 *                        change layer); keys wake it and act
 *   3. pomodoro          its keys, the alert, its EDIT / STATS / ADJUST modes
 *   4. SYSINFO           its view and SETUP (on the SYSTEM layer)
 *   5. numpad            operators on hold, calculator mode
 *   6. this keymap's own keycodes, below
 * Each open mode takes the knob's twist for its own values and lets go of it
 * the moment it closes, whatever closes it: the mode is worked out from what
 * is open, so no knob state is kept here. */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!boot_confirm_process_record(record)) return false;
#ifdef OLED_ENABLE
    if (IS_ENCODEREVENT(record->event) && ui_oled_asleep()) return false;
#endif
    if (!pomodoro_process_record(pomodoro_cmd(keycode), keycode, record)) return false;
    if (!sysinfo_process_record(record)) return false;
    if (!numpad_ops_process_record(keycode, record)) return false;

    switch (keycode) {
        case SYS_BOOT:
            if (record->event.pressed) boot_confirm_begin();
            return false;
        case SYS_INFO:
            if (record->event.pressed) sysinfo_toggle_view(); // inside the view this key is BACK
            return false;
        case KC_LUP:
            if (record->event.pressed) step_layer(1);
            return false;
        case KC_LDN:
            if (record->event.pressed) step_layer(-1);
            return false;
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

/* ---- VIA keymap reload ------------------------------------------------------
 * VIA keeps its own copy of the keymap in EEPROM, and the keyboard types
 * from that copy, not from keymaps[] above. VIA refreshes the copy only when
 * the build date changes, so a second flash on the same day kept the old
 * keys. Here the copy is reloaded whenever the compiled keymap (keys and
 * knob) differs from the one loaded last time, which a fingerprint in the
 * settings block remembers. Settings and stats are not touched. Remaps made
 * in the VIA app survive power cycles until the next keymap change in code. */

/* FNV-1a, 32 bit: a cheap fingerprint, enough to notice a change. */
static uint32_t fnv1a(uint32_t h, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619UL;
    }
    return h;
}

/* Reloads VIA's keymap from keymaps[] and encoder_map[] if they changed. */
static void load_compiled_keymap_if_changed(void) {
    uint32_t sig = fnv1a(2166136261UL, keymaps, sizeof(keymaps));
#ifdef ENCODER_MAP_ENABLE
    sig = fnv1a(sig, encoder_map, sizeof(encoder_map));
#endif
    if (storage_keymap_sig() == sig) return;
    dynamic_keymap_reset(); // VIA's keys and knob = the compiled ones
    storage_set_keymap_sig(sig);
    storage_commit();
}

/* ---- OLED ------------------------------------------------------------------ */

#ifdef OLED_ENABLE
/* Each layer's legend: header name, then the 12 keys left to right, top to
 * bottom (up to 6 characters). Keep in sync with keymaps[] above. The
 * secure macro keys show only their labels. "\x18".."\x1b" are the font's
 * up / down / right / left arrows. */
// clang-format off
static const ui_layer_legend_t layer_legends[] = {
    [L_NUMPAD] = {"NUMPAD", {"7 %",    "8 /",   "9 *",   // key, space, what a hold types
                             "4",      "5 -",   "6 +",
                             "1",      "2",     "3",
                             "ENT",    "0 .",   "BKSP"}, true},
    [L_MACRO]  = {"MACRO",  {"$",      "%",     "SQL",
                             NULL,     NULL,    NULL,
                             NULL,     NULL,    NULL,
                             "TCS",    "MACE",  "RUN"}},
    [L_MEDIA]  = {"MEDIA",  {NULL,     NULL,    NULL,
                             NULL,     "STOP",  NULL,
                             "PREV",   "PLAY",  "NEXT",
                             "VOL-",   "MUTE",  "VOL+"}},
    [L_NAV]    = {"NAV",    {"PGUP",   "\x18",  "PGDN",
                             "\x1b",   "\x19",  "\x1a",
                             "HOME",   NULL,    "END",
                             "S-TAB",  "TAB",   "DEL"}},
    [L_SYSTEM] = {"SYSTEM", {"TSKMGR", "MYPC",  "BOOT",
                             UI_LEGEND_NOSLEEP, "SYSINF", "RGB",
                             "MODE+",  "BRI+",  "SPD+",
                             "MODE-",  "BRI-",  "SPD-"}},
    [L_POMO]   = {"POMODORO", {UI_LEGEND_POMO_TOGGLE, "SKIP", "RESET",
                             "EDIT",   "ADJUST", "STATS",
                             "FOCUS",  "BREAK", "LONG",
                             "AUTO",   "UNDO",  "CYCLE"}},
};
// clang-format on
_Static_assert(ARRAY_SIZE(layer_legends) == LAYER_COUNT, "one OLED legend per layer");

/* Every OLED frame: ui_oled.c decides what to show (layer, mode, alert,
 * screensaver) and redraws only what changed. Returning false stops the
 * board's own OLED code. */
bool oled_task_user(void) {
    uint8_t layer  = get_highest_layer(layer_state);
    uint8_t status = nosleep_is_on() ? UI_STATUS_NOSLEEP : 0;
    if (layer == L_SYSTEM) status |= UI_STATUS_RGB_INFO;
    ui_oled_render(layer, LAYER_COUNT, layer < LAYER_COUNT ? &layer_legends[layer] : NULL, layer == L_POMO, status);
    return false;
}
#endif
