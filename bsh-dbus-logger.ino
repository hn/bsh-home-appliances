/*

   bsh-dbus-logger.ino
   
   B/S/H/ D-Bus data logger

   (C) 2024 Hajo Noerenberg

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

#include <SoftwareSerial.h>
#include "CRC16.h"
#include "CRC.h"

#define BSHDBUSRX D5
#define BSHDBUSTX D6    // not used here

#define FRAMEBUFLEN 64

SoftwareSerial dbus(BSHDBUSRX, BSHDBUSTX);

CRC16 crc(CRC16_XMODEM_POLYNOME,
          CRC16_XMODEM_INITIAL,
          CRC16_XMODEM_XOR_OUT,
          CRC16_XMODEM_REV_IN,
          CRC16_XMODEM_REV_OUT);

void setup() {
  // put your setup code here, to run once:

  dbus.begin(9600);   // 8N2 not supported, works nevertheless

  Serial.begin(115200);
  Serial.println("B/S/H/ D-Bus logger started.");

}

byte framebuf[FRAMEBUFLEN];
unsigned int framepos = 0;
unsigned int framelen = 0;

unsigned long readlast = 0;

void loop() {
  // put your main code here, to run repeatedly:

  while (dbus.available()) {
    unsigned long readnow = millis();
    byte rawbyte = dbus.read();

    if ((readnow - readlast) > 20) {
      if ((framelen > 0) && (framepos == framelen)) {
        Serial.printf("(ack=none)\n");
      } else if (framepos > 0) {
        Serial.printf("(read=timout)\n");
      }
      framepos = framelen = 0;
      crc.restart();
    } else if ((framelen > 0) && (framepos == framelen)) {
      framepos = framelen = 0;
      crc.restart();
      // Strictly speaking, this should not be necessary here
      // if the timing is precise, so it is a kind of workaround:
      if (rawbyte == ((framebuf[1] & 0xf0) | 0x0a)) {
        Serial.printf("%02x (ack=ok)\n", rawbyte);
        continue;
      } else {
        Serial.printf("(ack=err, re-evaluate byte)\n");
      }
    }

    readlast = readnow;
    crc.add(rawbyte);
    Serial.printf("%02x ", rawbyte);

    if (framepos < FRAMEBUFLEN) {
      framebuf[framepos++] = rawbyte;
    }

    if (framepos == 1) {
      framelen = 2 + 2 + (unsigned int) rawbyte;
      Serial.printf("| ");
    } else if (framepos == 4) {
      Serial.printf("| ");
    } else if (framepos == (framelen - 2)) {
      for (unsigned int p = framelen; p < 10; p++) {
        Serial.printf("   ");
      }
      Serial.printf("| ");
    } else if (framepos == framelen) {
      if (crc.calc() == 0) {
        Serial.printf("(crc=ok) | ");
        // We only process a small sample set of data here:
        if (memcmp(framebuf + 1, "\x14\x10\x04", 3) == 0) {
          Serial.printf("(Temperature=%d) ", framebuf[4]);
        } else if (memcmp(framebuf + 1, "\x14\x10\x05", 3) == 0) {
          Serial.printf("(Washing program=%d) ", framebuf[6]);
        } else if (memcmp(framebuf + 1, "\x14\x10\x06", 3) == 0) {
          Serial.printf("(Spin speed=%d) ", framebuf[4]);
        } else if (memcmp(framebuf + 1, "\x2a\x16\x00", 3) == 0) {
          Serial.printf("(Remaining time=%d) ", framebuf[4]);
        }
      } else {
        Serial.printf("(crc=err, len=%d)\n", framelen);
        framepos = framelen = 0;
        crc.restart();
      }
    }
  }
}
