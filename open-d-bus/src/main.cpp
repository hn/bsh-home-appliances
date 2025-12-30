/*

   Open-D-Bus implementation for B/S/H/ D-Bus-2

   This is an independent, open-source project and is not affiliated with,
   endorsed by, or sponsored by B/S/H/ or its affiliates. All product names
   and trademarks are the property of their respective owners. References
   to B/S/H/ appliances are for descriptive purposes only and do not imply
   any association with B/S/H/.

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

#include "Arduino.h"
#include "bsh-dbus-node.h"

#define LED_PIN 8

#define DBUS_RX 6
#define DBUS_TX 7

byte user_buf[DBUS_BUFLEN];
unsigned int user_buflen;

void setup() {
	Serial.begin(115200);

	pinMode(LED_PIN, OUTPUT);

	dbus_setup(Serial1, DBUS_RX, DBUS_TX);

	Serial.println("Open-D-Bus startup");
}

void loop() {
	if (millis() & 1024)
		digitalWrite(LED_PIN, !digitalRead(LED_PIN));

	dbus_loop();

	// Process D-Bus command submitted by the user via terminal
	while (Serial.available()) {
		byte c = Serial.read();
		Serial.write(c);
		if (((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
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
