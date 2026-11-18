from collections.abc import Callable, Iterator
from dataclasses import dataclass, field

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.config import iter_ids
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMMAND,
    CONF_DATA,
    CONF_DEST,
    CONF_ID,
    CONF_ON_ERROR,
    CONF_TX_PIN,
    CONF_UART_ID,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.types import ConfigType

# ESPHome component based on VBus component by @ssieb - thanks!

CODEOWNERS = ["@hn"]

DEPENDENCIES = ["uart"]

DOMAIN = "bshdbus"

MULTI_CONF = True

bshdbus_ns = cg.esphome_ns.namespace("bshdbus")
BSHDBus = bshdbus_ns.class_("BSHDBus", uart.UARTDevice, cg.Component)
SendAction = bshdbus_ns.class_(
    "SendAction", automation.Action, cg.Parented.template(BSHDBus)
)
SendResult = bshdbus_ns.enum("SendResult", is_class=True)

CONF_BSHDBUS_ID = "bshdbus_id"
CONF_CONTINUE_ON_ERROR = "continue_on_error"
CONF_MAX_QUEUE_LENGTH = "max_queue_length"
CONF_MAX_RETRIES = "max_retries"
CONF_NODE_ID = "node_id"
CONF_ON_FRAME = "on_frame"
CONF_ON_SENT = "on_sent"
CONF_WAIT_FOR_SENT = "wait_for_sent"

SEND_ACTION = "bshdbus.send"
# Frame length 255 minus LL, DS, CC CC and CRC
MAX_DATA_LENGTH = 249


@dataclass
class BSHDBusData:
    # IDs of the hubs whose uart has a tx_pin, so they can send
    tx_hubs: set[str] = field(default_factory=set)
    # max_queue_length per hub, for the slots of its send actions
    queue_lengths: dict[str, int] = field(default_factory=dict)


def _get_data() -> BSHDBusData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = BSHDBusData()
    return CORE.data[DOMAIN]


CONFIG_SCHEMA = cv.All(
    cv.require_esphome_version(2026, 11, 0),
    uart.UART_DEVICE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BSHDBus),
            cv.Optional(CONF_MAX_QUEUE_LENGTH, default=4): cv.int_range(1, 16),
            cv.Optional(CONF_NODE_ID): cv.hex_int_range(1, 0xF),
            cv.Optional(CONF_ON_FRAME): automation.validate_automation({}),
        }
    ),
)


def _send_actions(hub_id: ID) -> Iterator[tuple[ConfigType, str]]:
    """Yield each bshdbus.send action that sends via hub_id, with its location."""
    full_config = fv.full_config.get()
    for id_, path in iter_ids(full_config):
        # Only the id of the action itself; ids used in its lambdas or handlers
        # (e.g. id(dbus2) in data: or on_error:) do not make it send via that hub
        if path[-2:] != [SEND_ACTION, CONF_ID] or id_.id != hub_id.id:
            continue
        action_path = path[:-1]
        yield (
            full_config.get_config_for_path(action_path),
            "->".join(map(str, action_path)),
        )


def _final_validate(config: ConfigType) -> ConfigType:
    hub_id = config[CONF_ID]
    uart_config = fv.get_declaring_config(config[CONF_UART_ID])
    actions = list(_send_actions(hub_id))
    if actions:
        _, location = actions[0]
        if CONF_TX_PIN not in uart_config:
            raise cv.Invalid(
                f"{SEND_ACTION} needs a tx_pin on uart '{config[CONF_UART_ID]}' "
                f"(used at {location})"
            )
        if CORE.is_esp32 and uart_config.get(uart.CONF_RX_FULL_THRESHOLD) != 1:
            raise cv.Invalid(
                f"{SEND_ACTION} on ESP32 needs '{uart.CONF_RX_FULL_THRESHOLD}: 1' on uart "
                f"'{config[CONF_UART_ID]}', otherwise received bytes arrive too late "
                f"to see that the bus is busy (used at {location})"
            )
    if CONF_NODE_ID not in config:
        for action, location in actions:
            if isinstance(dest := action[CONF_DEST], int) and dest <= 0x0F:
                raise cv.Invalid(
                    f"Sending a broadcast to dest 0x{dest:02X} needs '{CONF_NODE_ID}' "
                    f"on bshdbus '{hub_id}' (used at {location})"
                )
    if CONF_TX_PIN in uart_config:
        _get_data().tx_hubs.add(str(hub_id))
    _get_data().queue_lengths[str(hub_id)] = config[CONF_MAX_QUEUE_LENGTH]
    # Only one call per hub: each call claims the uart pins for this hub
    return uart.final_validate_device_schema(
        DOMAIN, require_rx=True, require_tx=bool(actions)
    )(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)
    if str(config[CONF_ID]) in _get_data().tx_hubs:
        cg.add_define("USE_BSHDBUS_SEND")
        cg.add(var.set_tx_enabled(True))
        cg.add(var.set_max_queue_length(config[CONF_MAX_QUEUE_LENGTH]))
        cg.add_optional(var.set_node_id, config.get(CONF_NODE_ID))
    for conf in config.get(CONF_ON_FRAME, []):
        await automation.build_callback_automation(
            var,
            "add_on_frame_callback",
            [
                (
                    cg.std_vector.template(cg.uint8).operator("ref").operator("const"),
                    "x",
                ),
                (cg.uint8, "dest"),
                (cg.uint16, "command"),
                (
                    cg.std_vector.template(cg.uint8).operator("ref").operator("const"),
                    "message",
                ),
            ],
            conf,
        )


def _validate_send_action(config: ConfigType) -> ConfigType:
    if not config[CONF_WAIT_FOR_SENT] and not config[CONF_CONTINUE_ON_ERROR]:
        raise cv.Invalid(
            f"'{CONF_CONTINUE_ON_ERROR}' cannot be false if '{CONF_WAIT_FOR_SENT}' "
            "is false, because the automation does not wait for the result",
            path=[CONF_CONTINUE_ON_ERROR],
        )
    return config


SEND_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BSHDBus),
            cv.Required(CONF_DEST): cv.templatable(cv.hex_uint8_t),
            cv.Required(CONF_COMMAND): cv.templatable(cv.hex_uint16_t),
            cv.Optional(CONF_DATA, default=[]): cv.templatable(
                cv.All(cv.ensure_list(cv.hex_uint8_t), cv.Length(max=MAX_DATA_LENGTH))
            ),
            cv.Optional(CONF_MAX_RETRIES, default=0): cv.int_range(0, 10),
            cv.Optional(CONF_WAIT_FOR_SENT, default=True): cv.boolean,
            cv.Optional(CONF_CONTINUE_ON_ERROR, default=True): cv.boolean,
            cv.Optional(CONF_ON_SENT): automation.validate_action_list,
            cv.Optional(CONF_ON_ERROR): automation.validate_action_list,
        }
    ),
    _validate_send_action,
)


def _register_send_action(schema: cv.Schema) -> Callable[[Callable], Callable]:
    try:
        return automation.register_action(
            SEND_ACTION, SendAction, schema, synchronous=False
        )
    except TypeError:  # ESPHome < 2026.3; the hub schema reports the version
        return automation.register_action(SEND_ACTION, SendAction, schema)


@_register_send_action(SEND_ACTION_SCHEMA)
async def send_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: list[tuple],
) -> cg.MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    dest = await cg.templatable(config[CONF_DEST], args, cg.uint8)
    cg.add(var.set_dest(dest))
    command = await cg.templatable(config[CONF_COMMAND], args, cg.uint16)
    cg.add(var.set_command(command))
    await automation.templatable_bytes(
        config[CONF_DATA],
        args,
        var.set_data_template,
        var.set_data_static,
        "bshdbus_send_data",
    )
    if config[CONF_MAX_RETRIES]:
        cg.add(var.set_max_retries(config[CONF_MAX_RETRIES]))
    # Both default to true in C++ as well
    if not config[CONF_WAIT_FOR_SENT]:
        cg.add(var.set_wait_for_sent(False))
    if not config[CONF_CONTINUE_ON_ERROR]:
        cg.add(var.set_continue_on_error(False))
    cg.add(var.init_pending(_get_data().queue_lengths[str(config[CONF_ID])]))
    if on_sent := config.get(CONF_ON_SENT):
        actions = await automation.build_action_list(on_sent, template_arg, args)
        cg.add(var.add_on_sent(actions))
    if on_error := config.get(CONF_ON_ERROR):
        error_template_arg = cg.TemplateArguments(*template_arg, SendResult)
        error_args = [*args, (SendResult, "error")]
        actions = await automation.build_action_list(
            on_error, error_template_arg, error_args
        )
        cg.add(var.add_on_error(actions))
    return var
