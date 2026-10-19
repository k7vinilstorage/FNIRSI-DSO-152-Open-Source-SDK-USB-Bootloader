# FNIRSI DSO-152 Hardware Architecture & Reverse Engineering Reference

The **FNIRSI DSO-152** is a portable, battery-powered handheld digital storage oscilloscope. This document details its electrical architecture, reverse-engineered circuits, register configurations, and component mappings.

---

## 1. System Overview

```
                      +---------------------------------------+
                      |       3.7V Li-Ion Battery             |
                      +---------------------------------------+
                                          |
                                          v
                                +-------------------+
[Power Key (PC11)] ------------>| Latch Circuit     |
                                | (PC10: PWR_LATCH) |
                                +-------------------+
                                          | (3.3V System Rails)
                                          v
+------------------+     16-bit Data     +----------------------------------+
|                  |<===================>|  PB0..PB15 (8080 Parallel Bus)   |
|  ST7789 2.8" IPS |                     |                                  |
|  320x240 LCD     |<--------------------|  PA8 (RST), PA15 (CS)            |
|                  |<--------------------|  PC13 (RS), PC14 (WR), PC15 (RD) |
|                  |<--------------------|  PC9 (Backlight Active-Low)      |
+------------------+                     |                                  |
                                         |         CH32F103C8T6 /           |
+------------------+   Active-Low Pullup |         STM32F103C8T6            |
|  9 Pushbuttons   |-------------------->|   PA9, PA10, PC3..PC8, PC11      |
+------------------+                     |          (72 MHz Cortex-M3)      |
                                         |                                  |
+------------------+                     |                                  |
|   BNC / Clip     |                     |                                  |
|   Signal In      |                     |                                  |
|        |         |                     |                                  |
|        v         |                     |                                  |
|   [AC/DC Switch] |<--------------------|  PA3 (AFE_ACDC)                  |
|   [Atten Relay]  |<--------------------|  PA2 (AFE_RELAY)                 |
|   [CD4051 Mux]   |<--------------------|  PA4 (A0), PA5 (A1), PA6 (A2)   |
|   [OpAmp Stages] |                     |                                  |
|        |         |                     |                                  |
|        +-------->|      ADC1_IN1       |  PA1 (Analog Input)              |
+------------------+                     |                                  |
                                         |  PA0 (1kHz Calibrator Out)       |
                                         |  PA11/PA12 (USB Full Speed)      |
                                         +----------------------------------+
```

---

## 2. Microcontroller Unit (MCU)

- **Chip:** WCH CH32F103C8T6 (100% pin-compatible with STMicroelectronics STM32F103C8T6).
- **Core:** ARM Cortex-M3 32-bit RISC.
- **Clock:** 72 MHz (8 MHz external crystal + internal PLL x9).
- **Flash Memory:** 64 KB (organized into 1 KB pages).
- **SRAM:** 20 KB (`0x20000000` to `0x20004FFF`).
- **Debugging:** Serial Wire Debug (SWD) via `PA13` (SWDIO) and `PA14` (SWCLK).

### Flash Memory Layout

| Address Range | Size | Partition | Description |
|---|---|---|---|
| `0x08000000` - `0x08003FFF` | 16 KB | **USB Bootloader** | Emulates FAT32 `DSO BOOT` mass storage disk over USB-C. |
| `0x08004000` - `0x0800FFFF` | 48 KB | **User Application** | Custom firmware, retro games, oscilloscope apps. |

---

## 3. Power Latch Circuitry

The DSO-152 uses a soft-latch circuit powered by the internal Li-Ion battery:
1. **Turn ON:** When the physical power button is pressed, the power regulator turns on briefly. Firmware must immediately assert **PC10 HIGH (`PWR_LATCH = 1`)** within milliseconds to hold the system power rail active.
2. **Turn OFF:** To safely shut down, firmware drops **PC10 LOW (`PWR_LATCH = 0`)**. This instantly cuts power to the microcontroller and peripherals.
3. **Power Button Detection:** The power button is wired to **PC11** (active-low with internal pull-up). Firmware samples PC11; if held for $>800$ ms, it executes a clean shutdown sequence.
4. **Display Backlight:** Controlled via **PC9** (active-low push-pull). Setting `PC9 = 0` turns the backlight ON; `PC9 = 1` turns it OFF.

---

## 4. Display Subsystem (ST7789)

- **Controller:** Sitronix ST7789V.
- **Resolution:** 320 x 240 pixels (Landscape) or 240 x 320 pixels (Portrait).
- **Color Depth:** 16-bit RGB565 (65,536 colors).
- **Bus Interface:** 16-bit 8080-II parallel interface.
  - **Data Lines:** `PB0`..`PB15` mapped directly to GPIOB ODR.
  - **Control Lines:**
    - `PA8`: Hardware Reset (`RST`, active-low)
    - `PA15`: Chip Select (`CS`, active-low)
    - `PC13`: Register Select (`RS` / `DC`, `0` = Command, `1` = Data)
    - `PC14`: Write Strobe (`WR`, active-low, data latched on rising edge)
    - `PC15`: Read Strobe (`RD`, active-low)

---

## 5. Analog Front-End (AFE)

The analog front-end conditions the input signal before feeding it into the MCU's internal 12-bit ADC (`ADC1_IN1` on `PA1`).

### Control Lines

| Pin | Name | Logic '0' | Logic '1' |
|---|---|---|---|
| `PA0` | `CAL_OUT` | 0V | 3.3V (1 kHz square wave reference) |
| `PA1` | `ADC_IN` | Analog Input | Voltage signal to ADC1 Channel 1 |
| `PA2` | `AFE_RELAY` | Low Voltage Range (10mV..200mV) | High Voltage Range (500mV..20V) |
| `PA3` | `AFE_ACDC` | DC Coupling | AC Coupling |
| `PA4` | `AFE_A0` | CD4051 MUX Bit 0 | CD4051 MUX Bit 0 |
| `PA5` | `AFE_A1` | CD4051 MUX Bit 1 | CD4051 MUX Bit 1 |
| `PA6` | `AFE_A2` | CD4051 MUX Bit 2 | CD4051 MUX Bit 2 |

### Sensitivity Range Truth Table

Extracted directly from stock firmware disassembly at address `0x0800150c`:

| Range Index | Sensitivity | Attenuation Relay (`PA2`) | MUX A0 (`PA4`) | MUX A1 (`PA5`) | MUX A2 (`PA6`) |
|---|---|:---:|:---:|:---:|:---:|
| `0` | 10 mV / div | 0 | 0 | 0 | 0 |
| `1` | 20 mV / div | 0 | 1 | 0 | 0 |
| `2` | 50 mV / div | 0 | 0 | 1 | 0 |
| `3` | 100 mV / div | 0 | 1 | 1 | 0 |
| `4` | 200 mV / div | 0 | 0 | 0 | 1 |
| `5` | 500 mV / div | 1 | 0 | 0 | 0 |
| `6` | 1 V / div | 1 | 1 | 0 | 0 |
| `7` | 2 V / div | 1 | 0 | 1 | 0 |
| `8` | 5 V / div | 1 | 1 | 1 | 0 |
| `9` | 10 V / div | 1 | 0 | 0 | 1 |
| `10` | 20 V / div | 1 | 1 | 0 | 1 |

---

## 6. USB Interface

- `PA11`: USB D- (Full Speed 12 Mbps)
- `PA12`: USB D+ (Full Speed 12 Mbps)
- Powered via USB-C connector on top edge.
- Supports Mass Storage Class (MSC) with BOT (Bulk-Only Transport) protocol.
