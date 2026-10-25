// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "calc.h"
#include <string.h>

/* Values are int64 in millionths: 1.5 is 1500000. Anything with 11 or more
 * integer digits does not fit the display and counts as an overflow. */
#define SCALE 1000000LL
#define INT_LIMIT 10000000000LL   // 10^10, the first integer part that does not fit
#define LIMIT (INT_LIMIT * SCALE) // the same in millionths, 10^16

_Static_assert(CALC_DECIMALS == 6, "SCALE must be 10^CALC_DECIMALS");

/* 10^n. */
static uint64_t pow10u(uint8_t n) {
    uint64_t p = 1;
    while (n--)
        p *= 10;
    return p;
}

/* |v|. Values never reach INT64_MIN: they stay inside +-LIMIT. */
static uint64_t uabs(int64_t v) {
    return v < 0 ? (uint64_t)(-v) : (uint64_t)v;
}

/* ---- Arithmetic --------------------------------------------------------- */

/* Records an error; the value is then 0. */
static int64_t fail(calc_err_t *err, calc_err_t e) {
    *err = e;
    return 0;
}

/* A magnitude and sign as a value, or an overflow past the display. */
static int64_t signed_result(calc_err_t *err, uint64_t r, bool neg) {
    if (r >= (uint64_t)LIMIT) return fail(err, CALC_ERR_OVERFLOW);
    return neg ? -(int64_t)r : (int64_t)r;
}

/* a * b, rounded to 6 decimals. Each operand is split into its integer and
 * fraction parts so that no partial product overflows 64 bits. */
static int64_t mul_fx(calc_err_t *err, int64_t a, int64_t b) {
    uint64_t ua = uabs(a), ub = uabs(b);
    uint64_t ah = ua / SCALE, al = ua % SCALE;
    uint64_t bh = ub / SCALE, bl = ub % SCALE;
    if (ah && bh > (uint64_t)INT_LIMIT / ah) return fail(err, CALC_ERR_OVERFLOW);
    uint64_t hh = ah * bh;
    if (hh >= (uint64_t)INT_LIMIT) return fail(err, CALC_ERR_OVERFLOW);
    uint64_t r = hh * SCALE + ah * bl + al * bh + (al * bl + SCALE / 2) / SCALE;
    return signed_result(err, r, (a < 0) != (b < 0));
}

/* a / b, rounded to 6 decimals, by long division. */
static int64_t div_fx(calc_err_t *err, int64_t a, int64_t b) {
    if (b == 0) return fail(err, CALC_ERR_DIV_ZERO);
    uint64_t ua = uabs(a), ub = uabs(b);
    uint64_t q = ua / ub, r = ua % ub;
    if (q >= (uint64_t)INT_LIMIT) return fail(err, CALC_ERR_OVERFLOW);
    uint64_t frac = 0;
    for (uint8_t i = 0; i < CALC_DECIMALS; i++) {
        r *= 10;
        frac = frac * 10 + r / ub;
        r %= ub;
    }
    if (r * 10 / ub >= 5) frac++; // round half up on the 7th decimal
    return signed_result(err, q * SCALE + frac, (a < 0) != (b < 0));
}

/* a op b; with no operator, b. */
static int64_t apply(calc_err_t *err, int64_t a, calc_op_t op, int64_t b) {
    switch (op) {
        case CALC_OP_ADD:
            return signed_result(err, uabs(a + b), a + b < 0);
        case CALC_OP_SUB:
            return signed_result(err, uabs(a - b), a - b < 0);
        case CALC_OP_MUL:
            return mul_fx(err, a, b);
        case CALC_OP_DIV:
            return div_fx(err, a, b);
        default:
            return b;
    }
}

/* ---- Text --------------------------------------------------------------- */

/* Writes v in decimal, returns the number of chars. */
static uint8_t put_u64(char *out, uint64_t v) {
    char    tmp[20];
    uint8_t n = 0;
    do {
        tmp[n++] = '0' + v % 10;
        v /= 10;
    } while (v);
    for (uint8_t i = 0; i < n; i++)
        out[i] = tmp[n - 1 - i];
    return n;
}

/* A value with as many decimals as fit in CALC_DIGITS digits, rounded,
 * trailing zeros dropped, and no "-0". Returns true when decimals the
 * value has were dropped to fit (the text is then rounded). */
static bool format_value(int64_t v, char *out) {
    uint64_t u  = uabs(v);
    uint64_t ip = u / SCALE, fp = u % SCALE;
    uint8_t  n = 1;
    for (uint64_t t = ip; t >= 10; t /= 10)
        n++;
    uint8_t dec = n >= CALC_DIGITS ? 0 : CALC_DIGITS - n;
    if (dec > CALC_DECIMALS) dec = CALC_DECIMALS;
    uint64_t div     = pow10u(CALC_DECIMALS - dec);
    bool     rounded = fp % div != 0;
    uint64_t f       = (fp + div / 2) / div;
    if (f >= pow10u(dec)) { // rounding carried into the integer part
        if (ip + 1 < (uint64_t)INT_LIMIT) {
            ip++;
            f = 0;
        } else {
            f = fp / div; // no room for another digit: cut instead
        }
    }
    while (dec && f % 10 == 0) {
        f /= 10;
        dec--;
    }
    char *p = out;
    if (v < 0 && (ip || f)) *p++ = '-';
    p += put_u64(p, ip);
    if (dec) {
        *p++ = '.';
        for (uint8_t i = dec; i > 0; i--)
            *p++ = '0' + (f / pow10u(i - 1)) % 10;
    }
    *p = '\0';
    return rounded;
}

/* How an operator appears on line 1. */
static char op_char(calc_op_t op) {
    static const char chars[] = {' ', '+', '-', 'x', '/'};
    return chars[op];
}

/* ---- Numbers and operators of line 1 ---------------------------------------- */

static calc_tok_t *last_tok(calc_t *c) {
    return c->n ? &c->tok[c->n - 1] : NULL;
}

/* Adds an empty number or operator at the end of line 1. */
static calc_tok_t *push(calc_t *c, uint8_t kind) {
    calc_tok_t *t = &c->tok[c->n++];
    memset(t, 0, sizeof *t);
    t->kind = kind;
    return t;
}

/* Adds a given number: a value the sum reuses (a total, 0, ...). */
static void push_given(calc_t *c, int64_t v, uint8_t flags) {
    calc_tok_t *t = push(c, CALC_TOK_NUM);
    t->value      = v;
    t->flags      = flags;
}

static void push_op(calc_t *c, calc_op_t op) {
    push(c, CALC_TOK_OP)->op = op;
}

/* Digits typed so far, not counting the '.'. */
static uint8_t entry_digits(const calc_tok_t *t) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < t->len; i++)
        if (t->entry[i] != '.') n++;
    return n;
}

/* The '.' in a typed number, or NULL. */
static const char *entry_dot(const calc_tok_t *t) {
    return memchr(t->entry, '.', t->len);
}

/* A typed number's value. */
static int64_t entry_value(const calc_tok_t *t) {
    int64_t ip = 0, fp = 0;
    uint8_t nf  = 0;
    bool    dot = false;
    for (uint8_t i = 0; i < t->len; i++) {
        char ch = t->entry[i];
        if (ch == '.') {
            dot = true;
        } else if (dot) {
            fp = fp * 10 + (ch - '0');
            nf++;
        } else {
            ip = ip * 10 + (ch - '0');
        }
    }
    int64_t v = ip * SCALE + fp * (int64_t)pow10u(CALC_DECIMALS - nf);
    return (t->flags & CALC_NUM_NEG) ? -v : v;
}

/* A number's value where it stands, after the value acc and the operator
 * op before it: a percentage depends on both (see calc.h). */
static int64_t num_value(calc_err_t *err, const calc_tok_t *t, int64_t acc, calc_op_t op) {
    int64_t v = (t->flags & CALC_NUM_TYPED) ? entry_value(t) : t->value;
    if (!(t->flags & CALC_NUM_PCT)) return v;
    int64_t frac = div_fx(err, v, 100 * SCALE);
    return (op == CALC_OP_ADD || op == CALC_OP_SUB) ? mul_fx(err, acc, frac) : frac;
}

/* The value of the first count tokens of line 1, operators applied left to
 * right; a trailing operator is left out. *err gets the first error (the
 * value is then 0). */
static int64_t total_upto(const calc_t *c, uint8_t count, calc_err_t *err) {
    int64_t   acc  = c->folded ? c->base : 0;
    bool      have = c->folded; // acc holds a value
    calc_op_t op   = CALC_OP_NONE;
    for (uint8_t i = 0; i < count && !*err; i++) {
        const calc_tok_t *t = &c->tok[i];
        if (t->kind == CALC_TOK_OP) {
            op = (calc_op_t)t->op;
            continue;
        }
        int64_t v = num_value(err, t, acc, have ? op : CALC_OP_NONE);
        acc       = have ? apply(err, acc, op, v) : v;
        have      = true;
        op        = CALC_OP_NONE;
    }
    return *err ? 0 : acc;
}

static int64_t total(const calc_t *c, calc_err_t *err) {
    return total_upto(c, c->n, err);
}

/* A token as line 1 shows it. out: CALC_TEXT_LEN bytes. Returns its length. */
static uint8_t tok_text(const calc_tok_t *t, char *out) {
    char *p = out;
    if (t->kind == CALC_TOK_OP) {
        *p++ = op_char((calc_op_t)t->op);
    } else if (t->flags & CALC_NUM_TYPED) {
        if (t->flags & CALC_NUM_NEG) *p++ = '-';
        if (!t->len) *p++ = '0'; // cleared with CE, or only the sign so far
        memcpy(p, t->entry, t->len);
        p += t->len;
    } else {
        format_value(t->value, p);
        p += strlen(p);
    }
    if (t->kind == CALC_TOK_NUM && (t->flags & CALC_NUM_PCT)) *p++ = '%';
    *p = '\0';
    return (uint8_t)(p - out);
}

/* Length of line 1's text. */
static uint8_t text_len(const calc_t *c) {
    char    tmp[CALC_TEXT_LEN];
    uint8_t n = 0;
    for (uint8_t i = 0; i < c->n; i++)
        n += (uint8_t)(tok_text(&c->tok[i], tmp) + (i ? 1 : 0));
    return n;
}

/* Keeps line 1 inside CALC_EXPR_BUF characters (and the list inside
 * CALC_TOKENS): the oldest number, with the operator before the next one,
 * is folded into base, and line 1 then starts with "..". Called after an
 * operator was added without an error, so what is folded has none. */
static void fold(calc_t *c) {
    while (c->n >= 3 && (text_len(c) > CALC_EXPR_BUF - 1 || c->n > CALC_TOKENS - 3)) {
        calc_err_t err = CALC_ERR_NONE;
        if (!c->folded) { // number, operator, ...: the number becomes base
            c->base   = num_value(&err, &c->tok[0], 0, CALC_OP_NONE);
            c->folded = true;
            memmove(c->tok, c->tok + 1, (size_t)(c->n - 1) * sizeof c->tok[0]);
            c->n -= 1;
        } else { // operator, number, operator, ...: both go into base
            calc_op_t op = (calc_op_t)c->tok[0].op;
            c->base      = apply(&err, c->base, op, num_value(&err, &c->tok[1], c->base, op));
            memmove(c->tok, c->tok + 2, (size_t)(c->n - 2) * sizeof c->tok[0]);
            c->n -= 2;
        }
    }
}

/* A given number as typed digits, the way line 1 shows it, so BKSP can
 * edit it. */
static void to_typed(calc_tok_t *t) {
    char text[CALC_TEXT_LEN];
    format_value(t->value, text);
    const char *p     = text;
    uint8_t     flags = CALC_NUM_TYPED | (t->flags & CALC_NUM_PCT);
    if (*p == '-') {
        flags |= CALC_NUM_NEG;
        p++;
    }
    t->len = (uint8_t)strlen(p);
    memcpy(t->entry, p, t->len + 1);
    t->flags = flags;
}

/* ---- Keys --------------------------------------------------------------- */

/* A fresh sum: line 1 empty. "=" keeps its memory, so "9 + 3 = 7 =" gives
 * 10, like Windows Calculator. */
static void clear_sum(calc_t *c) {
    c->n        = 0;
    c->folded   = false;
    c->after_eq = false;
}

/* The number being typed, for a digit or '.'. After "=" or an error a new
 * sum starts; a given number or a percentage is replaced by what is typed
 * now; after an operator (or on an empty line) a new number starts. */
static calc_tok_t *typed_number(calc_t *c) {
    if (c->err) calc_reset(c);
    if (c->after_eq) clear_sum(c);
    calc_tok_t *t = last_tok(c);
    if (t && t->kind == CALC_TOK_NUM) {
        if ((t->flags & CALC_NUM_TYPED) && !(t->flags & CALC_NUM_PCT)) return t;
        c->n--;
    }
    calc_tok_t *num = push(c, CALC_TOK_NUM);
    num->flags      = CALC_NUM_TYPED;
    return num;
}

/* Adds a character to a typed number. */
static void append(calc_tok_t *t, char ch) {
    t->entry[t->len++] = ch;
    t->entry[t->len]   = '\0';
}

/* A digit: up to CALC_DIGITS digits and CALC_DECIMALS decimals. */
static void key_digit(calc_t *c, uint8_t d) {
    calc_tok_t *t = typed_number(c);
    if (entry_digits(t) >= CALC_DIGITS) return;
    const char *dot = entry_dot(t);
    if (dot && (uint8_t)(t->entry + t->len - dot - 1) >= CALC_DECIMALS) return;
    if (t->len == 1 && t->entry[0] == '0') t->len = 0; // "0" then "5" is "5"
    append(t, (char)('0' + d));
}

/* The decimal point, once per number ("0." when typed first). */
static void key_dot(calc_t *c) {
    calc_tok_t *t = typed_number(c);
    if (entry_dot(t) || entry_digits(t) >= CALC_DIGITS) return;
    if (t->len == 0) append(t, '0');
    append(t, '.');
}

/* + - x /: goes on from the total after "=", replaces an operator typed
 * just before, else is added (after a 0 on an empty line). The sum so far
 * is worked out now: an error then holds until a key clears it. */
static void key_op(calc_t *c, calc_op_t op) {
    if (c->err) return;
    calc_err_t err = CALC_ERR_NONE;
    if (c->after_eq) {
        int64_t r = total(c, &err);
        clear_sum(c);
        push_given(c, r, 0);
    } else if (!c->n) {
        push_given(c, 0, 0);
    } else if (last_tok(c)->kind == CALC_TOK_OP) {
        last_tok(c)->op = op;
        return;
    }
    push_op(c, op);
    total(c, &err);
    if (err) {
        c->err = err;
        return;
    }
    fold(c);
}

/* =: finishes the sum and remembers its last operation, which "=" right
 * after repeats ("9 + 3 = =" is 15). On a lone number it repeats the one
 * remembered ("9 + 3 = 7 =" is 10). Right after an operator the value so
 * far is the operand ("5 + =" is 10). */
static void key_eq(calc_t *c) {
    if (c->err) return;
    calc_err_t err = CALC_ERR_NONE;
    if (c->after_eq) {
        if (c->last_op == CALC_OP_NONE) return;
        int64_t r = total(c, &err);
        clear_sum(c);
        push_given(c, r, 0);
        push_op(c, c->last_op);
        push_given(c, c->last_operand, 0);
    } else {
        if (!c->n) push_given(c, 0, 0);
        if (last_tok(c)->kind == CALC_TOK_OP) {
            push_given(c, total(c, &err), 0);
        } else if (c->n == 1 && !c->folded && c->last_op != CALC_OP_NONE) {
            push_op(c, c->last_op);
            push_given(c, c->last_operand, 0);
        }
    }
    total(c, &err);
    if (err) {
        c->err = err;
        return;
    }
    if (c->n >= 2) { // the last number and the operator before it, for a repeat
        calc_op_t op    = (calc_op_t)c->tok[c->n - 2].op;
        int64_t   acc   = total_upto(c, c->n - 1, &err);
        c->last_op      = op;
        c->last_operand = num_value(&err, &c->tok[c->n - 1], acc, op);
    }
    c->after_eq = true;
}

/* %: makes the last number a percentage (see calc.h); after "=" the total,
 * and right after an operator (or on an empty line) the value so far. A
 * second % does nothing. */
static void key_pct(calc_t *c) {
    if (c->err) return;
    calc_err_t  err = CALC_ERR_NONE;
    calc_tok_t *t   = last_tok(c);
    if (c->after_eq) {
        int64_t r = total(c, &err);
        clear_sum(c);
        push_given(c, r, CALC_NUM_PCT);
    } else if (!t || t->kind == CALC_TOK_OP) {
        push_given(c, total(c, &err), CALC_NUM_PCT);
    } else {
        t->flags |= CALC_NUM_PCT;
    }
}

/* +/-: flips the sign of the number being typed or given; after "=" of the
 * total, which starts a new sum. Right after an operator it gives the value
 * so far, negated ("5 + +/-" is 5 + -5), or starts a negative number when
 * that value is 0. */
static void key_neg(calc_t *c) {
    if (c->err) return;
    calc_err_t  err = CALC_ERR_NONE;
    calc_tok_t *t   = last_tok(c);
    if (c->after_eq) {
        int64_t r = total(c, &err);
        clear_sum(c);
        push_given(c, -r, 0);
    } else if (t && t->kind == CALC_TOK_NUM) {
        if (t->flags & CALC_NUM_TYPED) {
            t->flags ^= CALC_NUM_NEG;
        } else {
            t->value = -t->value;
        }
    } else {
        int64_t v = total(c, &err);
        if (v) {
            push_given(c, -v, 0);
        } else {
            push(c, CALC_TOK_NUM)->flags = CALC_NUM_TYPED | CALC_NUM_NEG;
        }
    }
}

/* CE: the number being given becomes an empty typed number, shown as 0
 * (after "=", on a fresh line). With an error everything clears. */
static void key_ce(calc_t *c) {
    if (c->err) {
        calc_reset(c);
        return;
    }
    if (c->after_eq) clear_sum(c);
    calc_tok_t *t = last_tok(c);
    if (!t || t->kind == CALC_TOK_OP) t = push(c, CALC_TOK_NUM);
    memset(t, 0, sizeof *t);
    t->kind  = CALC_TOK_NUM;
    t->flags = CALC_NUM_TYPED;
}

/* BKSP: reopens a finished sum (after "=" or an error), then removes the
 * last character of line 1: a whole operator, a '%', or the last digit or
 * '.' of a number. A number whose last digit goes is removed, sign and all;
 * a given number is edited by the digits it shows. Backspacing into the
 * folded part brings it back as one number. */
static void key_bksp(calc_t *c) {
    c->err        = CALC_ERR_NONE;
    c->after_eq   = false;
    calc_tok_t *t = last_tok(c);
    if (!t) return; // empty: nothing to remove
    if (t->kind == CALC_TOK_OP) {
        c->n--;
        if (!c->n && c->folded) {
            c->folded = false;
            push_given(c, c->base, 0);
        }
        return;
    }
    if (t->flags & CALC_NUM_PCT) {
        t->flags &= (uint8_t)~CALC_NUM_PCT;
        return;
    }
    if (!(t->flags & CALC_NUM_TYPED)) to_typed(t);
    if (t->len) t->entry[--t->len] = '\0';
    if (!t->len) c->n--;
}

/* ---- Public API (see calc.h) ------------------------------------------ */

/* AC: everything cleared. */
void calc_reset(calc_t *c) {
    memset(c, 0, sizeof *c);
}

/* One calculator key. */
void calc_input(calc_t *c, calc_key_t key) {
    if (key <= CALC_KEY_9) {
        key_digit(c, (uint8_t)key);
        return;
    }
    switch (key) {
        case CALC_KEY_DOT:
            key_dot(c);
            break;
        case CALC_KEY_ADD:
            key_op(c, CALC_OP_ADD);
            break;
        case CALC_KEY_SUB:
            key_op(c, CALC_OP_SUB);
            break;
        case CALC_KEY_MUL:
            key_op(c, CALC_OP_MUL);
            break;
        case CALC_KEY_DIV:
            key_op(c, CALC_OP_DIV);
            break;
        case CALC_KEY_PCT:
            key_pct(c);
            break;
        case CALC_KEY_EQ:
            key_eq(c);
            break;
        case CALC_KEY_NEG:
            key_neg(c);
            break;
        case CALC_KEY_BKSP:
            key_bksp(c);
            break;
        case CALC_KEY_CE:
            key_ce(c);
            break;
        case CALC_KEY_AC:
            calc_reset(c);
            break;
        default:
            break;
    }
}

/* Line 1: the numbers and operators, a space between each. */
bool calc_expression(const calc_t *c, char *out) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < c->n; i++) {
        if (i) out[n++] = ' ';
        n += tok_text(&c->tok[i], out + n);
    }
    out[n] = '\0';
    return c->folded;
}

/* The live value of line 1, or "Err". */
calc_err_t calc_value(const calc_t *c, char *out) {
    calc_err_t err = c->err;
    int64_t    v   = err ? 0 : total(c, &err);
    if (err) {
        strcpy(out, "Err");
        return err;
    }
    format_value(v, out);
    return CALC_ERR_NONE;
}

/* The live value had to be rounded to fit CALC_DIGITS digits. */
bool calc_value_rounded(const calc_t *c) {
    char       out[CALC_TEXT_LEN];
    calc_err_t err = c->err;
    int64_t    v   = err ? 0 : total(c, &err);
    return !err && format_value(v, out);
}

/* "=" finished the sum and there is no error. */
bool calc_is_result(const calc_t *c) {
    return c->after_eq && !c->err;
}

/* ---- Display lines ---------------------------------------------------------- */

/* See CALC_CHAR_W in calc.h. */
uint8_t calc_char_w(char ch) {
    return ch == '.' ? CALC_DOT_W : ch == ' ' ? CALC_SPACE_W : CALC_CHAR_W;
}

/* Line 1 has tokens with one space between them; an operator is a token
 * of one character: + - x /. ("-5" is a number.) */
static bool op_token_at(const char *t, uint8_t k, uint8_t len) {
    return k < len && strchr("+-x/", t[k]) && (k + 1 == len || t[k + 1] == ' ');
}

/* End of the unit that starts at i: its first token, then every following
 * token up to the next operator. Units are where display lines may break. */
static uint8_t unit_end(const char *t, uint8_t i, uint8_t len) {
    uint8_t j = i;
    while (j < len && t[j] != ' ')
        j++;
    while (j < len && !op_token_at(t, j + 1, len)) { // t[j] is a space
        j++;
        while (j < len && t[j] != ' ')
            j++;
    }
    return j;
}

/* Greedy filling: units go onto a display line while they fit. */
uint8_t calc_display_lines(const calc_t *c, char *text, uint8_t *starts) {
    uint8_t off = 0;
    if (calc_expression(c, text + 3)) {
        memcpy(text, ".. ", 3); // older text was dropped
    } else {
        off = 3;
    }
    memmove(text, text + off, strlen(text + off) + 1);

    uint8_t len = (uint8_t)strlen(text), n = 0, w = 0, i = 0;
    while (i < len) {
        uint8_t j  = unit_end(text, i, len);
        uint8_t uw = 0;
        for (uint8_t k = i; k < j; k++)
            uw += calc_char_w(text[k]);
        if (w && w + CALC_SPACE_W + uw > CALC_WRAP_W) w = 0; // the unit starts a new line
        if (w) {
            w += CALC_SPACE_W;
        } else if (n < CALC_WRAP_MAX) {
            starts[n++] = i;
        }
        // Only a unit wider than a whole line is split: after its operator
        // if it has one ("x" / "1234567.891"), else between characters.
        uint8_t sp = 0xFF; // the space inside the unit, on the current line
        for (uint8_t k = i; k < j; k++) {
            uint8_t cw = calc_char_w(text[k]);
            if (w && w + cw > CALC_WRAP_W && n < CALC_WRAP_MAX) {
                if (sp != 0xFF) {
                    starts[n++] = sp + 1;
                    w           = 0;
                    for (uint8_t q = sp + 1; q < k; q++)
                        w += calc_char_w(text[q]);
                    sp = 0xFF;
                } else if (text[k] == ' ') {
                    starts[n++] = k + 1;
                    w           = 0;
                    continue;
                } else {
                    starts[n++] = k;
                    w           = 0;
                }
            }
            if (text[k] == ' ') sp = k;
            w += cw;
        }
        i = j < len ? j + 1 : len; // past the space after the unit
    }
    starts[n] = len;
    return n;
}

/* One display line, cut to CALC_ROW_LEN - 1 characters at most. */
void calc_display_line(const char *text, const uint8_t *starts, uint8_t i, char *out) {
    uint8_t a = starts[i], b = starts[i + 1];
    while (b > a && text[b - 1] == ' ')
        b--;
    if (b - a > CALC_ROW_LEN - 1) b = a + CALC_ROW_LEN - 1;
    memcpy(out, text + a, b - a);
    out[b - a] = '\0';
}
