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

#if defined(WIFI_SSID)
  #include <ESPAsyncWebServer.h>
  #include <WiFi.h>
#endif

FlexConsole::FlexConsole() {
}

void FlexConsole::pushWebData(uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    _webRxBuffer.push_back(data[i]);
  }
  _webRxBuffer.push_back('\n');
}

void FlexConsole::begin(unsigned long baud) {
  Serial.begin(baud);

#if defined(WIFI_SSID)
  _server = new AsyncWebServer(80);
  _server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/webserial"); });

  #if defined(WSL_CUSTOM_PAGE)
  _webSerial.setCustomHtmlPage(WEBSERIAL_HTML, WEBSERIAL_HTML_SIZE, "gzip");
  #endif
  _webSerial.begin(_server);
  // Buffer up to 100 bytes before flushing to WebSocket clients
  _webSerial.setBuffer(100);
  _webSerial.onMessage([this](uint8_t *data, size_t len) { this->pushWebData(data, len); });

  #if defined(OTA_PASS)
  _server
      ->on("/ota", HTTP_GET,
           [](AsyncWebServerRequest *r) {
             r->send(200, "text/html",
                     "<form method='POST' action='/ota' enctype='multipart/form-data'>"
                     "<input type='file' name='firmware'>"
                     "<input type='submit' value='Update'></form>");
           })
      .setAuthentication("ota", OTA_PASS);

  _server
      ->on(
          "/ota", HTTP_POST,
          [](AsyncWebServerRequest *r) {
            r->send(200, "text/plain", "OK, rebooting...");
            esp_restart();
          },
          [](AsyncWebServerRequest *r, const String &fn, size_t idx, uint8_t *data, size_t len, bool final) {
            if (idx == 0)
              Update.begin(UPDATE_SIZE_UNKNOWN);
            Update.write(data, len);
            if (final)
              Update.end(true);
          })
      .setAuthentication("ota", OTA_PASS);
  #endif

  Serial.printf("\n[FlexConsole] Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startAttempt = millis();
  while ((WiFi.status() != WL_CONNECTED) && (millis() - startAttempt < 10000)) {
    delay(500);
    Serial.print(".");
  }

  _server->begin();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[FlexConsole] WiFi connected!");
    Serial.printf("[FlexConsole] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.println("[FlexConsole] WebSerial active at /webserial");
  #if defined(OTA_PASS)
    Serial.println("[FlexConsole] OTA active at /ota (password protected)");
  #endif
  } else {
    Serial.println("\n[FlexConsole] WiFi connection timed out (server still running).");
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
  if (!_webRxBuffer.empty()) {
    uint8_t c = _webRxBuffer.front();
    _webRxBuffer.pop_front();
    return c;
  }
  return Serial.read();
}

int FlexConsole::peek() {
  if (!_webRxBuffer.empty()) {
    return _webRxBuffer.front();
  }
  return Serial.peek();
}

void FlexConsole::flush() {
  Serial.flush();
}
