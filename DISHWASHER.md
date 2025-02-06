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
