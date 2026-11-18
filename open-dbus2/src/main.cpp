#include "DBus2Log.h"
#include "DBus2Manager.h"
#include "nodes/Huda.h"
#include "nodes/LedStatus.h"
#include "nodes/Monitor.h"
#include "nodes/PingDemo.h"
#include "nodes/PongDemo.h"
// #include "nodes/Dishwasher39C3.h"
// #include "nodes/DishwasherCtrlA.h"
// #include "nodes/DishwasherUIPSimple.h"
// #include "nodes/MqttGateway.h"
// #include "nodes/UnbalanceDemo.h"

#ifndef DBUS_PIN_RX
  #error "DBUS_PIN_RX must be defined in platformio.ini build_flags"
#endif
#ifndef DBUS_PIN_TX
  #error "DBUS_PIN_TX must be defined in platformio.ini build_flags"
#endif

static const char *TAG = "DBus2";

DBus2::Manager          dbus(UART_NUM_1);
FlexConsole             console;
DBus2::Nodes::PingDemo  ping;
DBus2::Nodes::PongDemo  pong;
DBus2::Nodes::Monitor   monitor;
DBus2::Nodes::LedStatus leds;
DBus2::Nodes::Huda      huda;
// DBus2::Nodes::Dishwasher39C3      dishwasher39C3;
// DBus2::Nodes::DishwasherCtrlA     dishwasherCtrlA;
// DBus2::Nodes::DishwasherUIPSimple dishwasherUIPSimple;
// DBus2::Nodes::MqttGateway         mqttGateway;
// DBus2::Nodes::UnbalanceDemo       unbalanceDemo;

void setup() {
  console.begin(115200);
  DBUS2_LOGI("Starting Open-DBus2");

  if (!dbus.registerNode(&ping))
    DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", ping.getName(), ping.getNodeId());
  if (!dbus.registerNode(&pong))
    DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", pong.getName(), pong.getNodeId());
  if (!dbus.registerNode(&monitor))
    DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", monitor.getName(), monitor.getNodeId());
  if (!dbus.registerNode(&leds))
    DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", leds.getName(), leds.getNodeId());
  if (!dbus.registerNode(&huda))
    DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", huda.getName(), huda.getNodeId());
  // if (!dbus.registerNode(&dishwasher39C3))
  //   DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", dishwasher39C3.getName(), dishwasher39C3.getNodeId());
  // if (!dbus.registerNode(&dishwasherCtrlA))
  //   DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", dishwasherCtrlA.getName(), dishwasherCtrlA.getNodeId());
  // if (!dbus.registerNode(&dishwasherUIPSimple))
  //   DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", dishwasherUIPSimple.getName(),
  //   dishwasherUIPSimple.getNodeId());
  // if (!dbus.registerNode(&mqttGateway))
  //   DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", mqttGateway.getName(), mqttGateway.getNodeId());
  // if (!dbus.registerNode(&unbalanceDemo))
  //   DBUS2_LOGE("Failed to register node \"%s\" (id 0x%02X)", unbalanceDemo.getName(), unbalanceDemo.getNodeId());

  if (!dbus.begin(DBUS_PIN_RX, DBUS_PIN_TX, 9600)) {
    DBUS2_LOGE("Bus initialization failed on UART%d (baud %d)", UART_NUM_1, 9600);
    while (1)
      delay(1000);
  }

  DBUS2_LOGI("System online");
}

void loop() {
  dbus.process();
}
