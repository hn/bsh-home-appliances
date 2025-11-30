# B/S/H/ Home Appliances - WLAN modules

:warning: Warning: Please double check that you have read and followed the [safety notes](README.md#warning)

## Hardware

### Internet connection module BSH 8001056350 COM1/COMGEN1

The module ([PCB top](photos/bsh-8001056350-COM1-pcb-top.jpg), [PCB bottom](photos/bsh-8001056350-COM1-pcb-bottom.jpg))
is used to connect home appliances to the [Home Connect](https://www.home-connect.com/global) cloud.
The model `COM1 3/21` with FCC ID `2AHES-COMGEN1` uses an
ST [STM32F415](https://www.st.com/en/microcontrollers-microprocessors/stm32f415rg.html) MCU (LQFP64 package, 1024kB flash)
and an external 16MBIT/2024kB SPI MX25L1606EZNI flash.

There is a 30-pin connector on the board (looks like a Hirose DF12 board-to-board/BTB), which is used to connect to the antenna PCB.
The antenna PCB seems to have an [6-pin connector](https://fccid.io/2AHES-COMGEN1/External-Photos/External-photos-2952945) presumably used for programming or debugging the MCU.
Unfortunately the antenna PCB is missing on my side.

### Internet connection module BSH 9001688999 COM2

The module has not been analyzed in detail. Some information can be found [here](https://fcc.report/FCC-ID/2AHES-COM2).
It is suspected that the STM32F4 and the WLAN/Bluetooth were combined into a (propietary?) single IC for the COM2,
which is probably more cost-effective.

