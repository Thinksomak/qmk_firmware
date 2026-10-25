// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Desk calculator, pure C (no QMK headers), so it can be unit tested on
 * the PC (tests/calc_test.c).
 *
 * Two parts, like a desk calculator with a paper tape:
 *   line 1  the expression exactly as typed: "4 + 5 + 3" (no "=")
 *   line 2  the value, always live: 9 after "4 + 5", 12 after "... + 3"
 *
 * Left to right, no operator precedence: "4 + 5 x 2 =" is 18.
 *   =      finishes the sum; again right away it repeats the last
 *          operation ("9 + 3 = =" is 15). A digit then starts a new sum,
 *          an operator goes on from the total.
 *   op op  a second operator right after the first replaces it
 *   %      like Windows Calculator: "200 + 10 %" makes the 10 into 20
 *          (10 % of 200), "200 x 10 %" makes it 0.1; alone, "50 %" is 0.5
 *   +/-    flips the sign of the number being given, or of a result
 *   BKSP   removes the last character of line 1: a digit, '.', '%', or a
 *          whole operator; a number whose last digit goes is removed, never
 *          left as 0. After "=" (or Err) it first reopens the sum.
 *   CE     clears the number being given, AC clears everything
 *
 * Line 1 is kept as a list of numbers and operators, and the value is
 * worked out again from that list after every key. That is what lets
 * BKSP step back over an operator to the number before it, exactly.
 *
 * Numbers are decimal fixed point (int64, 6 decimals), shown with up to
 * CALC_DIGITS digits. Divide by zero or a result of 11 or more digits
 * shows "Err" (already on line 2 while typing); the next digit starts again.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CALC_DIGITS 10                                    // digits on the display
#define CALC_DECIMALS 6                                   // fixed-point decimals kept internally
#define CALC_TEXT_LEN 16                                  // buffer for one number: "-" 10 digits "." "%" NUL
#define CALC_EXPR_BUF 96                                  // kept expression text; older text is dropped first
#define CALC_LINE_LEN (CALC_EXPR_BUF + CALC_TEXT_LEN + 2) // buffer for calc_expression()

/* Line 1 on the OLED, in the screen's normal 6 px font: CALC_CHAR_W px per
 * character, '.' CALC_DOT_W, a space CALC_SPACE_W, at most CALC_WRAP_W px
 * (20 characters) per display line. The screen shows CALC_VIEW_LINES
 * display lines; the knob scrolls the rest. */
#define CALC_CHAR_W 6
#define CALC_DOT_W 6
#define CALC_SPACE_W 6
#define CALC_WRAP_W 120
#define CALC_VIEW_LINES 3
#define CALC_WRAP_MAX 24                     // display lines line 1 can take
#define CALC_DISPLAY_LEN (CALC_LINE_LEN + 3) // line 1 with ".. " in front
#define CALC_ROW_LEN 24                      // one display line, NUL included

typedef enum {
    CALC_KEY_0,
    CALC_KEY_1,
    CALC_KEY_2,
    CALC_KEY_3,
    CALC_KEY_4,
    CALC_KEY_5,
    CALC_KEY_6,
    CALC_KEY_7,
    CALC_KEY_8,
    CALC_KEY_9,
    CALC_KEY_DOT,
    CALC_KEY_ADD,
    CALC_KEY_SUB,
    CALC_KEY_MUL,
    CALC_KEY_DIV,
    CALC_KEY_PCT,
    CALC_KEY_EQ,
    CALC_KEY_NEG,
    CALC_KEY_BKSP,
    CALC_KEY_CE,
    CALC_KEY_AC,
} calc_key_t;

typedef enum { CALC_OP_NONE, CALC_OP_ADD, CALC_OP_SUB, CALC_OP_MUL, CALC_OP_DIV } calc_op_t;

typedef enum { CALC_ERR_NONE, CALC_ERR_DIV_ZERO, CALC_ERR_OVERFLOW } calc_err_t;

/* One number or operator of line 1. A number is either typed (its text is
 * what was typed, so "0.10" stays "0.10") or given: a result reused in the
 * sum, kept exactly and shown as format_value() writes it. */
#define CALC_TOKENS 50 // numbers and operators kept; the oldest are folded first

enum { CALC_TOK_NUM, CALC_TOK_OP };
enum {
    CALC_NUM_TYPED = 0x01, // text in entry; else value
    CALC_NUM_NEG   = 0x02, // typed with a minus sign
    CALC_NUM_PCT   = 0x04, // shown with %: worth that percent (see %)
};

typedef struct {
    uint8_t kind;                   // CALC_TOK_NUM or CALC_TOK_OP
    uint8_t op;                     // an operator's calc_op_t
    uint8_t flags;                  // a number's CALC_NUM_* bits
    uint8_t len;                    // typed: characters in entry
    char    entry[CALC_DIGITS + 2]; // typed: digits and '.', no sign, NUL-terminated
    int64_t value;                  // given: the exact value
} calc_tok_t;

typedef struct {
    calc_tok_t tok[CALC_TOKENS];
    uint8_t    n;        // tokens in use
    bool       folded;   // the oldest tokens were folded into base, shown as ".."
    int64_t    base;     // their value; tok[0] is then the operator after them
    bool       after_eq; // "=" finished the sum
    calc_err_t err;      // an operator or "=" hit an error: Err until a key clears it
    calc_op_t  last_op;  // repeated "=" uses these two
    int64_t    last_operand;
} calc_t;

void calc_reset(calc_t *c);
void calc_input(calc_t *c, calc_key_t key);

/* Line 1: the expression as typed, e.g. "4 + 5 + 3". out: CALC_LINE_LEN
 * bytes. Returns true when the oldest part was folded away (shown as ".."). */
bool calc_expression(const calc_t *c, char *out);

/* Line 2: the live value ("12", "0.333333") or "Err". out: CALC_TEXT_LEN
 * bytes. Returns why it is "Err", else CALC_ERR_NONE. */
calc_err_t calc_value(const calc_t *c, char *out);

/* Line 2 had to be rounded to fit CALC_DIGITS digits: the value has more
 * decimals than it shows (1000000000 / 3 shows 333333333.3). */
bool calc_value_rounded(const calc_t *c);

/* Line 2 is a finished result: "=" finished the sum without an error. */
bool calc_is_result(const calc_t *c);

/* Width in px of a character of line 1 on the display (see CALC_CHAR_W). */
uint8_t calc_char_w(char ch);

/* Line 1 broken into display lines, like a paper tape: a display line
 * starts before an operator, so each reads "+ 678.25 x 3", and a number
 * wider than a whole line is split between characters. text gets line 1
 * (CALC_DISPLAY_LEN bytes, ".. " in front when older text was dropped);
 * starts (CALC_WRAP_MAX + 1 entries) gets where each display line begins,
 * with starts[n] = strlen(text). Returns n, 0 for an empty line 1. */
uint8_t calc_display_lines(const calc_t *c, char *text, uint8_t *starts);

/* Display line i of text, without the space that ends it. out:
 * CALC_ROW_LEN bytes. */
void calc_display_line(const char *text, const uint8_t *starts, uint8_t i, char *out);
