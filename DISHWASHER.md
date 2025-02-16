# B/S/H/ Home Appliances - Dishwashers

:warning: Warning: Please double check that you have read and followed the [safety notes](README.md#warning)

## Hardware

### Timelight projector module EPG53533

The Timelight projector module breaks quite often,
the super-bright LED apparently burns the LCD over time.

The module ([Enclosure](photos/bsh-EPG53533-enclosure.jpg), [Enclosure open](photos/bsh-EPG53533-enclosure-open.jpg),
[PCB top](photos/bsh-EPG53533-pcb-top.jpg), [PCB top closeup](photos/bsh-EPG53533-pcb-top-closeup.jpg), [PCB bottom](photos/bsh-EPG53533-pcb-bottom.jpg))
has a 3-pin D-Bus connector (Attention! VBUS is 13.5V instead of 9V) and
uses a [RENESAS R5F104BGA MCU](https://www.renesas.com/en/products/microcontrollers-microprocessors/rl78-low-power-8-16-bit-mcus/rl78g14-low-power-high-function-general-purpose-microcontrollers-motor-control-industrial-and-metering)
(RL78/G14 CISC CPU core, 32pin, 128k flash ROM, 8kb Data flash, 16kb RAM, [datasheet](https://www.renesas.com/en/document/dst/rl78g14-data-sheet?r=1054296)).
The LCD MSGF013733-04 seems to be custom made for B/S/H/ ("04" likely is a hardware revision and
the second line on the sticker likely is the production date in YYMMDD format).

### Control board EPG700xx

The control boards (EPG70002, EPG70003, EPG70012 [PCB top](photos/bsh-EPG70012-pcb-top.jpg) [PCB bottom](photos/bsh-EPG70012-pcb-bottom.jpg) [Enclosure](photos/bsh-EPG70012-enclosure.jpg))
are equipped with an STM32F301VCT6 (sometimes STM32P301VCT6) ARM Cortex M4 processor.
The EPG70002 got two relays for heating while the 70003 got three.
Depending on the board model, there is also an additional motor driver chip FSB50550AB from fairchild semiconductors
to power the fan motor for the Zeolith active drying technology.

The board has three D-Bus connectors (X9, X10, X11), the latter on the
[left side with four pins](photos/bsh-EPG700xx-dbus.jpg): 1. Unknown, 2. VBUS, 3. DATA, 4. GND.
Additionally, there is one (galvanically isolated) D-Bus connector on the [daughter board](photos/bsh-EPG700xx-dbus-daughter.jpg).

On the [lower left side of the board](photos/bsh-EPG700xx-STlink-pins.jpg)
one can find connector X20 for [SWD debugging](https://stm32-base.org/guides/connecting-your-debugger.html) with an [STLinkV2-Dongle](bsh-EPG70012-STlink.jpg):

```
1. SWCLK
2. SWDIO
3. /* BOOT0 - not tested */
4. /* RESET - not tested */
5. GND
6. VCC (3.3V)
```

Since the MCU is *not* [protected with RDP](https://stm32world.com/wiki/STM32_Readout_Protection_(RDP))
one can dump the firmware like this:

```
$ openocd -f interface/stlink.cfg \
-c "transport select hla_swd" \
-f target/stm32f3x.cfg \
-c "stm32f3x.cpu configure -rtos auto" \
-c "init" \
-c "reset init" \
-c "flash read_bank 0 firmware.bin 0 0x40000" \
-c "exit"
```

## D-Bus Protocol

```
DS.CC-CC MM MM MM
17.10-00 00 yy       Select wash options (bitfield)
17.10-10 xx          Select wash program xx
24.20-06 xx          xx:bit0 => Rinse aid LED, xx:bit1 => Salt LED
25.20-08 xx yy zz aa bb cc dd ee    Remaining time xx in minutes

DESTINATION ("D" of DS-byte) (physical location in brackets)
0x0 Network management / Broadcast (control board)
0x1 Washing control unit (control board)
0x2 User control panel
0x6 TimeLight module
0xa Internet communication end point (control board)
0xb Internet WiFi connection module
```

Minimal command sequence to light up the TimeLight module:

```
65.10-17 | 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
65.20-07 | 04				# 04 => clock/digit animation
65.20-12 | 01
65.20-06 | 08				# bit 3: turn LED on/off
65.10-12 | 07 03 2a 00 00 01 01		# 03 2a => set HH:MM 03:42 for animation 04
```

## Further reading

- [nophead](https://github.com/nophead/) has some [interesting dishwasher findings](https://hydraraptor.blogspot.com/2022/07/diy-repair-nightmare.html).
