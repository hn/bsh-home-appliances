
/*

  I do not know what exactly the B/S/H/ UDA (or more precisely,
  the software for it) does, but this is a basic toolbox to
  tinker with D-Bus participiants.

  Quick'n'dirty hack style.

*/

//#include "node-uda-c.h"
#include "bsh-dbus-node.h"

#define CMD16(s)  ((uint16_t)((uint16_t)(s)[0] << 8 | (uint16_t)(s)[1]))

static byte user_buf[DBUS_BUFLEN];
static unsigned int user_buflen;
static uint8_t cmd_mode = 0;

static uint8_t  dump_dest  = 0xb;
static uint32_t dump_begin = 0x08000000;
static uint32_t dump_end   = 0x08000100;
static uint32_t dump_pos   = 0;
static uint8_t  dump_chunk = 16;

extern "C" const dbus_node_info_t node_uda_c;

// ### Setup

static void node_setup() {
}

// ### Loop

static void node_loop(tx_se tx_state) {

	// Process user input submitted via terminal
	while (Serial.available()) {
		byte c = Serial.read();
		Serial.write(c);

		if (cmd_mode) {
			user_buf[user_buflen++] = c;
		} else if ((user_buflen == 0) && (c == ':')) {
			cmd_mode = 1;
		} else if (((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
		    && user_buflen < DBUS_BUFLEN) {
			if (user_buflen % 2) {
				user_buf[user_buflen / 2] |= (c % 32 + 9) % 25;
			} else {
				user_buf[user_buflen / 2] = (c % 32 + 9) % 25 << 4;
			}
			user_buflen++;
		}

		if (c != '\r' && c != '\n')
			continue;

		if (cmd_mode) {
			user_buf[user_buflen++] = 0;
			switch ((user_buf[0] << 8) | user_buf[1]) {
				case CMD16("dd"): dump_dest = strtol((const char*) &user_buf[2], NULL, 16); break;
				case CMD16("db"): dump_begin = strtol((const char*) &user_buf[2], NULL, 16); break;
				case CMD16("de"): dump_end = strtol((const char*) &user_buf[2], NULL, 16); break;
				case CMD16("dp"): dump_pos = strtol((const char*) &user_buf[2], NULL, 16); break;
				case CMD16("dc"): dump_chunk = strtol((const char*) &user_buf[2], NULL, 10); break;
				case CMD16("d1"): {
					uint32_t dump_now = dump_begin + dump_pos;
					uint8_t msg_f000[] = { (uint8_t) (node_uda_c.node_id << 4), dump_chunk,
						(uint8_t) (dump_now >> 8), (uint8_t) dump_now };
					dbus_tx_qpush(dump_dest << 4, 0xf000, msg_f000, sizeof(msg_f000));
					break; }
				case CMD16("d3"): {
					uint32_t dump_now = dump_begin + dump_pos;
					uint8_t msg_f001[] = { (uint8_t) (node_uda_c.node_id << 4), dump_chunk,
						(uint8_t) (dump_now >> 24), (uint8_t) (dump_now >> 16),
						(uint8_t) (dump_now >> 8), (uint8_t) dump_now };
					dbus_tx_qpush(dump_dest << 4, 0xf001, msg_f001, sizeof(msg_f001));
					break; }
			}
			Serial.printf("dump dest 0x%x, begin 0x%06x, end 0x%06x, pos 0x%06x, chunk %u\r\n",
				dump_dest, dump_begin, dump_end, dump_pos, dump_chunk);
			cmd_mode = 0;
			user_buflen = 0;
			continue;
		}

		if (user_buflen < 6 || user_buflen % 2) {
			Serial.println("Error: Invalid input length");
			user_buflen = 0;
			continue;
		}

		Serial.println();
		dbus_tx_qpush(user_buf[0], (user_buf[1] << 8) | user_buf[2], &user_buf[3], (user_buflen / 2) - 3);
		user_buflen = 0;
	}
}

// ### Frame handlers

static void handle_f100(const uint8_t *rx_frame) {
	char buf[128];
	char *bpos = buf;

	bpos += sprintf(bpos, "\r\n%08x: ", dump_pos);
	for (size_t i = 0; i < (rx_frame[0] - 4); i++) {
		bpos += sprintf(bpos, "%02x", rx_frame[i + 6]);
		if ((i % 2) && ((i + 1) < (rx_frame[0] - 4))) *bpos++ = ' ';
	}
        Serial.println(buf);

        dump_pos += dump_chunk;
        uint32_t dump_now = dump_begin + dump_pos;
        if (dump_now >= dump_end) return;

        uint32_t todo = dump_end - dump_now;
        uint8_t len = (todo < dump_chunk) ? todo : dump_chunk;

        if (rx_frame[3]) {
		uint8_t msg_f001[] = { (uint8_t) (node_uda_c.node_id << 4), len,
	                       (uint8_t) (dump_now >> 24), (uint8_t) (dump_now >> 16),
	                       (uint8_t) (dump_now >> 8), (uint8_t) dump_now };
		dbus_tx_qpush(dump_dest << 4, 0xf001, msg_f001, sizeof(msg_f001));
	} else {
		uint8_t msg_f000[] = { (uint8_t) (node_uda_c.node_id << 4), len,
	                       (uint8_t) (dump_now >> 8), (uint8_t) dump_now };
		dbus_tx_qpush(dump_dest << 4, 0xf000, msg_f000, sizeof(msg_f000));
	}
}

// ### Command table

static dbus_rx_cmd_entry_t rx_cmd_table[] = {

	{0xf100, handle_f100, "Read16 response"},
	{0xf101, handle_f100, "Read32 response"},

};

extern "C" const dbus_node_info_t node_uda_c __attribute__((used)) = {
    .node_id = 0xC,
    .rx_cmd_table = rx_cmd_table,
    .rx_cmd_table_len = sizeof(rx_cmd_table) / sizeof(rx_cmd_table[0]),
    .loop_func = node_loop,
    .setup_func = node_setup
};

