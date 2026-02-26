#pragma once
#include <Arduino.h>
#include <RingBuf.h>
#include <MycilaWebSerial.h>

class AsyncWebServer;

class FlexConsole : public Stream {
  private:
    RingBuf<uint8_t, 512> _webRxBuffer;
    WebSerial _webSerial;
    AsyncWebServer* _server = nullptr;

  public:
    FlexConsole();

    void begin(unsigned long baud = 115200);
    
    void pushWebData(uint8_t* data, size_t len);

    virtual size_t write(uint8_t character) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual void flush() override;
};

extern FlexConsole console;
