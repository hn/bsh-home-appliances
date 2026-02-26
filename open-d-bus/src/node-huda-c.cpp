
/*

  Since I can never come up with names for anything, I foolishly
  called this module HUDA. It stands for Hacker's UDA,
  which is a nod to B/S/H/’s 'Universal Diagnostic Adapter.

  Essentially, it’s a basic toolbox designed for
  tinkering with D-Bus participants.

  Quick'n'dirty hack style. Update: Do not look at the code,
  quality is catastrophic. Zero time for anything.

*/

//#include "node-huda-c.h"
#include "bsh-dbus-node.h"
#include "FlexConsole.h"

#define CMD16(s)  ((uint16_t)((uint16_t)(s)[0] << 8 | (uint16_t)(s)[1]))

#define CMD_READ16_REQUEST	0xf000
#define CMD_READ32_REQUEST	0xf001
#define CMD_READ16_RESPONSE	0xf100
#define CMD_READ32_RESPONSE	0xf101

#define CMD_ID_REQUEST		0xff00
#define CMD_ID_RESPONSE		0xfe00

#define CMD_SET_MODULE		0xf300


static byte user_buf[DBUS_BUFLEN];
static unsigned int user_buflen;
static uint8_t cmd_mode = 0;

static uint8_t  work_node  = 0xb;
static uint8_t  work_module  = 0;
static uint8_t  work_bits  = 0; // 0 undef, 16 bit, 32 bit

static uint32_t memory_begin = 0x08000000;
static uint32_t memory_end   = 0x08000100;
static uint32_t memory_pos   = 0;
static uint8_t  memory_chunk = 16;

extern "C" const dbus_node_info_t node_huda_c;

// ### Setup

static void node_setup() {
}

// ### Loop

static void node_loop(tx_se tx_state) {

	// Process user input submitted via terminal
	while (console.available()) {
		byte c = console.read();
		console.write(c);

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

				/* Node commands */

				case CMD16("nn"): // set node number
					work_node = strtol((const char*) &user_buf[2], NULL, 16);
					work_bits = 0;
					break;
				case CMD16("nm"): { // set node module
					work_module = strtol((const char*) &user_buf[2], NULL, 16);
					uint8_t msg_CMD_SET_MODULE[] = { work_module };
					dbus_tx_qpush(work_node << 4, CMD_SET_MODULE, msg_CMD_SET_MODULE, sizeof(msg_CMD_SET_MODULE));
					} // break; // no break here by intention. Request node info to validate if module exists.
				case CMD16("ni"): { // request info
					uint8_t msg_CMD_ID_REQUEST[] = { (uint8_t) (node_huda_c.node_id << 4) };
					dbus_tx_qpush(work_node << 4, CMD_ID_REQUEST, msg_CMD_ID_REQUEST, sizeof(msg_CMD_ID_REQUEST));
					break; }

				/* Memory dump commands */

				case CMD16("mb"): memory_begin = strtol((const char*) &user_buf[2], NULL, 16); break;
				case CMD16("me"): memory_end = strtol((const char*) &user_buf[2], NULL, 16); break;
				case CMD16("mp"): memory_pos = strtol((const char*) &user_buf[2], NULL, 16); break;
				case CMD16("mc"): memory_chunk = strtol((const char*) &user_buf[2], NULL, 10); break;
				case CMD16("md"): {
					if (work_bits == 0) {
						console.println("Check node bits first with 'ni'");
						break;
					} else if (work_bits == 16) {
						uint32_t memory_now = memory_begin + memory_pos;
						uint8_t msg_CMD_READ16_REQUEST[] = { (uint8_t) (node_huda_c.node_id << 4), memory_chunk,
							(uint8_t) (memory_now >> 8), (uint8_t) memory_now };
						dbus_tx_qpush(work_node << 4, CMD_READ16_REQUEST, msg_CMD_READ16_REQUEST, sizeof(msg_CMD_READ16_REQUEST));
						break;
					} else {
						uint32_t memory_now = memory_begin + memory_pos;
						uint8_t msg_CMD_READ32_REQUEST[] = { (uint8_t) (node_huda_c.node_id << 4), memory_chunk,
							(uint8_t) (memory_now >> 24), (uint8_t) (memory_now >> 16),
							(uint8_t) (memory_now >> 8), (uint8_t) memory_now };
						dbus_tx_qpush(work_node << 4, CMD_READ32_REQUEST, msg_CMD_READ32_REQUEST, sizeof(msg_CMD_READ32_REQUEST));
						break;
					} }

			}
			console.printf("| NODE number=0x%x module=%d bits=%d | MEMORY begin=0x%06x end=0x%06x pos=0x%06x chunk=%u |\r\n",
				work_node, work_module, work_bits, memory_begin, memory_end, memory_pos, memory_chunk);
			cmd_mode = 0;
			user_buflen = 0;
			continue;
		}

		if (user_buflen < 6 || user_buflen % 2) {
			console.println("Error: Invalid input length");
			user_buflen = 0;
			continue;
		}

		console.println();
		dbus_tx_qpush(user_buf[0], (user_buf[1] << 8) | user_buf[2], &user_buf[3], (user_buflen / 2) - 3);
		user_buflen = 0;
	}
}

// ### Frame handlers

static void handle_CMD_READ16_RESPONSE(const uint8_t *rx_frame) {
	char buf[256];
	char *bpos = buf;

	bpos += sprintf(bpos, "\r\n%08x: ", memory_pos);

	// hex print
	for (size_t i = 0; i < (rx_frame[0] - 4); i++) {
		bpos += sprintf(bpos, "%02x", rx_frame[i + 6]);
		if ((i % 2) && ((i + 1) < (rx_frame[0] - 4))) *bpos++ = ' ';
	}

	bpos += sprintf(bpos, "  "); // delimiter between hex and ascii print

	// sane ascii print
	for (size_t i = 0; i < (rx_frame[0] - 4); i++) {
		uint8_t ascii = rx_frame[i + 6];
		if (ascii < 32 || ascii > 126) ascii = '.';
		bpos += sprintf(bpos, "%c", ascii);
	}

        console.println(buf);

        memory_pos += memory_chunk;
        uint32_t memory_now = memory_begin + memory_pos;
        if (memory_now >= memory_end) return;

        uint32_t todo = memory_end - memory_now;
        uint8_t len = (todo < memory_chunk) ? todo : memory_chunk;

        if (rx_frame[3]) {
		uint8_t msg_CMD_READ32_REQUEST[] = { (uint8_t) (node_huda_c.node_id << 4), len,
	                       (uint8_t) (memory_now >> 24), (uint8_t) (memory_now >> 16),
	                       (uint8_t) (memory_now >> 8), (uint8_t) memory_now };
		dbus_tx_qpush(work_node << 4, CMD_READ32_REQUEST, msg_CMD_READ32_REQUEST, sizeof(msg_CMD_READ32_REQUEST));
	} else {
		uint8_t msg_CMD_READ16_REQUEST[] = { (uint8_t) (node_huda_c.node_id << 4), len,
	                       (uint8_t) (memory_now >> 8), (uint8_t) memory_now };
		dbus_tx_qpush(work_node << 4, CMD_READ16_REQUEST, msg_CMD_READ16_REQUEST, sizeof(msg_CMD_READ16_REQUEST));
	}
}

static void handle_CMD_ID_RESPONSE(const uint8_t *rx_frame) {

	// 06 | 28.FE-00 | 1000F9D0 | 150C
	// 08 | A0.FE-00 | 60000001FFA8 | DEAC

	if (rx_frame[0] == 6) {
		memory_begin = (uint16_t) (rx_frame[6] << 8) | rx_frame[7];
		memory_end = memory_begin + 32;
		memory_chunk = 16;
		work_module = rx_frame[5];
		work_bits = 16;
		console.printf(", Node 0x%x, Module %d, info string at 0x%04x (16 bit node)\r\n",
				(rx_frame[4] >> 4), work_module, memory_begin );
	} else if (rx_frame[0] == 8) {
		memory_begin = (uint32_t) (rx_frame[6] << 24) | (rx_frame[7] << 16) | (rx_frame[8] << 8) | rx_frame[9];
		memory_end = memory_begin + 128;
		memory_chunk = 32;
		work_module = rx_frame[5];
		work_bits = 32;
		console.printf(", Node 0x%x, Module %d, info string at 0x%08x (32 bit node)\r\n",
				(rx_frame[4] >> 4), work_module, memory_begin );
	} else {
	        console.println(", Invalid or unknown identify response");
	        return;
	}

	memory_pos = 0;
}

// ### Command table

static dbus_rx_cmd_entry_t rx_cmd_table[] = {

	{CMD_READ16_RESPONSE, handle_CMD_READ16_RESPONSE, "Read16 response"},
	{CMD_READ32_RESPONSE, handle_CMD_READ16_RESPONSE, "Read32 response"},
	{CMD_ID_RESPONSE, handle_CMD_ID_RESPONSE, "Identify response"},

};

extern "C" const dbus_node_info_t node_huda_c __attribute__((used)) = {
    .node_id = 0xC,
    .rx_cmd_table = rx_cmd_table,
    .rx_cmd_table_len = sizeof(rx_cmd_table) / sizeof(rx_cmd_table[0]),
    .loop_func = node_loop,
    .setup_func = node_setup
};

