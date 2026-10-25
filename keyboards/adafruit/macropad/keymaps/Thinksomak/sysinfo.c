// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * PC stats from the host agent over Raw HID, the SYSINFO view and SETUP,
 * and the strip / screensaver timing (see sysinfo.h for the packet and the
 * keys). Drawing is in ui_oled.c.
 *
 * View state machine:
 *     closed --SYS_INFO--> view --SETUP key--> SETUP
 *     SETUP --SAVE--> view (settings written)
 *     SETUP --CANCEL, or 30 s untouched--> view (nothing saved)
 *     view --BACK, SYS_INFO or the knob press--> closed
 * In the view the knob turns the pages; in SETUP it changes the value.
 *
 * Settings are written to EEPROM only on SAVE, and only when they differ
 * from what is stored (storage_commit()).
 */

#include "sysinfo.h"
#include <string.h>
#include "timer.h"
#include "storage.h"

#define FLAG_GPU_VALID 0x01
#define FLAG_LEAVING 0x02

static sysinfo_values_t vals;
static uint8_t          hist[SYS_METRICS][SYSINFO_HISTORY];
static uint8_t          head; // newest sample
static uint16_t         count;
static uint8_t          rx;
static bool             seen, leaving;
static uint32_t         last_rx;
static uint32_t         stale_ms = SYSINFO_STALE_MS;

/* A rate16 field (see sysinfo.h) in KB/s: bit 15 set means MB/s. */
static uint32_t decode_rate(const uint8_t *p) {
    uint16_t r = (uint16_t)(p[0] | (p[1] << 8));
    uint32_t v = r & 0x7FFF;
    return (r & 0x8000) ? v * 1024 : v;
}

/* 8 x log2(1 + kbps): 3 bits of fraction after the leading one. */
static uint8_t log_scale(uint32_t kbps) {
    uint32_t x  = kbps + 1;
    uint8_t  ip = 0;
    while (x >> (ip + 1))
        ip++;
    uint8_t  frac = ip >= 3 ? (x >> (ip - 3)) & 7 : (x << (3 - ip)) & 7;
    uint16_t v    = (uint16_t)ip * 8 + frac;
    return v > 255 ? 255 : (uint8_t)v;
}

/* Takes one packet: checks the header, stores the values and one history
 * sample per metric, and sets the "no host" time from the host's send
 * interval (3 intervals, at least SYSINFO_STALE_MS). */
bool sysinfo_receive(const uint8_t *data, uint8_t length) {
    if (length < 17 || data[0] != SYSINFO_CMD || data[1] != SYSINFO_MAGIC) return false;
    if (data[2] != SYSINFO_PROTOCOL) return true; // ours, but a version this firmware does not know

    rx++;
    if (data[4] & FLAG_LEAVING) {
        leaving = true; // the host is going away: show "no host" now
        return true;
    }
    for (uint8_t i = 0; i < 3; i++)
        vals.percent[i] = data[5 + i] > 100 ? 100 : data[5 + i];
    vals.gpu_valid = data[4] & FLAG_GPU_VALID;
    if (!vals.gpu_valid) vals.percent[2] = 0;
    for (uint8_t i = 0; i < 4; i++)
        vals.kbps[i] = decode_rate(&data[8 + 2 * i]);

    uint32_t interval_ms = (uint32_t)data[16] * 100;
    stale_ms             = interval_ms * 3 > SYSINFO_STALE_MS ? interval_ms * 3 : SYSINFO_STALE_MS;

    head = (uint8_t)((head + 1) % SYSINFO_HISTORY);
    for (uint8_t i = 0; i < 3; i++)
        hist[SYS_CPU + i][head] = vals.percent[i];
    for (uint8_t i = 0; i < 4; i++)
        hist[SYS_DISK_READ + i][head] = log_scale(vals.kbps[i]);
    if (count < SYSINFO_HISTORY) count++;

    last_rx = timer_read32();
    seen    = true;
    leaving = false;
    return true;
}

/* ---- Values and history (see sysinfo.h) ------------------------------ */

bool sysinfo_seen(void) {
    return seen;
}

bool sysinfo_live(void) {
    return seen && !leaving && timer_elapsed32(last_rx) < stale_ms;
}

const sysinfo_values_t *sysinfo_values(void) {
    return &vals;
}

uint8_t sysinfo_rx_count(void) {
    return rx;
}

uint8_t sysinfo_sample(sysinfo_metric_t m, uint8_t age) {
    if (m >= SYS_METRICS || age >= count) return 0;
    return hist[m][(head + SYSINFO_HISTORY - age) % SYSINFO_HISTORY];
}

uint16_t sysinfo_sample_count(void) {
    return count;
}

/* n right-aligned in 4 chars. */
static void put4(char *out, uint32_t n) {
    for (int8_t i = 3; i >= 0; i--) {
        out[i] = (n || i == 3) ? (char)('0' + n % 10) : ' ';
        n /= 10;
    }
}

/* Three significant digits where they fit: "9.99", "99.9", then whole units. */
void sysinfo_format_rate(char *out, uint32_t kbps) {
    out[5] = '\0';
    if (kbps < 1000) {
        put4(out, kbps);
        out[4] = 'K';
        return;
    }
    uint64_t h    = ((uint64_t)kbps * 100 + 512) / 1024; // MB/s in hundredths
    char     unit = 'M';
    if (h >= 100000) { // 1000 MB/s and up
        h    = ((uint64_t)kbps * 100 + 524288) / 1048576;
        unit = 'G';
    }
    uint32_t t = (uint32_t)((h + 5) / 10); // tenths
    if (h <= 999) {
        out[0] = (char)('0' + h / 100);
        out[1] = '.';
        out[2] = (char)('0' + h / 10 % 10);
        out[3] = (char)('0' + h % 10);
    } else if (t <= 999) {
        out[0] = (char)('0' + t / 100);
        out[1] = (char)('0' + t / 10 % 10);
        out[2] = '.';
        out[3] = (char)('0' + t % 10);
    } else {
        put4(out, (uint32_t)((h + 50) / 100));
    }
    out[4] = unit;
}

/* ---- Settings ----------------------------------------------------------- */

static const uint16_t PROGMEM every_steps[] = {10, 15, 20, 30, 45, 60, 120, 180, 300, 600, 900, 1200, 1800};
#define EVERY_STEPS (sizeof every_steps / sizeof every_steps[0])
#define EVERY_DEFAULT 5 // 1:00
#define FOR_DEFAULT 5
#define FOR_MIN 2
#define FOR_MAX 30
static const uint16_t PROGMEM idle_steps[] = {15, 30, 60, 120, 300, 600, 900}; // seconds
#define IDLE_STEPS (sizeof idle_steps / sizeof idle_steps[0])
#define IDLE_DEFAULT 2                                                // 1 min
static const uint8_t PROGMEM off_steps[] = {1, 5, 10, 15, 30, 60, 0}; // minutes, 0 = never
#define OFF_STEPS (sizeof off_steps / sizeof off_steps[0])
#define OFF_DEFAULT 2 // 10 min
#define ITEMS_ALL ((1 << SI_ITEMS) - 1)
#define ITEMS_DEFAULT (ITEMS_ALL & ~(1 << SI_ITEM_GPU)) // custom set to start from

enum { VIEW_NONE, VIEW_DASH, VIEW_SETUP };

static sysinfo_config_t cfg, edit;
static uint8_t          view, page, field;
static uint32_t         touched;
static uint16_t         swallow_mask;

/* Setting steps to seconds / minutes, a bad index giving the default. */
uint16_t sysinfo_every_seconds(uint8_t index) {
    return pgm_read_word(&every_steps[index < EVERY_STEPS ? index : EVERY_DEFAULT]);
}

uint16_t sysinfo_idle_seconds(uint8_t index) {
    return pgm_read_word(&idle_steps[index < IDLE_STEPS ? index : IDLE_DEFAULT]);
}

uint8_t sysinfo_off_minutes(uint8_t index) {
    return pgm_read_byte(&off_steps[index < OFF_STEPS ? index : OFF_DEFAULT]);
}

/* A stored step byte (index + 1, 0 = default) to an index. */
static uint8_t step_index(uint8_t stored, uint8_t steps, uint8_t def) {
    return (stored && stored <= steps) ? stored - 1 : def;
}

/* The strip must end before the next one starts. */
static void clamp_for(sysinfo_config_t *c) {
    uint16_t max = sysinfo_every_seconds(c->every) - 1;
    if (max > FOR_MAX) max = FOR_MAX;
    if (c->dur_s > max) c->dur_s = (uint8_t)max;
    if (c->dur_s < FOR_MIN) c->dur_s = FOR_MIN;
}

/* Stored bytes to settings. Zero or out-of-range bytes give that field its
 * default, so a block from an older build, or a damaged field, is safe. */
void sysinfo_init(void) {
    const storage_sysinfo_t *s = storage_sysinfo();
    cfg.show                   = s->show >> 5;
    if (cfg.show >= SI_SHOW_COUNT) cfg.show = SI_SHOW_ALL;
    cfg.custom = s->show & ITEMS_ALL;
    if (!cfg.custom) cfg.custom = ITEMS_DEFAULT;
    cfg.style = s->style < SI_STYLE_COUNT ? s->style : SI_STYLE_OFF; // layout 3's "strip on" (1) is STRIP
    cfg.every = step_index(s->every, EVERY_STEPS, EVERY_DEFAULT);
    cfg.dur_s = (s->dur >= FOR_MIN && s->dur <= FOR_MAX) ? s->dur : FOR_DEFAULT;
    cfg.idle  = step_index(s->idle, IDLE_STEPS, IDLE_DEFAULT);
    cfg.off   = step_index(s->off, OFF_STEPS, OFF_DEFAULT);
    clamp_for(&cfg);
}

/* Settings to their stored form and EEPROM (written only if changed). */
static void save(const sysinfo_config_t *c) {
    storage_sysinfo_t s = {
        .show  = (uint8_t)((c->show << 5) | (c->custom & ITEMS_ALL)),
        .style = c->style,
        .every = (uint8_t)(c->every + 1),
        .dur   = c->dur_s,
        .idle  = (uint8_t)(c->idle + 1),
        .off   = (uint8_t)(c->off + 1),
    };
    storage_set_sysinfo(&s);
    storage_commit(); // writes only if something changed
}

/* The saved settings, and the copy SETUP edits. */
const sysinfo_config_t *sysinfo_config(void) {
    return &cfg;
}

const sysinfo_config_t *sysinfo_setup_values(void) {
    return &edit;
}

/* The items a SHOW setting stands for. ALL leaves the GPU out while the
 * host does not send it; an empty custom set falls back to the default. */
static uint8_t items_of(const sysinfo_config_t *c) {
    switch (c->show) {
        case SI_SHOW_ALL:
            return vals.gpu_valid ? ITEMS_ALL : ITEMS_ALL & ~(1 << SI_ITEM_GPU);
        case SI_SHOW_CUSTOM:
            return c->custom ? c->custom : ITEMS_DEFAULT;
        default:
            return (uint8_t)(1 << (c->show - SI_SHOW_CPU)); // CPU..GPU line up with the items
    }
}

/* The items shown now. */
uint8_t sysinfo_items(void) {
    return items_of(&cfg);
}

/* ---- Pages -------------------------------------------------------------- */

/* Page order: percentages first, then the rates. */
static const uint8_t PROGMEM page_order[SI_ITEMS] = {SI_ITEM_CPU, SI_ITEM_RAM, SI_ITEM_GPU, SI_ITEM_DISK, SI_ITEM_NET};
#define LOAD_ITEMS ((1 << SI_ITEM_CPU) | (1 << SI_ITEM_RAM) | (1 << SI_ITEM_GPU))
#define RATE_ITEMS ((1 << SI_ITEM_DISK) | (1 << SI_ITEM_NET))

/* Number of bits set. */
static uint8_t bit_count(uint8_t m) {
    uint8_t n = 0;
    for (; m; m &= m - 1)
        n++;
    return n;
}

/* Number of items shown. */
static uint8_t item_count(void) {
    return bit_count(sysinfo_items());
}

/* A percentage takes one row and a gap, a rate two rows and a gap, and an
 * overview holds six rows. */
uint8_t sysinfo_overview_items(uint8_t k) {
    uint8_t items = sysinfo_items(), load = items & LOAD_ITEMS, rate = items & RATE_ITEMS;
    if (2 * bit_count(load) + 3 * bit_count(rate) <= 6) return k ? 0 : items;
    return k == 0 ? load : k == 1 ? rate : 0;
}

/* Overview pages before the single pages: none with one item, else 1 or 2. */
static uint8_t overview_count(void) {
    return item_count() < 2 ? 0 : sysinfo_overview_items(1) ? 2 : 1;
}

/* Page order: the overviews, then one page per item in page_order. */
uint8_t sysinfo_page_count(void) {
    uint8_t n = item_count();
    return n > 1 ? overview_count() + n : 1;
}

/* The page shown, kept in range when the item set changes. */
uint8_t sysinfo_page(void) {
    return page % sysinfo_page_count();
}

/* What the page shows: an overview, or one item. */
uint8_t sysinfo_page_item(void) {
    uint8_t items = sysinfo_items(), p = sysinfo_page(), ov = overview_count();
    if (p < ov) return SI_OVERVIEW;
    p -= ov;
    for (uint8_t i = 0; i < SI_ITEMS; i++) {
        uint8_t it = pgm_read_byte(&page_order[i]);
        if (!(items & (1 << it))) continue;
        if (p-- == 0) return it;
    }
    return SI_OVERVIEW;
}

/* ---- View and SETUP ----------------------------------------------------- */

/* SYS_INFO key: opens the view on its first page, or closes it. */
void sysinfo_toggle_view(void) {
    view = view ? VIEW_NONE : VIEW_DASH;
    page = 0;
}

/* The view or SETUP is open. */
bool sysinfo_view_open(void) {
    return view != VIEW_NONE;
}

/* SETUP is open. */
bool sysinfo_in_setup(void) {
    return view == VIEW_SETUP;
}

/* The SETUP field selected. */
sysinfo_field_t sysinfo_setup_field(void) {
    return (sysinfo_field_t)field;
}

/* SETUP shows a field only where it applies: the custom items with SHOW
 * = CUSTOM, IDLE / SCREEN OFF with SAVER, EVERY / FOR with STRIP. */
bool sysinfo_field_shown(sysinfo_field_t f) {
    if (f >= SI_FIELD_CPU && f <= SI_FIELD_GPU) return edit.show == SI_SHOW_CUSTOM;
    if (f == SI_FIELD_IDLE || f == SI_FIELD_OFF) return edit.style == SI_STYLE_SAVER;
    if (f == SI_FIELD_EVERY || f == SI_FIELD_FOR) return edit.style == SI_STYLE_STRIP;
    return true;
}

/* A step index kept inside 0..steps-1 (times stop at the ends). */
static uint8_t clamp_step(int16_t i, uint8_t steps) {
    return (uint8_t)(i < 0 ? 0 : i >= steps ? steps - 1 : i);
}

/* Previous / next SETUP field, skipping hidden ones, wrapping round. */
static void step_field(int8_t dir) {
    do {
        field = (uint8_t)((field + SI_FIELDS + dir) % SI_FIELDS);
    } while (!sysinfo_field_shown((sysinfo_field_t)field));
}

/* DEFAULT: the selected field back to its default. */
static void set_default(void) {
    if (field == SI_FIELD_SHOW) {
        edit.show = SI_SHOW_ALL;
    } else if (field <= SI_FIELD_GPU) {
        uint8_t bit = (uint8_t)(1 << (field - SI_FIELD_CPU));
        edit.custom = (uint8_t)((edit.custom & ~bit) | (ITEMS_DEFAULT & bit));
        if (!edit.custom) edit.custom = ITEMS_DEFAULT;
    } else if (field == SI_FIELD_STYLE) {
        edit.style = SI_STYLE_OFF;
    } else if (field == SI_FIELD_IDLE) {
        edit.idle = IDLE_DEFAULT;
    } else if (field == SI_FIELD_OFF) {
        edit.off = OFF_DEFAULT;
    } else if (field == SI_FIELD_EVERY) {
        edit.every = EVERY_DEFAULT;
    } else {
        edit.dur_s = FOR_DEFAULT;
    }
    clamp_for(&edit);
}

/* -1 / +1 / -5 / +5 on the selected field. Choices (SHOW, STYLE, the
 * items) move one at a time and wrap; times move d steps and stop at the
 * ends. */
static void adjust(int8_t d) {
    int8_t sign = d > 0 ? 1 : -1;
    if (field == SI_FIELD_SHOW) {
        edit.show = (uint8_t)((edit.show + SI_SHOW_COUNT + sign) % SI_SHOW_COUNT);
    } else if (field <= SI_FIELD_GPU) {
        uint8_t bit = (uint8_t)(1 << (field - SI_FIELD_CPU));
        if ((edit.custom ^ bit) & ITEMS_ALL) edit.custom ^= bit; // never all off
    } else if (field == SI_FIELD_STYLE) {
        edit.style = (uint8_t)((edit.style + SI_STYLE_COUNT + sign) % SI_STYLE_COUNT);
    } else if (field == SI_FIELD_IDLE) {
        edit.idle = clamp_step(edit.idle + d, IDLE_STEPS);
    } else if (field == SI_FIELD_OFF) {
        edit.off = clamp_step(edit.off + d, OFF_STEPS);
    } else if (field == SI_FIELD_EVERY) {
        edit.every = clamp_step(edit.every + d, EVERY_STEPS);
    } else {
        int16_t s  = edit.dur_s + d;
        edit.dur_s = (uint8_t)(s < FOR_MIN ? FOR_MIN : s > FOR_MAX ? FOR_MAX : s);
    }
    clamp_for(&edit);
}

/* One key's job in the view or SETUP. */
static void act(sysinfo_key_t k) {
    uint8_t pages = sysinfo_page_count();
    switch (k) {
        case SI_KEY_BACK:
            view = VIEW_NONE;
            break;
        case SI_KEY_PREV:
            page = (uint8_t)((sysinfo_page() + pages - 1) % pages);
            break;
        case SI_KEY_NEXT:
            page = (uint8_t)((sysinfo_page() + 1) % pages);
            break;
        case SI_KEY_SETUP:
            edit  = cfg;
            field = SI_FIELD_SHOW;
            view  = VIEW_SETUP;
            break;
        case SI_KEY_SAVE:
            cfg = edit;
            save(&cfg);
            view = VIEW_DASH;
            page = 0;
            break;
        case SI_KEY_CANCEL:
            view = VIEW_DASH;
            break;
        case SI_KEY_DEFAULT:
            set_default();
            break;
        case SI_KEY_PREV_FIELD:
            step_field(-1);
            break;
        case SI_KEY_NEXT_FIELD:
            step_field(1);
            break;
        case SI_KEY_DEC1:
            adjust(-1);
            break;
        case SI_KEY_INC1:
            adjust(1);
            break;
        case SI_KEY_DEC5:
            adjust(-5);
            break;
        case SI_KEY_INC5:
            adjust(5);
            break;
        default:
            break;
    }
}

/* Routes keys and the knob while the view or SETUP is open, by the
 * positional maps in keymap.c. Knob turns never reach the layer change
 * while it is open. */
bool sysinfo_process_record(keyrecord_t *record) {
    keyevent_t ev  = record->event;
    bool       enc = IS_ENCODEREVENT(ev);
    uint16_t   bit = 0;
    if (!enc && ev.key.row < MATRIX_ROWS && ev.key.col < MATRIX_COLS) {
        bit = (uint16_t)1 << (ev.key.row * MATRIX_COLS + ev.key.col);
    }

    if (!ev.pressed) {
        if (swallow_mask & bit) {
            swallow_mask &= ~bit;
            return false;
        }
        return !(enc && view); // knob turns belong to the view
    }
    if (!view) return true;

    sysinfo_key_t k;
    if (enc) {
        bool cw = ev.key.row == KEYLOC_ENCODER_CW;
        k       = view == VIEW_SETUP ? (cw ? SI_KEY_INC1 : SI_KEY_DEC1) : (cw ? SI_KEY_NEXT : SI_KEY_PREV);
    } else if (bit) {
        const uint8_t *map = view == VIEW_SETUP ? &sysinfo_setup_map[ev.key.row][ev.key.col] : &sysinfo_view_map[ev.key.row][ev.key.col];
        k                  = (sysinfo_key_t)pgm_read_byte(map);
    } else {
        return true;
    }
    if (k == SI_KEY_PASS) return true;

    touched = timer_read32();
    act(k);
    swallow_mask |= bit;
    return false;
}

/* An untouched SETUP closes after SYSINFO_SETUP_TIMEOUT_MS, unsaved. */
void sysinfo_task(void) {
    if (view == VIEW_SETUP && timer_elapsed32(touched) >= SYSINFO_SETUP_TIMEOUT_MS) {
        view = VIEW_DASH; // untouched: nothing saved
    }
}

/* ---- Strip and screensaver timing (see sysinfo.h) -------------------- */

bool sysinfo_strip_visible(void) {
    if (cfg.style != SI_STYLE_STRIP || !sysinfo_live()) return false;
    uint32_t period = (uint32_t)sysinfo_every_seconds(cfg.every) * 1000;
    return timer_read32() % period < (uint32_t)cfg.dur_s * 1000;
}

/* The FOR seconds split evenly between the n groups, in order. */
uint8_t sysinfo_strip_slot(uint8_t n) {
    if (n < 2) return 0;
    uint32_t period = (uint32_t)sysinfo_every_seconds(cfg.every) * 1000;
    uint32_t in = timer_read32() % period, dur = (uint32_t)cfg.dur_s * 1000;
    uint32_t s = in * n / dur;
    return s < n ? (uint8_t)s : n - 1;
}

bool sysinfo_saver_due(uint32_t idle_ms) {
    return cfg.style == SI_STYLE_SAVER && sysinfo_live() && idle_ms >= (uint32_t)sysinfo_idle_seconds(cfg.idle) * 1000;
}

uint32_t sysinfo_screen_off_ms(uint32_t fallback) {
    if (cfg.style != SI_STYLE_SAVER || !sysinfo_live()) return fallback;
    uint8_t minutes = sysinfo_off_minutes(cfg.off);
    if (!minutes) return 0; // never
    return (uint32_t)sysinfo_idle_seconds(cfg.idle) * 1000 + (uint32_t)minutes * 60000;
}
