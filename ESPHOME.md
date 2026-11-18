# BSH D-Bus Component

The `bshdbus` component connects ESPHome to the internal D-Bus-2 of B/S/H/ home appliances
(Bosch, Siemens, Neff, Gaggenau, Constructa and others): washing machines, dryers, dishwashers,
cooker hoods and more. It reads the frames the appliance nodes exchange and turns them into
sensors, binary sensors and text sensors. Optionally, it can also send frames to the appliance.

The protocol, the hardware and the known commands of many appliances are described in the
[bsh-home-appliances](https://github.com/hn/bsh-home-appliances) project. Ready-made
configurations contributed by users are in [contrib/](https://github.com/hn/bsh-home-appliances/tree/master/contrib).

> [!WARNING]
> The D-Bus is not necessarily isolated from mains earth. Read the
> [safety notes](https://github.com/hn/bsh-home-appliances#warning) before connecting anything to a
> live appliance. Never connect the adapter to the appliance and to your computer at the same time.

## Contents

- [Requirements](#requirements)
- [Quick start](#quick-start)
- [How the D-Bus works](#how-the-d-bus-works)
- [Deciphering the frames of your appliance](#deciphering-the-frames-of-your-appliance)
- [UART](#uart)
- [Component](#component)
- [`on_frame` trigger](#on_frame-trigger)
- [Sensor](#sensor)
- [Binary sensor](#binary-sensor)
- [Text sensor](#text-sensor)
- [Decoding values: lambdas and filters](#decoding-values-lambdas-and-filters)
- [Sending](#sending)
  - [`bshdbus.send` action](#bshdbussend-action)
  - [Sending from lambdas](#sending-from-lambdas)
- [Logging](#logging)
- [Troubleshooting and FAQ](#troubleshooting-and-faq)
- [See also](#see-also)

## Requirements

You do not need much to get started:

- ESPHome 2026.11.0 or newer. Older versions stop at validation with a message naming the
  required version.
- A B/S/H/ appliance with a D-Bus-2. As a rule of thumb, these are appliances with an electronic
  user interface panel, not with mechanical rotary switches. This is not confirmed for every model.
- An ESP32- or ESP8266-based [D-Bus adapter](https://github.com/hn/bsh-home-appliances#d-bus-adapter).

## Quick start

```yaml
external_components:
  - source: github://hn/bsh-home-appliances@master
    components: [bshdbus]

uart:
  id: dbus_uart
  rx_pin: GPIO4
  baud_rate: 9600

bshdbus:
  uart_id: dbus_uart
  on_frame:
    - logger.log:
        format: "Received frame dest 0x%02x cmd 0x%04x: 0x%s"
        args: [dest, command, format_hex(message).c_str()]

sensor:
  - platform: bshdbus
    dest: 0x2a
    command: 0x1600
    sensors:
      - name: Remaining time
        device_class: duration
        unit_of_measurement: min
        accuracy_decimals: 0
        lambda: return x[0];
```

This configuration logs every frame on the bus and publishes the remaining time of a washing
machine (frame `2a.16-00`, first data byte = minutes). Start with the `on_frame` log, find the
frames that change when you operate the appliance, then add sensors for them.

## How the D-Bus works

The nodes of the appliance (washing control unit, user interface panel, motor controller, ...)
talk to each other over the D-Bus by sending short messages, called frames. Each frame has:

| Name | Meaning | Example |
|---|---|---|
| `dest` | the receiver of the frame | `0x14` |
| `command` | what the frame is about | `0x1005` |
| data | the values, zero or more bytes; in sensor lambdas they are in `x` | `00 FF 01` |

The component checks every frame and passes only valid ones on, so a sensor never gets a value
from a damaged frame. Frames with the same `dest` and `command` always have the same length and
format. The project documentation writes the example above as `14.10-05 00 ff 01`.

## Deciphering the frames of your appliance

Most of the work is finding out which frame carries which value. These steps have proven useful:

1. Check [contrib/](https://github.com/hn/bsh-home-appliances/tree/master/contrib) and the appliance
   pages of the project ([washing machines](https://github.com/hn/bsh-home-appliances#interpreting-frame-data-wm14s750),
   [dishwashers](https://github.com/hn/bsh-home-appliances/blob/master/DISHWASHER.md),
   [dryers](https://github.com/hn/bsh-home-appliances/blob/master/DRYER.md)) for an appliance like yours.
   Frames are often shared between models of one appliance type, but not always.
2. Log all frames with the `on_frame` example from the [Quick start](#quick-start).
3. Change one thing at a time on the appliance (program, temperature, start, door) and note
   which frame changes. Copy longer logs into the
   [D-Bus analyzer](https://html-preview.github.io/?url=https://github.com/hn/bsh-home-appliances/blob/master/bsh-dbus-analyzer.html)
   to decode them.
4. Add a sensor with that `dest` and `command` and a lambda that picks the right data byte.

Most appliances send a value only when it changes, and some send nothing while they are switched
off. A sensor therefore shows no value until the appliance sends the frame for the first time.

## UART

The component needs a [UART bus](https://esphome.io/components/uart/) with 9600 baud.

```yaml
uart:
  id: dbus_uart
  rx_pin: GPIO4
  baud_rate: 9600
```

- **rx_pin** (**Required**): The pin connected to the adapter's RX output.
- **tx_pin** (*Optional*): Only set this if you want to [send](#sending).
- **baud_rate** (**Required**): `9600`.
- **rx_full_threshold** (*Optional*): On ESP32, sending requires `rx_full_threshold: 1`, see
  [Sending](#sending). Leave it out if you only read.

> [!NOTE]
> Only set `tx_pin` if you want to send. Without `tx_pin`, the component only listens, uses less
> RAM and flash (see [Configuration for sending](#configuration-for-sending)), and cannot disturb
> the appliance.

## Component

The `bshdbus` component is the hub that reads the bus. Sensors, triggers and actions attach to it.

```yaml
bshdbus:
  id: dbus
  uart_id: dbus_uart
```

### Configuration variables

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Manually specify the
  ID of the bus. Needed for [sending from lambdas](#sending-from-lambdas).
- **uart_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): The UART bus
  the adapter is connected to. Only needed if there is more than one UART.
- **on_frame** (*Optional*, [Automation](https://esphome.io/automations/)): Actions to run for
  every valid frame, see [`on_frame` trigger](#on_frame-trigger).
- **max_queue_length** (*Optional*, int): Number of frames that can wait to be sent at the same
  time, `1` to `16`. Defaults to `4`. Only used with a `tx_pin`.
- **node_id** (*Optional*, int): The address of this device on the bus, `0x1` to `0xF`. Only
  needed to send frames to `dest` `0x00` to `0x0F` (broadcasts to all nodes). Pick an address that
  no node of your appliance uses, e.g. `0xC`. This does not make the component a full node on
  the bus: it does not confirm (ACK) or answer frames sent to this address, it only uses it for its
  own broadcasts.

## `on_frame` trigger

Runs for every valid frame on the bus, including frames this device sent itself.

```yaml
bshdbus:
  on_frame:
    - logger.log:
        format: "Received frame dest 0x%02x cmd 0x%04x: 0x%s"
        args: [dest, command, format_hex(message).c_str()]
```

Variables available in the actions:

- **dest** (`uint8_t`): The receiver of the frame.
- **command** (`uint16_t`): The command.
- **message** (`const std::vector<uint8_t> &`): The data bytes, like `x` in sensor lambdas.
- **x** (`const std::vector<uint8_t> &`): The raw frame including its protocol bytes; rarely needed.

`on_frame` is meant for logging and for things like an activity LED. To publish a value or react
to a specific frame, use a [sensor](#sensor), [binary sensor](#binary-sensor) or
[text sensor](#text-sensor) instead of filtering `dest` and `command` in `on_frame`.

### Example: activity LED

```yaml
output:
  - platform: gpio
    id: activity_led
    pin: GPIO7

bshdbus:
  on_frame:
    - output.turn_on: activity_led
    - delay: 20ms
    - output.turn_off: activity_led
```

### Example: hide frames you do not need

While you look for the frames of your appliance, the log fills up with broadcasts (e.g. pings
between the nodes) and with very frequent frames such as the readings of the unbalance sensor.
Leave them out of the log, so the frames you are still looking for stand out:

```yaml
bshdbus:
  on_frame:
    - if:
        condition:
          not:
            lambda: |-
              return (dest & 0xF0) == 0x00                   // broadcasts, e.g. pings
                  || (dest == 0x17 && command == 0x4010);    // unbalance sensor readings
        then:
          - logger.log:
              format: "Received frame dest 0x%02x cmd 0x%04x: 0x%s"
              args: [dest, command, format_hex(message).c_str()]
```

The other way round, to watch only one node, keep the frames of one `dest` (here `0x26`, the
user interface panel):

```yaml
bshdbus:
  on_frame:
    - if:
        condition:
          lambda: return dest == 0x26;
        then:
          - logger.log:
              format: "Received frame dest 0x%02x cmd 0x%04x: 0x%s"
              args: [dest, command, format_hex(message).c_str()]
```

Once your sensors are complete, remove the log action or reduce it, so the log stays readable.

## Sensor

Publishes numeric values from the data bytes of one frame type.

```yaml
sensor:
  - platform: bshdbus
    dest: 0x14
    command: 0x1006
    sensors:
      - id: spin_speed
        name: Spin speed
        device_class: speed
        state_class: measurement
        unit_of_measurement: rpm
        accuracy_decimals: 0
        lambda: return x[0];
        filters:
          - multiply: 10
```

### Configuration variables

- **dest** (**Required**, int): The destination byte of the frame (node and subsystem), e.g. `0x14`.
- **command** (**Required**, int): The two command bytes as one value, e.g. `0x1006`.
- **bshdbus_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): The bus to
  listen on. Only needed if there is more than one `bshdbus`.
- **sensors** (**Required**, list): One or more sensors fed from this frame. Each sensor takes:
  - **lambda** (**Required**, [lambda](https://esphome.io/automations/templates#config-lambda)):
    Returns the raw value as `float`. The data bytes are in `x` (`const std::vector<uint8_t> &`),
    `x[0]` is the first byte after the command.
  - All other options from [Sensor](https://esphome.io/components/sensor/), including `filters`.

Every received frame publishes a value, also if it did not change. See
[Decoding values](#decoding-values-lambdas-and-filters) for `delta`, `throttle` and `timeout`.

## Binary sensor

Publishes on/off states from the data bytes of one frame type, typically single bits.

```yaml
binary_sensor:
  - platform: bshdbus
    dest: 0x14
    command: 0x1007
    binary_sensors:
      - name: Prewash
        lambda: return x[0] & 0x80;
      - name: Anti-crease
        lambda: return x[1] & 0x80;
```

### Configuration variables

- **dest** (**Required**, int): The destination byte of the frame (node and subsystem), e.g. `0x14`.
- **command** (**Required**, int): The two command bytes as one value, e.g. `0x1006`.
- **bshdbus_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): The bus to
  listen on. Only needed if there is more than one `bshdbus`.
- **binary_sensors** (**Required**, list): One or more binary sensors fed from this frame. Each
  takes:
  - **lambda** (**Required**, [lambda](https://esphome.io/automations/templates#config-lambda)):
    Returns the state as `bool`. The data bytes are in `x`.
  - All other options from [Binary Sensor](https://esphome.io/components/binary_sensor/),
    including `filters`.

### Example: button press (momentary)

Some frames report an event rather than a state, e.g. "start button pressed on the user interface
panel" (`15.11-00 01`). A `delayed_off` filter turns the binary sensor off again shortly after the
frame, so every new press triggers `on_press` again:

```yaml
binary_sensor:
  - platform: bshdbus
    dest: 0x15
    command: 0x1100
    binary_sensors:
      - name: Start button
        lambda: return x[0] == 0x01;
        filters:
          - delayed_off: 500ms
```

For an [event entity](https://esphome.io/components/event/) in Home Assistant, add
`on_press: event.trigger` to this binary sensor.

## Text sensor

Publishes text from the data bytes of one frame type, typically a state or program number that a
`map` filter turns into a name.

```yaml
text_sensor:
  - platform: bshdbus
    dest: 0x26
    command: 0x1200
    text_sensors:
      - name: Door
        lambda: return std::to_string(x[0]);
        filters:
          - map:
            - 0 -> Closed
            - 1 -> Locked
            - 2 -> Open
```

### Configuration variables

- **dest** (**Required**, int): The destination byte of the frame (node and subsystem), e.g. `0x14`.
- **command** (**Required**, int): The two command bytes as one value, e.g. `0x1006`.
- **bshdbus_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): The bus to
  listen on. Only needed if there is more than one `bshdbus`.
- **text_sensors** (**Required**, list): One or more text sensors fed from this frame. Each takes:
  - **lambda** (**Required**, [lambda](https://esphome.io/automations/templates#config-lambda)):
    Returns the text as `std::string`. The data bytes are in `x`.
  - All other options from [Text Sensor](https://esphome.io/components/text_sensor/),
    including `filters`.

A value missing from the `map` passes through unchanged, so an unknown program shows up as its
number. That makes new values easy to spot.

## Decoding values: lambdas and filters

The lambda of a `bshdbus` sensor should do one thing: **pick the raw value out of the data bytes.**
Everything else — scaling, mapping numbers to names, suppressing duplicates, rate limiting,
handling a silent appliance — is what ESPHome [filters](https://esphome.io/components/sensor/#sensor-filters)
are for. Filters are easier to read, are checked during validation and show up in the
configuration dump.

### Picking the raw value

| Data | Lambda |
|---|---|
| one byte, unsigned | `return x[0];` |
| one byte, signed | `return (int8_t) x[0];` |
| two bytes, big-endian, unsigned | `return encode_uint16(x[1], x[2]);` |
| two bytes, big-endian, signed | `return (int16_t) encode_uint16(x[1], x[2]);` |
| two bytes, little-endian | `return encode_uint16(x[2], x[1]);` |
| one bit (binary sensor) | `return x[0] & 0x02;` |
| bit field as number (bits 5 and 6) | `return (x[0] >> 5) & 0x03;` |
| a bit is **cleared** | `return !(x[0] & 0x60);` |
| byte equals a value | `return x[0] == 0x25;` |
| number as text (text sensor) | `return std::to_string(x[2]);` |

`encode_uint16()` is an ESPHome helper and available in every lambda.

> [!NOTE]
> All frames with the same `dest` and `command` have the same length, so `x[n]` is safe once you
> know the frame. Reading past the end of `x` returns garbage or crashes the device. If you are not
> sure about the length yet, look at the `on_frame` log first.

### Turning raw values into real values

| Task | Filter | Instead of |
|---|---|---|
| scale | `multiply: 10` | `return x[0] * 10;` |
| shift | `offset: -40` | `return x[0] - 40;` |
| linear table (0 → 20 °C, 1 → 30 °C, ...) | `multiply: 10` + `offset: 20`, or `calibrate_linear` with `method: exact` | `if`/`switch` in the lambda |
| number to name (text sensor) | `map` | `switch` with string literals in the lambda |
| ignore a placeholder value (e.g. `0xFF` = unknown) | `filter_out: 255` | `if (x[0] == 0xFF) return NAN;` |
| publish only changes | `delta: 0` | `static` variables comparing with the previous value |
| limit the update rate of chatty frames | `throttle: 30s` or `throttle_average: 30s` | timestamps in globals |
| mark a value as unavailable when the appliance goes silent | `timeout: 90s` | `interval` + `millis()` + globals |
| debounce flickering bits | `delayed_on_off: 1s` or `settle: 1s` (binary sensor) | counters in globals |

Example with several filters:

```yaml
sensor:
  - platform: bshdbus
    dest: 0x14
    command: 0x1004
    sensors:
      - name: Temperature
        device_class: temperature
        unit_of_measurement: °C
        accuracy_decimals: 0
        lambda: return x[0];   # 0 = 20 °C, 1 = 30 °C, ... 7 = 90 °C
        filters:
          - multiply: 10
          - offset: 20
  - platform: bshdbus
    dest: 0x17
    command: 0x4010
    sensors:
      - name: Unbalance X
        entity_category: diagnostic
        accuracy_decimals: 0
        lambda: return (int16_t) encode_uint16(x[1], x[2]);
        filters:
          - throttle: 30s
          - timeout: 90s
```

### Patterns to avoid

The patterns below work, but they do in C++ what ESPHome already offers. The recommended way is
to use ESPHome's own building blocks (filters, conditions, sensor types) wherever possible and to
keep lambdas short, ideally just picking the raw value. Such configurations are easier to read,
share and maintain, and they benefit from improvements in ESPHome without changes.

**Long `if`/`else` or `switch` blocks that return names.** Return the number and use `map`:

```yaml
# Avoid
lambda: |-
  switch (x[0]) {
    case 0x0a: return {"Intensive"};
    case 0x0d: return {"Auto"};
    default: return {"Unknown"};
  }

# Better
lambda: return std::to_string(x[0]);
filters:
  - map:
    - 10 -> Intensive
    - 13 -> Auto
```

**`if`/`else` around a condition.** The condition already is the result:

```yaml
# Avoid
lambda: |-
  if (x[0] == 0x00) {
    return 1;
  } else {
    return 0;
  }

# Better
lambda: return x[0] == 0x00;
```

**Globals that store the last state** and are republished at boot or from an `interval`. A
`bshdbus` sensor keeps its state until the next frame; for an appliance that goes silent, use a
`timeout` filter. After a reboot of the ESP, the value comes back with the next frame of the
appliance.

**Decoding everything in one big `on_frame` lambda** that calls `publish_state()` on template
sensors. This bypasses the validation of `dest` and `command`, hides the decoding in C++, and is
hard to maintain. Use one `bshdbus` sensor platform entry per frame type; one entry can feed
several sensors from the same frame.

**Long lambdas in template sensors that combine other sensors.** Keep them short, e.g.
`return id(rpm_raw).state * 10;`, and check first whether a filter on the original sensor does
the same.

## Sending

> [!WARNING]
> Sending frames directly controls the appliance's hardware (motor, heater, water valve,
> firmware memory). The component does not and cannot protect you from harmful commands. If you
> connect a TX pin, you are solely responsible for what you send.
>
> The appliances were never designed to be controlled from outside this way. Normally a node
> sends a command itself, e.g. the user interface panel when a key is pressed, and updates its own
> state at the same time. A frame sent from outside bypasses this: the other nodes do not know
> about the change, and the appliance may react in ways that nobody can foresee. How an appliance
> reacts depends on the model, and nobody knows in advance.

> [!NOTE]
> Sending is experimental for now and only available on the `esphome-dev` branch. To try it, use
> that branch in `external_components`:
>
> ```yaml
> external_components:
>   - source: github://hn/bsh-home-appliances@esphome-dev
>     components: [bshdbus]
> ```
>
> To go back, use `@master` again.

The `dest`, `command` and data values in the sending examples are only examples. They do not
necessarily mean the same on your appliance.

### Configuration for sending

Sending needs a few settings on top of a configuration that only reads:

```yaml
uart:
  id: dbus_uart
  rx_pin: GPIO4
  tx_pin: GPIO5          # only set this if you want to send
  baud_rate: 9600
  rx_full_threshold: 1   # ESP32: required for sending

bshdbus:
  id: dbus
  uart_id: dbus_uart
  node_id: 0xC           # only needed for broadcasts
```

- **`tx_pin` on the UART.** Without it, `bshdbus.send` is a validation error and `send()` in a
  lambda returns `SEND_RESULT_NOT_ENABLED`.
- **ESP32: `rx_full_threshold: 1` on the UART.** Otherwise the ESP32 notices received data too
  late and would send into the frames of the appliance. Without it, `bshdbus.send` is a validation
  error, and every `send()` call returns `SEND_RESULT_NOT_ENABLED` and logs a warning that names
  the setting. ESP8266 does not need this setting.
- **`node_id` for broadcasts** (`dest` `0x00` to `0x0F`). Without it, such a frame is a validation
  error, or `SEND_RESULT_REJECTED` if `dest` comes from a lambda.

The configuration dump shows the state, e.g. `Sending: enabled`,
`Sending: disabled (no tx_pin)` or `Sending: disabled (uart rx_full_threshold must be 1)`.

With a `tx_pin`, sending needs about 300 bytes of RAM and 2 to 3 KB of flash, plus about 60 to
120 bytes per `bshdbus.send` action. Without `tx_pin`, none of this is used.

### How to send

There are two ways to send a frame:

- The [`bshdbus.send` action](#bshdbussend-action), e.g. in a button, a select or a script. This is
  the normal case: you can wait for the result, run actions on success or failure, and stop a
  sequence on the first error.
- [`send()` in a lambda](#sending-from-lambdas), if `dest`, `command` or the data need real
  computation, or if the call sits in the middle of other C++ code.

Both put the frame into a queue. The component sends it when the bus is quiet and reports the
[result](#results).

### Getting started

Sending can trigger things you did not expect, so take it step by step. Test one command at a time
and stay with the appliance, so you can switch it off if something goes wrong.

- Use a template button with one `bshdbus.send` action and an `on_error` that logs the reason. Set
  the logger to `bshdbus: VERBOSE` to see each step of sending.
- Leave `max_retries` at `0` until you know that repeating the command is harmless, see
  [Retries](#retries).

### Results

| Value | Meaning |
|---|---|
| `SEND_RESULT_OK` | The frame was sent and confirmed by the receiver (broadcasts: sent). |
| `SEND_RESULT_QUEUE_FULL` | Too many frames are waiting to be sent, see `max_queue_length`. |
| `SEND_RESULT_BUS_BUSY` | The bus was not quiet long enough to send within 500 ms after the frame's turn came. |
| `SEND_RESULT_COLLISION` | The frame could not be sent without disturbance, also after the retries. If the log says `no echo, check TX wiring`, the TX line is not connected. |
| `SEND_RESULT_NO_ACK` | The receiver did not confirm the frame, also after `max_retries` retries. |
| `SEND_RESULT_REJECTED` | The frame is invalid: more than 249 data bytes, or a broadcast without `node_id`. |
| `SEND_RESULT_NOT_ENABLED` | Sending is not possible: no `tx_pin`, or ESP32 without `rx_full_threshold: 1`. |

Frames are sent one after another, in the order they were queued. A frame typically has its result
10 to 50 ms after its turn came, and 1.5 s after it at the latest. A frame waiting behind others
gets its turn when the frame before it has its result.

The values live in the namespace `bshdbus` (`bshdbus::SendResult::SEND_RESULT_OK`). For logs,
`LOG_STR_ARG(bshdbus::send_result_to_str(error))` gives a readable text.

### Retries

The component handles the two kinds of failure differently:

- **Collisions** are always retried, up to 3 times. This is safe: a disturbed frame is dropped by
  every receiver.
- **A missing confirmation (ACK)** is only retried if you set `max_retries`.

All attempts of a frame have to fit into the 1.5 s after its turn came. Long frames (about 100 data
bytes and more) therefore get fewer retries than a high `max_retries` would allow.

> [!WARNING]
> A missing ACK does not mean the frame was not received. Retrying may execute the command twice.
> Only use `max_retries` for commands where this is harmless (e.g. setting a value), never for
> toggles or key presses.

## `bshdbus.send` action

Sends one frame and, by default, waits for the result.

```yaml
button:
  - platform: template
    name: Start
    on_press:
      - bshdbus.send:
          dest: 0x17
          command: 0x1013
          data: [0x01]
```

### Configuration variables

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): The bus to send on.
  Only needed if there is more than one `bshdbus`.
- **dest** (**Required**, int, [templatable](https://esphome.io/automations/templates)): The
  destination byte.
- **command** (**Required**, int, [templatable](https://esphome.io/automations/templates)): The
  command.
- **data** (*Optional*, list of bytes or [lambda](https://esphome.io/automations/templates#config-lambda)):
  The data bytes, at most 249. A lambda returns `std::vector<uint8_t>`. Defaults to no data.
- **max_retries** (*Optional*, int): How often to repeat the frame if the receiver does not
  confirm it, `0` to `10`. Defaults to `0`. See the warning under [Retries](#retries).
- **wait_for_sent** (*Optional*, boolean): Wait for the result before the next action runs.
  Defaults to `true`.
- **continue_on_error** (*Optional*, boolean): Continue with the next action if sending fails.
  Defaults to `true`.
- **on_sent** (*Optional*, [Action](https://esphome.io/automations/actions)): Actions to run when
  the frame was sent.
- **on_error** (*Optional*, [Action](https://esphome.io/automations/actions)): Actions to run when
  sending failed. The variable `error` (`bshdbus::SendResult`) holds the reason.

| `wait_for_sent` | `continue_on_error` | Behavior |
|---|---|---|
| `true` | `true` | Wait for the result, then continue in any case (default). |
| `true` | `false` | Wait for the result; on failure, stop the automation here. |
| `false` | `true` | Continue at once; `on_sent`/`on_error` run later. |
| `false` | `false` | Not allowed (validation error). |

> [!NOTE]
> Put an action with `continue_on_error: false` directly into the list of actions of a script or
> automation, not inside `if`, `while` or `repeat`. There, a failure leaves the script marked as
> running, so a script with `mode: single` cannot be started again.

In `on_sent` and `on_error`, the variables of the surrounding automation are available (e.g. `x`
in a `select`), in `on_error` also `error`. `delay` and `wait_until` work there too; with
`wait_for_sent: true`, the automation continues once these handlers have finished.

### Stopping a script

If a script is stopped (`script.stop`, or `mode: restart` starting it again) while it waits for a
result, it does not continue, and `on_sent`/`on_error` do not run. A frame that is already queued
is still sent.

### Example: value from a select, with retries

Setting a value twice is harmless, so `max_retries` is fine here:

```yaml
select:
  - platform: template
    id: program
    name: Program
    options: [Intensive, Auto, Eco, Glass, Quick, Pre-Rinse]
    optimistic: true
    set_action:
      - bshdbus.send:
          dest: 0x17
          command: 0x1010
          max_retries: 2
          data: !lambda |-
            static const uint8_t codes[] = {0x0A, 0x0D, 0x0E, 0x0F, 0x11, 0x12};
            return {codes[id(program).index_of(x.c_str(), x.size()).value_or(2)]};
```

### Example: sequence that stops on the first error

No `delay:` is needed between the frames: each action waits until its frame has been sent.

```yaml
script:
  - id: eco_start
    mode: single
    then:
      - bshdbus.send:
          dest: 0x17
          command: 0x1010
          data: [0x0E]
          continue_on_error: false
      - bshdbus.send:
          dest: 0x17
          command: 0x1000
          data: [0x00, 0x02]
          continue_on_error: false
      - bshdbus.send:
          dest: 0x17
          command: 0x1013
          data: [0x01]
          continue_on_error: false
          on_error:
            - logger.log:
                format: "Start failed: %s"
                args: [LOG_STR_ARG(bshdbus::send_result_to_str(error))]
      - homeassistant.event:
          event: esphome.dishwasher_started
```

If one of the three frames fails (e.g. is not ACKed), the rest of the script is skipped, including
the event. To report every failure, add `on_error` to each action. Note that the component only
knows whether a frame reached its receiver, not whether the appliance carried out the command.

### Example: queue frames without waiting

The queue keeps the order:

```yaml
- bshdbus.send: {dest: 0x17, command: 0x1010, data: [0x0E], wait_for_sent: false}
- bshdbus.send: {dest: 0x17, command: 0x1013, data: [0x01], wait_for_sent: false}
- logger.log: "Both frames queued"
```

### Example: only send when the appliance is ready

Use the state of your sensors as a condition instead of checks in a lambda:

```yaml
button:
  - platform: template
    name: Start
    on_press:
      - if:
          condition:
            text_sensor.state:
              id: door
              state: Closed
          then:
            - bshdbus.send:
                dest: 0x17
                command: 0x1013
                data: [0x01]
          else:
            - logger.log:
                level: WARN
                format: "Not starting: door is not closed"
```

## Sending from lambdas

When to use `send()` instead of the action is explained under [How to send](#how-to-send).

```cpp
namespace esphome::bshdbus {
SendResult send(uint8_t dest, uint16_t command, std::initializer_list<uint8_t> data = {},
                uint8_t max_retries = 0, send_callback_t callback = nullptr);
SendResult send(uint8_t dest, uint16_t command, const uint8_t *data, size_t len,
                uint8_t max_retries = 0, send_callback_t callback = nullptr);

using send_callback_t = void (*)(SendResult result, uint8_t dest, uint16_t command);
}
```

Sending happens in two steps:

1. **Queueing**, immediately: the frame is checked and queued before `send()` returns. The
   **return value** reports this step: `SEND_RESULT_OK`, `SEND_RESULT_QUEUE_FULL`,
   `SEND_RESULT_REJECTED` or `SEND_RESULT_NOT_ENABLED`.
2. **The result**, later (typically after 10 to 50 ms): the optional **callback** reports it:
   `SEND_RESULT_OK`, `SEND_RESULT_BUS_BUSY`, `SEND_RESULT_COLLISION` or `SEND_RESULT_NO_ACK`. The
   callback is only called if queueing succeeded.

A lambda must never wait for the result: the bus is read in the same loop, so the result could
never arrive.

With the pointer variant, `data` must be in RAM. It is copied while queueing and can be reused
right after `send()` returns.

### Examples

Send and ignore the result:

```yaml
- lambda: 'id(dbus).send(0x17, 0x1013, {0x01});'
```

Check whether the frame was queued:

```yaml
- lambda: |-
    auto res = id(dbus).send(0x17, 0x1013, {0x01});
    if (res != bshdbus::SendResult::SEND_RESULT_OK)
      ESP_LOGW("dishwasher", "Not queued: %s", LOG_STR_ARG(bshdbus::send_result_to_str(res)));
```

Callback that stores the result in a global:

```yaml
globals:
  - id: start_ok
    type: bool
    initial_value: "false"

# ...
- lambda: |-
    id(dbus).send(0x17, 0x1013, {0x01}, 0, [](bshdbus::SendResult res, uint8_t dest, uint16_t command) {
      id(start_ok) = (res == bshdbus::SendResult::SEND_RESULT_OK);
    });
```

Callback that starts a script:

```yaml
- lambda: |-
    id(dbus).send(0x17, 0x1013, {0x01}, 0, [](bshdbus::SendResult res, uint8_t, uint16_t) {
      if (res == bshdbus::SendResult::SEND_RESULT_OK)
        id(after_start).execute();
    });
```

### Callbacks cannot capture variables

The callback must start with an empty `[]`, otherwise it does not compile.

```cpp
// Works: no capture, access through id() and the parameters
id(dbus).send(0x17, 0x1013, {0x01}, 0, [](bshdbus::SendResult res, uint8_t dest, uint16_t command) { ... });

// Does not compile: captures 'x'
id(dbus).send(0x17, 0x1013, {0x01}, 0, [x](bshdbus::SendResult res, uint8_t, uint16_t) { ... });
```

`id(...)` still works inside the callback, because ESPHome creates IDs as global variables. Put
values the callback needs into `globals:` before calling `send()`.

## Logging

The component logs with the tags `bshdbus`, `bshdbus.sensor`, `bshdbus.binary_sensor` and
`bshdbus.text_sensor`. Set the level per tag to see more about one part:

```yaml
logger:
  level: DEBUG
  logs:
    bshdbus: VERBOSE
```

| Level | Messages |
|---|---|
| `VERBOSE` | every valid frame, details of sending |
| `DEBUG` | discarded data from damaged or incomplete frames, missing confirmations (ACKs), single collisions and retries |
| `WARN` | a frame that finally failed to send (once per frame); `send()` calls that are rejected or not possible, with the reason |

Some `DEBUG` messages about discarded data and missing ACKs are normal. Many of them
in a row point to a wiring or timing problem.

## Troubleshooting and FAQ

**No frames at all.**
Check the wiring and the level shifter, `rx_pin` and `baud_rate: 9600`. Set `bshdbus: VERBOSE` in
the logger to see whether valid frames arrive. Many appliances only talk while they are switched on.

**Many "Invalid CRC" or "Discarding" messages.**
Some are normal, e.g. while the ESP is busy with Wi-Fi. If most frames are discarded, check the
adapter and the ground connection. A different `baud_rate` than 9600 shows up the same way.

**A sensor never gets a value.**
`dest` is a full byte (`0x14`, not `0x1`), and `command` has four hex digits (`0x1006`, not
`0x06`). Compare with the `on_frame` log. Many frames are only sent when the
value changes, so operate the appliance once.

**A value is unknown after the ESP restarts.**
The appliance sends most values only on change. The sensor gets its value with the next frame.

**The values are off by a factor or offset.**
Fix that with filters (`multiply`, `offset`, `calibrate_linear`), not in the lambda. See
[Decoding values](#decoding-values-lambdas-and-filters).

**`send()` returns `SEND_RESULT_NOT_ENABLED`.**
The UART has no `tx_pin`, or on ESP32 `rx_full_threshold: 1` is missing. The log shows which one.

**Sending fails with `SEND_RESULT_COLLISION` and the log says "no echo, check TX wiring".**
The component did not read back its own frame. The TX line is not connected, or the adapter does not
pass TX to the bus.

**Sending fails with `SEND_RESULT_COLLISION` without that hint.**
Another node sent at the same time, also after the retries. Occasional collisions are normal,
especially while the appliance is busy. On ESP32, check `rx_full_threshold: 1`.

**Sending fails with `SEND_RESULT_NO_ACK`.**
No node with that address exists or is awake (some nodes only run while a program is active), or
the `dest` is wrong. The ACK may also have been lost although the command was executed; do not
blindly retry toggles.

**Sending fails with `SEND_RESULT_BUS_BUSY`.**
The bus was never quiet for long enough. This happens during phases with heavy traffic; try again
later or send at another point of the program.

**`SEND_RESULT_QUEUE_FULL`.**
More frames were sent at once than `max_queue_length` allows. Use `wait_for_sent: true` (default)
in sequences, or raise `max_queue_length`.

**My own frames show up in `on_frame` and in the sensors.**
That is intended: they are on the bus like any other frame.

## See also

- [bsh-home-appliances](https://github.com/hn/bsh-home-appliances): protocol, hardware, appliance pages
- [contrib/](https://github.com/hn/bsh-home-appliances/tree/master/contrib): configurations contributed by users
- [D-Bus analyzer](https://html-preview.github.io/?url=https://github.com/hn/bsh-home-appliances/blob/master/bsh-dbus-analyzer.html)
- [UART Bus](https://esphome.io/components/uart/)
- [Sensor filters](https://esphome.io/components/sensor/#sensor-filters),
  [Binary sensor filters](https://esphome.io/components/binary_sensor/#binary-sensor-filters),
  [Text sensor filters](https://esphome.io/components/text_sensor/#text-sensor-filters)
- [Automations](https://esphome.io/automations/) and [Templates](https://esphome.io/automations/templates)
