import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMMAND,
    CONF_DEST,
    CONF_ID,
    CONF_LAMBDA,
    CONF_TEXT_SENSORS,
)
from esphome.types import ConfigType

from .. import CONF_BSHDBUS_ID, BSHDBus, bshdbus_ns

BSHDBusTextSensor = bshdbus_ns.class_("BSHDBusTextSensor", cg.Component)
BSHDBusTextSensorSub = bshdbus_ns.class_("BSHDBusSubTextSensor", text_sensor.TextSensor)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BSHDBusTextSensor),
        cv.GenerateID(CONF_BSHDBUS_ID): cv.use_id(BSHDBus),
        cv.Required(CONF_COMMAND): cv.uint16_t,
        cv.Required(CONF_DEST): cv.uint8_t,
        cv.Required(CONF_TEXT_SENSORS): cv.ensure_list(
            text_sensor.text_sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(BSHDBusTextSensorSub),
                    cv.Required(CONF_LAMBDA): cv.lambda_,
                }
            )
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_COMMAND in config:
        cg.add(var.set_command(config[CONF_COMMAND]))
    if CONF_DEST in config:
        cg.add(var.set_dest(config[CONF_DEST]))
    tsensors = []
    for conf in config[CONF_TEXT_SENSORS]:
        tsens = await text_sensor.new_text_sensor(conf)
        lambda_ = await cg.process_lambda(
            conf[CONF_LAMBDA],
            [(cg.std_vector.template(cg.uint8), "x")],
            return_type=cg.std_string,
        )
        cg.add(tsens.set_message_parser(lambda_))
        tsensors.append(tsens)
    cg.add(var.set_tsensors(tsensors))

    bshdbus = await cg.get_variable(config[CONF_BSHDBUS_ID])
    cg.add(bshdbus.register_listener(var))
