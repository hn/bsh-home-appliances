# Open-D-Bus

This is an alpha quality approach for a B/S/H/ D-Bus 2 stack, which provides
the infrastructure to implement one or more nodes capable of bidirectional
frame communication on the bus.

To put it more vividly: This stack is intended to eventually facilitate
the development of custom components for B/S/H household appliances.
It is entirely conceivable to e.g. remove the original user interface panel
from a machine and replace it with a custom-made interface powered by
this software. While implementing a full replacement would be a significant
undertaking, the some of the initial groundwork is here.

This is an independent, open-source project and is not affiliated with,
endorsed by, or sponsored by B/S/H/ or its affiliates. All product names
and trademarks are the property of their respective owners. References
to B/S/H/ appliances are for descriptive purposes only and do not imply
any association with B/S/H/.

## Status

This code is a direct artifact of reverse engineering; any resemblance
to “good code” is purely coincidental. Feel free to improve.

The current implementation reads D-Bus traffic via UART, acknowledges valid
frames for local nodes, and handles local queuing and processing of frames
through dedicated handler functions. The D-Bus subsystem nibble is
currently ignored for incoming frames.

## Documentation

There is no documentation. The easiest way to understand how to send, receive,
and process frames is to examine the `node-*` files in the `src` directory.

## Example Nodes

You can enable a node by calling `DBUS_ADDNODE()` in main.cpp.
Do not enable more than one node with the same node id.

### node-dishwasher-EPG60110-1-39C3.cpp

Emulates the dishwasher power module EPG60110 at D-Bus address 0x1. It sends
frames to the user interface panel BSH9000329063 to display custom strings and UI
elements. This was used during the
[39C3 talk](https://media.ccc.de/v/39c3-hacking-washing-machines) to
demonstrate external control over the appliance display.

### node-dishwasher-a-COM1.cpp (not released yet)

Emulates endpoint 0xA of a dishwasher power module, performing the handshake
with a COM1 WiFi module until it is fully initialized. This was also featured
in the 39C3 demonstration.

### node-huda-c.cpp

A small toolbox node at 0xC designed for tinkering with participants on the D-Bus.

If `WIFI_SSID` and `WIFI_PASS` are defined during compilation, the module connects
to your WiFi and can be controlled via [WebSerial](https://github.com/mathieucarbou/MycilaWebSerial).
This is particularly useful for maintaining galvanic isolation from the household
appliance during testing.

Any text entered into the terminal is interpreted as frame content;
the module automatically adds the length and CRC before sending it directly to the D-Bus.
This allows for manual D-Bus testing and exploration.

Example output for the Unbalance sensor (D=4) with a request to send readings for 5 seconds:

```
47.40-02 c7 80 05

05 | 47.40-02 | C78005 | CFAC
09 | C7.40-10 | 00FADF01E501A4 | 2CF1
09 | C7.40-10 | 00F8FF00CA02AF | 977E
09 | C7.40-10 | 00F802FB6A032C | 2746
09 | C7.40-10 | 00FC5B02520180 | 8E93
```

If a terminal entry begins with a colon (`:`), it is treated as a special control command.
The following commands are available:

| Command | Description |
| :--- | :--- |
| _Node_ ||
| `:nn<hexnumber>` | Sets the node to be addressed (e.g., :nn2 for the user interface panel, often referred to as `D` in other parts of this project) |
| `:nm<hexnumber>` | Selects the software module within the node (note: this refers to the software module, which is _not_ to confuse with the subsystem typically referred to as `S`) |
| `:ni` | Requests the ID string from the software module. If a response is received, the module's info string memory address is automatically transferred to the memory start listed below |
| _Memory_ ||
| `:mb<hexaddr>` | Sets the start address for the memory dump |
| `:me<hexaddr>` | Sets the end address for the memory dump |
| `:mp<hexaddr>` | Sets the current read position for the memory dump (starts at 0) |
| `:mc<number>` | Sets the chunk size for the memory dump. Older modules sometimes require a small or even 1-byte chunk size |
| `:md` | Starts the dump. The output can be converted to binary using `xxd -r -l256` |

Example output for Timelight module (D=6):

```
:nn6
| NODE number=0x6 module=0 bits=0 | MEMORY begin=0x8000000 end=0x8000100 pos=0x000000 chunk=16 |
:ni
| NODE number=0x6 module=0 bits=0 | MEMORY begin=0x8000000 end=0x8000100 pos=0x000000 chunk=16 |
03 | 60.FF-00 | C0 | A12F
08 | C0.FE-00 | 60000001FFA8 | 2689, handled by: Identify response, Node 0x6, Module 0, info string at 0x0001ffa8 (32 bit node)
:md
| NODE number=0x6 module=0 bits=32 | MEMORY begin=0x01ffa8 end=0x020028 pos=0x000000 chunk=32 |
00000000: 4330 322d 3030 3600 3830 3031 315f 3031 3535 3400 3138 3031 3138 3133 3231 6400  C02-006.80011_01554.1801181321d.
00000020: 3930 3031 3336 3638 3437 0000 0056 574b 4936 4453 0535 33ff 3930 3031 3336 3638  9001366847...VWKI6DS.53.90013668
00000040: 3437 ffff 0307 ffff 1907 1515 aaff ffff 60ca ffff 0101 ffff ffff ffff ffff ffff  47..............`...............
```

