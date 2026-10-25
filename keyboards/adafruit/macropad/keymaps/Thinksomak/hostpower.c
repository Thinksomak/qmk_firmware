// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Host Power (see hostpower.h): the POWER menu, the request and reply
 * packets, the countdown and the cancel key. Drawing is in ui_oled.c, the
 * lights in ui_rgb.c. Nothing is stored in EEPROM.
 */

#include "hostpower.h"
#include "raw_hid.h"
#include "sysinfo.h" // sysinfo_live(): is the PC agent there at all?

_Static_assert((MATRIX_ROWS) * (MATRIX_COLS) <= 16, "swallow needs one bit per key");

#define REQUEST_CMD 0xC6
#define REPLY_CMD 0xC7
#define MAGIC 0x50 // 'P'
#define PROTOCOL 1
#define PACKET_SIZE 32

enum { STATUS_STARTED = 1, STATUS_ABORTED, STATUS_DISABLED, STATUS_ERROR };

/* The top row of keys, where 7, 8 and 9 are on the numpad. Row 0 is the
 * knob's press. */
#define ROW_TOP 1
enum { COL_7, COL_8, COL_9 };

enum { HP_IDLE, HP_MENU, HP_COUNT, HP_CANCELLED };

static uint8_t  state;
static uint32_t opened_at, touched_at; // the menu: when it opened, its last key
static uint8_t  note;                  // the menu's status line
static uint8_t  seq;                   // of the last request
static uint8_t  pending;               // the action sent and not answered yet
static uint32_t pending_at;
static bool     overdue;        // pending, no reply in HOSTPOWER_ACK_MS; a late one still counts
static bool     abort_on_start; // should the PC start now, stop it: cancel was pressed while
                                // asking, or the user was told the PC did not answer
static uint8_t  count_action;
static uint32_t count_end;   // when the PC acts
static bool     unconfirmed; // an abort got no reply, or an error
static uint32_t cancelled_at;
static uint16_t swallow; // keys whose press was used here: their release is too

/* CRC-8, polynomial 0x07, from 0: the same as the agent's. */
static uint8_t crc8(const uint8_t *p, uint8_t n) {
    uint8_t crc = 0;
    while (n--) {
        crc ^= *p++;
        for (uint8_t k = 0; k < 8; k++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}

/* Sends a request to the PC agent and waits for its reply. */
static void send(hostpower_action_t action) {
    uint8_t p[PACKET_SIZE] = {REQUEST_CMD, MAGIC, PROTOCOL, ++seq, action, HOSTPOWER_DELAY_S & 0xFF, HOSTPOWER_DELAY_S >> 8};
    p[7]                   = crc8(p, 7);
    pending                = action;
    pending_at             = timer_read32();
    overdue                = false;
    raw_hid_send(p, PACKET_SIZE);
}

/* A shutdown or restart request is out, answered or not. */
static bool asking(void) {
    return pending == HP_ACT_SHUTDOWN || pending == HP_ACT_RESTART;
}

void hostpower_open_menu(void) {
    if (state == HP_COUNT) return; // a countdown runs: its cancel key is what counts now
    state      = HP_MENU;
    opened_at  = timer_read32();
    touched_at = opened_at;
    note       = !asking() ? HP_NOTE_NONE : overdue ? HP_NOTE_NO_REPLY : HP_NOTE_ASKING;
}

bool hostpower_process_record(keyrecord_t *record) {
    keyevent_t ev  = record->event;
    bool       enc = IS_ENCODEREVENT(ev);
    uint16_t   bit = 0;
    if (!enc && ev.key.row < MATRIX_ROWS && ev.key.col < MATRIX_COLS) {
        bit = (uint16_t)1 << (ev.key.row * MATRIX_COLS + ev.key.col);
    }

    if (!ev.pressed) {
        if (swallow & bit) {
            swallow &= ~bit;
            return false;
        }
        return !(enc && state == HP_MENU); // the knob does nothing in the menu
    }
    bool top = bit && ev.key.row == ROW_TOP;

    if (state == HP_MENU) {
        swallow |= bit;
        if (enc) return false;
        touched_at = timer_read32();
        if (!top) return false; // the other keys are off in the menu
        if (ev.key.col == COL_9) {
            state = HP_IDLE;
            if (asking()) abort_on_start = true; // should the PC start anyway, stop it
            return false;
        }
        if (asking() || timer_elapsed32(opened_at) < HOSTPOWER_ARM_MS) return false;
        abort_on_start = false;
        note           = HP_NOTE_ASKING;
        send(ev.key.col == COL_7 ? HP_ACT_SHUTDOWN : HP_ACT_RESTART);
        return false;
    }

    if (state == HP_COUNT && top && ev.key.col == COL_9) { // cancel, on any layer
        swallow |= bit;
        if (pending != HP_ACT_ABORT || overdue) { // again if the last one got no reply
            unconfirmed = false;
            send(HP_ACT_ABORT);
        }
        return false;
    }
    return true;
}

bool hostpower_receive(const uint8_t *data, uint8_t length) {
    if (length < 9 || data[0] != REPLY_CMD) return false;
    if (data[1] != MAGIC || data[2] != PROTOCOL || data[8] != crc8(data, 8)) return true; // ours, but damaged
    if (!pending || data[3] != seq || data[4] != pending) return true;                    // not the reply awaited
    uint8_t  action = pending;
    uint16_t delay  = (uint16_t)(data[6] | data[7] << 8);
    pending         = HP_ACT_NONE;
    overdue         = false;

    if (action == HP_ACT_ABORT) { // anything but "aborted": the PC may still act
        if (data[5] != STATUS_ABORTED) {
            unconfirmed = true;
        } else if (state == HP_COUNT) {
            state        = HP_CANCELLED;
            cancelled_at = timer_read32();
        }
        return true;
    }
    switch (data[5]) {
        case STATUS_STARTED:
            // The PC started its timer between the request and now: count
            // from the request, so the screen never shows more time than
            // there is.
            state        = HP_COUNT;
            count_action = action;
            count_end    = pending_at + (uint32_t)delay * 1000;
            unconfirmed  = false;
            note         = HP_NOTE_NONE;
            if (abort_on_start) { // cancelled while asking, or too late
                abort_on_start = false;
                send(HP_ACT_ABORT);
            }
            break;
        case STATUS_DISABLED:
            note = HP_NOTE_DISABLED;
            break;
        default: // error, or a status this firmware does not know
            note = HP_NOTE_ERROR;
            break;
    }
    abort_on_start = false;
    return true;
}

void hostpower_task(void) {
    if (pending && !overdue && timer_elapsed32(pending_at) >= HOSTPOWER_ACK_MS) {
        overdue = true;
        if (pending == HP_ACT_ABORT) {
            unconfirmed = true; // the countdown goes on: it may still happen
        } else {
            note = HP_NOTE_NO_REPLY;
            if (sysinfo_live()) {
                // The agent is there, only slow. The screen now says the PC
                // did not answer, so it must not act: should "started"
                // still come, it is cancelled at once.
                abort_on_start = true;
            } else {
                // No agent: nothing read the request, so no reply can
                // come. Shutdown and Restart can be tried again at once.
                pending = HP_ACT_NONE;
                overdue = false;
            }
        }
    }
    if (pending && timer_elapsed32(pending_at) >= HOSTPOWER_LATE_MS) { // no reply will come now
        pending        = HP_ACT_NONE;
        overdue        = false;
        abort_on_start = false;
    }
    if (state == HP_MENU && (!asking() || overdue) && timer_elapsed32(touched_at) >= HOSTPOWER_MENU_TIMEOUT_MS) state = HP_IDLE;
    if (state == HP_COUNT && (int32_t)(timer_read32() - count_end) >= (int32_t)HOSTPOWER_GRACE_MS) state = HP_IDLE;
    if (state == HP_CANCELLED && timer_elapsed32(cancelled_at) >= HOSTPOWER_NOTE_MS) state = HP_IDLE;
}

/* ---- For the screen and the LEDs (see hostpower.h) ----------------------- */

bool hostpower_menu_open(void) {
    return state == HP_MENU;
}

hostpower_note_t hostpower_menu_note(void) {
    return (hostpower_note_t)note;
}

uint8_t hostpower_menu_secs(void) {
    uint32_t gone = timer_elapsed32(touched_at);
    return gone >= HOSTPOWER_MENU_TIMEOUT_MS ? 0 : (uint8_t)((HOSTPOWER_MENU_TIMEOUT_MS - gone + 999) / 1000);
}

bool hostpower_counting(void) {
    return state == HP_COUNT;
}

bool hostpower_cancelled(void) {
    return state == HP_CANCELLED;
}

hostpower_action_t hostpower_count_action(void) {
    return (hostpower_action_t)count_action;
}

uint16_t hostpower_count_secs(void) {
    int32_t left = (int32_t)(count_end - timer_read32());
    return left <= 0 ? 0 : (uint16_t)((left + 999) / 1000);
}

bool hostpower_cancelling(void) {
    return pending == HP_ACT_ABORT && !overdue;
}

bool hostpower_cancel_unconfirmed(void) {
    return unconfirmed;
}
