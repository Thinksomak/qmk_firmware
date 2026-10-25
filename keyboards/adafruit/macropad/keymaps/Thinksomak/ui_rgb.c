// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "lib/lib8tion/lib8tion.h"
#include "ui_rgb.h"
#include "pomodoro.h"
#include "nosleep.h"
#include "rgb_matrix_map.h"

/* Key that glows dimly while a phase is loaded (bottom right). */
#ifndef POMODORO_HINT_LED
#    define POMODORO_HINT_LED LED_K32
#endif

#define KEYS_PER_ROW 3
#define BREATHE_MS 2000 // focus-done breathing period
#define PULSE_MS 1200   // break-done double-pulse period
#define ROW_DELAY_MS 90 // break-done sweep delay per row

/* Who needs the lights on right now, even if the user switched RGB off:
 * the pomodoro alert, the Host Power menu or countdown. */
enum { HOLD_ALERT = 1 << 0, HOLD_POWER = 1 << 1 };
static uint8_t holders;
static bool    switched_on; // RGB was off and is on only for the holders

/* RGB on (not saved) while someone holds it, and back off when the last
 * one lets go, but only if it was off before. */
static void hold_lights(uint8_t who, bool on) {
    if (on) {
        if (!holders && !rgb_matrix_is_enabled()) {
            rgb_matrix_enable_noeeprom();
            switched_on = true;
        }
        holders |= who;
    } else if (holders & who) {
        holders &= (uint8_t)~who;
        if (!holders && switched_on) {
            rgb_matrix_disable_noeeprom();
            switched_on = false;
        }
    }
}

/* Alert brightness follows the user's RGB brightness, with a floor so the
 * alert stays visible when the LEDs are turned down very low. */
static uint8_t alert_peak(void) {
    uint8_t v = rgb_matrix_get_val();
    return v < 64 ? 64 : v;
}

/* Calm green breathing on every key: time for a break. */
static void draw_focus_done(uint32_t t) {
    uint8_t wave = sin8((uint8_t)((t * 256 / BREATHE_MS) & 0xFF));
    uint8_t v    = scale8(alert_peak(), qadd8(40, scale8(wave, 215)));
    rgb_matrix_set_color_all(0, v, scale8(v, 40));
}

/* Amber double pulse sweeping down the rows: back to work. */
static void draw_break_done(uint32_t t) {
    uint8_t peak = alert_peak();
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        uint8_t  row   = i / KEYS_PER_ROW;
        uint16_t phase = (t + PULSE_MS - (uint32_t)row * ROW_DELAY_MS) % PULSE_MS;
        bool     on    = phase < 110 || (phase >= 220 && phase < 330);
        uint8_t  v     = on ? peak : peak / 10;
        rgb_matrix_set_color(i, v, scale8(v, 100), 0);
    }
}

#if POMODORO_LED_HINT
/* One key, about a tenth of the user's brightness: red focus, green break,
 * blue long break. Blinks slowly while paused. */
static void draw_hint(void) {
    pomodoro_phase_t phase = pomodoro_phase();
    if (phase == POMO_PHASE_IDLE) return;
    if (pomodoro_is_paused() && (timer_read32() / 1000) % 2) {
        rgb_matrix_set_color(POMODORO_HINT_LED, 0, 0, 0);
        return;
    }
    uint8_t v = rgb_matrix_get_val() / 10;
    if (v < 6) v = 6;
    switch (phase) {
        case POMO_PHASE_FOCUS:
            rgb_matrix_set_color(POMODORO_HINT_LED, v, scale8(v, 40), 0);
            break;
        case POMO_PHASE_SHORT_BREAK:
            rgb_matrix_set_color(POMODORO_HINT_LED, 0, v, 0);
            break;
        default:
            rgb_matrix_set_color(POMODORO_HINT_LED, 0, scale8(v, 100), v);
            break;
    }
}
#endif

/* On top of the layer colours: the phase-end alert on every key, or the
 * phase hint on one key. */
void ui_rgb_pomodoro_overlay(void) {
    switch (pomodoro_alert()) {
        case POMO_ALERT_FOCUS_DONE:
            draw_focus_done(pomodoro_alert_elapsed());
            return;
        case POMO_ALERT_BREAK_DONE:
            draw_break_done(pomodoro_alert_elapsed());
            return;
        default:
            break;
    }
#if POMODORO_LED_HINT
    draw_hint();
#endif
}

/* The alert must be seen: RGB on for it if it was off (not saved). */
void ui_rgb_alert_begin(void) {
    hold_lights(HOLD_ALERT, true);
}

/* RGB back off if only the alert needed it. */
void ui_rgb_alert_end(void) {
    hold_lights(HOLD_ALERT, false);
}

/* The Host Power menu and countdown must be seen too. Called every tick
 * with whether one shows; only a change does anything. */
void ui_rgb_hold_for_host_power(bool on) {
    hold_lights(HOLD_POWER, on);
}

/* The cancel key of the Host Power menu and countdown: top-right, where 9
 * is on the numpad. */
#define HOST_POWER_CANCEL_LED LED_K02

/* Host Power menu or countdown: every key a vivid red, whatever the RGB
 * brightness is set to, so it is plain the PC is about to go off; the
 * cancel key green, steady in the menu and pulsing during a countdown
 * (about once a second). */
void ui_rgb_host_power(bool counting) {
    uint8_t g = 255;
    if (counting) g = qadd8(60, scale8(quadwave8((uint8_t)(timer_read32() / 4)), 195));
    rgb_matrix_set_color_all(255, 0, 0);
    rgb_matrix_set_color(HOST_POWER_CANCEL_LED, 0, g, 0);
}

#define NS_BLINK_PERIOD_MS 2000 // one soft blink every 2 s
#define NS_BLINK_MS 400         // length of that blink
#define NS_FLASH_MS 300         // cyan flash after each keep-awake nudge

/* The No-Sleep key: dark when off; when on, a dim blue with a soft blink
 * every 2 s and a short cyan flash at each nudge. */
void ui_rgb_nosleep_key(uint8_t led) {
    if (!nosleep_is_on()) {
        rgb_matrix_set_color(led, 0, 0, 0);
        return;
    }
    // Follow the user's brightness, with floors so it never vanishes.
    uint8_t val  = rgb_matrix_get_val();
    uint8_t base = val / 10 < 8 ? 8 : val / 10;
    uint8_t peak = val / 2 < 48 ? 48 : val / 2;

    if (nosleep_since_nudge() < NS_FLASH_MS) {
        rgb_matrix_set_color(led, 0, peak, peak);
        return;
    }
    uint16_t t = timer_read32() % NS_BLINK_PERIOD_MS;
    uint8_t  v = base;
    if (t < NS_BLINK_MS) {
        // Smooth rise and fall instead of a hard on/off blink.
        v = base + scale8(peak - base, quadwave8((uint8_t)(t * 255 / NS_BLINK_MS)));
    }
    rgb_matrix_set_color(led, 0, 0, v);
}

/* The confirm key of boot_confirm.c: bottom-right. */
#define BOOT_CONFIRM_LED LED_K32

/* Calculator mode, keys by role: amber = a digit with an operator on hold,
 * dim white = the other digits, green = "=" (ENT), red = BKSP / AC. */
void ui_rgb_calc(void) {
    // Per key, K00..K32: 0 digit, 1 digit with an operator on hold, 2 "=", 3 BKSP.
    static const uint8_t PROGMEM role[RGB_MATRIX_LED_COUNT] = {1, 1, 1, 1, 1, 1, 0, 0, 0, 2, 0, 3};
    uint8_t                      v                          = rgb_matrix_get_val();
    if (v < 48) v = 48; // keep the hint readable at low brightness
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        switch (pgm_read_byte(&role[i])) {
            case 0:
                rgb_matrix_set_color(i, v / 6, v / 6, v / 6);
                break;
            case 1:
                rgb_matrix_set_color(i, v / 2, v / 5, 0);
                break;
            case 2:
                rgb_matrix_set_color(i, 0, v / 2, v / 10);
                break;
            default:
                rgb_matrix_set_color(i, v / 2, 0, 0);
                break;
        }
    }
}

/* Boot confirm: every key dark, the confirm key pulsing red. */
void ui_rgb_boot_confirm(void) {
    uint8_t wave = quadwave8((uint8_t)(timer_read32() / 3)); // about 0.8 s per pulse
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_set_color(BOOT_CONFIRM_LED, scale8(alert_peak(), qadd8(50, scale8(wave, 205))), 0, 0);
}

/* Flash mode: every key red, sent out now (the main loop stops next). */
void ui_rgb_flash_mode(void) {
    rgb_matrix_set_color_all(alert_peak(), 0, 0);
    rgb_matrix_update_pwm_buffers();
}
