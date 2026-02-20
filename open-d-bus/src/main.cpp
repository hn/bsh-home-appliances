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

unsigned long led_tick;

void setup() {
	Serial.begin(115200);

#ifdef LED_PIN
	pinMode(LED_PIN, OUTPUT);
#endif

//	DBUS_ADDNODE(node_dishwasher_a_COM1);
//	DBUS_ADDNODE(node_dishwasher_1_39C3);
	DBUS_ADDNODE(node_uda_9);
	dbus_setup(Serial1, DBUS_RX, DBUS_TX);

	Serial.println("Open-D-Bus startup");
}

void loop() {
#ifdef LED_PIN
	if (millis() - led_tick >= 1000) {
		led_tick = millis();
		digitalWrite(LED_PIN, !digitalRead(LED_PIN));
	}
#endif

	dbus_loop();
}
