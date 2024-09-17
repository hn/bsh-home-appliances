import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

# ESPHome component based on VBus component by @ssieb - thanks!

CODEOWNERS = ["@hn"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

bshdbus_ns = cg.esphome_ns.namespace("bshdbus")
BSHDBus = bshdbus_ns.class_("BSHDBus", uart.UARTDevice, cg.Component)

CONF_BSHDBUS_ID = "bshdbus_id"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BSHDBus),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
