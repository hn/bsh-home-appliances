import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
    CONF_TEXT_SENSORS,
    CONF_COMMAND,
    CONF_DEST,
    CONF_LAMBDA,
)
from .. import (
    bshdbus_ns,
    BSHDBus,
    CONF_BSHDBUS_ID,
)

BSHDBusCustom = bshdbus_ns.class_("BSHDBusCustomTSensor", cg.Component)
BSHDBusCustomSub = bshdbus_ns.class_("BSHDBusCustomSubTSensor", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BSHDBusCustom),
            cv.GenerateID(CONF_BSHDBUS_ID): cv.use_id(BSHDBus),
            cv.Required(CONF_COMMAND): cv.uint16_t,
            cv.Required(CONF_DEST): cv.uint8_t,
            cv.Optional(CONF_TEXT_SENSORS): cv.ensure_list(
                text_sensor.text_sensor_schema().extend(
                    {
                        cv.GenerateID(): cv.declare_id(BSHDBusCustomSub),
                        cv.Required(CONF_LAMBDA): cv.lambda_,
                    }
                )
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
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
