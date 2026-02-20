/*

   bsh-dbus-node.cpp

   Reads D-Bus-2 traffic from UART, acknowledges valid frames for local nodes,
   locally queues und processes frames. The subsystem nibble is ignored for now.

   I'm not a developer. This code is a direct artifact of reverse engineering and
   any resemblance to “good code” is purely coincidental. Feel free to improve.

   (C) 2025 Hajo Noerenberg

   http://www.noerenberg.de/
   https://github.com/hn/bsh-home-appliances

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3.0 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program. If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.

*/

#include "bsh-dbus-node.h"

RingBuf < byte, DBUS_TX_QUEUE_SIZE > tx_queue;
byte tx_buf[DBUS_BUFLEN];
unsigned int tx_len = 0;
unsigned int tx_try = 0;
unsigned long tx_last = 0;
unsigned int tx_wait = DBUS_WAIT_MAX;
volatile enum tx_se tx_state = TX_IDLE;

RingBuf < byte, DBUS_RX_QUEUE_SIZE > rx_queue;
byte rx_buf[DBUS_BUFLEN];
byte rx_ackecho;
unsigned int rx_pos = 0;
volatile unsigned long rx_last = 0;
enum rx_se rx_state = RX_READING;
CRC16 rx_crc(CRC16_XMODEM_POLYNOME, CRC16_XMODEM_INITIAL, CRC16_XMODEM_XOR_OUT, CRC16_XMODEM_REV_IN,
	     CRC16_XMODEM_REV_OUT);

static const dbus_node_info_t *dbus_node_registry[DBUS_MAX_NODES];
static int dbus_node_count = 0;
bool dbus_node_id_cache[0xF] = { 0 };

#ifdef  DBUS_CB_DEBUG
String debugstr = "";
#endif

void rx_callback() {
	// Probably too heavy a callback function, but ACKs must be sent immediately after the end of a frame.
	// Hopefully not a problem, as the documentation states that this callback is created
	// in a separate task and not directly in an ISR.

	if ((rx_pos > 0) && ((micros() - rx_last) > DBUS_TIMEOUT)) {
		DBUS_CB_DEBUG_PRINT("unprocessed bytes: ", rx_buf, rx_pos)
		    rx_pos = 0;
		rx_state = RX_READING;
		rx_crc.restart();
	}

	while (Serial1.available()) {
		byte rx_byte = Serial1.read();
		rx_last = micros();

		if ((rx_state == RX_WAITFORACKECHO) && (rx_byte == rx_ackecho)) {
			rx_state = RX_READING;
			DBUS_CB_DEBUG_PRINT("ACK echo ok", "", 0);
			continue;
		}

		if ((tx_state == TX_WAITFORACK) && (rx_byte == ((tx_buf[1] & 0xF0) | 0x0A))) {
			tx_state = TX_DELIVERED;
			DBUS_CB_DEBUG_PRINT("tx ACK received from dest", "", 0);
			continue;
		}

		if (rx_pos >= DBUS_FRAME_MAX_SIZE) {
			DBUS_CB_DEBUG_PRINT("max frame size exceeded", "", 0);
			rx_pos = 0;
			rx_crc.restart();
		}

		rx_buf[rx_pos++] = rx_byte;
		rx_crc.add(rx_byte);
		rx_state = RX_READING;

		if (rx_pos == (1 + 1 + (unsigned int)rx_buf[0] + 2)) {	// LL + DS + MM[] + CRC
			if (rx_crc.calc()) {
				DBUS_CB_DEBUG_PRINT("CRC invalid for: ", rx_buf, rx_pos);
			} else {
				if ((tx_state == TX_WAITFORECHO) && (rx_pos == tx_len)
				    && (memcmp(tx_buf, rx_buf, tx_len) == 0)) {
					tx_state = TX_WAITFORACK;
					DBUS_CB_DEBUG_PRINT("CRC ok, frame echo received, waiting for tx ACK: ", rx_buf, rx_pos);
				} else if (dbus_node_id_cache[rx_buf[1] >> 4]) {
					if ((rx_pos + rx_queue.size()) >= rx_queue.maxSize()) {
						DBUS_CB_DEBUG_PRINT("CRC ok, received frame for me, queue full, no ACK for: ", rx_buf, rx_pos);
						rx_pos = 0;
					} else {
						rx_ackecho = (rx_buf[1] & 0xF0) | 0xA;
						Serial1.write(rx_ackecho);
						rx_state = RX_WAITFORACKECHO;
						DBUS_CB_DEBUG_PRINT("CRC ok, received frame for me, sent ACK for: ", rx_buf, rx_pos);
					}
				} else {
					DBUS_CB_DEBUG_PRINT("CRC ok, promisc frame received: ", rx_buf, rx_pos);
				}
				for (int i = 0; i < rx_pos; i++)
					rx_queue.push(rx_buf[i]);
			}
			rx_pos = 0;
			rx_crc.restart();
		}
	}
}

int dbus_tx_qpush(uint8_t destnode, uint16_t cmd, const uint8_t * message, const uint8_t size) {
	tx_queue.push(1 + 2 + size);		// DS + CMD + (MM[] - CMD)
	tx_queue.push(destnode);
	tx_queue.push(cmd >> 8);
	tx_queue.push(cmd & 0xFF);
	for (int i = 0; i < size; i++)
		tx_queue.push(message[i]);
	return size;
}

void dbus_setup(HardwareSerial & serialport, unsigned int pin_rx, unsigned int pin_tx) {

	serialport.begin(DBUS_BAUDRATE, SERIAL_8N1, pin_rx, pin_tx);

	// sets the callback function that will be
	// executed BOTH when full and after RX Timeout
	serialport.onReceive(rx_callback, false);

	// Zero disables timeout, thus, onReceive callback will only be
	// called when RX FIFO reaches setRxFIFOFull bytes
	serialport.setRxTimeout(0);

	// set the FIFO Full threshold for the interrupt to 1,
	// this will force to emulate byte-by-byte reading
	serialport.setRxFIFOFull(1);

	for (int n = 0; n < dbus_node_count; n++) {
		dbus_node_id_cache[dbus_node_registry[n]->node_id] = 1;
	}

	for (int n = 0; n < dbus_node_count; n++) {
		if (dbus_node_registry[n]->setup_func) dbus_node_registry[n]->setup_func();
	}
}

void dbus_loop() {

#ifdef  DBUS_CB_DEBUG
	if (debugstr.length() > 0) {
		Serial.print("Debug: ");
		Serial.print(debugstr);
		debugstr = "";
	}
#endif

	byte p_len;
	while (rx_queue.lockedPeek(p_len, 0) && (rx_queue.size() >= (p_len + 2 + 2))) {
		byte p_buf[DBUS_BUFLEN];

		for (int i = 0; i < (1 + 1 + p_len + 2); i++) {	// LL + DS + MM[] + CRC
			byte d;
			char h[4];
			rx_queue.lockedPop(d);
			p_buf[i] = d;
			sprintf(h, "%02X", d);

			if (i == 1) {
				Serial.printf(" | ");
			} else if (i == 2) {
				Serial.printf(".");
			} else if (i == 3) {
				Serial.printf("-");
			} else if (i == 4) {
				Serial.printf(" | ");
			} else if (i == p_len + 2) {
				Serial.printf(" | ");
			}

			Serial.print(h);
		}

		for (int n = 0; n < dbus_node_count; n++) {
			if ((p_buf[1] >> 4) == dbus_node_registry[n]->node_id) {
				for (size_t j = 0; j < dbus_node_registry[n]->rx_cmd_table_len; j++) {
					if (dbus_node_registry[n]->rx_cmd_table[j].cmd == (p_buf[2] << 8 | p_buf[3])) {
						Serial.print(", handled by: ");
						Serial.print(dbus_node_registry[n]->rx_cmd_table[j].description);
						dbus_node_registry[n]->rx_cmd_table[j].handler(p_buf);
						break;
					}
				}
			}
		}

		Serial.println();
	}

	if (tx_state == TX_DELIVERED) {
		// ...
		tx_state = TX_IDLE;
	} else if (tx_state == TX_FAILURE) {
		// ...
		tx_state = TX_IDLE;
	}

	if ((tx_state == TX_IDLE) && !tx_queue.isEmpty()) {
		CRC16 tx_crc(CRC16_XMODEM_POLYNOME, CRC16_XMODEM_INITIAL, CRC16_XMODEM_XOR_OUT, CRC16_XMODEM_REV_IN,
			     CRC16_XMODEM_REV_OUT);
		byte l;
		tx_queue.pop(l);		// DS + MM[]
		tx_buf[0] = l - 1;		// MM[]
		tx_len = 1 + l + 2;		// LL + DS + MM[] + CRC
		for (int i = 0; i < l; i++)
			tx_queue.pop(tx_buf[1 + i]);
		tx_crc.add(tx_buf, tx_len - 2);
		uint16_t ccrc = tx_crc.calc();
		tx_buf[tx_len - 2] = (ccrc >> 8) & 0xFF;
		tx_buf[tx_len - 1] = ccrc & 0xFF;

		tx_try = 3;
		tx_state = TX_RDYTOSEND;
	}

	// this is crude ... but works
	unsigned long l_rx_last;
	noInterrupts();
	l_rx_last = rx_last;
	interrupts();

	// delta_tx: wait a little while for tx echo
	// delta_rx: wait for silence before sending
	if (((micros() - tx_last) > DBUS_WAIT_MIN) && ((micros() - l_rx_last) > tx_wait)) {
		if ((tx_state == TX_WAITFORECHO) || (tx_state == TX_WAITFORACK)) {
			if (tx_try) {
				tx_state = TX_RDYTOSEND;
			} else {
				tx_state = TX_FAILURE;
			}
		}
		if (tx_state == TX_RDYTOSEND) {
			Serial1.write(tx_buf, tx_len);
			tx_last = micros();
			tx_state = TX_WAITFORECHO;
			tx_try--;
			tx_wait = random(DBUS_WAIT_MIN, DBUS_WAIT_MAX);
		}
	}

	for (int i = 0; i < dbus_node_count; i++) {
		if (dbus_node_registry[i]->loop_func) dbus_node_registry[i]->loop_func(tx_state);
	}
}

void dbus_addnode(const dbus_node_info_t *dbus_node) {
    if (dbus_node_count < DBUS_MAX_NODES)
        dbus_node_registry[dbus_node_count++] = dbus_node;
}
