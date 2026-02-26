/*

   Open-D-Bus implementation for B/S/H/ D-Bus-2

   (C) 2025 Hajo Noerenberg

   https://www.noerenberg.de/
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
#include "FlexConsole.h"

unsigned long led_tick;

void setup() {
	console.begin(115200);

#ifdef LED_PIN
	pinMode(LED_PIN, OUTPUT);
#endif

//	DBUS_ADDNODE(node_dishwasher_a_COM1);
//	DBUS_ADDNODE(node_dishwasher_1_EPG60110_39C3);
	DBUS_ADDNODE(node_huda_c);
	dbus_setup(Serial1, DBUS_RX, DBUS_TX);

	console.println("Open-D-Bus started.");
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
