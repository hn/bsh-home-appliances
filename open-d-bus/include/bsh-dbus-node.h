
#pragma once

#include "Arduino.h"
#include "CRC16.h"
#include <stdint.h>
#include <RingBuf.h>				// https://github.com/Locoduino/RingBuffer

// At 9600 baud, a single byte takes approximately 10 * 104.167 us = 1041.67 us to transmit
#define DBUS_BAUDRATE 9600
#define DBUS_BYTETIME ( 10 * 1000000UL / DBUS_BAUDRATE )

// Throw away frame fragment after timeout
#define DBUS_TIMEOUT ( 3 * DBUS_BYTETIME )

// nodes on the D-Bus seem to wait 8 to 18 byte-times silence before sending
#define DBUS_WAIT_MIN ( 8 * DBUS_BYTETIME )
#define DBUS_WAIT_MAX ( 18 * DBUS_BYTETIME )

#define DBUS_FRAME_MAX_SIZE (UINT8_MAX - 2 - 2)
#define DBUS_BUFLEN 256

#define DBUS_TX_QUEUE_SIZE (DBUS_BUFLEN * 8)
#define DBUS_RX_QUEUE_SIZE (DBUS_BUFLEN * 4)

#define DBUS_MAX_NODES 8

#define DBUS_ADDNODE(node) \
    do { \
        extern const dbus_node_info_t node; \
        dbus_addnode(&node); \
    } while(0)

#ifdef	DBUS_CB_DEBUG
#define DBUS_CB_DEBUG_PRINT(prefix, buf, len) do { \
        debugstr += prefix; \
        for (int i = 0; i < len; i++) { char b[3]; sprintf(b, "%02X", buf[i]); debugstr += b; } \
        debugstr += "\r\n"; \
        } while (0);
#else
#define DBUS_CB_DEBUG_PRINT(prefix, buf, len) do { } while (0);
#endif

enum tx_se {
	TX_IDLE,
	TX_RDYTOSEND,
	TX_WAITFORECHO,
	TX_WAITFORACK,
	TX_DELIVERED,
	TX_FAILURE,
};

enum rx_se {
	RX_READING,
	RX_WAITFORACKECHO,
};

typedef void (*rx_cmd_handler_t)(const uint8_t *rx_frame);

typedef struct {
	uint16_t cmd;
	rx_cmd_handler_t handler;
	const char *description;
} dbus_rx_cmd_entry_t;

typedef void (*dbus_node_loop_func_t)(tx_se tx_state);
typedef void (*dbus_node_setup_func_t)();

typedef struct {
	uint8_t node_id;
	const dbus_rx_cmd_entry_t *rx_cmd_table;
	size_t rx_cmd_table_len;
	dbus_node_loop_func_t loop_func;
	dbus_node_setup_func_t setup_func;
} dbus_node_info_t;

void dbus_setup(HardwareSerial &serialport, unsigned int pin_rx, unsigned int pin_tx);
int dbus_tx_qpush(uint8_t destnode, uint16_t cmd, const uint8_t *message, const uint8_t size);
void dbus_loop();
void dbus_addnode(const dbus_node_info_t *dbus_node);
