# FNIRSI DSO-152 Open-Source SDK & USB Bootloader

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![MCU](https://img.shields.io/badge/MCU-CH32F103%20%2F%20STM32F103-orange.svg)](#hardware-specifications)
[![Clock](https://img.shields.io/badge/Clock-72%20MHz-green.svg)](#hardware-specifications)
[![Display](https://img.shields.io/badge/Display-ST7789%20320x240%20IPS-yellow.svg)](#hardware-specifications)

A lightweight, bare-metal C Software Development Kit (**`libdso152`**) and native **USB Mass Storage Bootloader** for the **FNIRSI DSO-152** handheld digital oscilloscope.

Turn your DSO-152 into an open platform for custom firmware, retro arcade games, signal generators, educational tools, and embedded development.

---

## Features

- **Drag-and-Drop USB Bootloader:**
  - Emulates a FAT32 USB drive labeled **`DSO BOOT`** over the native USB-C port.
  - Flash firmware by simply copying `.bin` or `.hex` files without needing an ST-Link.
  - On-screen visual progress bar and automatic reboot.
  - macOS/Linux hidden file (`._*`) immunity.
  - Sub-millisecond fast-boot for application execution.

- **Lightweight Bare-Metal SDK (`libdso152`):**
  - **Zero heavy dependencies:** No bulky Arduino cores or bloated HALs. Pure CMSIS/register-level performance.
  - **Power Management:** Soft power latch control (`PC10`), graceful power-button hold detection (`PC11`), and reboot to bootloader.
  - **High-Performance Display Driver:** Full 16-bit 8080-II parallel interface on Port B with hardware-accelerated fills, primitives, text rendering, and rotation.
  - **Responsive Input:** Full debounced polling for all 9 tactile pushbuttons.
  - **Analog Front-End (AFE):** Complete control of input attenuation relay (`PA2`), AC/DC coupling (`PA3`), CD4051 gain multiplexer (`PA4..PA6`), 1 kHz calibration generator (`PA0`), and ADC1 DMA sampling (`PA1`).

---

## Directory Structure

```
├── libdso152/               # Core SDK Library
│   ├── include/             # Public SDK headers (dso152.h, dso152_hw.h, etc.)
│   ├── src/                 # Hardware drivers (LCD, AFE, Buttons, Power Latch)
│   ├── common/              # Linker script (dso152_app.ld), startup.c, Makefile.include
│   └── Makefile             # Builds libdso152.a
│
├── dso152_bootloader/       # USB Mass Storage Bootloader (16 KB partition)
│   ├── Src/, Inc/           # USB MSC Class, FAT32 emulation, ST7789 UI
│   ├── flash_bootloader.sh  # SWD flashing script for OpenOCD
│   └── Makefile             # Builds dso152_bootloader.bin
│
├── examples/                # Ready-to-flash example projects
│   ├── 01_hello_world/      # System info, color swatches, graphic primitives & button tester
│   ├── 02_breakout_game/    # Full 60 FPS Breakout arcade game
│   └── 03_mini_oscilloscope/# Real-time oscilloscope with AFE range switching & triggering
│
├── docs/                    # Technical & Reverse Engineering Documentation
│   ├── HARDWARE.md          # Schematics, CD4051 truth table, power latch architecture
│   ├── PINOUT.md            # Complete GPIO mapping and SWD pinouts
│   └── BOOTLOADER.md        # Bootloader mechanics and firmware flashing guide
│
├── Makefile                 # Master Makefile (builds SDK and all examples)
└── README.md
```

---

## Quick Start

### 1. Prerequisites

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

### 2. Building the SDK & Examples

Clone the repository and run `make`:
```bash
git clone https://github.com/your-username/fnirsi-dso152-sdk.git
cd fnirsi-dso152-sdk
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

---

## Hardware Specifications

| Component | Specification |
|---|---|
| **Microcontroller** | WCH CH32F103C8T6 / ST STM32F103C8T6 (ARM Cortex-M3 @ 72 MHz) |
| **Flash Memory** | 64 KB (16 KB Bootloader + 48 KB User App Space) |
| **SRAM** | 20 KB |
| **Display** | 2.8" IPS TFT, 320x240 RGB565, ST7789V Controller |
| **Display Bus** | 16-bit 8080-II Parallel (`PB0`..`PB15`) |
| **Buttons** | 9 tactile switches (Run, Auto, Mode, Menu, Up, Down, Left, Right, Power) |
| **Analog Input** | 12-bit ADC (up to 1 MSPS), PA1 |
| **Analog Front-End** | Attenuation Relay (`PA2`), AC/DC Coupling (`PA3`), CD4051 MUX (`PA4..PA6`) |
| **Calibration Out** | 1 kHz square wave reference (`PA0`) |
| **Battery & Power** | Internal 3.7V Li-Ion with software power latch (`PC10`) |

---

## Documentation

- [Hardware & Circuit Architecture](docs/HARDWARE.md) - Deep dive into power latching, AFE gain tables, and reverse-engineered schematics.
- [Pinout & GPIO Allocation](docs/PINOUT.md) - Complete table of pins, MCU functions, and SWD debug pads.
- [USB Bootloader Guide](docs/BOOTLOADER.md) - Virtual FAT32 mechanics, flashing instructions, and SWD recovery.

---

## Contributing

Contributions, bug reports, and additional examples are warmly welcomed! Please feel free to submit a Pull Request or open an Issue.

---

## License

This project is licensed under the [MIT License](LICENSE).
