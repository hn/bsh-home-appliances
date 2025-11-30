# B/S/H/ Home Appliances

## Preamble

[B/S/H/](https://www.bsh-group.com/) is one of the world's largest manufacturers of home appliances (according to [this source](https://www.faz.net/aktuell/technik-motor/technik/bsh-geschirrspueler-so-schnell-produziert-das-werk-in-dillingen-19341085.html), 10,000 dishwashers are produced _every day_ at their factory in [Dillingen](https://wiki.bsh-group.com/de/wiki/Die_Fabrik_und_das_Technologiezentrum_Dillingen/en) alone).
Some of [their well-known brands](https://en.wikipedia.org/wiki/BSH_Hausger%C3%A4te#Brands) are Bosch, Siemens, Gaggenau, Neff, Constructa and Balay.
They have a [well-maintained wiki](https://wiki.bsh-group.com/de/wiki/Hauptseite/en) where you can find all kinds of information about the company's history and the factories.

Despite this popularity, little is publicly known about the inner workings of the appliances.
This project shows some of the things that have been discovered through reverse engineering.
This project is in no way affilliated with B/S/H/.
No intelectual property of B/S/H/ was used at any time.

Please do me a favor: :thumbsup: If you use any information or code you find here, please link back to this page.
:star: Also, please consider to star this project. I really like to keep track of who is using this to do creative things, especially if you are from other parts of the world.
:smiley: You are welcome to open an issue to report on your personal success project and share it with others.

## Warning

Household appliances work with high voltages and currents. They generate heat and steam, they shake, vibrate and have rotating parts.
You should have a good knowledge of electrics and take appropriate safety measures before opening or modifying such an appliance. Parts can get wet or hot, move or fall off.

:warning: Warning: Even if parts of the appliances operate at low voltages, depending on your specific model they may or may not be isolated from earth. This is a good chance of death or serious injury.
It is also not recommended to lead cables out of the appliance. So if you're dreaming of connecting your Raspberry Pi to the control electronics, it's probably not a good idea.

:warning: Warning: I'm not responsible if you kill your cat, your wife or yourself, whichever of these is the worst for you. You have been warned.

## Hardware

Even if the marketing materials suggest otherwise, the technology of household appliances is relatively straightforward.
A number of motors, heaters, valves and sensors are connected to a control unit and made available to the user via the fanciest possible user interface.
It also seems to be the case that a manufacturer's devices, regardless of their external appearance and brand logo, differ only marginally on the inside.

:raised_hand: The following findings are probably only applicable to a certain generation of B/S/H/ home appliances, namely those with "EP" circuit boards.
The washing machine boards are labeled "EPW", which probably stands for "Elektronik Platine Waschmaschine" (electronic circuit board washing machine).
The dryer boards are labeled "EPT", with "T" for "Trockner" (dryer) and the dishwasher boards are using "EPG", with "G" for "Geschirrspüler".
However, there is currently no known way of deducing the board version used internally from the external appearance or the model name.
You have to open the machine and look inside.

The control electronics were likely developed by [BSH PED in Regensburg](https://wiki.bsh-group.com/de/wiki/BSH_Regensburg_%E2%80%93_Produktbereich_Electronic_Systems,_Drives_(PED)).
The site has worked for the automotive industry in the past, which explains why some components and approaches share similarities.

### Washing machine Siemens WM14S750

The washing machine WM14S750 was sold from around 2010, when it was the test winner of the german magazine Stiftung Warentest.
It was likely produced at the [Nauen factory](https://wiki.bsh-group.com/de/wiki/Die_Fabrik_Nauen/en).
B/S/H/ has released a very nice [video series](https://www.youtube.com/watch?v=T_dcbVZmExU&list=PL8rSAtdt1rTpldsxcCMCIHwK_7gH2FWEZ) in which many details from the production in Nauen can be seen.

#### Control board EPW66018

The control board ([PCB front](photos/bsh-EPW66018-pcb-front.jpg), [PCB back](photos/bsh-EPW66018-pcb-back.jpg)) is based on an Freescale (now NXP) Semiconductor `MC9S12Q128C-PBE16` MCU (52-pin LQFP package).
It also supplies power to all connected electronics.
The sticker suggests it was produced by [melecs](https://www.melecs.com/en).

It has 11 [RAST connectors](https://de.wikipedia.org/wiki/RAST-Steckverbinder) for I/O (numbering according to [PCB front](photos/bsh-EPW66018-pcb-front.jpg)):
```
01 ?
02 Switch (not front door?)
03 ?
04 ?
05 ?
06 ?
07 ?
08 Pressure sensor and likely D-Bus on one half of the connector, not tested
09 D-Bus
10 D-Bus
11 D-Bus
```

#### User control panel EPW66027

The control panel ([Front view](photos/bsh-EPW66027-front.jpg), [back view](photos/bsh-EPW66027-back.jpg)) lets the user select 15 different washing programs, 8 temperatures, 7 spin speeds and and other options.
The (mechanical) rotary switch is not only used to select the program, but also to switch the mains power on and off.
The control panel is connected to the control board via D-Bus.

#### Unbalance sensor SEUFFER 9000444823

This sensors detects unbalanced loads ([PCB front](photos/bsh-9000444823-pcb-front.jpg), [PCB back](photos/bsh-9000444823-pcb-back.jpg), [Enclosure](photos/bsh-9000444823-enclosure.jpg)).
It uses an [PIC18F24J10](https://www.microchip.com/en-us/product/pic18f24j10) MCU (28 pin QFN package)
and an `A007MPL DREMAS` IC, which [according to the manufacturer](https://www.ast-international.com/en.products.position-force-sensors.html#product-11) is a specialized 3D hall sensor
(the [corresponding magnet](https://www.siemens-home.bsh-group.com/uk/shop-productlist/00615666) is attached to the washing drum).
The device is connected to the control board via D-Bus.

There are 5 contact points on the back of the board that form a contact for [in-circuit serial programming (ICSP)](https://en.wikipedia.org/wiki/In-system_programming#Microchip_ICSP) (top to bottom):
`(+ sign), VDD, PGC, PGD, MCLR, VSS/GND`. The chip can be interfaced with a [PICkit](https://www.microchip.com/en-us/development-tool/pg164130) programmer
and the [MPLAB IPE](https://www.microchip.com/en-us/tools-resources/production/mplab-integrated-programming-environment).
The code can not easily be downloaded because the device has the Code Protection Bit set.

### D-Bus

According to the [B/S/H/ patent documents](#misc), [this](https://www.mikrocontroller.net/topic/395115#4543950) and [this](https://forums.ni.com/t5/Instrument-Control-GPIB-Serial/Has-anybody-used-D-Bus-to-communicate-with-and-or-control/m-p/4284296#M84901) forum post, the electronics inside the device are interconnected via a proprietary serial bus called D-Bus or D-Bus-2.
Since there are no public technical specifications, it can only be speculated that the D-Bus-1 corresponds more to the CAN bus and the D-Bus-2 more to a bus with UART data framing.

The bus found on the "EP" circuit boards likely is a D-Bus-2 and consists of three wires: GND, VBUS and DATA. VBUS is 9V (13.5V for dishwashers) and DATA is TTL (5V).
Connections are established using 3 pin 2.5 pitch [RAST connectors](https://de.wikipedia.org/wiki/RAST-Steckverbinder).
The connectors have coding lugs to ensure that they cannot be plugged into the wrong socket.
They are commercially available from [Lumberg](https://www.lumberg.com/en/products/product/3521), [Stocko](https://www.stocko-contact.com/downloads/STOCKO_Connector%20systems_pitch%202.5_ECO-TRONIC_de_en.pdf) and probably many more suppliers.
B/S/H/ sells somewhat pricy pre-assembled cables as well, e.g. the [spare part 00631780](https://www.siemens-home.bsh-group.com/de/produktliste/00631780).

:warning: Watch out: The assignment of the connector depends on the end point: on the control board the connector is configured as GND-DATA-VBUS and then the cable is crossed and on the other side (e.g. for sensors) the wiring is VBUS-DATA-GND:

![BSH D-Bus 2 pinout, control bord bottom, slave top right](bsh-dbus-pinout.jpg)

It looks as if B/S/H/ has gradually introduced the D-Bus more and more into home appliances over the years:
- pre-2006 washing machines use the D-Bus only to control the display (in a rather [simplistic way](https://github.com/hn/bsh-home-appliances/blob/master/contrib/bsh-dbus-wae284f0nl.yaml#L122)),
- 2006-2010 washing machines only use the D-Bus for the unbalance sensor and the control panel,
- post-2010 appliances also [control the motor](https://github.com/hn/bsh-home-appliances/issues/3#issuecomment-2367437363) (and presumably other components) via the D-Bus.

All dates are only rough estimates, as the various models were produced and sold over longer periods of time.

### Other home appliance types

The hardware of other home appliance types has been examined:

- [Dishwashers](DISHWASHER.md)
- [Dryers](DRYER.md)
- [WLAN module to connect to the Home Connect cloud service](WLANMODULE.md)

## Protocol

### D-Bus 2

Data is transmitted on the D-Bus 2 in a `8N1` configuration and the washing machine uses a transfer rate of 9600 baud.
The `COM1` internet connection module cyclically tries out transfer rates from 9600 up to 38400 baud during startup, so newer devices probably use one of the higer rates.

The D-Bus 2 is a (1-wire, the DATA wire) bus and not a straight serial cable (2-wire, RX and TX).
All participants are reading (and possibly writing) the bus simultaneously.
It is not yet clear whether the control panel acts as a master for the coordination or whether it is a [multi-master bus](https://en.wikipedia.org/wiki/Multi-master_bus)
(where something simliar to [CMSA/CR](https://de.wikipedia.org/wiki/Carrier_Sense_Multiple_Access/Collision_Resolution) or [CSMA/CD](https://en.wikipedia.org/wiki/Carrier-sense_multiple_access_with_collision_detection) is used).

This is by no means a complete description of the D-Bus 2 protocol, but rather a layman's approach.
If you look at the comparable specification for other serial buses like the [CAN bus](https://en.wikipedia.org/wiki/CAN_bus) or [LIN bus](https://en.wikipedia.org/wiki/Local_Interconnect_Network),
you will see that there may be many more bits and tricks.

#### Frame format

Data on the bus is transmitted in frames.
These frames consist of a length byte, a control byte, some data bytes and a two-byte (CRC16-XMODEM, thanks [reveng](https://reveng.sourceforge.io/)) checksum.
For example, the frame `05 14 10 05 00 FF 00 DE 62` can be decoded as follows:

```
05 14 10 05 00 FF 00 DE 62
LL DS MM MM MM MM MM RR RR

LL = length, 5 message data bytes follow, total frame length is 9 = 1+1+5+2
DS = destination node and subsystem
MM = message data with length LL
RR = checksum (CRC16-XMODEM)
```

The destination node ("D" of DS-byte) designates the (unique) logical receiver of the frame and not necessarily a piece of hardware.

The subsystem nibble ("S" of DS-byte) specifies the subsystem within the destination node.
For lower complexity nodes it is of little importance (e.g. the unbalance sensor ignores the value of the “S” nibble),
for higher complexity nodes (e.g. the control board) it seperates logical sections within the program code.

##### Command frames

Some further considerations suggest that the first and second message byte have a special meaning:

```
05 14 10 05 00 FF 00 DE 62
LL DS M1 M2 MM MM MM RR RR  =>  LL DS.CC-CC MM MM MM RR RR
```

All frames with identical DS-M1-M2 bytes have the same length.
M1-M2 is therefore relabeled CC-CC (command),
which then also determines the length and format of the following message bytes.
The notation DS.CC-CC is introduced here as a distinct declaration for a command frame.

For the DS.CC-CC tuples, there is only a very small, overarching defined set of commands ([subsystem S=0](#subsystem-s0)).
All other commands and communication concepts depend heavily on the machine type (washing machine, dryer, etc.),
the respective model and the components used.

##### Other frame types

It is very possible that there are also other 'non CC-CC' types of frames
(e.g. a "stream mode" for uploading binary firmware).
However, these have not yet been observed in normal operation.

#### Acknowledgement byte

After sending a frame, the sender leaves a gap of at least one byte
(with 9600 baud data rate and 8N1 coding an idle time of 1.042 ms = 10 x 104.167 µs)
before before possibly sending the next frame.
The receiver responsible for processing the frame inserts an acknowledgement byte precisely into this gap.
The sender can read this byte and knows that the frame has been delivered successfully.

The lower bits of the acknowledgement byte are always set to 0xA (can it be that it is "A" for "Acknowledgement", really?)
and the upper bits correspond to those of the receiving target: `ACK = 0x0A | (DS & 0xF0)`.
This needs further investigation: on certain models or software versions, the acknowledgement byte
contains the identifier of the control unit (`ACK = 0x1A`) if DS was a network broadcast (`DS=0x0F`).

#### Boot log

A typical boot sequence just for the control board and control panel starts like this:

```
        LL   DS CC CC   MM MM MM MM   RR RR            ACK
0.412s  04 | 0f.e7-00 | 01 02       | a5 ec (crc=ok) | 0a (ack=ok)
0.412s  03 | 0f.e0-00 | 00          | 9a 0d (crc=ok) | 0a (ack=ok)
0.412s  04 | 1f.e8-00 | 01 02       | 75 58 (crc=ok) | 1a (ack=ok)
0.412s  04 | 1f.e0-01 | 01 02       | c7 ab (crc=ok) | 1a (ack=ok)
0.581s  04 | 0f.e7-00 | 02 02       | f0 bf (crc=ok) | 0a (ack=ok)
0.581s  04 | 1f.e8-00 | 02 02       | 20 0b (crc=ok) | 1a (ack=ok)
0.617s  02 | 0f.ef-ff |             | df 25 (crc=ok) | 0a (ack=ok)
0.617s  03 | 13.15-00 | 00          | c6 3a (crc=ok) | 1a (ack=ok)
0.768s  04 | 0f.e7-00 | 32 03       | e5 0b (crc=ok) | 0a (ack=ok)
0.849s  04 | 26.17-01 | 25 25       | 33 fe (crc=ok) | 2a (ack=ok)
0.849s  03 | 22.13-01 | 0a          | bc bc (crc=ok) | 2a (ack=ok)
0.849s  12 | 12.13-00 | 04 00 01 01 00 00 00 00 01 04 00 00 01 00 01 5a | e8 05 (crc=ok) | 1a (ack=ok)
1.028s  05 | 14.10-05 | 00 ff 01    | ce 43 (crc=ok) | 1a (ack=ok)
1.028s  03 | 14.10-04 | 04          | f0 a7 (crc=ok) | 1a (ack=ok)
1.028s  03 | 14.10-06 | 89          | d6 e0 (crc=ok) | 1a (ack=ok)
1.062s  03 | 14.10-08 | 00          | f5 4e (crc=ok) | 1a (ack=ok)
1.062s  05 | 14.10-07 | 40 00 01    | 3d 79 (crc=ok) | 1a (ack=ok)
1.100s  04 | 14.10-09 | 00 00       | 0e cb (crc=ok) | 1a (ack=ok)
1.634s  04 | 0f.e7-00 | 32 03       | e5 0b (crc=ok) | 0a (ack=ok)
1.832s  04 | 0f.e7-00 | 32 03       | e5 0b (crc=ok) | 0a (ack=ok)
1.912s  06 | 26.12-00 | 02 02 00 00 | 5d d4 (crc=ok) | 2a (ack=ok)
```

In the test setup, the motor controller was not connected, so you can see that
the control unit tries to communicate with it several times
in vain (request `0f.e7-00 32 03` with no matching `1f.e8-00` response).

#### Interpreting frame data (WM14S750)

A closer look at the frame data for the washing machine reveals the following meaning:

```
Washing machine WM14S750

DS CC CC MM MM MM
14.10-04 xx          Temperature: xx = 0=>20°, 1=>30°, 2=>40°, 3=>50°, 4=>60°, 5=>70°, 6=>80°, 7=>90°
14.10-05 xx ff yy    Washing program: yy_dec = 1 .. 15 (xx = ?)
14.10-06 xx          Spinning speed, multiply xx by 10 to get rpm: xx_dec = 0, 40, 60, 80, 100, 120, 137
14.10-07 xx yy zz    xx = FEATUREBITS1, yy = FEATUREBITS2, zz = 0 .. 2 VarioPerfect program number
14.10-08 00          ?
14.10-09 00 00       ?
15.11-00 01          Start button pressed on user control panel
26.10-20 xx          Wash program module xx
26.11-01 xx yy       Wash programm status: xx = 0=>Stopped, 1=>Running/end 2=>Running
26.12-00 xx yy zz    Front door status: xx = 0=>Closed+Unlocked, 1=>Closed+Locked, 2=>Open
26.17-01 00 ff       Wash program started by the washing control unit
2a.16-00 xx          Remaining time xx in minutes
47.40-02 xx yy zz    Request the unbalance sensor to send data for zz seconds (xx = node to which the response is to be sent, usually xx=17)
17.40-10 00 xh xl yh yl zh zl    Response of the 3D unbalance sensor: x/y/z readings (3x signed int16)

FEATUREBITS1 = Logical OR of
0x02 = Water Plus / Wasser Plus
0x20 = Stains / Flecken
0x40 = ?
0x80 = Prewash / Vorwäsche

FEATUREBITS2 = Logical OR of
0x80 = Anti-crease protection / Knitterschutz

DESTINATION ("D" of DS-byte) (physical location in brackets)
0x0 Network management / Broadcast (control board)
0x1 Washing control unit (control board)
0x2 User control panel
0x3 Motor controller
0x4 Unbalance sensor
0xa Internet communication end point (control board)
0xb Internet WiFi connection module

SUBSYSTEM ("S" of DS-byte)
0x4 Set parameters
0x5 Start or stop processes
0x6 User info I
0xa User info II
0x7 Unbalance sensor communication
0xf Network management / Broadcast
```

Pure guesswork:

```
DS CC CC
0f.e7-00 xx yy    Request dest yy to change operating state to xx
1f.e8-00 xx yy    Response of dest yy that operating state has been changed to xx
0f.e0-00 xx       Request dests to ping back if their operating state is >= xx
1f.e0-01 xx yy    Response of dest yy that operating state is xx
0f.ef-ff          End of initialization, changes operating states of all dests to 0x5
22.13-01 0a       Read hardware/state/something info from user control panel request
12.13-00          Read hardware/state/something response
13.15-00 00       ?
```

## Software

With just an [ESP8266, a piggyback 9V-to-5V-DC-DC converter and a few cables](bsh-dbus-esp8266-logger.jpg) one easily can interface the D-Bus
(lab setup only, not recommended to be installed in a real home appliance, see notes below).

:warning: Warning: Please double check that you have read and followed the [safety notes](#warning) before connecting anything to a live device.
I repeat: It is not guaranteed that the D-Bus is isolated from earth.

:question: The [ESP8266 GPIOs are 5V-tolerant](https://twitter.com/ba0sh1/status/759239169071837184),
but unsure whether this is the best way to connect to the D-Bus or whether pull-up resistors or similar are missing.
Please help if you have expertise in this area.

:question: Additionally, the ESP8266 is drawing too much power from the 9V VBUS pin.
During the WiFi connection phase, [current spikes of up to 430mA](https://www.ondrovo.com/a/20170207-esp-consumption/) occur, which seems to exceed the maximum available current of the machine's power supply.
Adding a 470uF capacitor between the GND and VBUS pin of the D1 and limiting the WiFi output power (`output_power: 10.5dB`) seems to be a dirty workaround, though not a reliable solution everyone should use.
Please help if you have expertise in this area.

### Arduino

[bsh-dbus-logger.ino](bsh-dbus-logger.ino) aims to fully interpret bus traffic including acknowledgements. It's work in progress, though.

### Home Assistant / ESPHome

[bsh-dbus-wm14s750.yaml](bsh-dbus-wm14s750.yaml) is a ESPHome config file to integrate the washing machine in Home Assistant:

![BSH washing machine Home Assistant](bsh-wm-home-assistant.png)

It uses an [external component](https://esphome.io/components/external_components.html)
called [bshdbus](components/bshdbus/). There is no documentation (yet) but
you'll quickly get the idea on how to add your own sensors.

The old proof-of-concept code [bsh-dbus-wm14s750-poc.yaml](bsh-dbus-wm14s750-poc.yaml)
is preserved for debugging and archiving purposes.

#### Other home appliance models

Fortunately, people have started to create configuration files for devices they own.
You can find them in the [contrib](contrib/) directory.
You are very welcome to add more devices, just open a [pull request](https://docs.github.com/de/pull-requests/collaborating-with-pull-requests).

## Firmware analysis

### Basics

Some components are [shipped without readout protection and with debug interface exposed](DISHWASHER.md#control-board-epg700xx),
making them well suited for static and dynamic firmware analysis.

With your [favourite RE tool](https://reverseengineering.stackexchange.com/questions/1817/is-there-any-disassembler-to-rival-ida-pro) one can find arrays of structs defining
the subsystems compiled into the specific version of the device firmware.
For example, these structures can be identified for the dishwasher firmware:

```
struct dbus_subsystem {
    byte subsystemid;			/* S */
    struct dbus_cmdin * p_s_cmdin;	/* array of incoming cmds */
    struct dbus_cmdout * p_s_cmdout;	/* array of outgoing cmds */
    byte * p_b_numcmdout;		/* size of cmdout array */
    pointer p_sramunknown;		/* likely state byte */
};
```

The subsystem definition refers to more detailed arrays of incoming and outgoing
commands defined for this subsystem:

```
struct dbus_cmdin {
    byte eolist;			/* byte & 0x80 != 0 => end of list */
    byte unkknown;
    word cccc;
    pointer p_f_cmdin_handle;		/* int f_cmdin_handle(int len, byte* buf) */
};

struct dbus_cmdout {
    byte unknown;
    byte ds;
    word cccc;
    byte len;
    pointer p_f_fillbuf;		/* int f_fillbuf(int len, byte* buf) */
    pointer p_f_unknown;		/* mostly no-op */
};
```

The best way to initially locate these structures is to search the firmware binary
for known cmdout DS.CC-CC byte sequences (litte endian).
By iterating through the arrays, one can list all subsystems
with all corresponding incoming and outgoing CC-CC commands.
The intended action of the respective commands can be determined by analyzing the linked functions.
Some arrays are located in SRAM, as the firmware dynamically patches
the definitions according to its specific runtime requirements.

### Subsystem S=0

The subsystem S=0 appears to be the same for all devices and allows, among other things,
the memory content to be read out, which is a convenient way of
dumping the firmware (flash) and RAM content via D-Bus:

```
DS.CC-CC
_0.f0-00 xx yy zz zz        Request dest to dump yy bytes of memory starting at address zz (16 bit)
_0.f0-01 xx yy zz zz zz zz  Request dest to dump yy bytes of memory starting at address zz (32 bit)
                            (xx = node to which the response is to be sent)

_0.f1-00 xx yy zz[]         Response of node xx with yy bytes of memory data zz (16 bit)
_0.f1-01 xx yy zz[]         Response of node xx with yy bytes of memory data zz (32 bit)
```

The subsystem offers some more commands which have not been analyzed yet.

## Serial numbers

B/S/H/ type plates consist of [a variety of different information](https://www.bosch-home.com/de/service/typenschildfinder).
Of particular interest are the "E", "FD" and "Z" numbers as well as the serial number, which contain various (redundant) information.

"E", "FD" and "Z" are usually grouped in one block and look like this: `E-Nr.: WAY32541FG/37 FD: 9504 Z-Nr. 200221`.
Sometimes the letters and punctuation seem to be omitted, as here `E-Nr.: WAY32541FG/37 9504 200221`.
Serial numbers contain just digits and look like this: `485040275653002210`.

With the above example, the numbers have the following meaning:

```
E-Nr.  WAY32541FG/37
       MMMMMMMMMM/RR  Model M=WAY32541FG with hardware revision R=37
       If you buy spare parts, you need to know the revision, as these may differ.
       "E" stands for "Erzeugnis-Nummer" (produce number)

FD-Nr. 9504
       YYMM  Year = (YY + 20)%100, Month = MM, e.g. (95+20)%100=15, the device has been produced in April 2015
       "FD" stands for "Fertigungs-Datum" (manufacturing date)

Z-Nr.  200221
       LNNNNN
       This number identifies the production line L=2 within the factory and
       the sequence number N=221 of the device assembled on this line.
```

The serial number contains partially redundant information:

```
485040275653002210
FFYMMPPPPPPPNNNNNC

     FF = Code for the factory where the device was assembled
      Y = Manufacturing year (without decade)
     MM = Manufacturing month
PPPPPPP = Manufacturer internal code, which contains information about the suppliers
  NNNNN = Sequence number "N" of the device assembled (without production line "L" info)
      C = Check digit for all preceding digits
```

The check digit "C" is calculated using the [GTIN algorithm](https://en.wikipedia.org/wiki/Check_digit#UPC,_EAN,_GLN,_GTIN,_numbers_administered_by_GS1).

The information in this paragraph was compiled from various forum posts, there is no known primary source.

## Misc

- B/S/H/ has received several patents that mention the D-Bus:
  - [DE102009046706A1](https://worldwide.espacenet.com/patent/search/family/043510107/publication/DE102009046706A1?q=pn%3DDE102009046706A1)
    describes a USB to CAN or D-Bus gateway.
  - [DE102009026752A1](https://worldwide.espacenet.com/patent/search/family/042480518/publication/DE102009026752A1?q=pn%3DDE102009026752A1)
    describes a way to interact with the home appliance for remote customer service.
  - [DE102013205754A1](https://worldwide.espacenet.com/patent/search/family/050343707/publication/DE102013205754A1?q=pn%3DDE102013205754A1)
    describes how to add a cryptography layer to the D-Bus (a kind of DRM to protect e.g. cooking recipes).

- The eBUS (“energy bus” used by numerous heating systems) has similarities to the D-Bus. The [eBUSd project](https://ebusd.eu/) is very
  mature, they even have designed a clever [hardware adapter](https://adapter.ebusd.eu/index.en.html).
  The [specification](https://adapter.ebusd.eu/Spec_Prot_12_V1_3_1.pdf) is well worth reading and helps a lot to understand the D-Bus,
  although the protocol differs in some aspects.

- I don't own a WM14S750 washing machine, but only, let's call it, a [lab setup](bsh-wm-lab-setup.jpg).

- If you think this is all weird stuff, you haven't seen the [Zabex washing machine project](https://www.zabex.de/site/waschmaschine.html).
