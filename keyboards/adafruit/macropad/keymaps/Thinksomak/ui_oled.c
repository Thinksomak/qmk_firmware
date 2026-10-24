// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include <string.h>
#include "ui_oled.h"
#include "ui_oled_font.h"
#include "pomodoro.h"
#include "stats.h"
#include "boot_confirm.h"
#include "version.h"

#define SCREEN_W OLED_DISPLAY_WIDTH // 128 px
#define TEXT_COLS 21                // 6 px font, 126 px of text per line
#define BAR_W 118                   // progress bar fill width on the clock screen
#define MAX_DOTS 8                  // most layer dots the header capsule holds
#define MAX_PIPS 8                  // most session pips on the clock screen
#define UI_STATUS_CAPS 0x80         // internal: Caps Lock is on

enum { SCR_NONE, SCR_LAYER, SCR_POMO, SCR_EDIT, SCR_STATS, SCR_ADJUST, SCR_ALERT, SCR_BOOT };
enum { PIP_TODO, PIP_NOW, PIP_DONE };

_Static_assert((int)STATS_SESSION == (int)POMO_STATS_PAGE_SESSION && (int)STATS_SINCE_RESET == (int)POMO_STATS_PAGE_SINCE_RESET && (int)STATS_LIFETIME == (int)POMO_STATS_PAGE_LIFETIME, "stats pages and scopes must line up");

/* Everything a screen shows. A redraw happens only when this changes. */
typedef struct {
    uint8_t             screen, layer, state, phase, status, blink, bar, done;
    uint8_t             field, alert, rgb_on, rgb_mode, rgb_val;
    uint8_t             page, clear_px, cleared, ready, reopen, autostart, toast, boot_px, splash;
    int16_t             delta; // minutes added (+) or taken (-) by ADJUST
    uint16_t            secs;
    pomodoro_settings_t vals, saved;
    stats_values_t      stats;
} view_t;

static view_t last;
static bool   force_redraw = true;

/* Layer dots, drawn lit on the dark capsule at the right of the header. */
static const uint8_t PROGMEM dot_current[5] = {0x1C, 0x3E, 0x3E, 0x3E, 0x1C}; // filled: this layer
static const uint8_t PROGMEM dot_other[5]   = {0x1C, 0x22, 0x22, 0x22, 0x1C}; // ring: other layers

/* Session pips are squares, so they never read as layer dots. */
static const uint8_t PROGMEM pip_glyph[3][5] = {
    {0x3E, 0x22, 0x22, 0x22, 0x3E}, // still to do
    {0x3E, 0x22, 0x2A, 0x22, 0x3E}, // focus in progress
    {0x3E, 0x3E, 0x3E, 0x3E, 0x3E}, // done
};

static const uint8_t PROGMEM icon_play[6]  = {0x3E, 0x3E, 0x1C, 0x1C, 0x08, 0x00}; // right-pointing triangle
static const uint8_t PROGMEM icon_pause[6] = {0x3E, 0x3E, 0x00, 0x3E, 0x3E, 0x00}; // two vertical bars

/* ---- Drawing helpers ------------------------------------------------- */

static void raw(uint8_t x, uint8_t page, uint8_t byte) {
    oled_write_raw_byte(byte, (uint16_t)page * SCREEN_W + x);
}

static void raw_fill(uint8_t x0, uint8_t x1, uint8_t page, uint8_t byte) {
    for (uint8_t x = x0; x < x1; x++)
        raw(x, page, byte);
}

/* Writes s at (col, line), cut or space-padded to exactly width chars. */
static void text(uint8_t col, uint8_t line, const char *s, uint8_t width, bool invert) {
    char    buf[TEXT_COLS + 1];
    uint8_t n = 0;
    while (n < width && s[n]) {
        buf[n] = s[n];
        n++;
    }
    while (n < width)
        buf[n++] = ' ';
    buf[n] = '\0';
    oled_set_cursor(col, line);
    oled_write(buf, invert);
}

/* A full-width line, including the 2 px right of the last text column. */
static void line_text(uint8_t line, const char *s, bool invert) {
    text(0, line, s, TEXT_COLS, invert);
    raw_fill(TEXT_COLS * 6, SCREEN_W, line, invert ? 0xFF : 0x00);
}

static void line_center(uint8_t line, const char *s, bool invert) {
    char    buf[TEXT_COLS + 1];
    uint8_t len = strlen(s);
    if (len > TEXT_COLS) len = TEXT_COLS;
    memset(buf, ' ', TEXT_COLS);
    buf[TEXT_COLS] = '\0';
    memcpy(buf + (TEXT_COLS - len) / 2, s, len);
    line_text(line, buf, invert);
}

/* Right-aligned, space-padded decimal. Writes exactly width chars. */
static void fmt_num(char *dst, uint32_t n, uint8_t width) {
    uint8_t i = width;
    do {
        dst[--i] = '0' + n % 10;
        n /= 10;
    } while (n && i);
    while (i)
        dst[--i] = ' ';
}

/* Appends n in decimal without padding. */
static void append_num(char *dst, uint32_t n) {
    char    tmp[11];
    uint8_t i = 0;
    fmt_num(tmp, n, 10);
    tmp[10] = '\0';
    while (tmp[i] == ' ')
        i++;
    strcat(dst, tmp + i);
}

/* Focus time: "25m", "1h 05m", and whole hours from 100 h ("123h"). */
static void fmt_duration(char *dst, uint32_t minutes) {
    dst[0] = '\0';
    if (minutes < 60) {
        append_num(dst, minutes);
        strcat(dst, "m");
    } else if (minutes < 100UL * 60) {
        char mm[5] = {'h', ' ', '0' + (minutes % 60) / 10, '0' + (minutes % 60) % 10, '\0'};
        append_num(dst, minutes / 60);
        strcat(dst, mm);
        strcat(dst, "m");
    } else {
        append_num(dst, minutes / 60);
        strcat(dst, "h");
    }
}

/* "MM:SS" in 5 chars. */
static void fmt_mmss(char *dst, uint16_t secs, bool colon) {
    uint8_t m = secs / 60, s = secs % 60;
    dst[0] = '0' + m / 10;
    dst[1] = '0' + m % 10;
    dst[2] = colon ? ':' : ' ';
    dst[3] = '0' + s / 10;
    dst[4] = '0' + s % 10;
}

/* Inverted header bar on page 0. With ndots > 0, a dark capsule on the
 * right shows where this layer sits in the encoder cycle: the current
 * layer is the filled, lit dot; the others are rings. Every byte is
 * written once, so an unchanged header sends nothing to the display. */
static void header(const char *label, uint8_t ndots, uint8_t current) {
    if (ndots > MAX_DOTS) ndots = MAX_DOTS;
    uint8_t cap_w   = ndots ? (uint8_t)(ndots * 7 - 2 + 8) : 0; // dots plus 4 px each side
    uint8_t cap_x   = SCREEN_W - 1 - cap_w;                     // keep 1 px of bar on the right
    uint8_t label_w = cap_x / 6;
    if (label_w > TEXT_COLS) label_w = TEXT_COLS;

    text(0, 0, label, label_w, true);
    raw_fill(label_w * 6, cap_x, 0, 0xFF);
    for (uint8_t x = 0; x < cap_w; x++) {
        uint8_t b = 0x00; // dark capsule body
        if (x == 0 || x == cap_w - 1) {
            b = 0xC3; // rounded ends
        } else if (x == 1 || x == cap_w - 2) {
            b = 0x81;
        } else if (x >= 4) {
            uint8_t i = (x - 4) / 7, c = (x - 4) % 7;
            if (i < ndots && c < 5) b = pgm_read_byte(&(i == current ? dot_current : dot_other)[c]);
        }
        raw(cap_x + x, 0, b);
    }
    raw_fill(cap_x + cap_w, SCREEN_W, 0, 0xFF);
}

static void big_glyph(uint8_t x, const uint8_t *bitmap, uint8_t w, bool visible) {
    for (uint8_t p = 0; p < BIG_PAGES; p++) {
        for (uint8_t c = 0; c < w; c++)
            raw(x + c, 2 + p, visible ? pgm_read_byte(&bitmap[p * w + c]) : 0);
    }
}

/* Large MM:SS on pages 2-5, centred. */
static void big_time(uint16_t secs, bool visible) {
    static const uint8_t xs[4] = {17, 39, 69, 91};
    uint8_t              m = secs / 60, s = secs % 60;
    uint8_t              d[4] = {m / 10, m % 10, s / 10, s % 10};
    for (uint8_t i = 0; i < 4; i++)
        big_glyph(xs[i], &big_digit[d[i]][0][0], BIG_DIGIT_W, visible);
    big_glyph(61, &big_colon[0][0], BIG_COLON_W, visible);
}

/* Rounded progress bar on a page, fill 0..BAR_W. */
static void progress_bar(uint8_t page, uint8_t fill) {
    raw(4, page, 0x3C);
    for (uint8_t x = 0; x < BAR_W; x++)
        raw(5 + x, page, x < fill ? 0x7E : 0x42);
    raw(5 + BAR_W, page, 0x3C);
}

/* Progress bar for the layer screens: SEG_N dots across the page that fill
 * up, left to right, toward the time shown at the bottom right. Dots are
 * still to come, filled circles are used up, and the one in use grows from
 * a dot into a circle. While running, a ring around the one in use blinks
 * (beat), in step with the clock's colon, so "now" is easy to find even on
 * a long phase. While paused, the filled circles show as rings and nothing
 * blinks. used = time used, in fifths of a segment (0..SEG_N * 5). Every
 * byte is written once per redraw. */
#define SEG_N 20
#define SEG_PITCH 6 // 5 px circle + 1 px gap
#define SEG_X0 ((SCREEN_W - (SEG_N * SEG_PITCH - 1)) / 2)

static void progress_dots(uint8_t page, uint8_t used, bool paused, bool beat) {
    // clang-format off
    static const uint8_t PROGMEM stage[5][5] = {
        {0x00, 0x00, 0x08, 0x00, 0x00}, // dot: still to come
        {0x00, 0x08, 0x1C, 0x08, 0x00}, // growing ...
        {0x00, 0x1C, 0x1C, 0x1C, 0x00},
        {0x08, 0x3E, 0x3E, 0x3E, 0x08},
        {0x1C, 0x3E, 0x3E, 0x3E, 0x1C}, // ... filled circle: used up
    };
    // clang-format on
    static const uint8_t PROGMEM ring[5] = {0x1C, 0x22, 0x22, 0x22, 0x1C};
    raw_fill(0, SEG_X0, page, 0x00);
    for (uint8_t i = 0; i < SEG_N; i++) {
        uint8_t x    = SEG_X0 + i * SEG_PITCH;
        uint8_t done = used > i * 5 ? used - i * 5 : 0; // fifths of this segment used
        if (done > 5) done = 5;
        uint8_t s       = done == 5 ? 4 : done >= 3 ? 3 : done; // growth stage
        bool    working = i == used / 5;                        // the one in use now
        for (uint8_t c = 0; c < 5; c++) {
            uint8_t b = (paused && done == 5) ? pgm_read_byte(&ring[c]) : pgm_read_byte(&stage[s][c]);
            if (working && !paused && beat) b |= pgm_read_byte(&ring[c]);
            raw(x + c, page, b);
        }
        if (i + 1 < SEG_N) raw(x + 5, page, 0x00);
    }
    raw_fill(SEG_X0 + SEG_N * SEG_PITCH - 1, SCREEN_W, page, 0x00);
}

/* ---- Timer facts shared by the screens ------------------------------- */

static uint16_t remaining_secs(void) {
    return (pomodoro_remaining_ms() + 999) / 1000;
}

static uint8_t progress(uint8_t width) {
    uint32_t total = pomodoro_total_ms();
    if (total == 0 || pomodoro_phase() == POMO_PHASE_IDLE) return 0;
    return (uint8_t)((total - pomodoro_remaining_ms()) * width / total);
}

static const char *phase_name(pomodoro_phase_t p) {
    switch (p) {
        case POMO_PHASE_FOCUS:
            return "FOCUS";
        case POMO_PHASE_SHORT_BREAK:
            return "BREAK";
        case POMO_PHASE_LONG_BREAK:
            return "LONG BREAK";
        default:
            return "READY";
    }
}

/* Paused part-way through a phase, as opposed to waiting at full length. */
static bool paused_mid(const view_t *v) {
    return v->state == POMO_PAUSED && !v->ready;
}

/* ---- Screens --------------------------------------------------------- */

/* True for the first FIRMWARE_SPLASH_MS after power-up, then never again. */
#define FIRMWARE_SPLASH_MS 6000
static bool splash_on(void) {
    static bool over = false;
    if (!over && timer_read32() >= FIRMWARE_SPLASH_MS) over = true;
    return !over;
}

/* "firmware 10-24 14:32": the build stamp, so you can tell which firmware
 * is running. QMK_BUILDDATE looks like "2026-10-24-14:32:10". */
static void firmware_stamp(char *dst) {
    const char *b = QMK_BUILDDATE;
    strcpy(dst, "firmware ");
    memcpy(dst + 9, b + 5, 5); // month-day
    dst[14] = ' ';
    memcpy(dst + 15, b + 11, 5); // hour:minute
    dst[20] = '\0';
}

/* Bottom line of the layer screens: CAPS and NS on the left; while a phase
 * is loaded, its name, a play or pause icon and the time left on the right.
 * For the first seconds after power-up it shows the firmware build stamp. */
static void draw_status_strip(const view_t *v) {
    if (v->splash) {
        char stamp[TEXT_COLS + 1];
        firmware_stamp(stamp);
        line_center(7, stamp, false);
        return;
    }
    static const char *const words[]  = {"", "FOCUS", "BREAK", " LONG"};
    char                     left[10] = "         ";
    if (v->status & UI_STATUS_CAPS) memcpy(left, "CAPS", 4);
    if (v->status & UI_STATUS_NOSLEEP) memcpy(left + 5, "NS", 2);
    text(0, 7, left, 9, false);

    if (v->phase == POMO_PHASE_IDLE) {
        text(9, 7, "", 12, false);
    } else {
        char mmss[6];
        fmt_mmss(mmss, v->secs, !v->blink);
        mmss[5] = '\0';
        text(9, 7, words[v->phase], 6, false); // phase name and a space, cols 9-14
        const uint8_t *icon = v->state == POMO_PAUSED ? icon_pause : icon_play;
        for (uint8_t c = 0; c < 6; c++)
            raw(15 * 6 + c, 7, pgm_read_byte(&icon[c])); // col 15
        text(16, 7, mmss, 5, false);                     // cols 16-20
    }
    raw_fill(TEXT_COLS * 6, SCREEN_W, 7, 0x00);
}

static void draw_layer(const view_t *v, const ui_layer_legend_t *legend, uint8_t layer_count) {
    char name[TEXT_COLS + 1] = " LAYER ";
    if (legend) {
        name[1] = '\0';
        strncat(name, legend->name, TEXT_COLS - 1);
    } else {
        name[7] = '0' + v->layer % 10;
        name[8] = '\0';
    }
    header(name, layer_count, v->layer);

    // Under the header: the RGB line on SYSTEM, empty elsewhere.
    if (v->status & UI_STATUS_RGB_INFO) {
        char info[TEXT_COLS + 1] = "RGB ON  MODE      00%";
        if (!v->rgb_on) memcpy(info + 4, "OFF", 3);
        fmt_num(info + 13, v->rgb_mode, 3);
        fmt_num(info + 17, (uint16_t)v->rgb_val * 100 / 255, 3);
        line_text(1, info, false);
    } else {
        line_text(1, "", false);
    }

    // Key legend: 4 rows of 3 cells, 7 chars per cell.
    for (uint8_t r = 0; r < 4; r++) {
        char row[TEXT_COLS + 1];
        memset(row, ' ', TEXT_COLS);
        row[TEXT_COLS] = '\0';
        for (uint8_t c = 0; legend && c < 3; c++) {
            const char *label = legend->keys[r * 3 + c];
            if (!label) continue;
            if (label[0] == UI_LEGEND_POMO_TOGGLE[0]) {
                label = (v->phase == POMO_PHASE_IDLE || v->ready) ? "START" : paused_mid(v) ? "RESUME" : "PAUSE";
            } else if (label[0] == UI_LEGEND_NOSLEEP[0]) {
                label = (v->status & UI_STATUS_NOSLEEP) ? "NS ON" : "NS OFF";
            }
            uint8_t len = strlen(label);
            if (len > 6) len = 6;
            memcpy(row + c * 7 + (7 - len + 1) / 2, label, len);
        }
        line_text(2 + r, row, false);
    }

    // Above the bottom line: the progress dots while a phase is loaded.
    if (v->phase != POMO_PHASE_IDLE && !v->splash) {
        progress_dots(6, v->bar, v->state == POMO_PAUSED, !v->blink); // ring on while the colon shows
    } else {
        line_text(6, "", false);
    }

    draw_status_strip(v);
}

/* Line under the clock header: square session pips and "k/N" on the left;
 * AUTO while auto start is on and NS while No-Sleep is on, on the right.
 * k is the focus session in progress, or the one just finished. */
static void session_line(const view_t *v) {
    uint8_t n    = v->vals.val[POMO_FIELD_SESSIONS];
    uint8_t done = v->done;
    uint8_t k    = v->phase == POMO_PHASE_FOCUS ? done + 1 : v->phase == POMO_PHASE_IDLE ? 0 : done;
    if (n > MAX_PIPS) n = MAX_PIPS;
    if (k > n) k = n;

    raw_fill(0, 2, 1, 0x00);
    for (uint8_t i = 0; i < n; i++) {
        uint8_t kind = i < done ? PIP_DONE : (i == done && v->phase == POMO_PHASE_FOCUS) ? PIP_NOW : PIP_TODO;
        uint8_t x    = 2 + i * 7;
        for (uint8_t c = 0; c < 5; c++)
            raw(x + c, 1, pgm_read_byte(&pip_glyph[kind][c]));
        raw_fill(x + 5, x + 7, 1, 0x00);
    }
    uint8_t x_end = 2 + n * 7;
    uint8_t col   = (x_end + 4 + 5) / 6; // at least 4 px after the last pip
    raw_fill(x_end, col * 6, 1, 0x00);

    char count[8] = "";
    append_num(count, k);
    strcat(count, "/");
    append_num(count, n);
    text(col, 1, count, 14 - col, false);                                // up to col 13
    text(14, 1, v->autostart ? "AUTO" : "", 4, false);                   // cols 14-17
    text(18, 1, (v->status & UI_STATUS_NOSLEEP) ? " NS" : "", 3, false); // cols 18-20
    raw_fill(TEXT_COLS * 6, SCREEN_W, 1, 0x00);
}

/* Hint while the next phase waits and the one that just reached its set
 * time can still be re-opened with ADJUST. Exactly 21 characters. */
#define HINT_START_OR_ADJUST "START go  ADJUST time"

static const char *toast_text(uint8_t t) {
    switch (t) {
        case POMO_TOAST_AUTO_ON:
            return "auto start ON";
        case POMO_TOAST_AUTO_OFF:
            return "auto start OFF";
        case POMO_TOAST_UNDONE:
            return "undone";
        case POMO_TOAST_NO_UNDO:
            return "nothing to undo";
        case POMO_TOAST_NEW_CYCLE:
            return "new cycle";
        case POMO_TOAST_NOTHING_RUNNING:
            return "START a phase first";
        default:
            return "";
    }
}

/* "+5m" / "-2m": the change ADJUST made to the set length. */
static void append_delta(char *dst, int16_t delta) {
    strcat(dst, delta < 0 ? "-" : "+");
    append_num(dst, (uint32_t)(delta < 0 ? -delta : delta));
    strcat(dst, "m");
}

static const char *short_name(pomodoro_phase_t p) {
    return p == POMO_PHASE_SHORT_BREAK ? "BREAK" : p == POMO_PHASE_LONG_BREAK ? "LONG" : "FOCUS";
}

static void draw_pomo(const view_t *v, uint8_t layer_count) {
    char label[TEXT_COLS + 1] = " ";
    strncat(label, paused_mid(v) ? "PAUSED" : phase_name(v->phase), TEXT_COLS - 1);
    header(label, layer_count, v->layer);
    session_line(v);

    big_time(v->secs, !v->blink);
    progress_bar(6, v->bar);

    char line[TEXT_COLS + 1];
    if (v->toast) {
        line_center(7, toast_text(v->toast), false); // short confirmation, 2 s
    } else if (v->phase != POMO_PHASE_IDLE && v->ready && v->reopen) {
        line_center(7, HINT_START_OR_ADJUST, false);
    } else if (v->phase == POMO_PHASE_IDLE || v->ready) {
        line_center(7, "START to begin", false);
    } else if (paused_mid(v)) {
        strcpy(line, "START resumes ");
        strcat(line, short_name(v->phase));
        line_center(7, line, false);
    } else if (v->delta) {
        // Adjusted time: "+5m, next BREAK".
        line[0] = '\0';
        append_delta(line, v->delta);
        strcat(line, ", next ");
        strcat(line, short_name(pomodoro_next_phase()));
        line_center(7, line, false);
    } else {
        pomodoro_phase_t next = pomodoro_next_phase();
        strcpy(line, "next: ");
        strcat(line, next == POMO_PHASE_SHORT_BREAK ? "SHORT BREAK " : next == POMO_PHASE_LONG_BREAK ? "LONG BREAK " : "FOCUS ");
        append_num(line, pomodoro_phase_minutes(next));
        strcat(line, "m");
        line_center(7, line, false);
    }
}

static void draw_edit(const view_t *v) {
    static const char *const names[POMO_FIELD_COUNT]  = {"FOCUS", "SHORT BREAK", "LONG BREAK", "SESSIONS"};
    static const char *const ranges[POMO_FIELD_COUNT] = {"range 1-90 min", "range 1-30 min", "range 1-60 min", "range 1-8"};
    header(" EDIT SETTINGS", 0, 0);
    line_text(1, "", false);
    for (uint8_t f = 0; f < POMO_FIELD_COUNT; f++) {
        char row[TEXT_COLS + 1];
        memset(row, ' ', TEXT_COLS);
        row[TEXT_COLS] = '\0';
        if (f == v->field) row[0] = '\x10'; // right-pointing triangle
        memcpy(row + 1, names[f], strlen(names[f]));
        fmt_num(row + 12, v->vals.val[f], 3);
        if (f != POMO_FIELD_SESSIONS) memcpy(row + 15, " min", 4);
        if (v->vals.val[f] != v->saved.val[f]) row[19] = '*';
        line_text(2 + f, row, f == v->field);
    }
    line_center(6, ranges[v->field], false);
    line_center(7, "START:save RESET:esc", false);
}

/* "Label ........ value": label from column 1, value ends at column 19. */
static void stat_row(uint8_t line, const char *label, const char *value) {
    char    row[TEXT_COLS + 1];
    uint8_t len = strlen(value);
    if (len > TEXT_COLS - 2) len = TEXT_COLS - 2;
    memset(row, ' ', TEXT_COLS);
    row[TEXT_COLS] = '\0';
    memcpy(row + 1, label, strlen(label));
    memcpy(row + TEXT_COLS - 1 - len, value, len);
    line_text(line, row, false);
}

static void draw_stats(const view_t *v, uint8_t layer_count) {
    static const char *const titles[POMO_STATS_PAGES] = {"Current session", "Since stats reset", "Lifetime"};
    static const char *const notes[POMO_STATS_PAGES]  = {"since power-up", "saved at phase end", "saved at phase end"};
    char                     val[16];

    char label[TEXT_COLS + 1] = " STATS ";
    append_num(label, v->page + 1);
    strcat(label, "/");
    append_num(label, POMO_STATS_PAGES);
    header(label, layer_count, v->layer);
    line_center(1, titles[v->page], false);
    line_center(2, notes[v->page], false);
    line_text(3, "", false);

    fmt_duration(val, v->stats.focus_min);
    stat_row(4, "Focus time", val);
    val[0] = '\0';
    append_num(val, v->stats.sessions);
    stat_row(5, "Sessions done", val);

    if (v->page == POMO_STATS_PAGE_SESSION) {
        val[0] = '\0';
        append_num(val, v->stats.breaks);
        stat_row(6, "Breaks done", val);
    } else if (v->clear_px) {
        line_text(6, "", false);
        progress_bar(6, v->clear_px); // hold-to-clear progress
    } else if (v->cleared) {
        line_center(6, "cleared", false);
    } else if (v->page == POMO_STATS_PAGE_SINCE_RESET) {
        char hint[TEXT_COLS + 1] = "hold RESET ";
        append_num(hint, (POMODORO_CLEAR_HOLD_MS + 500) / 1000);
        strcat(hint, "s: clear");
        line_center(6, hint, false);
    } else {
        line_center(6, "never cleared", false);
    }

    // Same footer on every page, so the way out is always on screen.
    line_center(7, v->clear_px ? "keep holding..." : "< > page  STATS back", false);
}

static void draw_alert(const view_t *v) {
    bool focus_done = v->alert == POMO_ALERT_FOCUS_DONE;
    line_center(0, focus_done ? "FOCUS COMPLETE" : "BREAK OVER", true);

    char line[TEXT_COLS + 1] = "session ";
    append_num(line, focus_done ? v->done : v->done + 1);
    strcat(line, " of ");
    append_num(line, v->vals.val[POMO_FIELD_SESSIONS]);
    strcat(line, focus_done ? " done" : " next");
    line_center(1, line, false);

    big_time(v->secs, true);

    strcpy(line, v->phase == POMO_PHASE_SHORT_BREAK ? "SHORT BREAK" : phase_name(v->phase));
    strcat(line, v->ready ? " ready" : " started");
    line_center(6, line, false);
    line_center(7, v->reopen ? HINT_START_OR_ADJUST : v->ready ? "START to begin" : "any key to dismiss", false);
}

/* Adjust-time mode: the clock with the change so far; the knob does the rest. */
static void draw_adjust(const view_t *v) {
    char line[TEXT_COLS + 1];
    header(" ADJUST TIME", 0, 0);
    strcpy(line, v->phase == POMO_PHASE_SHORT_BREAK ? "SHORT BREAK" : phase_name(v->phase));
    if (v->delta) {
        strcat(line, "  ");
        append_delta(line, v->delta);
    }
    line_center(1, line, false);
    big_time(v->secs, true);
    progress_bar(6, v->bar);
    line_center(7, "turn -/+1m  press: ok", false);
}

/* Bootloader confirmation, above everything else. The bar runs down to the
 * automatic cancel. */
static void draw_boot(const view_t *v) {
    line_center(0, "ENTER BOOTLOADER?", true);
    line_text(1, "", false);
    line_center(2, "Keyboard stops until", false);
    line_center(3, "you flash or replug.", false);
    line_text(4, "", false);
    line_center(5, "bottom-right: CONFIRM", false);
    line_center(6, "any other key: cancel", false);
    line_text(7, "", false);
    progress_bar(7, v->boot_px);
}

/* ---- Entry point ----------------------------------------------------- */

void ui_oled_render(uint8_t layer, uint8_t layer_count, const ui_layer_legend_t *legend, bool pomodoro_layer, uint8_t status) {
    pomodoro_alert_t alert = pomodoro_alert();

#if OLED_TIMEOUT > 0
    // Sleep after OLED_TIMEOUT without input, even while the clock ticks.
    // Alerts wake the display. This gate, not the driver, decides.
    if (alert == POMO_ALERT_NONE && last_input_activity_elapsed() >= OLED_TIMEOUT) {
        if (is_oled_on()) {
            oled_invert(false);
            oled_off();
        }
        force_redraw = true;
        return;
    }
    if (!is_oled_on()) force_redraw = true;
    oled_on(); // restarts the driver's own timeout; no bus traffic when already on
#endif

    view_t v;
    memset(&v, 0, sizeof v);
    if (boot_confirm_active()) {
        v.screen = SCR_BOOT;
    } else if (alert != POMO_ALERT_NONE) {
        v.screen = SCR_ALERT;
    } else if (pomodoro_is_editing()) {
        v.screen = SCR_EDIT;
    } else if (pomodoro_is_viewing_stats()) {
        v.screen = SCR_STATS;
    } else if (pomodoro_is_adjusting()) {
        v.screen = SCR_ADJUST;
    } else {
        v.screen = pomodoro_layer ? SCR_POMO : SCR_LAYER;
    }
    // Screens below ALERT / EDIT only need to know paused vs. phase.
    v.state     = pomodoro_is_paused() ? POMO_PAUSED : pomodoro_state();
    v.ready     = pomodoro_is_ready();
    v.phase     = pomodoro_phase();
    v.vals      = *pomodoro_settings();
    v.layer     = layer;
    v.delta     = pomodoro_delta_minutes();
    v.reopen    = pomodoro_can_reopen();
    v.autostart = pomodoro_auto_start();

    switch (v.screen) {
        case SCR_LAYER:
            v.splash = splash_on();
            v.status = status | (host_keyboard_led_state().caps_lock ? UI_STATUS_CAPS : 0);
            if (v.phase != POMO_PHASE_IDLE) {
                v.secs  = remaining_secs();
                v.bar   = progress(SEG_N * 5);
                v.blink = v.state == POMO_PAUSED ? 0 : (timer_read32() / 500) & 1;
            }
#ifdef RGB_MATRIX_ENABLE
            if (status & UI_STATUS_RGB_INFO) {
                v.rgb_on   = rgb_matrix_is_enabled();
                v.rgb_mode = rgb_matrix_get_mode();
                v.rgb_val  = rgb_matrix_get_val();
            }
#endif
            break;
        case SCR_POMO:
            v.status = status;
            v.secs   = remaining_secs();
            v.bar    = progress(BAR_W);
            v.done   = pomodoro_sessions_done();
            v.blink  = paused_mid(&v) ? (timer_read32() / 500) & 1 : 0;
            v.toast  = pomodoro_toast();
            break;
        case SCR_ADJUST:
            v.secs = remaining_secs();
            v.bar  = progress(BAR_W);
            break;
        case SCR_BOOT:
            v.boot_px = (uint8_t)(boot_confirm_remaining_ms() * BAR_W / BOOT_CONFIRM_TIMEOUT_MS);
            break;
        case SCR_EDIT:
            v.field = pomodoro_edit_field();
            v.saved = v.vals;
            v.vals  = *pomodoro_edit_values();
            break;
        case SCR_STATS: {
            uint8_t hold = pomodoro_stats_clear_progress();
            v.page       = pomodoro_stats_page();
            v.stats      = stats_get((stats_scope_t)v.page);
            v.clear_px   = hold ? (uint8_t)(1 + (uint16_t)(hold - 1) * (BAR_W - 1) / 254) : 0;
            v.cleared    = pomodoro_stats_just_cleared();
            break;
        }
        case SCR_ALERT:
            v.alert = alert;
            v.secs  = remaining_secs();
            v.done  = pomodoro_sessions_done();
            break;
    }

    // Alert: flash the whole panel by hardware invert for the first 3 s.
    uint32_t since = v.screen == SCR_ALERT ? pomodoro_alert_elapsed() : 0;
    oled_invert(v.screen == SCR_ALERT && since < 3000 && ((since / 300) & 1));

    if (!force_redraw && memcmp(&v, &last, sizeof v) == 0) return;
    if (force_redraw || v.screen != last.screen) oled_clear();
    force_redraw = false;
    last         = v;

    switch (v.screen) {
        case SCR_LAYER:
            draw_layer(&v, legend, layer_count);
            break;
        case SCR_ADJUST:
            draw_adjust(&v);
            break;
        case SCR_BOOT:
            draw_boot(&v);
            break;
        case SCR_POMO:
            draw_pomo(&v, layer_count);
            break;
        case SCR_EDIT:
            draw_edit(&v);
            break;
        case SCR_STATS:
            draw_stats(&v, layer_count);
            break;
        case SCR_ALERT:
            draw_alert(&v);
            break;
    }
}

/* Drawn from shutdown_user() just before the jump to the bootloader. The
 * display keeps this picture while the board waits to be flashed. */
void ui_oled_flash_mode(void) {
    oled_invert(false);
    oled_clear();
    line_center(0, "FLASH MODE", true);
    line_center(2, "Copy the .uf2 file to", false);
    line_center(3, "the RPI-RP2 drive.", false);
    line_center(5, "Replug to cancel.", false);
    oled_on();
    oled_render_dirty(true);
}
