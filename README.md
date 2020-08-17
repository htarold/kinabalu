# What
This is the kinabalu recorder, an open stereo autonomous passive audio
recorder.  We use it to record wildlife and environmental sounds
for research and conservation.

The boards/ directory contains design files for the recorder,
which will be needed to replicate the hardware.  All these
design files are licenced CC BY-SA 4.0.

The src/ directory contains the source code that runs on the
recorder.  All these source files are licenced GPLv3 (if they
contain a main function) or LGPLv3.

# Other hardware needed
In order to complete the recorder, you'll need the following:
- Micro SD card (8 to 64GB), FAT32 formatted.
- Lithium ion battery, 1S (3.7V) with JST-XH2 connector (Note
  polarity!).  We use 18650s.
- CH340 or similar 3.3V USB-serial adapter.
- DS3231 RTC module, which you'll need to modify slightly.
- Optionally a BME280 or BMP280 environmental sensor breakout.
- Optionally an STLINK V2 USB adapter.
- A weather proof housing to put everything in.

# Usage
A manual will be released shortly.

# Thanks
Comments and gripes to
htarold@gmail.com
