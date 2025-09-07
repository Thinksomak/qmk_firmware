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


#pragma once

#define NO_MUSIC_MODE
#define LAYER_STATE_8BIT

// TAP Dance Settings
#define DEBOUNCE 4
#define TAP_CODE_DELAY 15

// tap vs hold time difference
#define TAPPING_TERM 195
#define TAPPING_TERM_PER_KEY
#define TAP_DANCE_RESYNC

#define ONESHOT_TAP_TOGGLE 2      // Number of taps to toggle the modifier (default: 2)
#define ONESHOT_TIMEOUT 3000      // Timeout in ms before the one shot modifier is cleared (default: 3000)
#define ONESHOT_ON_PRESS          // Uncomment to activate OSM on press instead of release

#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CYCLE_UP_DOWN

// Leader config
// #define LEADER_PER_KEY_TIMING
// #define LEADER_TIMEOUT 500
// #define LEADER_KEY_STRICT_KEY_PROCESSING
