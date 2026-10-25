// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * PC stats from the host agent (host/main.py) over Raw HID, shown by the
 * SYSINFO view and the strip on the other layers.
 *
 * Packet: 32 bytes, little-endian, protocol 1. The host writes a report ID
 * of 0 in front, so it sends 33 bytes. Nothing is sent back.
 *    0  command 0xC5 (VIA uses 0x01-0x15 and 0xFF)
 *    1  magic 0x53 ('S')
 *    2  protocol version, 1
 *    3  sequence number, 0-255, rolling
 *    4  flags: bit 0 GPU value valid, bit 1 host is leaving (show "no host" now)
 *    5  CPU %, 0-100
 *    6  RAM %, 0-100
 *    7  GPU %, 0-100
 *    8  disk read    rate16     10  disk write   rate16
 *   12  net download rate16     14  net upload   rate16
 *   16  host send interval in 100 ms units (10 = 1 s)
 *   17-31 reserved, 0
 * rate16: bit 15 is the unit (0 = KB/s, 1 = MB/s), bits 0-14 the value.
 *
 * Only these numbers come in; anything that does not match is ignored.
 *
 * SYSINFO view (SYS_INFO key on the SYSTEM layer). Keys by position, like
 * the pomodoro stats view; PASS keys keep their normal job:
 *     top row     page <      page >      SETUP
 *     second row  PASS (NS)   BACK        PASS (RGB)
 *     rows 3-4    PASS        PASS        PASS
 *     knob        turn: page, press: back
 * SETUP has the same key map as the pomodoro EDIT settings:
 *     SAVE    < field   CANCEL
 *     DEFAULT field >   -
 *     -1      -         +1
 *     -5      -         +5
 *     knob    turn: -1 / +1, press: next field
 * Settings are saved to EEPROM on SAVE only.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "quantum.h"

#define SYSINFO_CMD 0xC5
#define SYSINFO_MAGIC 0x53
#define SYSINFO_PROTOCOL 1

/* "No host" after this long without a packet (or 3 host intervals). */
#ifndef SYSINFO_STALE_MS
#    define SYSINFO_STALE_MS 3000
#endif

/* Samples kept per metric for the sparklines, one per packet. */
#define SYSINFO_HISTORY 128

typedef enum {
    SYS_CPU,
    SYS_RAM,
    SYS_GPU,
    SYS_DISK_READ,
    SYS_DISK_WRITE,
    SYS_NET_DOWN,
    SYS_NET_UP,
    SYS_METRICS,
} sysinfo_metric_t;

typedef struct {
    uint8_t  percent[3]; // CPU, RAM, GPU
    uint32_t kbps[4];    // disk read, disk write, net down, net up, in KB/s
    bool     gpu_valid;
} sysinfo_values_t;

/* Raw HID: call from via_command_kb(). True when the packet was ours. */
bool sysinfo_receive(const uint8_t *data, uint8_t length);

bool                    sysinfo_seen(void); // a packet came in since power-up
bool                    sysinfo_live(void); // ...and recently
const sysinfo_values_t *sysinfo_values(void);
uint8_t                 sysinfo_rx_count(void); // +1 per packet: redraw when it changes

/* History, newest first (age 0). Percent metrics hold 0-100; rates hold
 * 8 x log2(1 + KB/s), 0-200, so small and large traffic both show. */
uint8_t  sysinfo_sample(sysinfo_metric_t m, uint8_t age);
uint16_t sysinfo_sample_count(void); // samples held, up to SYSINFO_HISTORY

/* A rate in 5 chars, the unit picked to fit: " 820K", "1.24M", "12.6M",
 * " 126M", "1.24G". K = KB/s, M = MB/s, G = GB/s (1 KB = 1024 bytes).
 * out: 6 bytes. */
void sysinfo_format_rate(char *out, uint32_t kbps);

/* ---- What to show, and the strip on other layers -------------------- */

/* Items, as bits of the custom "show" set. */
typedef enum { SI_ITEM_CPU, SI_ITEM_RAM, SI_ITEM_DISK, SI_ITEM_NET, SI_ITEM_GPU, SI_ITEMS } sysinfo_item_t;
#define SI_OVERVIEW 0xFF // sysinfo_page_item() for the page with every item

/* SHOW setting: what the strip and the screensaver show (the view always
 * has every page). ALL leaves GPU out while the host does not send it. */
typedef enum { SI_SHOW_ALL, SI_SHOW_CPU, SI_SHOW_RAM, SI_SHOW_DISK, SI_SHOW_NET, SI_SHOW_GPU, SI_SHOW_CUSTOM, SI_SHOW_COUNT } sysinfo_show_t;

/* How the stats show outside the SYSINFO view:
 *   OFF    only in the view
 *   STRIP  a one-line strip on the other layers for FOR s every EVERY s
 *   SAVER  a full-screen screensaver after IDLE without a key or twist; the
 *          screen turns off SCREEN OFF later (or never) instead of after
 *          the usual screen timeout */
typedef enum { SI_STYLE_OFF, SI_STYLE_STRIP, SI_STYLE_SAVER, SI_STYLE_COUNT } sysinfo_style_t;

typedef struct {
    uint8_t show;   // sysinfo_show_t
    uint8_t custom; // item bits, for SI_SHOW_CUSTOM
    uint8_t style;  // sysinfo_style_t
    uint8_t every;  // strip period, index into the EVERY steps
    uint8_t dur_s;  // strip shown for this many seconds
    uint8_t idle;   // screensaver after, index into the IDLE steps
    uint8_t off;    // then screen off after, index into the SCREEN OFF steps
} sysinfo_config_t;

/* SETUP fields, top to bottom. STYLE and SHOW always show; SHOW chooses what
 * the strip and the screensaver show. */
typedef enum {
    SI_FIELD_STYLE,
    SI_FIELD_SHOW,
    SI_FIELD_CPU, // the five item fields only while SHOW is CUSTOM
    SI_FIELD_RAM,
    SI_FIELD_DISK,
    SI_FIELD_NET,
    SI_FIELD_GPU,
    SI_FIELD_IDLE, // IDLE and SCREEN OFF only with the SAVER style
    SI_FIELD_OFF,
    SI_FIELD_EVERY, // EVERY and FOR only with the STRIP style
    SI_FIELD_FOR,
    SI_FIELDS,
} sysinfo_field_t;

/* Key jobs in the view and in SETUP (positional maps in keymap.c). */
typedef enum {
    SI_KEY_NONE,
    SI_KEY_PASS,
    SI_KEY_BACK,
    SI_KEY_PREV,
    SI_KEY_NEXT,
    SI_KEY_SETUP,
    SI_KEY_SAVE,
    SI_KEY_CANCEL,
    SI_KEY_DEFAULT,
    SI_KEY_PREV_FIELD,
    SI_KEY_NEXT_FIELD,
    SI_KEY_DEC1,
    SI_KEY_INC1,
    SI_KEY_DEC5,
    SI_KEY_INC5,
} sysinfo_key_t;

extern const uint8_t sysinfo_view_map[MATRIX_ROWS][MATRIX_COLS];
extern const uint8_t sysinfo_setup_map[MATRIX_ROWS][MATRIX_COLS];

#ifndef SYSINFO_SETUP_TIMEOUT_MS
#    define SYSINFO_SETUP_TIMEOUT_MS 30000 // untouched SETUP closes without saving
#endif

void sysinfo_init(void); // keyboard_post_init_user, after storage_init()
void sysinfo_task(void); // housekeeping_task_user

void sysinfo_toggle_view(void); // the SYS_INFO key
bool sysinfo_view_open(void);   // view or SETUP: they own the keys and the knob
bool sysinfo_in_setup(void);

/* Call from process_record_user after the pomodoro. False when used. */
bool sysinfo_process_record(keyrecord_t *record);

uint8_t sysinfo_items(void); // item bits the strip and the screensaver show (SHOW)

/* The view always has every page, whatever SHOW says: the overviews, then
 * one page per item, CPU / RAM / GPU / DISK / NETWORK. */
uint8_t sysinfo_page(void);       // 0 .. sysinfo_page_count() - 1
uint8_t sysinfo_page_count(void); // overviews + one page per item
uint8_t sysinfo_page_item(void);  // SI_OVERVIEW or a sysinfo_item_t

/* Overview pages, k = 0 and 1: every item on one page when they fit,
 * otherwise CPU / RAM / GPU on the first and DISK / NET on the second.
 * Item bits of overview k, 0 when there is no such page. The view's hold
 * every item (the GPU while the host sends it); the screensaver's, which
 * it shows in turn, hold the items SHOW asks for. */
uint8_t sysinfo_overview_items(uint8_t k);
uint8_t sysinfo_saver_items(uint8_t k);

const sysinfo_config_t *sysinfo_config(void);       // saved settings
const sysinfo_config_t *sysinfo_setup_values(void); // values being edited
sysinfo_field_t         sysinfo_setup_field(void);
bool                    sysinfo_field_shown(sysinfo_field_t f); // see the field list
uint16_t                sysinfo_every_seconds(uint8_t index);
uint16_t                sysinfo_idle_seconds(uint8_t index);
uint8_t                 sysinfo_off_minutes(uint8_t index); // 0 = never

/* The strip on the other layers is due now: STRIP style, host live, and
 * inside the "for M s" part of the "every N s" period. */
bool sysinfo_strip_visible(void);

/* While the strip shows n groups, the one due now: the FOR time is shared
 * out evenly, so each group shows once per strip. */
uint8_t sysinfo_strip_slot(uint8_t n);

/* The screensaver is due: SAVER style, host live, and no key or twist for
 * idle_ms of at least the IDLE setting. */
bool sysinfo_saver_due(uint32_t idle_ms);

/* After how much input idle time the screen turns off: with the SAVER
 * style and a live host, IDLE + SCREEN OFF (0 = never); otherwise
 * fallback (the usual screen timeout). */
uint32_t sysinfo_screen_off_ms(uint32_t fallback);
