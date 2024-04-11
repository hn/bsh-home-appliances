import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_COMMAND,
    CONF_DEST,
    CONF_LAMBDA,
    CONF_SENSORS,
)
from .. import (
    bshdbus_ns,
    BSHDBus,
    CONF_BSHDBUS_ID,
)

BSHDBusCustom = bshdbus_ns.class_("BSHDBusCustomSensor", cg.Component)
BSHDBusCustomSub = bshdbus_ns.class_("BSHDBusCustomSubSensor", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BSHDBusCustom),
            cv.GenerateID(CONF_BSHDBUS_ID): cv.use_id(BSHDBus),
            cv.Required(CONF_COMMAND): cv.uint16_t,
            cv.Required(CONF_DEST): cv.uint8_t,
            cv.Optional(CONF_SENSORS): cv.ensure_list(
                sensor.sensor_schema().extend(
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
    sensors = []
    for conf in config[CONF_SENSORS]:
        sens = await sensor.new_sensor(conf)
        lambda_ = await cg.process_lambda(
            conf[CONF_LAMBDA],
            [(cg.std_vector.template(cg.uint8), "x")],
            return_type=cg.float_,
        )
        cg.add(sens.set_message_parser(lambda_))
        sensors.append(sens)
    cg.add(var.set_sensors(sensors))

    bshdbus = await cg.get_variable(config[CONF_BSHDBUS_ID])
    cg.add(bshdbus.register_listener(var))
