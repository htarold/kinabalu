# What
This is the kinabalu recorder, a stereo autonomous passive audio
recorder.  We use it to record wildlife and environmental sounds
for research and conservation.

The boards/ directory contains design files for the recorder,
which will be needed to replicate the hardware.  All these
design files are licenced CC BY-SA 4.0.

# Hardware
The `boards/`
directory contains the design files; use `gEDA pcb`
and `gschem `
to modify them as desired.  All the hardware source files are CC-BY-SA-4.0.

If you wish to
replicate the boards, you'll want to zip the contents of the
    `boards/fab/`
directory (Gerbers and such) to send to the board house.  The `audio2`
board is the recorder proper; the `preamp2`
board is the stereo preamp for the microphones.  The version
here is for recording normal audio from electret condenser mics,
and it provides the requisite phantom power.
But you can make a custom preamp for other frequencies and
microphone types.

These are both 2-layer boards and very lax in their
requirements.  The usual 6/6 rules are more than sufficient.

The bills of materials allow for substantial flexibility in
sourcing the components, especially passives, so you can use
your favourite suppliers.

# Software

The `src/`
directory contains the source code that runs on the
recorder.  All these source files are licenced GPLv3 (if they
contain a main function) or LGPLv3 otherwise.  As it stands, use
`arm-none-eabi-gcc` and GNU `make`
to build.  You will also need to install `libopencm3`
available at [libopencm3](https://github.com/libopencm3/libopencm3).

The main target is `kinabalu.elf`
which you build as `make kinabalu.elf`
in the `src/` directory.

If you are running Windows, download and install
[FLASHER-STM32](https://www.st.com/en/development-tools/flasher-stm32.html)
from the microcontroller manufacturer (registration may
be needed).  `FLASHER-STM32` uses the
same USB-serial adapter (CH340 or equivalent) you use to talk to
the recorder, so you don't need to buy extra hardware.

This [video](https://www.youtube.com/watch?v=VDNJUsWvI0o)
(thanks Raul!) is a quick demo on using `FLASHER-STM32`.

If you are running Linux (Ubuntu etc.), then download and build
[stm32flash](https://github.com/stm32duino/stm32flash/tree/master).
You can use this command to upload the firmware:
```
$ stm32flash -w firmware.bin /dev/ttyUSB0
```
to upload `firmware.bin` via serial port `/dev/ttyUSB0`.

# Other hardware needed
In order to complete the recorder, you'll need the following:
- Micro SD card (8 to 64GB), FAT32 formatted.
- Lithium ion battery, 1S (3.7V) with JST-XH2 connector (Note
  polarity!).  A single 18650 is good for about 24 hours of
  recorded sound.
- CH340 or similar 3.3V USB-serial adapter.
- DS3231 RTC module, which you'll need to modify slightly (see
  [boards/README.md](boards/README.md)).
- Optionally a BME280 or BMP280 environmental sensor breakout.
- Optionally an STLINK V2 USB adapter for debugging.
- A weather proof housing to put everything in.

# Usage
The [sample config
file](https://github.com/htarold/kinabalu/blob/master/src/SITEA-0.LOG)
contains usage and troubleshooting instructions.

# Thanks
Comments and gripes to
htarold@gmail.com
