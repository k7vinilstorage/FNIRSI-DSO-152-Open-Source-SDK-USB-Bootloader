# FNIRSI DSO-152 USB Mass Storage Bootloader

The **DSO-152 USB Bootloader** provides native drag-and-drop firmware flashing over the device's USB-C port without requiring any hardware programmers (like ST-Link or J-Link).

When activated, the oscilloscope enumerates as a standard USB Flash Drive with volume label **`DSO BOOT`**. Dropping a `.bin` or `.hex` firmware file onto the drive automatically flashes the user application partition at `0x08004000`.

---

## 1. Bootloader Architecture

- **Flash Partition:** `0x08000000` - `0x08003FFF` (16 KB allocated; binary size ~14.5 KB).
- **Application Partition:** `0x08004000` - `0x0800FFFF` (48 KB available).
- **USB Class:** USB Mass Storage Class (MSC) Bulk-Only Transport (BOT).
- **Filesystem Emulation:** Virtual FAT32 disk with dynamically generated FAT and directory entries.
- **Visual Feedback:** Built-in ST7789 display driver shows real-time connection status, filename being written, and flashing progress bar.

---

## 2. Boot Modes & Triggering

```
               [ Power On / Reset ]
                         |
                         v
            +--------------------------+
            | Check Magic Key at       |
            | RAM 0x20004FFC ==        |--- Yes ---> [ Stay in Bootloader ]
            |       0x12345678         |
            +--------------------------+
                         | No
                         v
            +--------------------------+
            | Is MENU / OK Button      |
            |     (PC4) Held?          |--- Yes ---> [ Stay in Bootloader ]
            +--------------------------+
                         | No
                         v
            +--------------------------+
            | Is SP at 0x08004000      |
            |      Valid RAM?          |--- No ----> [ Stay in Bootloader ]
            +--------------------------+
                         | Yes
                         v
            [ Jump to Application ]
              (< 1 millisecond boot)
```

### Entering Bootloader Mode

1. **Hardware Button:** Hold the **`MENU` / `OK` button (PC4)** on the front panel and turn on the oscilloscope.
2. **From Application Code:** Call `dso152_reboot_to_bootloader()` from `dso152_hw.h`:
   ```c
   // Sets magic flag 0x12345678 at 0x20004FFC and resets MCU
   dso152_reboot_to_bootloader();
   ```

---

## 3. How to Flash Firmware via USB (Drag-and-Drop)

### Step 1: Put DSO-152 into Bootloader Mode
1. Connect the DSO-152 to your PC using a USB-C data cable.
2. Hold down the **`OK / MENU`** button and press the **Power** button.
3. The screen will illuminate in dark blue displaying:
   ```
   ==============================
     DSO-152 USB BOOTLOADER
     Volume: DSO BOOT
     Ready for firmware (.bin/.hex)
   ==============================
   ```

### Step 2: Copy Firmware File
1. Your operating system will detect a new drive named **`DSO BOOT`**.
2. Build your application using `make`:
   ```bash
   make -C examples/02_breakout_game
   ```
3. Copy the generated `.bin` or `.hex` file to the drive:
   - **Linux / macOS:**
     ```bash
     cp examples/02_breakout_game/build/breakout_game.bin /media/$USER/DSO\ BOOT/
     sync
     ```
   - **Windows:** Simply drag and drop `breakout_game.bin` into the `DSO BOOT` drive in File Explorer.

### Step 3: Automatic Programming & Reboot
- While writing, the DSO-152 screen displays:
  ```
  Flashing: BREAKOUT.BIN
  [====================] 100%
  Flash Complete! Rebooting...
  ```
- Once the write completes, the device automatically launches your application!

---

## 4. Flashing the Bootloader via SWD (One-Time Setup)

If your device currently has blank flash or stock firmware and needs the bootloader installed for the first time:

### Hardware Connections (SWD)

Connect your ST-Link V2 / PyOCD programmer to the DSO-152 SWD pads:
- **SWDIO** $\rightarrow$ `PA13`
- **SWCLK** $\rightarrow$ `PA14`
- **GND**   $\rightarrow$ `GND`
- **3.3V**  $\rightarrow$ `3.3V`

### Flashing with OpenOCD
Run the provided helper script:
```bash
cd dso152_bootloader
./flash_bootloader.sh
```

Or execute directly with OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "init" \
  -c "reset halt" \
  -c "flash write_image erase dso152_bootloader.bin 0x08000000" \
  -c "verify_image dso152_bootloader.bin 0x08000000" \
  -c "reset run" \
  -c "exit"
```

### Flashing with PyOCD
```bash
pyocd flash -t stm32f103c8 --base-address 0x08000000 dso152_bootloader.bin
```

---

## 5. Cross-Platform Linux, macOS & Windows FAT32 Engine

Many stock virtual-FAT USB bootloaders only work under Windows because Windows writes the **FAT32 Root Directory entry first** (telling the bootloader the file's starting cluster and byte size) before sending the file's data sectors.

By contrast, the **Linux kernel `vfat` driver** and **macOS Finder** frequently:
1. Write the **file data sectors first** (into clusters `>= 3`) while holding the directory entry in the OS page cache.
2. Read and compare **both `FAT1` and `FAT2` tables** before mounting or committing data.
3. Create **zero-byte placeholder entries** (`open(O_CREAT)`) before writing data, and flush the final directory entry with the true `file_size` and `first_cluster` only on `close()` / `sync`.
4. Write hidden metadata files (`._*`, `.Trash-1000`, `.xdg-volume-info`).

The DSO-152 open-source bootloader (`dso152_bootloader/Src/fat32.c`) solves all of these at the block layer:
- **Order-Independent Flashing (`DIR -> DATA` and `DATA -> DIR`):**
  - If the host OS writes **data sectors before the directory entry** (Linux default), the bootloader inspects the first words of the incoming stream for a valid ARM Cortex-M vector table (`SP` in `0x20000000..0x20010000`, `Reset_Handler` in `0x08004000..0x08040000`) or Intel HEX header (`:`), erases pages on-demand, and streams the sectors directly into Flash starting at `0x08004000`. When the directory entry arrives later (or after a short idle timeout), the transfer is finalized and the device reboots.
  - If the host OS writes the **directory entry first** (Windows default), the bootloader records the target cluster range and size, flashes the incoming sectors as they arrive, and reboots as soon as all bytes are received.
- **FAT1 / FAT2 Mirror Normalization:** Reads to both FAT1 (`0x00004000`) and FAT2 (`0x00403000`) return identical valid cluster chains so Linux `vfat` never remounts the volume read-only.
- **Hidden/Metadata File Immunity:** Directory entries beginning with `.` (`0x2E`), `._`, or `0xE5` (deleted) are ignored so macOS and Linux desktop indexers never corrupt Flash.

---

## 6. One-Click Upload from Arduino IDE

In addition to drag-and-drop via your file manager or `cp`, the **`DSO BOOT`** USB bootloader integrates directly with the **Arduino IDE Upload button**:
- Select **Tools -> Upload method: "FNIRSI DSO-152 USB Drive (DSO BOOT @ 0x08004000)"**
- See [docs/ARDUINO_IDE.md](ARDUINO_IDE.md) for full details.
