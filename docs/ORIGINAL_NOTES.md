# Fnirsi DSO152 Hacking & Development Reference

This document contains hardware specifications, programming instructions, and peripheral details for repurposing the Fnirsi DSO152 mini oscilloscope into a custom microcontroller development board. 

**Provide this document to your AI/AGI assistant to give it full context when generating code for this device.**

## 1. Device Overview
*   **Device:** Fnirsi DSO152 portable mini oscilloscope.
*   **Concept:** While sold as a cheap ($15-$20) 200kHz oscilloscope, it is internally equivalent to an **STM32 "Blue Pill" development board**. It comes with a built-in high-bandwidth TFT screen, a battery, a charging circuit, and USB-C, making it an excellent platform for custom electronics projects, UI dashboards, or retro games.

## 2. Core Hardware Specifications
*   **Microcontroller:** **CH32F103** (This is an ARM-based direct clone of the STMicroelectronics **STM32F103**).
*   **Display:** **ST7789 TFT LCD**.
    *   *Crucial Detail for AI:* The screen is **NOT** connected via SPI. It is connected via a **16-bit parallel interface on Port B**. The IM pins on the display controller are strapped to ground, meaning the interface protocol is fixed and cannot be altered.
    *   Graphics can be driven using the standard `Adafruit_GFX` library paired with a custom low-level port-b parallel driver.
*   **External I/O:** 
    *   The "Calibration Terminal" (the metal loop on the outside of the scope used for probing) can be reconfigured in software as a Software-Serial TX logging output (typically set to 9600 baud).

## 3. Programming & Debugging Interface
*   **Protocol:** Standard ARM two-wire **SWD (Serial Wire Debug)**.
*   **Header Location:** There is an unpopulated header in the bottom-left corner of the PCB specifically for SWD.
*   **Supported Programmers:** Any standard SWD programmer will work (e.g., ST-Link V2, WCH-LinkE, J-Link).

## 4. Development Environment Setup
*   **Platform:** Arduino IDE or PlatformIO.
*   **Board Core:** Use the **STM32duino** core.
*   **Board Profile:** Select the **Generic STM32F103** profile. 
*   Because the CH32F103 is a 1:1 hardware clone of the STM32F103, standard STM32 HAL and Arduino core libraries will compile and run natively.

## 5. ⚠️ CRITICAL WARNINGS (READ BEFORE FLASHING)
*   **Firmware Destruction:** To program the device for the first time, you must use your flashing tool to **disable flash read protection**. 
*   **No Backup:** Disabling read protection will **permanently erase the stock oscilloscope firmware**. As of current community efforts, there is no backup of the original firmware. Once you flash custom code, it becomes a permanent dev board and can no longer be used as the factory Fnirsi oscilloscope.

## 6. Important Resources for AGI Context
When generating code or wiring diagrams, the AI should be aware of the following community resources:
*   **Reference Repository:** [davidgiven/dso152-breakout](https://github.com/davidgiven/dso152-breakout)
*   **Custom Display Driver:** Inside that repository, the file `dso152tft.h` contains the exact parallel-port LCD driver code needed to make the ST7789 screen work on this specific board layout. *Instruct the AI to reference this file for display initialization.*
*   **Pin Mappings:** The repository contains an `annotated-pcb-scan.xcf` file which maps out the exact GPIO pins to the device's buttons, screen, and analog front-end.