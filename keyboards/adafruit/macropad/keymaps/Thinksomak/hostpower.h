// Copyright 2026 Somak Mondal <mondalsomak@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Host Power: shut down or restart the PC from the macropad. A keyboard
 * cannot restart Windows by itself, so it asks the PC agent (host/main.py)
 * over Raw HID; the agent runs Windows' shutdown command, and only when its
 * settings allow it (allow_power_commands = true). Off by default there.
 *
 * Keys, by position (like the other modes):
 *   POWER (SYSTEM layer, top middle) opens the menu, where the top row is
 *       left (7) Shutdown     middle (8) Restart     right (9) Cancel
 *   Nothing else works while it is open, the knob included, and it closes
 *   by itself after HOSTPOWER_MENU_TIMEOUT_MS without a key. For its first
 *   HOSTPOWER_ARM_MS, 7 and 8 are ignored: POWER sits where 8 is, so a
 *   double tap must not restart the PC.
 *   While a countdown runs, the top-right key cancels it on every layer.
 *
 * States:
 *   IDLE --POWER--> MENU --7 / 8--> MENU, asking (request sent)
 *     asking --reply "started"--> COUNTDOWN
 *     asking --reply "disabled" / "error", or no reply in
 *             HOSTPOWER_ACK_MS--> MENU with a note saying so
 *   MENU --9, or HOSTPOWER_MENU_TIMEOUT_MS untouched--> IDLE
 *   COUNTDOWN --top-right key--> abort sent
 *     --reply "aborted"--> CANCELLED (a note, HOSTPOWER_NOTE_MS) --> IDLE
 *     --no reply--> the countdown goes on, marked "cancel not confirmed";
 *       the key sends the abort again
 *   COUNTDOWN --reaches 0--> "now", until HOSTPOWER_GRACE_MS later, should
 *     the keyboard still have power --> IDLE
 *   A reply is still taken up to HOSTPOWER_LATE_MS after its request, if
 *   the agent is sending stats (without it nothing read the request, so it
 *   is dropped at once and 7 / 8 work again). Should "started" come after 9
 *   closed the menu, or after the screen said the PC did not answer, the
 *   abort goes out at once: the PC does what the screen said.
 *
 * Packets: 32 bytes, little-endian; the agent writes a report ID of 0 in
 * front. 0xC6 and 0xC7 are outside VIA's commands (0x01-0x15, 0xFF).
 *   keyboard -> PC: request, sent only when 7, 8 or a cancel is pressed
 *     0 0xC6   1 'P' (0x50)   2 protocol 1   3 sequence
 *     4 action: 1 shutdown, 2 restart, 3 abort
 *     5-6 delay asked for, in s (HOSTPOWER_DELAY_S)   7 CRC-8 of bytes 0-6
 *   PC -> keyboard: reply
 *     0 0xC7   1 'P'   2 protocol 1   3 sequence (echoed)   4 action (echoed)
 *     5 status: 1 started, 2 aborted, 3 disabled, 4 error
 *     6-7 delay the PC uses, in s   8 CRC-8 of bytes 0-7
 *   CRC-8: polynomial 0x07, from 0. A reply with the wrong sequence, action
 *   or CRC is ignored. The keyboard counts down from the PC's delay, so the
 *   screen always shows what the PC will really do.
 */
#pragma once

#include "quantum.h"

#ifndef HOSTPOWER_MENU_TIMEOUT_MS
#    define HOSTPOWER_MENU_TIMEOUT_MS 10000 // the menu closes after this long without a key
#endif
#ifndef HOSTPOWER_DELAY_S
#    define HOSTPOWER_DELAY_S 15 // asked of the PC; the agent's power_delay setting wins, never under 5
#endif
#define HOSTPOWER_ARM_MS 500     // 7 and 8 do nothing this soon after the menu opens
#define HOSTPOWER_ACK_MS 2000    // no reply by then: "Host agent not responding"
#define HOSTPOWER_LATE_MS 20000  // a later reply is still taken until then (the agent gives up after 10 s)
#define HOSTPOWER_NOTE_MS 3000   // "Cancelled" shows this long
#define HOSTPOWER_GRACE_MS 60000 // the countdown screen gives up this long after 0

typedef enum { HP_ACT_NONE, HP_ACT_SHUTDOWN, HP_ACT_RESTART, HP_ACT_ABORT } hostpower_action_t;

/* The menu's status line. */
typedef enum {
    HP_NOTE_NONE,
    HP_NOTE_ASKING,   // a request is out, waiting for the reply
    HP_NOTE_NO_REPLY, // no reply in HOSTPOWER_ACK_MS: the agent is not running or too old
    HP_NOTE_DISABLED, // the agent's settings do not allow it
    HP_NOTE_ERROR,    // the agent tried, and Windows refused
} hostpower_note_t;

void hostpower_open_menu(void); // the POWER key
void hostpower_task(void);      // housekeeping_task_user

/* Call from process_record_user right after the boot confirm. Returns false
 * when the event was used: every key and the knob while the menu is open,
 * the top-right key while a countdown runs. */
bool hostpower_process_record(keyrecord_t *record);

/* Raw HID: call from via_command_kb(). True when the packet was a reply. */
bool hostpower_receive(const uint8_t *data, uint8_t length);

/* For the screen and the LEDs. */
bool               hostpower_menu_open(void);
hostpower_note_t   hostpower_menu_note(void);
uint8_t            hostpower_menu_secs(void);    // until the menu closes by itself
bool               hostpower_counting(void);     // a countdown shows (including "now")
bool               hostpower_cancelled(void);    // "Cancelled" shows
hostpower_action_t hostpower_count_action(void); // shutdown or restart
uint16_t           hostpower_count_secs(void);   // seconds left, 0 = now
bool               hostpower_cancelling(void);   // an abort is out, no reply yet
bool               hostpower_cancel_unconfirmed(void);
