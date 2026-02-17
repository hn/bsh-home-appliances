from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_TRIGGER_ID

# ESPHome component based on VBus component by @ssieb - thanks!

CODEOWNERS = ["@hn"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

bshdbus_ns = cg.esphome_ns.namespace("bshdbus")
BSHDBus = bshdbus_ns.class_("BSHDBus", uart.UARTDevice, cg.Component)

CONF_BSHDBUS_ID = "bshdbus_id"
CONF_ON_FRAME = "on_frame"

FrameTrigger = bshdbus_ns.class_(
    "FrameTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.uint8).operator("ref")),
)

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BSHDBus),
        cv.Optional(CONF_ON_FRAME): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FrameTrigger),
            }
        ),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    for conf in config.get(CONF_ON_FRAME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (cg.std_vector.template(cg.uint8).operator("ref").operator("const"), "x"),
                (cg.uint8, "dest"),
                (cg.uint16, "command"),
                (cg.std_vector.template(cg.uint8).operator("ref").operator("const"), "message"),
            ],
            conf,
        )
