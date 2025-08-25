/* Copyright 2022
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


#define CONFIG_H

// Configuration settings
#define OLED_DISPLAY_128X64
#define OLED_UPDATE_INTERVAL 200
#define OLED_TIMEOUT 600000

// #define RGB_MATRIX_ENABLE
#define RGB_MATRIX_KEYPRESSES
#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define RGB_MATRIX_DEFAULT_VAL 200
#define RGB_MATRIX_TIMEOUT 600000
#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define RGB_MATRIX_KEYPRESSES
#define ENABLE_RGB_MATRIX_BREATHING
#define ENABLE_RGB_MATRIX_CYCLE_ALL
#define ENABLE_RGB_MATRIX_TYPING_HEATMAP
#define ENABLE_RGB_MATRIX_ALPHAS_MODS
// ------------------
#define USB_POLLING_INTERVAL_MS 1
#define DEBOUNCE 4
#define TAP_CODE_DELAY 15
#define FORCE_NKRO
// TAP Dance Settings
#define TAPPING_TERM 175
#define TAPPING_TERM_PER_KEY
// Sensitive credentials
#define CRED_USER1 "1214973@tcs.com"
#define CRED_PASS1 "54321$TCS.com"

#define CRED_USER2 "somak.mondal@macegroup.com"
#define CRED_PASS2 "MaceKolkata2027"

#pragma once

/* OLED SPI Defines */
#define OLED_DISPLAY_128X64
#define OLED_IC OLED_IC_SH1106

/* OLED SPI Pins */
#define OLED_DC_PIN GP24
#define OLED_CS_PIN GP22
#define OLED_RST_PIN GP23

/* Shift OLED columns by 2 pixels */
#define OLED_COLUMN_OFFSET 2

/* Divisor for OLED */
#define OLED_SPI_DIVISOR 4

/* ChibiOS SPI definitions */
#define SPI_DRIVER SPID1
#define SPI_SCK_PIN GP26
#define SPI_MOSI_PIN GP27
#define SPI_MISO_PIN GP28

#define DYNAMIC_KEYMAP_LAYER_COUNT 7

     /* RGB Defines */
#define RGB_MATRIX_LED_COUNT 12
