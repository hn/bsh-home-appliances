import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSORS,
    CONF_COMMAND,
    CONF_DEST,
    CONF_ID,
    CONF_LAMBDA,
)
from esphome.types import ConfigType

from .. import CONF_BSHDBUS_ID, BSHDBus, bshdbus_ns

BSHDBusBinarySensor = bshdbus_ns.class_("BSHDBusBinarySensor", cg.Component)
BSHDBusBinarySensorSub = bshdbus_ns.class_(
    "BSHDBusSubBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BSHDBusBinarySensor),
        cv.GenerateID(CONF_BSHDBUS_ID): cv.use_id(BSHDBus),
        cv.Required(CONF_COMMAND): cv.uint16_t,
        cv.Required(CONF_DEST): cv.uint8_t,
        cv.Required(CONF_BINARY_SENSORS): cv.ensure_list(
            binary_sensor.binary_sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(BSHDBusBinarySensorSub),
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
    bsensors = []
    for conf in config[CONF_BINARY_SENSORS]:
        bsens = await binary_sensor.new_binary_sensor(conf)
        lambda_ = await cg.process_lambda(
            conf[CONF_LAMBDA],
            [(cg.std_vector.template(cg.uint8).operator("ref").operator("const"), "x")],
            return_type=cg.bool_,
        )
        cg.add(bsens.set_message_parser(lambda_))
        bsensors.append(bsens)
    cg.add(var.set_bsensors(bsensors))

    bshdbus = await cg.get_variable(config[CONF_BSHDBUS_ID])
    cg.add(bshdbus.register_listener(var))
