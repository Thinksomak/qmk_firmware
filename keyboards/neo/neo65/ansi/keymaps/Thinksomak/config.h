/*
 * Copyright 2023 Thinksomak
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

// TT set to two taps
// #define TAPPING_TOGGLE 2

// one shot tap settings
// #define ONESHOT_TAP_TAGGLE 3
// #define ONESHOT_TIMEOUT 1500
#pragma once

#define NO_MUSIC_MODE
#define LAYER_STATE_8BIT

// TAP Dance Settings
#define DEBOUNCE 4
#define TAP_CODE_DELAY 15
// #define FORCE_NKRO
// tap vs hold time difference
#define TAPPING_TERM 175
#define TAPPING_TERM_PER_KEY

#define ONESHOT_TAP_TOGGLE 2
#define ONESHOT_TIMEOUT 3000  // Timeout in milliseconds

// Reactive RGB
// #define ENABLE_RGB_MATRIX_TYPING_HEATMAP
// #define RGB_MATRIX_TYPING_HEATMAP_DECREASE_DELAY_MS 50
// #define RGB_MATRIX_STARTUP_VAL        240
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CYCLE_UP_DOWN

// Leader config
#define LEADER_PER_KEY_TIMING
#define LEADER_TIMEOUT 500
#define LEADER_KEY_STRICT_KEY_PROCESSING

/* RGB matrix */
#ifdef RGB_MATRIX_ENABLE    // i2c setting
#define USE_I2CV1
#define I2C1_SCL 8
#define I2C1_SDA 9
#define I2C1_CLOCK_SPEED 400000
#define I2C1_DUTY_CYCLE FAST_DUTY_CYCLE_2
#define RGB_DISABLE_AFTER_TIMEOUT 0         // number of ticks to wait until disabling effects
#define RGB_MATRIX_SLEEP                    // turn off effects when suspended
#define RGB_MATRIX_LED_PROCESS_LIMIT  4
// #define RGB_MATRIX_LED_FLUSH_LIMIT    16
// #define RGB_MATRIX_STARTUP_VAL        240
#define ENABLE_RGB_MATRIX_CYCLE_UP_DOWN
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CYCLE_UP_DOWN
#define DRIVER_ADDR_1                 0b0110000
#define DRIVER_COUNT                  1
#endif


