/*

   FLexConsole is a minimally intrusive approach to duplicate serial i/o
   to a web frontend. Uses MycilaWebSerial by @mathieucarbou.

   Is 'works for most cases' quality.

   (C) 2026 Hajo Noerenberg

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

#include "FlexConsole.h"
#include <ESPAsyncWebServer.h>

#if defined(WIFI_SSID)
  #include <WiFi.h>
#endif

FlexConsole::FlexConsole() {}

void FlexConsole::pushWebData(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        _webRxBuffer.push(data[i]);
    }
    _webRxBuffer.push('\n');
}

void FlexConsole::begin(unsigned long baud) {
    Serial.begin(baud);

    #if defined(WIFI_SSID)
        Serial.printf("\n[FlexConsole] Connecting to %s ", WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        
        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {

            _server = new AsyncWebServer(80);
            _server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/webserial"); });
            
            _webSerial.begin(_server);
            _webSerial.setBuffer(100);
            _webSerial.onMessage([this](uint8_t* data, size_t len) {
                this->pushWebData(data, len);
            });
            
            _server->begin();
            
            Serial.println("\n[FlexConsole] WiFi connected!");
            Serial.printf("[FlexConsole] IP Address: %s\n", WiFi.localIP().toString().c_str());
            Serial.println("[FlexConsole] WebSerial active at /webserial");
        } else {
            Serial.println("\n[FlexConsole] WiFi connection failed.");
        }
    #endif
}

size_t FlexConsole::write(uint8_t character) {
    size_t s = Serial.write(character);
    #if defined(WIFI_SSID)
        if (WiFi.status() == WL_CONNECTED) {
            _webSerial.write(character);
        }
    #endif
    return s;
}

size_t FlexConsole::write(const uint8_t *buffer, size_t size) {
    size_t s = Serial.write(buffer, size);
    #if defined(WIFI_SSID)
        if (WiFi.status() == WL_CONNECTED) {
            _webSerial.write(buffer, size);
        }
    #endif
    return s;
}

int FlexConsole::available() {
    return _webRxBuffer.size() + Serial.available();
}

int FlexConsole::read() {
    if (!_webRxBuffer.isEmpty()) {
        uint8_t c;
        _webRxBuffer.pop(c);
        return c;
    }
    return Serial.read();
}

int FlexConsole::peek() {
    if (!_webRxBuffer.isEmpty()) {
        uint8_t c;
        _webRxBuffer.peek(c, 0); 
        return c;
    }
    return Serial.peek();
}

void FlexConsole::flush() {
    Serial.flush();
}

FlexConsole console;
