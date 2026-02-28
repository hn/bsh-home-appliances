
/*

  Just a quick and dirty hack to emulate the EPG60110 power module at D-Bus 0x1.
  Displays some hopefully entertaining things at the user interface panel BSH9000329063.

  Used for the dishwasher control panel, which was on the lectern
  during the 39C3 talk https://media.ccc.de/v/39c3-hacking-washing-machines

*/

//#include "node-dishwasher-1-EPG60110-39C3.h"
#include "bsh-dbus-node.h"

#define BUTTON_BLUE 8
#define BUTTON_WHITE 9

#define CLEN 8

static unsigned long node_last = 0;

static unsigned int tick = 0;

static unsigned int node_state = 0;

// ### Setup

static void node_setup() {
	pinMode(BUTTON_BLUE, INPUT_PULLUP);
	pinMode(BUTTON_WHITE, INPUT_PULLUP);
}

// ### Loop

static void node_loop(tx_se tx_state) {

	if (tx_state != TX_IDLE)
		return;

	unsigned long now = millis();

	if ((now - node_last) < 100) return;

	node_last = now;
	tick++;

	if ((node_state == 0) && (tick == 40)) {
		dbus_tx_qpush(0x22, 0x7ff1, (const uint8_t[]){0x10}, 1);	// pong
		dbus_tx_qpush(0x27, 0x2007, (const uint8_t[]){0x01}, 1);	// prg?
		dbus_tx_qpush(0x24, 0x2006, (const uint8_t[]){0x08}, 1);	// flags: door closed
		dbus_tx_qpush(0x26, 0x2002, (const uint8_t[]){0x00, 0x00, 0x00, 0x00 }, 4);	// no errors
		dbus_tx_qpush(0x25, 0x2008, (const uint8_t[]){0x2a}, 1);	// 0:42 remaining time
		node_state = 'I'; // initialized
	}

	if (digitalRead(BUTTON_BLUE) == 0) {
		if (node_state != 'B') {
			dbus_tx_qpush(0x27, 0x2007, (const uint8_t[]){0x07}, 1);
			node_state = 'B'; // blue button
			tick = 0;
		}

		if (tick < 40) {
			dbus_tx_qpush(0x20, 0xf200, (const uint8_t[]){0x06, 0x00, 0xbe, 0x00, 0x53, 0x00, 0x53, 0x00, 0x53 }, 9);	// CCC
		} else {


/*
			 GGGGG
			F     E
			 DDDDD
			C     B
			 AAAAA

			7 segment LED bits: ABCDEFG, e.g. "3" => 01101101 = 0x6d
*/

			const uint8_t msg[] = { 0x6d, 0x6f, 0x53, 0x6d };	// 39C3 in bits
			uint8_t canvas[] = { 0x06, 0x00, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
			unsigned int pos = (tick/2) % CLEN;
			pos = CLEN - 1 - pos;

			for (size_t i = 0; i < sizeof(msg); i++) {
				unsigned int npos = (pos + i) % CLEN;
				if (npos < 3) {
					canvas[4 + 2 * npos] = msg[i];
				}
			}

			dbus_tx_qpush(0x20, 0xf200, canvas, 9);	// directly to mem
		}

		if (tick > 120) tick = 0;
	}

	else if (digitalRead(BUTTON_WHITE) == 0) {
		if (node_state != 'W') {
			dbus_tx_qpush(0x27, 0x2007, (const uint8_t[]){0x01}, 1);
			node_state = 'W'; // white button
		}

		uint8_t rest = tick & 0xFF;
		dbus_tx_qpush(0x25, 0x2008, &rest, 1);	// remaining time
	}

	else if ((node_state == 'B') || (node_state == 'W')){
		node_state = 0;
		tick = 40 - 1;
	}
}

// ### Basic info

static void handle_8000(const uint8_t *rx_frame) {
	uint8_t msg_8200[] = { 0x00, 0x02, 0x02 };
	dbus_tx_qpush(0xb1, 0x8200, msg_8200, sizeof(msg_8200));
}

// ### Command table

static dbus_rx_cmd_entry_t rx_cmd_table[] = {

	{0x8000, handle_8000, "Dummy skeleton"},

};

extern "C" const dbus_node_info_t node_dishwasher_1_EPG60110_39C3 __attribute__((used)) = {
    .node_id = 0x1,
    .rx_cmd_table = rx_cmd_table,
    .rx_cmd_table_len = sizeof(rx_cmd_table) / sizeof(rx_cmd_table[0]),
    .loop_func = node_loop,
    .setup_func = node_setup
};

