# Stock Firmware & Reverse Engineering Artifacts

This directory contains the original factory firmware dump from the FNIRSI DSO-152 and reverse engineering analysis evidence.

## Files

- **`DSO152_APP_V0.1.bin`**: Factory stock application binary (base address `0x08004000`, 39,532 bytes).
- **`dso152_stock_firmware_evidence.json`**: Static analysis and disassembly evidence extracted using REA (Reverse Engineer Anything), including vector tables, AFE MUX GPIO registers, and hardware access patterns.

## Restoring Stock Firmware

### Via USB Mass Storage Bootloader
1. Power on the device while holding **`OK / MENU`** to enter the bootloader (`DSO BOOT`).
2. Copy `DSO152_APP_V0.1.bin` into the drive.

### Via SWD Programmer
```bash
./flash_app_swd.sh stock_firmware/DSO152_APP_V0.1.bin
```
