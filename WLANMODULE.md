# B/S/H/ Home Appliances - WLAN modules

:warning: Warning: Please double check that you have read and followed the [safety notes](README.md#warning)

## Internet connection module BSH 8001056350 COM1/COMGEN1

### Hardware

The module ([PCB top](photos/bsh-8001056350-COM1-pcb-top.jpg), [PCB bottom](photos/bsh-8001056350-COM1-pcb-bottom.jpg))
is used to connect home appliances to the [Home Connect](https://www.home-connect.com/global) cloud.
The model `COM1 3/21` with FCC ID `2AHES-COMGEN1` uses an
ST [STM32F415](https://www.st.com/en/microcontrollers-microprocessors/stm32f415rg.html) MCU (LQFP64 package, 1024kB flash)
and an external 16MBIT/2024kB SPI MX25L1606EZNI flash.

There is a 30-pin connector on the board (looks like a Hirose DF12 board-to-board/BTB) that,
in certain module configurations (e.g. [EPH71082](https://www.siemens-home.bsh-group.com/de/de/product/12016807)),
is used to connect to the antenna PCB.
The antenna PCB seems to have an [6-pin connector](https://fccid.io/2AHES-COMGEN1/External-Photos/External-photos-2952945) presumably used for programming or debugging the MCU.

### Software

The firmware of the STM32F4 MCU can be dumped via D-Bus with [subsystem S=0 cmds](README.md#subsystem-s0).

The external flash seems not to be memory-mapped within the MCU. It can be dumped with a WSON8 clip (6x5 spacing) like this:
```
flashrom -V -p ch341a_spi -c "MX25L1605A/MX25L1606E/MX25L1608E" -r com1-extflash.bin
```

The dump contains
an SSL client certificate (unique for each module),
various CA certificates for the (private) PKI infrastructure,
various runtime settings e.g. WiFi credentials
and a ZIP file with update images for the module.

## Internet connection module BSH 9001688999 COM2

The module has not been analyzed in detail. Some information can be found [here](https://fcc.report/FCC-ID/2AHES-COM2).
It is suspected that the STM32F4 and the WLAN/Bluetooth were combined into a (propietary?) single IC for the COM2,
which is probably more cost-effective.

## System Master Module 2AHES-M2 - SystemMaster M2

No info yet. See [here](https://fcc.report/FCC-ID/2AHES-M2)

It is suspected that this module not only establishes Internet connectivity,
but also controls the entire logic of the household appliance (hence “system master”),
i.e., it takes over the tasks that were previously performed by the power module.

The SMM modules use a proprietary `BSH Embedded Linux Platform` operating system.
Some parts have been open-sourced, e.g. they have been included in the [Buildroot](https://github.com/buildroot/buildroot/tree/master/board/bsh) distribution.

## System Master Module 2AHES-SMB - SystemMaster S2

No info yet. See [here](https://fcc.report/FCC-ID/2AHES-SMB)

