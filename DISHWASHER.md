# B/S/H/ Home Appliances

:warning: Warning: Please double check that you have read and followed the [safety notes](README.md#warning)

## Dishwashers

### Timelight projector module EPG53533

The timelight projector module is used in dishwashers.
It breaks quite often, the super-bright LED apparently burns the LCD over time.

The module ([Enclosure](photos/bsh-EPG53533-enclosure.jpg), [Enclosure open](photos/bsh-EPG53533-enclosure-open.jpg),
[PCB top](photos/bsh-EPG53533-pcb-top.jpg), [PCB top closeup](photos/bsh-EPG53533-pcb-top-closeup.jpg), [PCB bottom](photos/bsh-EPG53533-pcb-bottom.jpg))
has a 3-pin D-Bus connector (Attention! VBUS is 13.5V instead of 9V) and
uses a [RENESAS R5F104BGA MCU](https://www.renesas.com/en/products/microcontrollers-microprocessors/rl78-low-power-8-16-bit-mcus/rl78g14-low-power-high-function-general-purpose-microcontrollers-motor-control-industrial-and-metering)
(RL78/G14 CISC CPU core, 32pin, 128k flash ROM, 8kb Data flash, 16kb RAM, [datasheet](https://www.renesas.com/en/document/dst/rl78g14-data-sheet?r=1054296)).
The LCD MSGF013733-04 seems to be custom made for B/S/H/ ("04" likely is a hardware revision and
the second line on the sticker likely is the production date in YYMMDD format).

The module uses D-Bus address D=6. Command frames have not yet been analyzed.

### Control board EPG700xx

The control boards (EPG70002, EPG70003, EPG70012 [Enclosure](bsh-EPG70012-enclosure.jpg))
are equipped with an STM32F301VCT6 (sometimes STM32P301VCT6) ARM Cortex M4 processor.
The EPG70002 got two relays for heating while the 70003 got three.
Depending on the board model, there is also an additional motor driver chip FSB50550AB from fairchild semiconductors
to power the fan motor for the Zeolith active drying technology.

The board has three D-Bus connectors (X9, X10, X11), the latter on the
[left side](photos/bsh-EPG700xx-dbus.jpg) with four pins: 1. Unknown, 2. VBUS, 3. DATA, 4. GND.
Additionally, there is one (galvanically isolated) D-Bus connector on the [daughter board](photos/bsh-EPG700xx-dbus-daughter.jpg).

On the [lower left side of the board](photos/bsh-EPG700xx-STlink-pins.jpg)
one can find pins for [SWD debugging](https://stm32-base.org/guides/connecting-your-debugger.html) with an [STLinkV2-Dongle](bsh-EPG70012-STlink.jpg):

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

### Further reading

- [nophead](https://github.com/nophead/) has some [interesting dishwasher findings](https://hydraraptor.blogspot.com/2022/07/diy-repair-nightmare.html).
