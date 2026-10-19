# FNIRSI DSO-152 Open-Source SDK, Arduino IDE Suite & USB Bootloader

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![MCU](https://img.shields.io/badge/MCU-AT32F403A%20%2F%20CH32F103%20%2F%20STM32F103-orange.svg)](#hardware-specifications)
[![Arduino IDE](https://img.shields.io/badge/Arduino_IDE-Supported-00979D.svg)](docs/ARDUINO_IDE.md)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-Ready-brightgreen.svg)](docs/ARDUINO_IDE.md)
[![Clock](https://img.shields.io/badge/Clock-72%20MHz-green.svg)](#hardware-specifications)
[![Display](https://img.shields.io/badge/Display-ST7789%20320x240%20IPS-yellow.svg)](#hardware-specifications)

An open-source **Arduino IDE & FreeRTOS Library (`FNIRSI_DSO152`)**, bare-metal **C Software Development Kit (`libdso152`)**, and cross-platform **USB Mass Storage Bootloader (`DSO BOOT`)** for the **FNIRSI DSO-152** handheld digital oscilloscope.

Turn your DSO-152 into an open platform for custom firmware, multi-tasking FreeRTOS instruments, `Adafruit_GFX` applications, retro arcade games, signal generators, and embedded development.

---

## Features

- **Cross-Platform Drag-and-Drop USB Bootloader (Linux, macOS & Windows):**
  - Emulates a FAT32 USB drive labeled **`DSO BOOT`** over the native USB-C port.
  - Works natively on **Linux (`vfat`)**, **macOS**, and **Windows** — supports both data-first (`DATA -> DIR`) and directory-first (`DIR -> DATA`) write orderings, plus FAT1/FAT2 mirror normalization.
  - Flash firmware by simply copying `.bin` or `.hex` files (or clicking **Upload** in the Arduino IDE) without needing an ST-Link.
  - On-screen visual progress bar and automatic reboot.
  - macOS/Linux hidden metadata file (`._*`, `.Trash-*`) immunity.
  - Sub-millisecond fast-boot for application execution.

- **Arduino IDE & FreeRTOS Suite (`arduino/FNIRSI_DSO152`):**
  - Full C++ Arduino library inheriting from **`Adafruit_GFX`** for rich graphics, custom fonts, and UI widgets.
  - Out-of-the-box compatibility with **`STM32duino FreeRTOS`** (`<STM32FreeRTOS.h>`) — automatically relocates `SCB->VTOR = 0x08004000`, latches battery power (`PC10`), and configures the 72 MHz HSE PLL clock before `HAL_Init()` / scheduler start.
  - One-click **Upload** directly from the Arduino IDE to the **`DSO BOOT`** USB drive (or via ST-Link SWD at `0x08004000`).

- **Lightweight Bare-Metal C SDK (`libdso152`):**
  - **Zero heavy dependencies:** Pure CMSIS/register-level performance (~4 KB footprint).
  - **Power Management:** Soft power latch control (`PC10`), graceful power-button hold detection (`PC11`), and software reboot to USB bootloader.
  - **High-Performance Display Driver:** Full 16-bit 8080-II parallel interface on Port B with hardware-accelerated fills, primitives, text rendering, and rotation.
  - **Responsive Input:** Full debounced polling for all 9 tactile pushbuttons.
  - **Analog Front-End (AFE):** Complete control of input attenuation relay (`PA2`), AC/DC coupling (`PA3`), HC4051 gain multiplexer (`PA4..PA6`), 1 kHz calibration generator (`PA0`), and 12-bit ADC1 sampling (`PA1`).

---

## Directory Structure

```
├── arduino/                 # Arduino IDE & FreeRTOS Support
│   ├── FNIRSI_DSO152/       # Arduino Library (Adafruit_GFX + FreeRTOS ready)
│   │   ├── src/             # FNIRSI_DSO152.h / FNIRSI_DSO152.cpp
│   │   └── examples/        # 01_HelloWorld_GFX, 02_Breakout_Arduino, 03_FreeRTOS_Oscilloscope
│   ├── board_support/       # STM32duino boards.local.txt, platform.local.txt & uploaders
│   ├── FNIRSI_DSO152.zip    # Ready-to-import Arduino .ZIP library
│   └── install_arduino_ide.sh # Automated installer for Arduino IDE
│
├── libdso152/               # Core Bare-Metal C SDK Library
│   ├── include/             # Public SDK headers (dso152.h, dso152_hw.h, etc.)
│   ├── src/                 # Hardware drivers (LCD, AFE, Buttons, Power Latch)
│   ├── common/              # Linker script (dso152_app.ld), startup.c, Makefile.include
│   └── Makefile             # Builds libdso152.a
│
├── dso152_bootloader/       # USB Mass Storage Bootloader (16 KB partition)
│   ├── Src/, Inc/           # USB MSC Class, FAT32 emulation (Linux/macOS/Windows), ST7789 UI
│   ├── flash_bootloader.sh  # SWD flashing script for OpenOCD
│   └── Makefile             # Builds dso152_bootloader.bin
│
├── examples/                # Ready-to-flash bare-metal C example projects
│   ├── 01_hello_world/      # System info, color swatches, graphic primitives & button tester
│   ├── 02_breakout_game/    # Full 60 FPS Breakout arcade game
│   └── 03_mini_oscilloscope/# Real-time oscilloscope with AFE range switching & triggering
│
├── docs/                    # Technical & Reverse Engineering Documentation
│   ├── ARDUINO_IDE.md       # Complete Arduino IDE & FreeRTOS setup and API guide
│   ├── HARDWARE.md          # Schematics, CD4051 truth table, power latch architecture
│   ├── PINOUT.md            # Complete GPIO mapping and SWD pinouts
│   └── BOOTLOADER.md        # Bootloader mechanics and firmware flashing guide
│
├── Makefile                 # Master Makefile (builds SDK and all examples)
└── README.md
```

---

## Quick Start

### Option A: Using the Arduino IDE (with `Adafruit_GFX` & `FreeRTOS`)

1. Clone the repository and run the Arduino IDE installer script:
   ```bash
   git clone https://github.com/k7vinilstorage/FNIRSI-DSO-152-Open-Source-SDK-USB-Bootloader.git
   cd FNIRSI-DSO-152-Open-Source-SDK-USB-Bootloader
   ./arduino/install_arduino_ide.sh
   ```
2. In the Arduino IDE:
   - Open **Tools -> Board -> Boards Manager...** and install **`STM32 MCU based boards`** (the Boards Manager URL is added automatically by the installer script).
   - Open **Tools -> Manage Libraries...** and install **`Adafruit GFX Library`** and **`STM32duino FreeRTOS`**.
   - Re-run `./arduino/install_arduino_ide.sh` once after installing the STM32 core to enable the **FNIRSI DSO-152** board profile and **`DSO BOOT`** one-click upload option.
3. Open **File -> Examples -> FNIRSI DSO-152 -> `03_FreeRTOS_Oscilloscope`** and click **Upload**!
   - Full details in **[docs/ARDUINO_IDE.md](docs/ARDUINO_IDE.md)**.

### Option B: Building Bare-Metal C Projects (`Makefile` + `arm-none-eabi-gcc`)

#### 1. Prerequisites

Install the standard ARM GCC toolchain and Make:

- **Debian / Ubuntu / Raspberry Pi OS:**
  ```bash
  sudo apt-get update
  sudo apt-get install gcc-arm-none-eabi binutils-arm-none-eabi make
  ```
- **Fedora / RHEL / Arch:**
  ```bash
  sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-binutils make  # Fedora
  sudo pacman -S arm-none-eabi-gcc arm-none-eabi-binutils make       # Arch
  ```
- **macOS:**
  ```bash
  brew install --cask gcc-arm-embedded
  ```

#### 2. Building the SDK & Examples

Clone the repository and run `make`:
```bash
git clone https://github.com/k7vinilstorage/FNIRSI-DSO-152-Open-Source-SDK-USB-Bootloader.git
cd FNIRSI-DSO-152-Open-Source-SDK-USB-Bootloader
make
```

This compiles:
- `libdso152/libdso152.a`
- `examples/01_hello_world/build/hello_world.bin`
- `examples/02_breakout_game/build/breakout_game.bin`
- `examples/03_mini_oscilloscope/build/mini_oscilloscope.bin`

---

## Flashing Firmware via USB-C

1. Connect the DSO-152 to your computer using a USB-C cable.
2. Hold down the **`MENU` / `OK`** button and press the **Power** button to power on.
3. The oscilloscope screen will show the **`DSO-152 USB BOOTLOADER`** interface, and a new USB drive named **`DSO BOOT`** will appear on your computer.
4. Copy any compiled `.bin` or `.hex` file into the `DSO BOOT` drive:
   ```bash
   cp examples/02_breakout_game/build/breakout_game.bin /media/$USER/DSO\ BOOT/
   sync
   ```
5. The DSO-152 will display a progress bar and automatically launch your application upon completion!

> [!NOTE]
> If your oscilloscope does not yet have the USB bootloader installed, see the one-time installation instructions in [docs/BOOTLOADER.md](docs/BOOTLOADER.md).

---

## SDK Architecture & API Quick Tour

To build your own custom application, simply include `<dso152.h>`:

```c
#include "dso152.h"

int main(void) {
    // 1. Initialize power latch, 72MHz clock, buttons, LCD, and AFE
    dso152_init();

    // 2. Clear screen and draw
    st7789_fill(COLOR_BLACK);
    st7789_draw_string(20, 20, "Hello DSO-152!", COLOR_YELLOW, COLOR_BLACK, 2);

    while (1) {
        // Read all 9 pushbuttons
        dso152_buttons_t btns = dso152_btn_read();

        // Check power button hold (>800ms) for clean battery shutdown
        dso152_btn_check_power_hold();

        if (btns.ok) {
            st7789_fill_circle(160, 120, 30, COLOR_GREEN);
        }

        delay_ms(10);
    }
}
```

### Application Makefile

Creating a new application takes just 3 lines in your Makefile:

```makefile
TARGET = my_app
SRCS = main.c

include ../../libdso152/common/Makefile.include
```

### Arduino IDE & FreeRTOS Quick Tour

Install the Arduino library and board definitions with `./arduino/install_arduino_ide.sh`, then open **File -> Examples -> FNIRSI DSO-152**:

```cpp
#include <FNIRSI_DSO152.h>
#include <STM32FreeRTOS.h>

void TaskUI(void *pvParameters) {
    for (;;) {
        dso.updatePowerButton(true); // Hold PWR to shut down, Hold OK (2s) for DSO BOOT
        dso.setCursor(20, 60);
        dso.setTextColor(COLOR_CYAN, COLOR_BLACK);
        dso.printf("Probe: %4lu mV\n", (unsigned long)dso.readMillivolts());
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    dso.begin(1); // 320x240 Landscape, Adafruit_GFX ready
    xTaskCreate(TaskUI, "UI", 256, NULL, 2, NULL);
    vTaskStartScheduler();
}

void loop() {}
```

See [docs/ARDUINO_IDE.md](docs/ARDUINO_IDE.md) for the complete Arduino IDE, `Adafruit_GFX`, and `STM32duino FreeRTOS` setup guide.

---

## Hardware Specifications

| Component | Specification |
|---|---|
| **Microcontroller** | Artery AT32F403ARCT7 / WCH CH32F103 / ST STM32F103 (ARM Cortex-M3/M4 @ 72 MHz) |
| **Flash Memory** | 64 KB – 256 KB (16 KB Bootloader + User App Space at `0x08004000`) |
| **SRAM** | 20 KB – 48 KB |
| **Display** | 2.8" IPS TFT, 320x240 RGB565, ST7789V Controller |
| **Display Bus** | 16-bit 8080-II Parallel (`PB0`..`PB15`) |
| **Buttons** | 9 tactile switches (Run, Auto, Mode, Menu, Up, Down, Left, Right, Power) |
| **Analog Input** | 12-bit ADC (up to 1 MSPS), PA1 |
| **Analog Front-End** | Attenuation Relay (`PA2`), AC/DC Coupling (`PA3`), CD4051 MUX (`PA4..PA6`) |
| **Calibration Out** | 1 kHz square wave reference (`PA0`) |
| **Battery & Power** | Internal 3.7V Li-Ion with software power latch (`PC10`) |

---

## Documentation

- [Arduino IDE & FreeRTOS Guide](docs/ARDUINO_IDE.md) - Full guide to using the Arduino IDE, `Adafruit_GFX`, `STM32duino FreeRTOS`, and one-click `DSO BOOT` uploads.
- [Hardware & Circuit Architecture](docs/HARDWARE.md) - Deep dive into power latching, AFE gain tables, and reverse-engineered schematics.
- [Pinout & GPIO Allocation](docs/PINOUT.md) - Complete table of pins, MCU functions, and SWD debug pads.
- [USB Bootloader Guide](docs/BOOTLOADER.md) - Virtual FAT32 mechanics, flashing instructions, and SWD recovery.

---

## Contributing

Contributions, bug reports, and additional examples are warmly welcomed! Please feel free to submit a Pull Request or open an Issue.

---

## License

This project is licensed under the [MIT License](LICENSE).
