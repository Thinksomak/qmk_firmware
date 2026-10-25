// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * PC unit test for calc.c (pure C, no QMK). Build and run from QMK MSYS:
 *   sh keyboards/adafruit/macropad/keymaps/Thinksomak/tests/run_calc_test.sh
 *
 * Each check types keys into a fresh calculator and compares the two
 * display lines: line 1 (the expression) and line 2 (the live value).
 * Keys: 0-9 . + - * / % =, n = +/-, b = backspace, c = CE, a = AC.
 * Spaces are ignored.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../calc.h"

static int failures, checks;

static calc_key_t key_of(char ch) {
    if (ch >= '0' && ch <= '9') return (calc_key_t)(CALC_KEY_0 + (ch - '0'));
    switch (ch) {
        case '.':
            return CALC_KEY_DOT;
        case '+':
            return CALC_KEY_ADD;
        case '-':
            return CALC_KEY_SUB;
        case '*':
            return CALC_KEY_MUL;
        case '/':
            return CALC_KEY_DIV;
        case '%':
            return CALC_KEY_PCT;
        case '=':
            return CALC_KEY_EQ;
        case 'n':
            return CALC_KEY_NEG;
        case 'b':
            return CALC_KEY_BKSP;
        case 'c':
            return CALC_KEY_CE;
        default:
            return CALC_KEY_AC;
    }
}

static void type(calc_t *c, const char *keys) {
    for (const char *k = keys; *k; k++)
        if (*k != ' ') calc_input(c, key_of(*k));
}

static void check(const char *keys, const char *line1, const char *line2) {
    calc_t c;
    char   l1[CALC_LINE_LEN], l2[CALC_TEXT_LEN];
    calc_reset(&c);
    type(&c, keys);
    calc_expression(&c, l1);
    calc_value(&c, l2);
    checks++;
    if (strcmp(l1, line1) != 0 || strcmp(l2, line2) != 0) {
        failures++;
        printf("FAIL  %-30s  shows \"%s\" / \"%s\", want \"%s\" / \"%s\"\n", keys, l1, l2, line1, line2);
    }
}

/* The display: keys, then the expected display lines of the expression
 * (as calc_display_lines breaks them for the OLED's 2x font, oldest
 * first, NULL-terminated) and the value shown after "= ". */
static void check_lines(const char *keys, const char *const *want, const char *value) {
    calc_t  c;
    char    text[CALC_DISPLAY_LEN], row[CALC_ROW_LEN], v[CALC_TEXT_LEN];
    uint8_t starts[CALC_WRAP_MAX + 1], want_n = 0;
    calc_reset(&c);
    type(&c, keys);
    uint8_t n = calc_display_lines(&c, text, starts);
    calc_value(&c, v);
    while (want[want_n])
        want_n++;
    checks++;
    bool ok = n == want_n && strcmp(v, value) == 0;
    for (uint8_t i = 0; ok && i < n; i++) {
        calc_display_line(text, starts, i, row);
        ok = strcmp(row, want[i]) == 0;
    }
    if (!ok) {
        failures++;
        printf("FAIL  %s\n      got %u lines:", keys, n);
        for (uint8_t i = 0; i < n; i++) {
            calc_display_line(text, starts, i, row);
            printf(" [%s]", row);
        }
        printf("  = %s\n      want %u lines:", v, want_n);
        for (uint8_t i = 0; i < want_n; i++)
            printf(" [%s]", want[i]);
        printf("  = %s\n", value);
    }
}

#define LINES(...) ((const char *const[]){__VA_ARGS__, NULL})

static void expect(bool ok, const char *what) {
    checks++;
    if (!ok) {
        failures++;
        printf("FAIL  %s\n", what);
    }
}

int main(void) {
    // The sequences from the change request
    check("", "", "0");
    check("7", "7", "7");
    check("4 + 5", "4 + 5", "9");
    check("4 + 5 +", "4 + 5 +", "9");
    check("4 + 5 + 3", "4 + 5 + 3", "12");
    check("4 + 5 + 3 =", "4 + 5 + 3", "12");

    // Left to right, chaining on a result, repeated =
    check("4 + 5 * 2 =", "4 + 5 x 2", "18");
    check("2 + 3 = * 4 =", "5 x 4", "20");
    check("2 + 3 = * 4", "5 x 4", "20");
    check("3 - 5 =", "3 - 5", "-2");
    check("9 + 3 = =", "12 + 3", "15");
    check("9 + 3 = 7 =", "7 + 3", "10");
    check("5 =", "5", "5");
    check("5 = 3 =", "3", "3");
    check("5 + =", "5 + 5", "10");

    // Operator replacement
    check("5 + *", "5 x", "5");
    check("5 + * 2 =", "5 x 2", "10");

    // Decimals and rounding
    check("1 / 3 =", "1 / 3", "0.333333");
    check("2 / 3 =", "2 / 3", "0.666667");
    check("1 / 3 * 3 =", "1 / 3 x 3", "0.999999");
    check("0.1 + 0.2 =", "0.1 + 0.2", "0.3");
    check("2.5 * 4 =", "2.5 x 4", "10");
    check(". 5 + . 5 =", "0.5 + 0.5", "1");
    check("0 . . 5", "0.5", "0.5");
    check("0.10", "0.10", "0.1");
    check("1 . 1234567", "1.123456", "1.123456");
    check("10 / 4 =", "10 / 4", "2.5");
    check("1000000000 / 3 =", "1000000000 / 3", "333333333.3");
    check("0.000001 * 0.5 =", "0.000001 x 0.5", "0.000001");

    // Negatives
    check("n 5", "-5", "-5");
    check("n", "-0", "0");
    check("5 n * 3 =", "-5 x 3", "-15");
    check("5 + n", "5 + -5", "0");
    check("5 + n =", "5 + -5", "0");
    check("2 + 3 = n", "-5", "-5");
    check("2 + 3 = n + 1 =", "-5 + 1", "-4");

    // Percent (Windows Calculator style)
    check("200 + 10 %", "200 + 10%", "220");
    check("200 + 10 % =", "200 + 10%", "220");
    check("200 - 10 % =", "200 - 10%", "180");
    check("200 * 10 % =", "200 x 10%", "20");
    check("200 / 10 % =", "200 / 10%", "2000");
    check("50 %", "50%", "0.5");
    check("4 + 5 = %", "9%", "0.09");

    // Errors and limits: line 2 shows Err as soon as it is true
    check("7 / 0", "7 / 0", "Err");
    check("7 / 0 =", "7 / 0", "Err");
    check("7 / 0 = +", "7 / 0", "Err");
    check("7 / 0 = 5", "5", "5");
    check("7 / 0 = a", "", "0");
    check("7 / 0 = b", "7 /", "7");
    check("9999999999 + 1", "9999999999 + 1", "Err");
    check("9999999999 + 1 =", "9999999999 + 1", "Err");
    check("100000 * 100000 =", "100000 x 100000", "Err");
    check("99999 * 99999 =", "99999 x 99999", "9999800001");
    check("12345678901", "1234567890", "1234567890");
    check("1234567890 .", "1234567890", "1234567890");

    // Editing
    check("1 2 3 b", "12", "12");
    check("5 b", "", "0");
    check("4 + 5 b", "4 +", "4");
    check("12 + 34 c", "12 + 0", "12");
    check("12 + 34 c 5 =", "12 + 5", "17");
    check("12 + c =", "12 + 0", "12");
    check("12 + 3 a", "", "0");
    check("2 + 3 = b", "2 +", "2");
    check("0 0 7", "7", "7");

    // Backspace: one character (a digit, '.', '%') or one operator per
    // press, from the end; a number whose last digit goes is removed, never
    // left as 0. After "=" it reopens the sum first.
    check("1 2 3 5 b", "123", "123");
    check("1 2 b", "1", "1");
    check("1 b", "", "0");
    check("4 + 1 2 b", "4 + 1", "5");
    check("4 + 1 b", "4 +", "4");
    check("4 + 5 + b", "4 + 5", "9");
    check("4 + 5 + b b", "4 +", "4");
    check("4 + 5 =", "4 + 5", "9");
    check("4 + 5 + 3 5 = b", "4 + 5 + 3", "12");
    check("3 . 7 5 b", "3.7", "3.7");
    check("4 + 1 2 b b b", "4", "4");
    check("4 + 1 2 b b b b", "", "0");
    check("b", "", "0");
    check("b b b", "", "0");
    check("3 . b", "3", "3");
    check(". b b", "", "0");
    check("200 + 10 % b", "200 + 10", "210");
    check("200 + 10 % b b", "200 + 1", "201");
    check("4 + 5 = * 2 b", "9 x", "9");
    check("4 + 5 = * 2 b b", "9", "9");
    check("4 + 5 = * 2 b b b", "", "0");
    check("9 + 3 = = b", "12 +", "12");
    check("2 + 3 = n b", "", "0");
    check("7 / 0 b", "7 /", "7");
    check("7 / 0 + b", "7 / 0", "Err");
    check("4 + 5 + 3 5 = b 7 =", "4 + 5 + 37", "46");
    check("4 + 5 = 7", "7", "7");        // a digit after "=" starts a new sum
    check("4 + 5 = + 1", "9 + 1", "10"); // an operator after "=" goes on from the total
    // Each press takes exactly one character (or an operator and the space
    // before it) off the end, all the way to empty.
    {
        static const char *const sums[] = {"4 + 12", "1234.5 + 678.25 * 3 - 99.5 / 2 + 77", "200 + 10 % * 3", "0.10 / 4 =", "9 + 3 = =", "1 2 3 = * 4 5 6"};
        for (unsigned k = 0; k < sizeof sums / sizeof sums[0]; k++) {
            calc_t c;
            char   before[CALC_LINE_LEN], after[CALC_LINE_LEN];
            calc_reset(&c);
            type(&c, sums[k]);
            bool ok = true;
            for (int guard = 0; guard < 80; guard++) {
                calc_expression(&c, before);
                if (!before[0]) break;
                calc_input(&c, CALC_KEY_BKSP);
                calc_expression(&c, after);
                size_t n = strlen(before) - 1;
                while (n && before[n - 1] == ' ')
                    n--;
                if (strlen(after) != n || strncmp(after, before, n) != 0) {
                    ok = false;
                    printf("      \"%s\" -> \"%s\"\n", before, after);
                }
            }
            calc_expression(&c, after);
            checks++;
            if (!ok || after[0]) {
                failures++;
                printf("FAIL  backspace one character at a time: %s\n", sums[k]);
            }
        }
    }

    // Long expressions keep everything until the buffer is full, then drop
    // the oldest text.
    check("123456 + 123456 + 123456 + 123456 +", "123456 + 123456 + 123456 + 123456 +", "493824");
    {
        calc_t c;
        char   l1[CALC_LINE_LEN], l2[CALC_TEXT_LEN];
        calc_reset(&c);
        for (int i = 0; i < 20; i++)
            type(&c, "123456 +");
        bool cut = calc_expression(&c, l1);
        calc_value(&c, l2);
        size_t n = strlen(l1);
        expect(cut, "long expression: marked as cut");
        expect(n < CALC_EXPR_BUF, "long expression: fits the buffer");
        expect(n >= 8 && strcmp(l1 + n - 8, "123456 +") == 0, "long expression: newest text kept");
        expect(strcmp(l2, "2469120") == 0, "long expression: value still right");
    }

    // Backspacing a long sum back into its folded part: the ".." goes, the
    // folded part comes back as one number (its value), then empties.
    {
        calc_t c;
        char   l1[CALC_LINE_LEN], l2[CALC_TEXT_LEN];
        calc_reset(&c);
        for (int i = 0; i < 20; i++)
            type(&c, "123456 +");
        bool unfolded = false, steady = true;
        for (int guard = 0; guard < 400 && !unfolded; guard++) {
            calc_input(&c, CALC_KEY_BKSP);
            unfolded = !calc_expression(&c, l1);
            calc_value(&c, l2);
            if (strcmp(l2, "Err") == 0) steady = false;
        }
        long v = atol(l2);
        expect(unfolded && strchr(l1, ' ') == NULL && strcmp(l1, l2) == 0, "long expression: the folded part comes back as one number");
        expect(steady && v > 0 && v % 123456 == 0, "long expression: its value is the folded sum");
        for (int guard = 0; guard < 20; guard++)
            calc_input(&c, CALC_KEY_BKSP);
        calc_expression(&c, l1);
        calc_value(&c, l2);
        expect(l1[0] == '\0' && strcmp(l2, "0") == 0, "long expression: backspaced to empty");
    }

    // Display lines (the screen's 6 px font, 20 characters a line). A new
    // line starts before an operator, like a paper tape; three lines show at
    // once and the knob scrolls the rest.
    check_lines("", (const char *const[]){NULL}, "0");
    check_lines("4 + 5 + 3", LINES("4 + 5 + 3"), "12");
    check_lines("4 + 5 + 3 =", LINES("4 + 5 + 3"), "12");
    check_lines("1234.5 + 678.25 * 3 - 99.5 / 2 + 77", LINES("1234.5 + 678.25 x 3", "- 99.5 / 2 + 77"), "2896.375");
    check_lines("1234.5 + 678.25 * 3 - 99.5 / 2 + 77 =", LINES("1234.5 + 678.25 x 3", "- 99.5 / 2 + 77"), "2896.375");
    // Four lines: one more than the screen shows, so the knob can scroll.
    check_lines("1234.5 + 678.25 * 3 - 99.5 / 2 + 77 + 1000 * 2 - 12.5 / 4 =", LINES("1234.5 + 678.25 x 3", "- 99.5 / 2 + 77", "+ 1000 x 2 - 12.5", "/ 4"), "1945.0625");
    check_lines("12 + 34 + 56 + 78 + 90", LINES("12 + 34 + 56 + 78", "+ 90"), "270");
    check_lines("1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9", LINES("1 + 2 + 3 + 4 + 5", "+ 6 + 7 + 8 + 9"), "45");
    check_lines("1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 =", LINES("1 + 2 + 3 + 4 + 5", "+ 6 + 7 + 8 + 9"), "45");
    check_lines("5 * 4 / 2 =", LINES("5 x 4 / 2"), "10");
    check_lines("-", LINES("0 -"), "0");
    check_lines("12345 b b", LINES("123"), "123");
    check_lines("200 + 10 % =", LINES("200 + 10%"), "220");
    check_lines("5 n * 3 =", LINES("-5 x 3"), "-15");
    check_lines("7 / 0", LINES("7 / 0"), "Err");
    check_lines("1234567.891 * 1234567.891", LINES("1234567.891", "x 1234567.891"), "Err");
    check_lines("123456789.1 + 2", LINES("123456789.1 + 2"), "123456791.1");

    // Long values: the widest the calculator makes, and the "rounded" mark
    // (shown as an approx sign instead of "=").
    check("123456789.1 n", "-123456789.1", "-123456789.1");
    check("9999999999 n", "-9999999999", "-9999999999");
    {
        static const struct {
            const char *keys, *value;
            bool        rounded;
        } r[] = {
            {"1000000000 / 3 =", "333333333.3", true}, // 333333333.333333 inside
            {"1234567.891 + 0.0004 =", "1234567.891", true},
            {"2 / 3 =", "0.666667", false}, // exact to the 6 decimals kept
            {"1 / 3 * 3 =", "0.999999", false},
            {"10 / 4 =", "2.5", false},
            {"123456789 * 9 =", "1111111101", false},
            {"123456789.1", "123456789.1", false}, // typed: never rounded
            {"7 / 0", "Err", false},
        };
        for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) {
            calc_t c;
            char   v[CALC_TEXT_LEN];
            calc_reset(&c);
            type(&c, r[i].keys);
            calc_value(&c, v);
            checks++;
            if (strcmp(v, r[i].value) != 0 || calc_value_rounded(&c) != r[i].rounded) {
                failures++;
                printf("FAIL  %s: shows %s, rounded %d; want %s, rounded %d\n", r[i].keys, v, calc_value_rounded(&c), r[i].value, r[i].rounded);
            }
        }
    }
    // Past the 96-character memory the oldest text goes, and ".." marks it.
    {
        calc_t  c;
        char    text[CALC_DISPLAY_LEN], row[CALC_ROW_LEN];
        uint8_t starts[CALC_WRAP_MAX + 1];
        calc_reset(&c);
        for (int i = 0; i < 20; i++)
            type(&c, "123456 +");
        uint8_t n = calc_display_lines(&c, text, starts);
        calc_display_line(text, starts, 0, row);
        expect(n > CALC_VIEW_LINES, "long expression: more lines than the screen shows");
        expect(strncmp(row, "..", 2) == 0, "long expression: first line starts with ..");
        calc_display_line(text, starts, n - 1, row);
        size_t rl = strlen(row);
        expect(rl >= 8 && strcmp(row + rl - 8, "123456 +") == 0, "long expression: newest text on the last line");
        bool narrow = true;
        for (uint8_t i = 0; i < n; i++) {
            uint16_t w = 0;
            calc_display_line(text, starts, i, row);
            for (const char *p = row; *p; p++)
                w += calc_char_w(*p);
            if (w > CALC_WRAP_W) narrow = false;
        }
        expect(narrow, "long expression: every line fits the screen");
    }

    // Result flag (line 2 is drawn emphasised after "=")
    {
        calc_t c;
        calc_reset(&c);
        type(&c, "4 + 5");
        expect(!calc_is_result(&c), "not a result while typing");
        type(&c, "=");
        expect(calc_is_result(&c), "a result after =");
        type(&c, "7 / 0 =");
        expect(!calc_is_result(&c), "Err is not a result");
    }

    printf("%d of %d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
