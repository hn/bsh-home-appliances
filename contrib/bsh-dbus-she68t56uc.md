# Bosch SHE68T56UC/02 dishwasher

Contributed by [Dave / BluThunder2k](https://github.com/BluThunder2k).

Companion configuration: [bsh-dbus-she68t56uc.yaml](bsh-dbus-she68t56uc.yaml).
Model E-Nr. SHE68T56UC/02, FD 9405. Adapter: [Bouni BSH-Board v1.1](https://github.com/Bouni/BSH-Board/tree/1.1), ESP32-C6, ESP-IDF, UART RX GPIO4 / TX GPIO5, 9600 8N1. SW3 pull-up was OFF during the owner's successful tests. Other board revisions may use different pins.

Read the [upstream electrical safety and connector notes](../README.md#warning) before installation. Do not connect USB and the appliance simultaneously. This configuration monitors received frames; it exposes no bus-write controls. It pins the existing bshdbus component without modifying it.

## Setup

Add local secrets named `wifi_ssid`, `wifi_password`, `dishwasher_api_key` (32-byte base64 API encryption key), and `dishwasher_ota_password`. Validate and compile with ESPHome before installation. The owner's capture identifies ESPHome 2026.7.4. These observations are model-specific, not a claim of compatibility with every Bosch dishwasher.

No web server or time server is required. If adding a web server, use `log: false` to avoid accumulating a long browser log. Diagnostics update every 60 seconds. Raw parser-delivered frames remain available at the `dishwasher_bus` DEBUG tag; storage requires an external collector. This firmware does not write log files to flash. Candidate entity names are retained from the owner's installation.

## Observations and confidence

Byte indices below are zero-based, after the command bytes.

| Destination / command | Interpretation in this example | Evidence / limitation |
| --- | --- | --- |
| 17 / 1011 | Selected program, byte 0 | Auto `0d` and Rinse `12` corroborated in the owner's captures. Other program labels in the configuration need further independent capture validation. |
| 17 / 1010 | Raw program selection | Stored separately from 1011. |
| 25 / 2010 | Program status snapshot, byte 0 | Auto/Rinse corroboration; options are deliberately not decoded from this payload. |
| 17 / 1000 | Option bits in byte 1 | Half Load `20` physically selected in this run. Other configured masks need independent toggle captures before being treated as universally confirmed. Masks differ from some other models. |
| 24 / 2006 | Door candidate: bit `08` clear means open | Consistent with observed door changes. Bit `02` as rinse-aid low remains provisional; no confirmed low-rinse-aid event. |
| 17 or 25 / 2012 | Panel power, byte 0 `00`/`01` | Used by the owner's decoder; not present in the complete-run excerpt below. |
| 17 / 1013 | Start/resume observed, byte 0 `01` | Followed by running state at cycle start. Does not itself prove physical operation. |
| 17 / 1014 | Cancel observed | Latches cancellation until a new start; software checks exercise this behavior. |
| 27 / 2007 | Operating state | `01` active and `03` ended observed in full run. Decoder also handles ready `02` and cancel `06`; this full run does not independently validate those paths. |
| 17 / 1012 | Delay candidate: byte 1 hours, byte 2 minutes | Earlier delayed-start capture counted down through zero. Byte 0 changed `02` to `01` at 30 minutes; not treated as a running flag. |
| 25 / 2008 | Remaining-time candidate: byte 0 minutes | Minute countdown, revisions, and reset to selected program duration at end observed. Trailing `64 64` remains unexplained; wider duration encoding is not established. |
| 25 / 2000 | Progress candidate: byte 0 percent | Observed 0 through 99; can decrease when the time estimate increases. |
| 25 / 2005 | Broad cycle phase | See below. Not a direct pump, valve, or heater indication. |

## Phase interpretation

| Value | Display label | Basis |
| --- | --- | --- |
| `21` | Initial wash | First broad phase in this run; exact internal meaning inferred. |
| `22` | Washing | Broad wash-stage interpretation; not individual fill/drain actions. |
| `24` | Rinsing | Broad rinse-stage interpretation; exact internal meaning inferred. |
| `28` | Drying | Final approximately 15-minute phase, consistent with other BSH observations. Does not imply an exposed cabin heating element. |
| `12` | Wash transition | Brief excursions from `22`, followed by time revisions; exact mechanism unknown. |
| `14` | Rinse transition | Brief excursions from `24`; exact mechanism unknown. |
| `20` | Finishing | Appears immediately before operating state `03`; not sufficient by itself to declare completion. |

The complete Auto + Half Load capture ran from 20:58:43 to 22:42:53, approximately 104 minutes 10 seconds. Initial estimate: 105 minutes. Phase `28` began at 22:27:51.748 and phase `20` at 22:42:52.791, about 15 minutes 1 second later. HA activity also recorded decimal phase 40 followed by 32 and Ended.

## Completion and state handling

The appliance's final received progress was 99, and its remaining-time payload reset to `69 64 64` (105 minutes) immediately after operating state `03`. The normalized dashboard entities display 0 minutes / 100% for Ended only after an observed start and without a known cancellation. These are presentation values, not received zero/100 frames. Raw diagnostics retain the original payloads.

Ended is not a verified fault-free or Clean indication. State `03` must not be assumed to distinguish every normal termination, cancellation, and fault. Unknown faults are not decoded here. A reboot clears inferred history; an ended frame without a witnessed start becomes `Ended (history unknown)`. No completion is inferred from bus silence, which commonly lasts about a minute. Door-open pauses retain the last phase but turn the Drying binary sensor off.

## Unresolved payloads

- `25/2004`: `800000` then `200000` at start; `200000` also at stage boundaries and completion. No pump/heater interpretation assigned.
- `25/2009`: repeated `00` during operation. Not treated as uptime or drain status.
- `25/2015`: `07ff` during selection. Program-dependent data; option-mask interpretation unconfirmed.
- `17/1017`: `620011` at the end of this run. Retained as bytes, not a success trigger.
- `25/2013`: `ffff` in this capture. No fault code decoding.
- `26/2002`: `00000000` then `00000002` early in this completed run. The latter is not sufficient evidence of a fault.

## Selected capture excerpt

This is a manually selected, non-contiguous excerpt from the owner's HA Core log. Only time, destination, command and payload are retained. These are decoded callback payloads, not complete wire frames with CRC/ACK.

```text
20:58:31.894  17/1010  0d
20:58:31.905  17/1000  0020
20:58:31.918  17/1011  0d
20:58:31.935  25/2008  696464
20:58:31.965  25/2015  07ff
20:58:43.065  17/1013  01
20:58:43.141  27/2007  01
20:58:43.313  25/2000  00
20:58:43.437  26/2002  00000000
20:58:43.533  25/2005  21
21:00:04.806  26/2002  00000002
21:24:34.571  25/2005  22
21:54:13.882  25/2005  12
21:54:42.107  25/2008  336464
21:54:42.127  25/2005  22
22:04:59.098  25/2005  24
22:09:27.862  25/2008  256464
22:09:27.887  25/2000  41
22:09:44.197  25/2005  14
22:09:53.175  25/2008  296464
22:09:53.186  25/2005  24
22:09:53.822  25/2000  3d
22:27:51.748  25/2005  28
22:27:51.763  25/2000  56
22:41:21.071  25/2008  016464
22:41:21.082  25/2000  63
22:42:52.791  25/2005  20
22:42:53.551  27/2007  03
22:42:53.579  17/1017  620011
22:42:53.606  25/2008  696464
22:42:53.631  25/2005  20
```

## Related work

- [hn/bsh-home-appliances](https://github.com/hn/bsh-home-appliances), existing bshdbus component and contrib examples.
- [SHXM78W56N example](bsh-dbus-shxm78w56n.yaml); note different option assignments.
- [SX66T096EU command notes](https://github.com/jowi24/bsh-dbus-sx66t096eu/blob/main/COMMANDS.md).
- [Dishwasher protocol discussion](https://github.com/hn/bsh-home-appliances/issues/10).
