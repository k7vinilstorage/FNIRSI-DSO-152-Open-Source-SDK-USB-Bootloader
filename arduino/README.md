# FNIRSI DSO-152 Arduino IDE & FreeRTOS Suite

This directory contains everything needed to program the **FNIRSI DSO-152** oscilloscope using the **Arduino IDE**, **`Adafruit_GFX`**, and **`STM32duino FreeRTOS`**.

## Contents

- **[`FNIRSI_DSO152/`](FNIRSI_DSO152/)**: Standard Arduino 1.5+ C++ library (`#include <FNIRSI_DSO152.h>`).
  - Inherits from `Adafruit_GFX` for hardware-accelerated 16-bit parallel ST7789 graphics (`320x240`).
  - Automatically relocates `SCB->VTOR = 0x08004000`, latches battery power (`PC10`), and configures the 72 MHz HSE PLL clock prior to `HAL_Init()` and FreeRTOS scheduler startup.
  - Includes 3 complete example sketches:
    - [`01_HelloWorld_GFX`](FNIRSI_DSO152/examples/01_HelloWorld_GFX/01_HelloWorld_GFX.ino)
    - [`02_Breakout_Arduino`](FNIRSI_DSO152/examples/02_Breakout_Arduino/02_Breakout_Arduino.ino)
    - [`03_FreeRTOS_Oscilloscope`](FNIRSI_DSO152/examples/03_FreeRTOS_Oscilloscope/03_FreeRTOS_Oscilloscope.ino)
- **[`FNIRSI_DSO152.zip`](FNIRSI_DSO152.zip)**: Ready-to-import ZIP archive for **Sketch -> Include Library -> Add .ZIP Library...**
- **[`board_support/`](board_support/)**: Custom STM32duino configuration (`boards.local.txt`, `platform.local.txt`, `upload_dso_boot.sh`, `upload_dso_pyocd.sh`) adding the **FNIRSI DSO-152** board profile and one-click **`DSO BOOT`** USB uploading to the Arduino IDE.
- **[`install_arduino_ide.sh`](install_arduino_ide.sh)**: Automated installer script.

## Quick Setup

```bash
./arduino/install_arduino_ide.sh
```

For full step-by-step instructions and the complete C++ API reference, see **[docs/ARDUINO_IDE.md](../docs/ARDUINO_IDE.md)**.
