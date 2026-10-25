// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Every OLED screen: the layer screens with their key legends, the pomodoro
 * clock and its views, the calculator, SYSINFO and its screensaver, the
 * boot confirm and flash mode.
 *
 * ui_oled_render() runs every frame. It works out which screen is due
 * (boot confirm, alert and open modes first, then the screensaver, then the
 * layer), puts everything that screen shows into a view_t, and redraws only
 * when that view differs from the last one, so an unchanged screen costs
 * nothing. It also decides when the display sleeps (OLED_TIMEOUT, or the
 * SYSINFO screensaver's own time), instead of the driver.
 *
 * Text uses the OLED driver's 6x8 font. Screens that need exact pixel
 * positions (key legends, calculator, SYSINFO) are drawn in a canvas first.
 */

#include QMK_KEYBOARD_H
#include <string.h>
#include "ui_oled.h"
#include "ui_oled_font.h"
#include "pomodoro.h"
#include "stats.h"
#include "boot_confirm.h"
#include "numpad_ops.h"
#include "sysinfo.h"
#include "hostpower.h"
#include "version.h"

/* The OLED driver keeps its font to itself (static), so the 2x calculator
 * digits read their own copy of the same font. */
#include OLED_FONT_H

#define SCREEN_W OLED_DISPLAY_WIDTH // 128 px
#define TEXT_COLS 21                // 6 px font, 126 px of text per line
#define BAR_W 118                   // progress bar fill width on the clock screen
#define MAX_DOTS 8                  // most layer dots the header capsule holds
#define MAX_PIPS 8                  // most session pips on the clock screen
#define UI_STATUS_CAPS 0x80         // internal: Caps Lock is on

enum { SCR_NONE, SCR_LAYER, SCR_POMO, SCR_EDIT, SCR_STATS, SCR_ADJUST, SCR_ALERT, SCR_BOOT, SCR_CALC, SCR_SYSINFO, SCR_SI_SETUP, SCR_SAVER, SCR_POWER, SCR_POWER_COUNT };

enum { HPF_CANCELLED = 1, HPF_CANCELLING = 2, HPF_UNCONFIRMED = 4 }; // view_t.hp_flags

#define SAVER_SHIFT_MS 30000 // the screensaver picture moves this often (burn-in)
#define SAVER_PAGE_MS 10000  // its two overview pages, if there are two, take turns this often
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
    char                calc_rows[CALC_VIEW_LINES][CALC_ROW_LEN]; // the expression lines in view
    char                calc_total[CALC_TEXT_LEN];
    uint8_t             calc_err, calc_result, calc_rounded, calc_lines, calc_first; // calc_first: the line on top
    uint8_t             si_rx, si_live, si_seen, si_strip, si_page, si_pages, si_item, si_items, si_field, si_shift;
    uint8_t             si_rows, si_slot;                               // overview rows (view, screensaver); strip group
    uint8_t             hp_note, hp_secs, hp_live, hp_action, hp_flags; // Host Power menu and countdown
    uint16_t            hp_count;
    sysinfo_config_t    si_cfg, si_saved;
} view_t;

static view_t last;
static bool   force_redraw = true;
static bool   asleep; // display off or screensaver, as of the last render

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

/* 7x7 icons as column bytes, bit 0 = top row. */

/* What a twist of the knob does, as a plain glyph at the left of the
 * header, only where it changes a value instead of the layer. Stats and
 * SYSINFO page with the twist and show "k/N" on the right instead; the
 * calculator shows a scrollbar when the twist can scroll. */
enum { KNOB_NONE, KNOB_VALUE };
static const uint8_t PROGMEM knob_icon[1][7] = {
    {0x00, 0x44, 0x44, 0x5F, 0x44, 0x44, 0x00}, // KNOB_VALUE, plus-minus: changes a value
};

/* Status, at the left of the bottom line, each always in the same place. */
static const uint8_t PROGMEM icon_caps[7]     = {0x3F, 0x27, 0x33, 0x35, 0x33, 0x27, 0x3F}; // "A" in a box
static const uint8_t PROGMEM icon_nosleep[7]  = {0x38, 0x7A, 0x79, 0x7A, 0x79, 0x28, 0x10}; // steaming cup
static const uint8_t PROGMEM icon_pc_live[7]  = {0x0F, 0x2F, 0x2F, 0x3F, 0x2F, 0x2F, 0x0F}; // monitor, screen lit
static const uint8_t PROGMEM icon_pc_stale[7] = {0x0F, 0x29, 0x29, 0x39, 0x29, 0x29, 0x0F}; // monitor, screen dark

/* ---- Drawing helpers ------------------------------------------------- */

/* One byte (8 pixels, bit 0 on top) at column x of a page. */
static void raw(uint8_t x, uint8_t page, uint8_t byte) {
    oled_write_raw_byte(byte, (uint16_t)page * SCREEN_W + x);
}

/* The same byte in columns x0..x1-1 of a page. */
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

/* A full-width line with s centred. */
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

/* A 7 px icon at x (7 blank columns when bmp is NULL), cut at the edge. */
static void icon7(uint8_t x, uint8_t page, const uint8_t *bmp) {
    for (uint8_t c = 0; c < 7 && x + c < SCREEN_W; c++)
        raw(x + c, page, bmp ? pgm_read_byte(&bmp[c]) : 0x00);
}

/* Font text from pixel x, dark on the lit header bar, up to limit. */
static uint8_t text_px_inv(uint8_t x, uint8_t page, const char *s, uint8_t limit) {
    for (; *s && x < limit; s++) {
        for (uint8_t c = 0; c < OLED_FONT_WIDTH && x < limit; c++, x++)
            raw(x, page, (uint8_t)~pgm_read_byte(&font[((uint8_t)*s - OLED_FONT_START) * OLED_FONT_WIDTH + c]));
    }
    return x;
}

/* The header, an inverted bar on page 0, laid out the same on every screen:
 *   left   only on screens where the twist does not change layer, a plain
 *          glyph: KNOB_VALUE (+-); KNOB_NONE none
 *   label  the screen's name
 *   right  with ndots > 0, a dark capsule of layer dots (this layer filled,
 *          the others rings); otherwise the text `right`, if any
 * Every byte is written once, so an unchanged header sends nothing. */
static void header(uint8_t knob, const char *label, const char *right, uint8_t ndots, uint8_t current) {
    if (ndots > MAX_DOTS) ndots = MAX_DOTS;
    uint8_t cap_w = ndots ? (uint8_t)(ndots * 7 - 2 + 8) : 0; // dots plus 4 px each side
    uint8_t cap_x = SCREEN_W - 1 - cap_w;                     // keep 1 px of bar on the right
    uint8_t rlen  = right && !ndots ? (uint8_t)strlen(right) : 0;
    uint8_t end   = ndots ? cap_x : rlen ? (uint8_t)(SCREEN_W - 2 - rlen * 6) : SCREEN_W;

    uint8_t lx = 6; // the label starts one character in
    if (knob != KNOB_NONE) {
        raw_fill(0, 4, 0, 0xFF);
        for (uint8_t c = 0; c < 7; c++)
            raw(4 + c, 0, (uint8_t)~pgm_read_byte(&knob_icon[knob - 1][c])); // dark on the lit bar
        raw_fill(11, 15, 0, 0xFF);
        lx = 15;
    } else {
        raw_fill(0, lx, 0, 0xFF);
    }
    lx = text_px_inv(lx, 0, label, end);
    raw_fill(lx, end, 0, 0xFF);
    if (rlen) {
        text_px_inv(end, 0, right, SCREEN_W - 2);
        raw_fill(SCREEN_W - 2, SCREEN_W, 0, 0xFF);
    }
    if (!ndots) return;
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

/* A 32 px tall glyph of the clock font on pages 2-5 (blank when hidden). */
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

/* ---- Text at twice the size --------------------------------------------
 * 10x14 px glyphs (the font, every pixel doubled) for the calculator's
 * value and the SYSINFO numbers: 11 px a character (10 + a 1 px gap), 6 px
 * for '.', whose glyph is cut to its two middle columns, 4 px a space.
 * The medium size, for a calculator value too wide for 2x, keeps the
 * height but doubles only glyph columns 1 and 3: 8 px a character. */
#define BIG2_W 11
#define BIG2_DOT_W 6
#define BIG2_SPACE_W 4
#define MED_W 8

static uint16_t stretch2(uint8_t b) { // a font column, every row doubled: 8 rows -> 16
    uint16_t out = 0;
    for (uint8_t i = 0; i < 8; i++)
        if (b & (1 << i)) out |= (uint16_t)(3u << (i * 2));
    return out;
}

/* Font column c of ch at twice the size: 16 px, bit 0 at the top. */
static uint16_t glyph2x(char ch, uint8_t c) {
    return stretch2(pgm_read_byte(&font[((uint8_t)ch - OLED_FONT_START) * OLED_FONT_WIDTH + c]));
}

/* Width in px of 2x text, or of the medium size. */
static uint8_t big_width(const char *s, bool medium) {
    uint16_t w = 0;
    for (; *s; s++)
        w += *s == '.' ? BIG2_DOT_W : *s == ' ' ? BIG2_SPACE_W : medium ? MED_W : BIG2_W;
    return w > SCREEN_W ? SCREEN_W : (uint8_t)w;
}

static uint8_t text2x_width(const char *s) {
    return big_width(s, false);
}

/* Font text drawn pixel by pixel from x, so it can sit anywhere. Returns
 * the x after it. */
static uint8_t text_px(uint8_t x, uint8_t page, const char *s) {
    for (; *s && x < SCREEN_W; s++) {
        for (uint8_t c = 0; c < OLED_FONT_WIDTH && x < SCREEN_W; c++, x++)
            raw(x, page, pgm_read_byte(&font[((uint8_t)*s - OLED_FONT_START) * OLED_FONT_WIDTH + c]));
    }
    return x;
}

/* ---- Canvas -------------------------------------------------------------- */

/* Pictures that need exact pixel positions (key legends, the calculator,
 * SYSINFO) are put together here, where anything can sit at any x and
 * height, then sent page by page with each byte written once, so an
 * unchanged byte costs no display traffic. All 8 pages; most screens draw
 * their bottom line straight to the display and use pages 0-6 only. */
static uint8_t canvas[8][SCREEN_W];

/* Empties pages of the canvas. */
static void cv_clear(uint8_t first_page, uint8_t last_page) {
    memset(canvas[first_page], 0, (size_t)(last_page - first_page + 1) * SCREEN_W);
}

/* ORs up to 16 pixels of column x into the canvas, bit 0 at height y. */
static void cv_col(uint8_t x, uint8_t y, uint16_t bits) {
    uint8_t  p = y >> 3;
    uint32_t v = (uint32_t)bits << (y & 7);
    for (; v && p < 8 && x < SCREEN_W; p++, v >>= 8)
        canvas[p][x] |= (uint8_t)v;
}

/* Sends pages of the canvas to the display (unchanged bytes cost nothing). */
static void cv_flush(uint8_t first_page, uint8_t last_page) {
    for (uint8_t p = first_page; p <= last_page; p++)
        for (uint8_t x = 0; x < SCREEN_W; x++)
            raw(x, p, canvas[p][x]);
}

/* Font text with its top row at y. Returns the x after it. */
static uint8_t cv_text(uint8_t x, uint8_t y, const char *s) {
    for (; *s && x < SCREEN_W; s++)
        for (uint8_t c = 0; c < OLED_FONT_WIDTH; c++, x++)
            cv_col(x, y, pgm_read_byte(&font[((uint8_t)*s - OLED_FONT_START) * OLED_FONT_WIDTH + c]));
    return x;
}

/* Font text that ends at x1 (its last column, the font's spacing, at x1-1). */
static void cv_right(uint8_t x1, uint8_t y, const char *s) {
    cv_text((uint8_t)(x1 - strlen(s) * OLED_FONT_WIDTH), y, s);
}

/* Font text centred on the screen, top row at y. */
static void cv_center(uint8_t y, const char *s) {
    cv_text((uint8_t)((SCREEN_W - strlen(s) * OLED_FONT_WIDTH) / 2), y, s);
}

/* 2x text, or the medium size, from x with its top row at y (its baseline
 * is y + 13). Returns the x after it. */
static uint8_t cv_text_big(uint8_t x, uint8_t y, const char *s, bool medium) {
    for (; *s && x < SCREEN_W; s++) {
        if (*s == ' ') {
            x += BIG2_SPACE_W;
            continue;
        }
        bool dot = *s == '.';
        for (uint8_t c = dot ? 1 : 0; c < (dot ? 3 : 5); c++) {
            uint16_t col  = glyph2x(*s, c);
            uint8_t  reps = (medium && !dot && c != 1 && c != 3) ? 1 : 2;
            for (uint8_t k = 0; k < reps; k++)
                cv_col(x++, y, col);
        }
        x += dot ? 2 : 1;
    }
    return x;
}

static uint8_t cv_text2x(uint8_t x, uint8_t y, const char *s) {
    return cv_text_big(x, y, s, false);
}

/* Turns the pixels in x0..x1-1, y0..y1 over: lit becomes dark. */
static void cv_invert(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1) {
    for (uint8_t x = x0; x < x1 && x < SCREEN_W; x++)
        for (uint8_t y = y0; y <= y1; y++)
            canvas[y >> 3][x] ^= (uint8_t)(1 << (y & 7));
}

/* Key legends sit on three fixed columns centred at x 21, 64 and 107 (the
 * middle one on the middle of the screen), so labels line up whatever
 * their length; spaces never do the aligning. */
#define LEGEND_COL(c) (21 + 43 * (c))
#define LEGEND_OP_DX 8 // numpad: a hold operator sits this far right of its digit

/* One key label (up to 6 chars) in the row at y. Numpad style: the part
 * before a space starts at the column's left edge and the hold operator
 * after it LEGEND_OP_DX further right, so digits, ENT / BKSP and
 * operators each form a straight column. Other layers: centred on the
 * column. */
static void legend_cell(uint8_t col, uint8_t y, const char *label, bool numpad) {
    char    key[7];
    uint8_t n = (uint8_t)strlen(label);
    if (n > 6) n = 6;
    memcpy(key, label, n);
    key[n] = '\0';
    if (!numpad) {
        cv_text((uint8_t)(LEGEND_COL(col) + 1 - 3 * n), y, key); // visible width 6n - 1
        return;
    }
    uint8_t x  = (uint8_t)(LEGEND_COL(col) - 6); // a "7 %" cell is 13 px wide
    char   *op = strchr(key, ' ');
    if (op) *op++ = '\0';
    cv_text(x, y, key);
    if (op) cv_text(x + LEGEND_OP_DX, y, op);
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

/* Time left in the phase in whole seconds, rounded up, so a running clock
 * never shows 00:00. */
static uint16_t remaining_secs(void) {
    return (pomodoro_remaining_ms() + 999) / 1000;
}

/* How much of the phase is used, 0..width. */
static uint8_t progress(uint8_t width) {
    uint32_t total = pomodoro_total_ms();
    if (total == 0 || pomodoro_phase() == POMO_PHASE_IDLE) return 0;
    return (uint8_t)((total - pomodoro_remaining_ms()) * width / total);
}

/* The phase's name for the clock header. */
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

/* The PC agent's icon: lit while it sends, dark once it stopped, none
 * before it was ever seen. */
static const uint8_t *pc_icon(const view_t *v) {
    return v->si_live ? icon_pc_live : v->si_seen ? icon_pc_stale : NULL;
}

/* The bottom line of the main screens, the same everywhere:
 *   left   status icons, each always in its place: Caps Lock, No-Sleep,
 *          the PC agent (see pc_icon)
 *   right  while a pomodoro phase is loaded: its name, play or pause, and
 *          the time left
 * dx (0-3) moves it right, for the screensaver. For the first seconds
 * after power-up it shows the firmware build stamp instead. */
static void draw_status_strip(const view_t *v, uint8_t dx) {
    if (v->splash) {
        char stamp[TEXT_COLS + 1];
        firmware_stamp(stamp);
        line_center(7, stamp, false);
        return;
    }
    raw_fill(0, dx, 7, 0x00);
    icon7(dx, 7, (v->status & UI_STATUS_CAPS) ? icon_caps : NULL);
    raw_fill(dx + 7, dx + 11, 7, 0x00);
    icon7(dx + 11, 7, (v->status & UI_STATUS_NOSLEEP) ? icon_nosleep : NULL);
    raw_fill(dx + 18, dx + 22, 7, 0x00);
    icon7(dx + 22, 7, pc_icon(v));

    uint8_t x = dx + 29;
    if (v->phase != POMO_PHASE_IDLE) {
        static const char *const words[] = {"", "FOCUS ", "BREAK ", " LONG "};
        char                     mmss[6];
        fmt_mmss(mmss, v->secs, !v->blink);
        mmss[5] = '\0';
        raw_fill(x, 52 + dx, 7, 0x00);
        x                   = text_px(52 + dx, 7, words[v->phase]);
        const uint8_t *icon = v->state == POMO_PAUSED ? icon_pause : icon_play;
        for (uint8_t c = 0; c < 6; c++, x++)
            raw(x, 7, pgm_read_byte(&icon[c]));
        x = text_px(x, 7, mmss);
    }
    raw_fill(x, SCREEN_W, 7, 0x00);
}

static void draw_si_strip(const view_t *v);

/* A main layer: header with name and layer dots; the line under it (the
 * SYSINFO strip, or RGB on SYSTEM); the key legend; the pomodoro dots
 * while a phase is loaded; the bottom line. */
static void draw_layer(const view_t *v, const ui_layer_legend_t *legend, uint8_t layer_count) {
    char name[12] = "LAYER ";
    if (legend) {
        strncpy(name, legend->name, sizeof name - 1);
        name[sizeof name - 1] = '\0';
    } else {
        name[6] = '0' + v->layer % 10;
        name[7] = '\0';
    }
    header(KNOB_NONE, name, NULL, layer_count, v->layer);

    // Under the header: the SYSINFO strip when due, the RGB line on SYSTEM,
    // empty elsewhere.
    if (v->si_strip) {
        draw_si_strip(v);
    } else if (v->status & UI_STATUS_RGB_INFO) {
        char info[TEXT_COLS + 1] = "RGB ON  MODE      00%";
        if (!v->rgb_on) memcpy(info + 4, "OFF", 3);
        fmt_num(info + 13, v->rgb_mode, 3);
        fmt_num(info + 17, (uint16_t)v->rgb_val * 100 / 255, 3);
        line_text(1, info, false);
    } else {
        line_text(1, "", false);
    }

    // Key legend: 4 rows of 3 keys on the fixed columns, one text line apart
    // (pages 2-5; the strip line above and the pomodoro dots below keep
    // their places).
    cv_clear(2, 5);
    for (uint8_t r = 0; legend && r < 4; r++) {
        for (uint8_t c = 0; c < 3; c++) {
            const char *label = legend->keys[r * 3 + c];
            if (!label) continue;
            if (label[0] == UI_LEGEND_POMO_TOGGLE[0]) {
                label = (v->phase == POMO_PHASE_IDLE || v->ready) ? "START" : paused_mid(v) ? "RESUME" : "PAUSE";
            } else if (label[0] == UI_LEGEND_NOSLEEP[0]) {
                label = (v->status & UI_STATUS_NOSLEEP) ? "NS ON" : "NS OFF";
            }
            legend_cell(c, (uint8_t)(16 + r * 8), label, legend->numpad);
        }
    }
    cv_flush(2, 5);

    // Above the bottom line: the progress dots while a phase is loaded.
    if (v->phase != POMO_PHASE_IDLE && !v->splash) {
        progress_dots(6, v->bar, v->state == POMO_PAUSED, !v->blink); // ring on while the colon shows
    } else {
        line_text(6, "", false);
    }

    draw_status_strip(v, 0);
}

/* Line under the clock header: square session pips and "k/N" on the left;
 * AUTO while auto start is on, then the No-Sleep and PC agent icons (as on
 * the bottom line of the other screens), on the right. k is the focus
 * session in progress, or the one just finished. */
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
    text(col, 1, count, 14 - col, false);              // up to col 13
    text(14, 1, v->autostart ? "AUTO" : "", 4, false); // cols 14-17, x 84-107
    raw_fill(108, 110, 1, 0x00);
    icon7(110, 1, (v->status & UI_STATUS_NOSLEEP) ? icon_nosleep : NULL);
    raw_fill(117, 120, 1, 0x00);
    icon7(120, 1, pc_icon(v));
    raw_fill(127, SCREEN_W, 1, 0x00);
}

/* Hint while the next phase waits and the one that just reached its set
 * time can still be re-opened with ADJUST. Exactly 21 characters. */
#define HINT_START_OR_ADJUST "START go  ADJUST time"

/* The words of a pomodoro toast. */
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

/* A phase's short name for one-line hints. */
static const char *short_name(pomodoro_phase_t p) {
    return p == POMO_PHASE_SHORT_BREAK ? "BREAK" : p == POMO_PHASE_LONG_BREAK ? "LONG" : "FOCUS";
}

/* The pomodoro clock: phase header, session squares, the big time, a bar,
 * and a hint line (a toast, what START does, or what comes next). */
static void draw_pomo(const view_t *v, uint8_t layer_count) {
    header(KNOB_NONE, paused_mid(v) ? "PAUSED" : phase_name(v->phase), NULL, layer_count, v->layer);
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

/* Pomodoro settings: four fields, the selected one inverted, '*' on an
 * unsaved change, the selected field's range below. */
static void draw_edit(const view_t *v) {
    static const char *const names[POMO_FIELD_COUNT]  = {"FOCUS", "SHORT BREAK", "LONG BREAK", "SESSIONS"};
    static const char *const ranges[POMO_FIELD_COUNT] = {"range 1-90 min", "range 1-30 min", "range 1-60 min", "range 1-8"};
    header(KNOB_VALUE, "EDIT SETTINGS", NULL, 0, 0);
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

/* Pomodoro stats: one page per scope, and the hold-to-clear bar. */
static void draw_stats(const view_t *v) {
    static const char *const titles[POMO_STATS_PAGES] = {"Current session", "Since stats reset", "Lifetime"};
    static const char *const notes[POMO_STATS_PAGES]  = {"since power-up", "saved at phase end", "saved at phase end"};
    char                     val[16];

    char page[8] = "";
    append_num(page, v->page + 1);
    strcat(page, "/");
    append_num(page, POMO_STATS_PAGES);
    header(KNOB_NONE, "STATS", page, 0, 0);
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

/* Phase-end alert, full screen (it flashes for its first 3 s). */
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
    header(KNOB_VALUE, "ADJUST TIME", NULL, 0, 0);
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

/* ---- SYSINFO ------------------------------------------------------------ */

static const char *const si_names[SI_ITEMS] = {"CPU", "RAM", "DISK", "NET", "GPU"};

/* One grid for every SYSINFO picture:
 *   across  labels from x0, charts from x0 + SI_CHART to x0 + SI_CHART_END,
 *           values right-aligned to x0 + SI_RIGHT; x0 is SI_X0 in the
 *           view and 0-3 in the screensaver
 *   down    rows SI_LINE apart, a blank row between items; in the view the
 *           first at SI_TOP, so the gaps under the header and above the
 *           bottom line come out even
 * A bar or line chart fills its row the way the text does: the line moves
 * between the top of the capitals (100 %) and the baseline (0 %), the bar
 * sits on the baseline. */
#define SI_X0 2
#define SI_CHART 30
#define SI_CHART_END 96
#define SI_RIGHT 125
#define SI_LINE 8
#define SI_TOP 12
#define SI_BOTTOM 48 // lowest row top that keeps a gap above the bottom line

/* s without its leading spaces. */
static const char *si_trim(const char *s) {
    while (*s == ' ')
        s++;
    return s;
}

/* A percentage item's value, -1 for a GPU the host does not send. */
static int8_t si_percent(sysinfo_item_t it) {
    const sysinfo_values_t *s = sysinfo_values();
    if (it == SI_ITEM_GPU) return s->gpu_valid ? (int8_t)s->percent[2] : -1;
    return (int8_t)s->percent[it == SI_ITEM_CPU ? 0 : 1];
}

/* "34%", or "--%" without a value. out: 5 bytes. */
static void si_percent_text(char *out, int8_t pct) {
    out[0] = '\0';
    if (pct < 0) {
        strcpy(out, "--");
    } else {
        append_num(out, (uint8_t)pct);
    }
    strcat(out, "%");
}

/* A rate as a number and its unit: "12.6" and "MB/s". 6 bytes each. */
static void si_rate_parts(char *num, char *unit, uint32_t kbps) {
    char r[6];
    sysinfo_format_rate(r, kbps);
    const char *t = si_trim(r);
    uint8_t     n = (uint8_t)strlen(t);
    memcpy(num, t, n - 1);
    num[n - 1] = '\0';
    unit[0]    = t[n - 1];
    strcpy(unit + 1, "B/s");
}

/* A bar from x0 to x1 in the row at y: 4 px tall up to pct, a 1 px line on
 * the baseline for the rest (all of it when pct < 0). */
static void si_bar(uint8_t x0, uint8_t x1, uint8_t y, int8_t pct) {
    uint8_t w    = x1 - x0;
    uint8_t fill = pct > 0 ? (uint8_t)(((uint16_t)pct * w + 50) / 100) : 0;
    for (uint8_t i = 0; i < w; i++)
        cv_col(x0 + i, y + 3, i < fill ? 0x0F : 0x08);
}

/* Height of a CPU sample in its row: 6 (the baseline) at 0 %, 0 at 100 %. */
static uint8_t si_line_row(uint8_t age) {
    return (uint8_t)(6 - ((uint16_t)sysinfo_sample(SYS_CPU, age) * 6 + 50) / 100);
}

/* CPU history as a thin line from x0 to x1 in the row at y, one column per
 * sample, newest on the right. Where neighbours are more than a pixel
 * apart, the pixels between are shared out between the two columns, so
 * the line stays unbroken. Columns without a sample yet stay dark. */
static void si_line(uint8_t x0, uint8_t x1, uint8_t y) {
    uint16_t have = sysinfo_sample_count();
    for (uint8_t i = 0; i < x1 - x0 && i < have; i++) {
        uint8_t r = si_line_row(i), lo = r, hi = r;
        for (uint8_t older = 0; older < 2; older++) { // the newer, then the older neighbour
            if (older ? i + 1 >= have : i == 0) continue;
            uint8_t n    = si_line_row(older ? i + 1 : i - 1);
            uint8_t d    = n > r ? n - r : r - n;
            uint8_t gap  = d > 1 ? d - 1 : 0;
            uint8_t take = older ? gap / 2 : (gap + 1) / 2; // the older column of a pair takes the odd pixel
            if (n < r && r - take < lo) lo = r - take;
            if (n > r && r + take > hi) hi = r + take;
        }
        cv_col(x1 - 1 - i, y + lo, (uint16_t)((1 << (hi - lo + 1)) - 1));
    }
}

/* One overview item in the row at y, on the grid moved right by x0, and
 * the rows it took:
 *   CPU     _/\_/\__      34%   line chart, one column per sample
 *   RAM     ###------     61%   bar (GPU the same)
 *   DISK    R       12.6 MB/s   read, and write on the next row
 *           W        820 KB/s   (NET: D down, U up) */
static uint8_t si_row(uint8_t y, sysinfo_item_t it, uint8_t x0) {
    char val[12];
    cv_text(x0, y, si_names[it]);
    if (it == SI_ITEM_DISK || it == SI_ITEM_NET) {
        static const char *const sub[2][2] = {{"R", "W"}, {"D", "U"}};
        uint8_t                  a         = it == SI_ITEM_DISK ? 0 : 2; // kbps[] index of read / down
        for (uint8_t k = 0; k < 2; k++) {
            char unit[6];
            si_rate_parts(val, unit, sysinfo_values()->kbps[a + k]);
            strcat(val, " ");
            strcat(val, unit);
            cv_text(x0 + SI_CHART, y + k * SI_LINE, sub[a / 2][k]);
            cv_right(x0 + SI_RIGHT, y + k * SI_LINE, val);
        }
        return 2;
    }
    int8_t pct = si_percent(it);
    if (it == SI_ITEM_CPU) {
        si_line(x0 + SI_CHART, x0 + SI_CHART_END, y);
    } else {
        si_bar(x0 + SI_CHART, x0 + SI_CHART_END, y, pct);
    }
    si_percent_text(val, pct);
    cv_right(x0 + SI_RIGHT, y, val);
    return 1;
}

/* The items in `rows` from the row at y down, a blank row between them.
 * An item that would come too close to the bottom line is left out. */
static void draw_si_rows(uint8_t rows, uint8_t y, uint8_t x0) {
    static const uint8_t order[SI_ITEMS] = {SI_ITEM_CPU, SI_ITEM_RAM, SI_ITEM_GPU, SI_ITEM_DISK, SI_ITEM_NET};
    for (uint8_t i = 0; i < SI_ITEMS; i++) {
        uint8_t it = order[i], h = (it == SI_ITEM_DISK || it == SI_ITEM_NET) ? 2 : 1;
        if (!(rows & (1 << it)) || y + (h - 1) * SI_LINE > SI_BOTTOM) continue;
        y += (uint8_t)((si_row(y, (sysinfo_item_t)it, x0) + 1) * SI_LINE);
    }
}

/* Single page for CPU, RAM or GPU: the number big in the middle, a bar
 * across the screen under it, and for CPU its history as a line. */
static void draw_si_percent(sysinfo_item_t it) {
    int8_t pct = si_percent(it);
    if (pct < 0) {
        cv_center(16, "no GPU data");
        cv_center(28, "gpu = nvidia in");
        cv_center(36, "the agent settings");
        return;
    }
    uint8_t d[3], n = 0, rest = (uint8_t)pct;
    do {
        d[n++] = rest % 10;
        rest /= 10;
    } while (rest);
    uint8_t x = (uint8_t)((SCREEN_W - (n * (BIG_DIGIT_W + 2) + text2x_width("%"))) / 2);
    for (uint8_t i = 0; i < n; i++, x += BIG_DIGIT_W + 2)
        for (uint8_t p = 0; p < BIG_PAGES; p++)
            for (uint8_t c = 0; c < BIG_DIGIT_W; c++)
                cv_col(x + c, 8 + p * 8, pgm_read_byte(&big_digit[d[n - 1 - i]][p][c])); // drawn y 9-38
    cv_text2x(x, 25, "%");                                                               // on the digits' baseline
    si_bar(SI_X0, SI_X0 + SI_RIGHT - 1, 38, pct);
    if (it == SI_ITEM_CPU) si_line(SI_X0, SI_X0 + SI_RIGHT - 1, 47);
}

/* Single page for disk or network: each rate big on its own row, its name
 * at the left on the same baseline. */
static void draw_si_rates(sysinfo_item_t it) {
    static const char *const names[2][2] = {{"READ", "WRITE"}, {"DOWN", "UP"}};
    uint8_t                  a           = it == SI_ITEM_DISK ? 0 : 2; // kbps[] index of read / down
    for (uint8_t k = 0; k < 2; k++) {
        uint8_t y = k ? 35 : 13; // top of the 2x digits; label and unit start 7 lower
        char    num[6], unit[6];
        si_rate_parts(num, unit, sysinfo_values()->kbps[a + k]);
        uint8_t ux = (uint8_t)(SI_X0 + SI_RIGHT - strlen(unit) * OLED_FONT_WIDTH);
        cv_text(SI_X0, y + 7, names[a / 2][k]);
        cv_text2x((uint8_t)(ux - 3 - text2x_width(num)), y, num);
        cv_text(ux, y + 7, unit);
    }
}

/* The SYSINFO view: header with the page's name and k/N, the page, the
 * usual bottom line. Without a host, a plain message on every page. */
static void draw_sysinfo(const view_t *v) {
    cv_clear(1, 6);
    if (!v->si_live) {
        header(KNOB_NONE, "SYSINFO", NULL, 0, 0);
        cv_text2x((uint8_t)((SCREEN_W - text2x_width("NO HOST")) / 2), 17, "NO HOST");
        cv_center(40, "start the PC agent");
    } else {
        sysinfo_item_t it       = (sysinfo_item_t)v->si_item;
        char           right[8] = "";
        if (v->si_pages > 1) {
            append_num(right, v->si_page + 1);
            strcat(right, "/");
            append_num(right, v->si_pages);
        }
        header(KNOB_NONE, v->si_item == SI_OVERVIEW ? "SYSINFO" : it == SI_ITEM_NET ? "NETWORK" : si_names[it], right, 0, 0);
        if (v->si_item == SI_OVERVIEW) {
            draw_si_rows(v->si_rows, SI_TOP, SI_X0);
        } else if (it == SI_ITEM_DISK || it == SI_ITEM_NET) {
            draw_si_rates(it);
        } else {
            draw_si_percent(it);
        }
    }
    cv_flush(1, 6);
    draw_status_strip(v, 0);
}

/* Screensaver: the overview on the whole screen, no header, the status
 * line at the bottom; two overview pages take turns every SAVER_PAGE_MS.
 * Every SAVER_SHIFT_MS it all moves 0-3 px right, and every fourth step a
 * row down, so no pixel stays lit in one place. */
static void draw_saver(const view_t *v) {
    uint8_t dx = v->si_shift & 3;
    cv_clear(0, 6);
    draw_si_rows(v->si_rows, SI_TOP - 8 + ((v->si_shift >> 2) & 1) * SI_LINE, dx);
    cv_flush(0, 6);
    draw_status_strip(v, dx);
}

/* The strip on page 1 of the layer screens shows one group at a time:
 * CPU and RAM, GPU, DISK, NET (those shown). */
static const uint8_t si_strip_group[4] = {(1 << SI_ITEM_CPU) | (1 << SI_ITEM_RAM), 1 << SI_ITEM_GPU, 1 << SI_ITEM_DISK, 1 << SI_ITEM_NET};

/* Bits of the strip groups with something to show. */
static uint8_t si_strip_groups(uint8_t items) {
    uint8_t g = 0;
    if (!sysinfo_values()->gpu_valid) items &= (uint8_t) ~(1 << SI_ITEM_GPU);
    for (uint8_t k = 0; k < 4; k++)
        if (items & si_strip_group[k]) g |= (uint8_t)(1 << k);
    return g;
}

/* "CPU 34%  RAM 61%", "GPU 12%", "DISK R 12.6M  W 820K", "NET D 1.2M  U 96K":
 * the group in slot si_slot. */
static void draw_si_strip(const view_t *v) {
    uint8_t groups = si_strip_groups(v->si_items), slot = v->si_slot, k;
    char    line[TEXT_COLS + 1] = "";
    for (k = 0; k < 4; k++)
        if (((groups >> k) & 1) && slot-- == 0) break;
    if (k < 2) {
        for (uint8_t it = 0; it < SI_ITEMS; it++) {
            if (!(v->si_items & si_strip_group[k] & (1 << it))) continue;
            if (line[0]) strcat(line, "  ");
            strcat(line, si_names[it]);
            strcat(line, " ");
            si_percent_text(line + strlen(line), si_percent((sysinfo_item_t)it));
        }
    } else if (k < 4) {
        const uint32_t *kbps = sysinfo_values()->kbps + (k == 2 ? 0 : 2);
        char            r[6];
        strcpy(line, k == 2 ? "DISK R " : "NET D ");
        sysinfo_format_rate(r, kbps[0]);
        strcat(line, si_trim(r));
        strcat(line, k == 2 ? "  W " : "  U ");
        sysinfo_format_rate(r, kbps[1]);
        strcat(line, si_trim(r));
    }
    line_center(1, line, false);
}

/* SETUP: the same look as the pomodoro EDIT screen. A window of five rows
 * follows the selected field; '*' marks a change not saved yet. */
static void si_value_text(char *out, const sysinfo_config_t *c, sysinfo_field_t f) {
    static const char *const show[SI_SHOW_COUNT] = {"ALL", "CPU", "RAM", "DISK", "NET", "GPU", "CUSTOM"};
    out[0]                                       = '\0';
    switch (f) {
        case SI_FIELD_SHOW:
            strcpy(out, show[c->show]);
            break;
        case SI_FIELD_STYLE: {
            static const char *const styles[SI_STYLE_COUNT] = {"OFF", "STRIP", "SAVER"};
            strcpy(out, styles[c->style]);
            break;
        }
        case SI_FIELD_IDLE: {
            uint16_t s = sysinfo_idle_seconds(c->idle);
            append_num(out, s < 60 ? s : s / 60);
            strcat(out, s < 60 ? " s" : " min");
            break;
        }
        case SI_FIELD_OFF: {
            uint8_t m = sysinfo_off_minutes(c->off);
            if (m) {
                append_num(out, m);
                strcat(out, " min");
            } else {
                strcpy(out, "never");
            }
            break;
        }
        case SI_FIELD_EVERY: {
            uint16_t s = sysinfo_every_seconds(c->every);
            append_num(out, s < 60 ? s : s / 60);
            strcat(out, s < 60 ? " s" : " min");
            break;
        }
        case SI_FIELD_FOR:
            append_num(out, c->dur_s);
            strcat(out, " s");
            break;
        default:
            strcpy(out, (c->custom >> (f - SI_FIELD_CPU)) & 1 ? "on" : "off");
            break;
    }
}

/* SYSINFO SETUP: five rows of the fields that apply, round the selected
 * one, '*' on an unsaved change, a help line for the field. */
static void draw_si_setup(const view_t *v) {
    static const char *const names[SI_FIELDS]           = {"SHOW", " CPU", " RAM", " DISK", " NET", " GPU", "STYLE", "IDLE", "SCREEN OFF", "EVERY", "FOR"};
    static const char *const help[SI_FIELDS]            = {"what the view shows", "custom: show CPU", "custom: show RAM", "custom: show disk", "custom: show network", "custom: show GPU", "", "screensaver after", "then the screen off", "how often the strip", "how long it stays"};
    static const char *const style_help[SI_STYLE_COUNT] = {"only in this view", "strip on other layers", "full screen when idle"};
    header(KNOB_VALUE, "SYSINFO SETUP", NULL, 0, 0);

    // Visible fields, and the window of 5 rows round the selected one.
    uint8_t list[SI_FIELDS], n = 0, sel = 0;
    for (uint8_t f = 0; f < SI_FIELDS; f++) {
        if (!sysinfo_field_shown((sysinfo_field_t)f)) continue;
        if (f == v->si_field) sel = n;
        list[n++] = f;
    }
    uint8_t first = sel > 2 ? sel - 2 : 0;
    if (first + 5 > n) first = n > 5 ? n - 5 : 0;
    for (uint8_t r = 0; r < 5; r++) {
        char row[TEXT_COLS + 1], val[12], was[12];
        memset(row, ' ', TEXT_COLS);
        row[TEXT_COLS] = '\0';
        if (first + r >= n) {
            line_text(1 + r, "", false);
            continue;
        }
        sysinfo_field_t f = (sysinfo_field_t)list[first + r];
        if (f == v->si_field) row[0] = '\x10';
        memcpy(row + 1, names[f], strlen(names[f]));
        si_value_text(val, &v->si_cfg, f);
        si_value_text(was, &v->si_saved, f);
        memcpy(row + 19 - strlen(val), val, strlen(val));
        if (strcmp(val, was) != 0) row[19] = '*';
        line_text(1 + r, row, f == v->si_field);
    }
    line_center(6, v->si_field == SI_FIELD_STYLE ? style_help[v->si_cfg.style] : help[v->si_field], false);
    line_center(7, "TSKMGR save BOOT esc", false);
}

/* Calculator mode gets the whole screen (no layer dots, no bottom line):
 *   CALC header
 *   up to CALC_VIEW_LINES display lines of the expression in the normal
 *   small font, left-aligned; a new line starts before each operator
 *   (calc_display_lines)
 *   a thin divider; on an error the reason in front of it
 *   the total row: "=" always at the left ("≈" when the value had to be
 *   rounded to fit 10 digits), the live value right-aligned in 2x, boxed
 *   once "=" made it a result. A value too wide for 2x next to the "="
 *   steps down to the medium size; the widest one the calculator makes
 *   ("-123456789.1") is 94 px there, so it is never cut off.
 * A scrollbar beside the expression appears when it has more lines than
 * the screen shows: the knob scrolls them, and the thumb shows which are in
 * view. */
#define CALC_X0 2
#define CALC_ROW0 10    // top of the first expression line
#define CALC_PITCH 9    // between expression lines
#define CALC_RULE 40    // the divider
#define CALC_REASON 37  // top of an error's reason, in front of the divider
#define CALC_TOTAL 47   // top of the total row
#define CALC_VALUE_X 16 // the value starts no further left: 4 px clear of the "="
#define CALC_BAR_X 125  // scrollbar, 2 px wide, beside the expression
#define CALC_BAR_H (CALC_PITCH * (CALC_VIEW_LINES - 1) + 7)
_Static_assert((CALC_DIGITS + 1) * MED_W + BIG2_DOT_W <= SCREEN_W - CALC_VALUE_X, "the longest value must fit in the medium size");

/* "≈", which the font does not have: two waves. */
static const uint8_t PROGMEM approx_glyph[5] = {0x24, 0x12, 0x24, 0x48, 0x24};

/* The calculator's scrollbar: a dotted track and a thumb as tall as the
 * share of lines in view, placed by the first line in view. */
static void calc_scrollbar(uint8_t lines, uint8_t first) {
    uint8_t len = (uint8_t)(CALC_BAR_H * CALC_VIEW_LINES / lines);
    if (len < 4) len = 4;
    uint8_t top = (uint8_t)(CALC_ROW0 + (CALC_BAR_H - len) * first / (lines - CALC_VIEW_LINES));
    for (uint8_t y = CALC_ROW0; y < CALC_ROW0 + CALC_BAR_H; y += 2)
        cv_col(CALC_BAR_X + 1, y, 1); // dotted track
    for (uint8_t y = top; y < top + len; y++) {
        cv_col(CALC_BAR_X, y, 1);
        cv_col(CALC_BAR_X + 1, y, 1);
    }
}

static void draw_calc(const view_t *v) {
    header(KNOB_NONE, "CALC", NULL, 0, 0);
    cv_clear(1, 7);
    for (uint8_t i = 0; i < CALC_VIEW_LINES; i++)
        cv_text(CALC_X0, (uint8_t)(CALC_ROW0 + i * CALC_PITCH), v->calc_rows[i]);

    // The divider, after an error's reason if there is one.
    uint8_t x = CALC_X0;
    if (v->calc_err) x = cv_text(CALC_X0, CALC_REASON, v->calc_err == CALC_ERR_DIV_ZERO ? "! divide by 0" : "! too big") + 2;
    for (; x < SCREEN_W - 2; x++)
        cv_col(x, CALC_RULE, 1);

    // The total row: "=" (or "≈") at the left, the value right-aligned.
    if (v->calc_rounded) {
        for (uint8_t c = 0; c < 5; c++) {
            uint16_t col = stretch2(pgm_read_byte(&approx_glyph[c]));
            cv_col(CALC_X0 + 2 * c, CALC_TOTAL, col);
            cv_col(CALC_X0 + 2 * c + 1, CALC_TOTAL, col);
        }
    } else {
        cv_text2x(CALC_X0, CALC_TOTAL, "=");
    }
    const char *t      = v->calc_err ? "Err" : v->calc_total;
    bool        medium = big_width(t, false) > SCREEN_W - CALC_VALUE_X;
    uint8_t     tx     = (uint8_t)(SCREEN_W - big_width(t, medium));
    cv_text_big(tx, CALC_TOTAL, t, medium);
    if (v->calc_result) cv_invert(tx - 3, SCREEN_W, CALC_TOTAL - 2, 63);

    if (v->calc_lines > CALC_VIEW_LINES) calc_scrollbar(v->calc_lines, v->calc_first);
    cv_flush(1, 7);
}

/* ---- Host Power ------------------------------------------------------------ */

/* The POWER menu: the top row's keys with their numbers, as on the numpad,
 * then two status lines: whether the PC agent is there and when the menu
 * closes by itself, or what the PC answered. */
static void draw_power_menu(const view_t *v) {
    static const char *const label[3] = {"Shutdown", "Restart", "Cancel"};
    header(KNOB_NONE, "HOST POWER", NULL, 0, 0);
    cv_clear(1, 7);
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t y      = (uint8_t)(11 + i * 11);
        char    num[2] = {(char)('7' + i), '\0'};
        cv_text(5, y, num);
        cv_invert(3, 12, y - 1, y + 7); // the key number in a lit box
        cv_text(17, y, label[i]);
    }
    char        secs[TEXT_COLS + 1] = "Closes in ";
    const char *a = NULL, *b = NULL;
    switch (v->hp_note) {
        case HP_NOTE_ASKING:
            a = "Asking the PC...";
            break;
        case HP_NOTE_NO_REPLY:
            a = "! Host agent";
            b = "  not responding";
            break;
        case HP_NOTE_DISABLED:
            a = "! Disabled in";
            b = "  agent config";
            break;
        case HP_NOTE_ERROR:
            a = "! The PC refused it";
            b = "  see the agent log";
            break;
        default:
            a = v->hp_live ? "PC agent connected" : "PC agent not running";
            append_num(secs, v->hp_secs);
            strcat(secs, " s");
            b = secs;
            break;
    }
    cv_text(3, 45, a);
    if (b) cv_text(3, 54, b);
    cv_flush(1, 7);
}

/* The countdown, over the layer screens, the SYSINFO view and the
 * screensaver: what will happen, the seconds left in the clock digits and
 * how to stop it; then "Cancelled" for a moment once the PC confirms. The
 * keys and the twist work as usual underneath (but for the cancel key),
 * so the header keeps the layer dots. */
static void draw_power_count(const view_t *v, uint8_t layer_count) {
    bool restart = v->hp_action == HP_ACT_RESTART;
    cv_clear(1, 7);
    if (v->hp_flags & HPF_CANCELLED) {
        header(KNOB_NONE, "HOST POWER", NULL, layer_count, v->layer);
        cv_text2x((uint8_t)((SCREEN_W - text2x_width("Cancelled")) / 2), 17, "Cancelled");
        cv_center(41, "The PC stays on.");
        cv_flush(1, 7);
        return;
    }
    header(KNOB_NONE, restart ? "RESTART" : "SHUTDOWN", NULL, layer_count, v->layer);

    uint16_t left = v->hp_count > 999 ? 999 : v->hp_count;
    uint8_t  d[3], n = 0;
    do {
        d[n++] = left % 10;
        left /= 10;
    } while (left);
    uint8_t x = (uint8_t)((SCREEN_W - (n * (BIG_DIGIT_W + 2) - 2)) / 2);
    for (uint8_t i = 0; i < n; i++, x += BIG_DIGIT_W + 2)
        for (uint8_t p = 0; p < BIG_PAGES; p++)
            for (uint8_t c = 0; c < BIG_DIGIT_W; c++)
                cv_col(x + c, (uint8_t)(8 + p * 8), pgm_read_byte(&big_digit[d[n - 1 - i]][p][c])); // drawn y 9-38

    char line[TEXT_COLS + 1];
    if (v->hp_count) {
        strcpy(line, restart ? "Restarting in " : "Shutting down in ");
        append_num(line, v->hp_count);
        strcat(line, v->hp_count < 100 ? " s" : "s");
    } else {
        strcpy(line, restart ? "Restarting now" : "Shutting down now");
    }
    cv_center(43, line);
    if (v->hp_flags & HPF_CANCELLING) {
        cv_center(54, "Cancelling...");
    } else if (v->hp_flags & HPF_UNCONFIRMED) {
        cv_center(54, "! Cancel unconfirmed");
    } else if (v->hp_count) {
        cv_center(54, "Top-right key: cancel");
    }
    cv_flush(1, 7);
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

/* See the file header: pick the screen, fill its view, redraw on change. */
void ui_oled_render(uint8_t layer, uint8_t layer_count, const ui_layer_legend_t *legend, bool pomodoro_layer, uint8_t status) {
    pomodoro_alert_t alert = pomodoro_alert();

    // Sleep after OLED_TIMEOUT without input, even while the clock ticks;
    // with the SYSINFO screensaver, after its own time instead (or never).
    // Alerts and Host Power keep the display on. This gate, not the
    // driver, decides.
    uint32_t idle      = last_input_activity_elapsed();
    uint32_t off_after = sysinfo_screen_off_ms(OLED_TIMEOUT);
    bool     power     = hostpower_menu_open() || hostpower_counting() || hostpower_cancelled();
    if (alert == POMO_ALERT_NONE && !power && off_after && idle >= off_after) {
        if (is_oled_on()) {
            oled_invert(false);
            oled_off();
        }
        force_redraw = true;
        asleep       = true;
        return;
    }
    if (!is_oled_on()) force_redraw = true;
    oled_on(); // restarts the driver's own timeout; no bus traffic when already on

    view_t v;
    memset(&v, 0, sizeof v);
    if (boot_confirm_active()) {
        v.screen = SCR_BOOT;
    } else if (hostpower_menu_open()) {
        v.screen = SCR_POWER; // over an alert too: the menu has the keys
    } else if (alert != POMO_ALERT_NONE) {
        v.screen = SCR_ALERT;
    } else if (pomodoro_is_editing()) {
        v.screen = SCR_EDIT;
    } else if (pomodoro_is_viewing_stats()) {
        v.screen = SCR_STATS;
    } else if (pomodoro_is_adjusting()) {
        v.screen = SCR_ADJUST;
    } else if (sysinfo_in_setup()) {
        v.screen = SCR_SI_SETUP;
    } else if (numpad_calc_active()) {
        v.screen = SCR_CALC;
    } else if (hostpower_counting() || hostpower_cancelled()) {
        v.screen = SCR_POWER_COUNT; // over the SYSINFO view, the screensaver and the layers
    } else if (sysinfo_view_open()) {
        v.screen = SCR_SYSINFO;
    } else if (sysinfo_saver_due(idle)) {
        v.screen = SCR_SAVER; // over the layer screens only, never over a mode
    } else {
        v.screen = pomodoro_layer ? SCR_POMO : SCR_LAYER;
    }
    asleep = v.screen == SCR_SAVER; // a twist now only wakes
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
        case SCR_SI_SETUP:
            v.si_field = sysinfo_setup_field();
            v.si_cfg   = *sysinfo_setup_values();
            v.si_saved = *sysinfo_config();
            break;
        case SCR_SYSINFO:
            v.si_rx    = sysinfo_rx_count();
            v.si_seen  = sysinfo_seen();
            v.si_page  = sysinfo_page();
            v.si_pages = sysinfo_page_count();
            v.si_item  = sysinfo_page_item();
            if (v.si_item == SI_OVERVIEW) v.si_rows = sysinfo_overview_items(v.si_page);
            __attribute__((fallthrough)); // the live flag and the strip below
        case SCR_SAVER:
            if (v.screen == SCR_SAVER) {
                uint32_t now = timer_read32();
                v.si_rx      = sysinfo_rx_count();
                v.si_shift   = (uint8_t)((now / SAVER_SHIFT_MS) % 8);
                v.si_rows    = sysinfo_overview_items(sysinfo_overview_items(1) ? (uint8_t)((now / SAVER_PAGE_MS) & 1) : 0);
            }
            __attribute__((fallthrough)); // the bottom line, as on the layer screens
        case SCR_LAYER:
            v.si_live  = sysinfo_live();
            v.si_seen  = sysinfo_seen();
            v.si_items = sysinfo_items();
            if (v.screen == SCR_LAYER && sysinfo_strip_visible()) {
                v.si_strip = 1;
                v.si_rx    = sysinfo_rx_count(); // redraw with each packet
                v.si_slot  = sysinfo_strip_slot((uint8_t)__builtin_popcount(si_strip_groups(v.si_items)));
            }
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
        case SCR_CALC: {
            // The lines in view: all of them while they fit, then the newest
            // CALC_VIEW_LINES, or older ones while the knob has scrolled back.
            char    text[CALC_DISPLAY_LEN];
            uint8_t starts[CALC_WRAP_MAX + 1];
            uint8_t n     = calc_display_lines(numpad_calc(), text, starts);
            uint8_t back  = numpad_calc_scroll();
            uint8_t first = 0;
            if (n > CALC_VIEW_LINES) {
                uint8_t most = n - CALC_VIEW_LINES;
                first        = most - (back < most ? back : most);
            }
            for (uint8_t i = 0; i < CALC_VIEW_LINES && first + i < n; i++)
                calc_display_line(text, starts, first + i, v.calc_rows[i]);
            v.calc_lines   = n;
            v.calc_first   = first;
            v.calc_err     = calc_value(numpad_calc(), v.calc_total);
            v.calc_result  = calc_is_result(numpad_calc());
            v.calc_rounded = calc_value_rounded(numpad_calc());
            break;
        }
        case SCR_POMO:
            v.status  = status;
            v.si_live = sysinfo_live();
            v.si_seen = sysinfo_seen();
            v.secs    = remaining_secs();
            v.bar     = progress(BAR_W);
            v.done    = pomodoro_sessions_done();
            v.blink   = paused_mid(&v) ? (timer_read32() / 500) & 1 : 0;
            v.toast   = pomodoro_toast();
            break;
        case SCR_ADJUST:
            v.secs = remaining_secs();
            v.bar  = progress(BAR_W);
            break;
        case SCR_POWER:
            v.hp_note = hostpower_menu_note();
            v.hp_secs = hostpower_menu_secs();
            v.hp_live = sysinfo_live();
            break;
        case SCR_POWER_COUNT:
            v.hp_action = hostpower_count_action();
            v.hp_count  = hostpower_count_secs();
            v.hp_flags  = (uint8_t)((hostpower_cancelled() ? HPF_CANCELLED : 0) | (hostpower_cancelling() ? HPF_CANCELLING : 0) | (hostpower_cancel_unconfirmed() ? HPF_UNCONFIRMED : 0));
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
        case SCR_CALC:
            draw_calc(&v);
            break;
        case SCR_SYSINFO:
            draw_sysinfo(&v);
            break;
        case SCR_SI_SETUP:
            draw_si_setup(&v);
            break;
        case SCR_SAVER:
            draw_saver(&v);
            break;
        case SCR_ADJUST:
            draw_adjust(&v);
            break;
        case SCR_POWER:
            draw_power_menu(&v);
            break;
        case SCR_POWER_COUNT:
            draw_power_count(&v, layer_count);
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
            draw_stats(&v);
            break;
        case SCR_ALERT:
            draw_alert(&v);
            break;
    }
}

/* See ui_oled.h. */
bool ui_oled_asleep(void) {
    return asleep;
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
