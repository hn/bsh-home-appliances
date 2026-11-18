#pragma once

#include <Arduino.h>
#include <deque>

#if defined(WIFI_SSID)
  #include "FlexConsolePage.h"
  #include <MycilaWebSerial.h>
  #if defined(OTA_PASS)
    #include <Update.h>
  #endif
class AsyncWebServer;
#endif

class FlexConsole : public Stream {
private:
  std::deque<uint8_t> _webRxBuffer;
#if defined(WIFI_SSID)
  WebSerial       _webSerial;
  AsyncWebServer *_server = nullptr;
#endif

public:
  FlexConsole();

  void begin(unsigned long baud = 115200);

  void pushWebData(uint8_t *data, size_t len);

  virtual size_t write(uint8_t character) override;
  virtual size_t write(const uint8_t *buffer, size_t size) override;
  virtual int    available() override;
  virtual int    read() override;
  virtual int    peek() override;
  virtual void   flush() override;
};

extern FlexConsole console;
