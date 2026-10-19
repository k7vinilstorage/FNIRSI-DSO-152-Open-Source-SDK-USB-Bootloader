# Arduino IDE & FreeRTOS Guide for FNIRSI DSO-152

The **FNIRSI DSO-152 Open SDK** includes a full **Arduino IDE library (`FNIRSI_DSO152`)** and **STM32duino board/upload definitions**. This allows you to program the DSO-152 using the standard Arduino ecosystem, including:

- **`Adafruit_GFX`** (custom fonts, shapes, bitmaps, canvases, and widgets on the 320x240 ST7789 display)
- **`STM32duino FreeRTOS`** (`<STM32FreeRTOS.h>` preemptive multi-tasking, queues, mutexes, software timers)
- **Arduino Libraries** (`arduinoFFT`, `ArduinoJson`, math/DSP libraries, etc.)
- **One-Click USB Upload (`DSO BOOT`)** directly from the **Upload** button in the Arduino IDE

---

## 1. Quick Installation

Run the automated installer script from the repository root:

```bash
./arduino/install_arduino_ide.sh
```

This script:
1. Installs the **`FNIRSI_DSO152`** library into `~/Arduino/libraries/FNIRSI_DSO152`.
2. Generates `arduino/FNIRSI_DSO152.zip` (in case you prefer **Sketch -> Include Library -> Add .ZIP Library...**).
3. Adds the STM32duino Boards Manager URL to `~/.arduinoIDE/arduino-cli.yaml`.
4. Installs the custom **FNIRSI DSO-152** board profile and **DSO BOOT** USB uploader into your STM32duino hardware folder (`~/.arduino15/packages/STMicroelectronics/hardware/stm32/<version>/`).

---

## 2. Setting Up Arduino IDE (2.x or 1.8.x)

### Step 1: Install the STM32 Board Core (`STM32duino`)
1. Open **File -> Preferences** in the Arduino IDE.
2. Add the following URL to **Additional Boards Manager URLs**:
   ```text
   https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
   ```
3. Open **Tools -> Board -> Boards Manager...**, search for **`STM32 MCU based boards`** (by STMicroelectronics), and click **Install**.
4. Once installed, run `./arduino/install_arduino_ide.sh` once more so the **FNIRSI DSO-152** board and **DSO BOOT** upload options are added to the newly installed core.

### Step 2: Install Recommended Libraries (Library Manager)
Open **Tools -> Manage Libraries...** and install:
- **`Adafruit GFX Library`** by *Adafruit* (for full graphics primitives and custom fonts)
- **`STM32duino FreeRTOS`** by *Richard Barry / STMicroelectronics* (for multi-tasking with `<STM32FreeRTOS.h>`)

### Step 3: Select the Board & Upload Settings
In the Arduino IDE **Tools** menu, select:

| Setting | Recommended Value |
| :--- | :--- |
| **Board** | `STM32 MCU based boards` -> `Generic STM32F1 series` |
| **Board part number** | `FNIRSI DSO-152 (AT32F403A / STM32F103RC)` *(or `Generic F103RCTx`)* |
| **Upload method** | `FNIRSI DSO-152 USB Drive (DSO BOOT @ 0x08004000)` *(or `FNIRSI DSO-152 ST-Link (pyOCD @ 0x08004000)`)* |
| **Optimize** | `Fast (-O2)` or `Fastest (-O3)` |

---

## 3. Uploading Sketches to the DSO-152

### Method A: USB Drag-and-Drop / One-Click Upload (`DSO BOOT`)
1. Connect the DSO-152 to your computer via USB-C.
2. Enter USB Bootloader mode:
   - **From Power-Off:** Hold the **OK / MENU** button while turning on the power switch.
   - **From a Running Sketch:** Hold **MENU + AUTO** together (supported in all example sketches), or call `dso.rebootToBootloader();` in your code.
3. The **`DSO BOOT`** USB Mass Storage drive will appear on your computer.
4. Click **Upload** (`->`) in the Arduino IDE!
   - Alternatively, use **Sketch -> Export Compiled Binary** and drag the generated `.bin` file onto the **`DSO BOOT`** drive.

### Method B: ST-Link SWD Upload (Preserving the Bootloader)
Select **Tools -> Upload method: "FNIRSI DSO-152 ST-Link (pyOCD @ 0x08004000)"** and click **Upload**. Your sketch is flashed directly to `0x08004000` while keeping the USB bootloader at `0x08000000` intact.

---

## 4. How Bootloader Compatibility (`0x08004000`) & FreeRTOS Work Under the Hood

The DSO-152 USB bootloader occupies the first 16 KB of Flash (`0x08000000` – `0x08003FFF`), and user applications start at **`0x08004000`**.

To make Arduino sketches and **FreeRTOS** (`SVC_Handler`, `PendSV_Handler`, `SysTick_Handler`) work reliably without manual setup:
1. **Early `init()` Override:** `FNIRSI_DSO152.cpp` overrides STM32duino's weak `init()` hook to dynamically detect when the sketch is running at `0x08004000`, set `SCB->VTOR = 0x08004000` before `HAL_Init()` enables SysTick interrupts, and immediately latch `PC10 = HIGH` so the battery regulator stays powered.
2. **Automatic 72 MHz HSE Clock:** `FNIRSI_DSO152.cpp` overrides STM32duino's weak `SystemClock_Config()` so that the MCU always runs at full **72 MHz** from the onboard 8 MHz HSE crystal (with 48 MHz USB and 12 MHz ADC clocks), regardless of which STM32F1 board variant is selected.
3. **`build_opt.h` Included in Examples:** Every example includes a `build_opt.h` with `-DVECT_TAB_OFFSET=0x4000` so `SystemInit()` also preserves the vector table offset.

---

## 5. `FNIRSI_DSO152` Library API Reference

Include the library at the top of your sketch:

```cpp
#include <FNIRSI_DSO152.h>
```

A global instance named **`dso`** is provided automatically.

### Lifecycle & Power Management
| Method | Description |
| :--- | :--- |
| `dso.begin(rotation = 1)` | Initializes battery latch (`PC10`), backlight (`PC9`), ST7789 320x240 display (`1` = Landscape, `0` = Portrait), 9 buttons, 1 kHz cal pin (`PA0`), and AFE/ADC (`PA1`). |
| `dso.updatePowerButton(allow_bootloader_shortcut = true)` | Checks if the Power button (`PC11`) is held (>800ms) to shut down cleanly, or if `OK` (`PC4`) is held (>2s) to enter `DSO BOOT` USB mode. |
| `dso.powerOff()` | Releases the battery latch (`PC10 = LOW`) to shut down the device. |
| `dso.rebootToBootloader()` | Writes magic key `0x12345678` to RAM (`0x20004FFC`) and resets into the `DSO BOOT` USB bootloader. |
| `dso.setBacklight(bool on)` | Turns the LCD backlight (`PC9`, active-low) ON or OFF. |

### Display & `Adafruit_GFX` Methods
Because `FNIRSI_DSO152` inherits from **`Adafruit_GFX`**, all standard `Adafruit_GFX` methods are available and hardware-accelerated via the 16-bit parallel `GPIOB->ODR` bus:
- `dso.fillScreen(color)` / `dso.fill(color)`
- `dso.drawPixel(x, y, color)`
- `dso.drawLine(x0, y0, x1, y1, color)`
- `dso.drawFastHLine(x, y, w, color)` / `dso.drawFastVLine(x, y, h, color)`
- `dso.drawRect(x, y, w, h, color)` / `dso.fillRect(x, y, w, h, color)`
- `dso.drawCircle(x, y, r, color)` / `dso.fillCircle(x, y, r, color)`
- `dso.drawRoundRect(...)` / `dso.fillRoundRect(...)` / `dso.drawTriangle(...)`
- `dso.setCursor(x, y)` / `dso.setTextColor(fg, bg)` / `dso.setTextSize(size)` / `dso.setFont(&FreeSans9pt7b)`
- `dso.print(...)` / `dso.println(...)` / `dso.printf(...)`
- `dso.drawString(x, y, str, fg, bg, size)` *(built-in ultra-fast 8x8 windowed font renderer)*

### Buttons Input (`DSO152_Buttons`)
| Method / Field | Description |
| :--- | :--- |
| `DSO152_Buttons b = dso.readButtons()` | Reads all 9 buttons into a struct (`b.up`, `b.down`, `b.left`, `b.right`, `b.ok`, `b.run`, `b.auto_btn`, `b.mode`, `b.pwr`). |
| `b.anyPressed()` | Returns `true` if any front-panel action button is pressed. |

### Oscilloscope Analog Front-End (AFE) & ADC
| Method | Description |
| :--- | :--- |
| `dso.setRange(range)` | Selects vertical sensitivity (`AFE_RANGE_10mV` .. `AFE_RANGE_20V`) via HC4051 (`PA4`–`PA6`) and input relay (`PA2`). |
| `dso.getRangeName(range)` | Returns human-readable string (`"10mV"`, `"100mV"`, `"1V"`, etc.). |
| `dso.setCoupling(bool ac_coupling)` | Selects AC (`true`) or DC (`false`) coupling on `PA3`. |
| `dso.readADC()` | Performs a single 12-bit conversion (`0`..`4095`) on `ADC1_IN1` (`PA1`). |
| `dso.readMillivolts()` | Returns the ADC reading in millivolts (`0` to `3300` mV). |
| `dso.captureWaveform(buf, len, delay_loops, trig_level)` | Captures `len` triggered 12-bit samples from `ADC1_IN1` into `buf`. |
| `dso.setCalPin(high)` / `dso.toggleCalPin()` | Controls the 1 kHz probe calibration tab on `PA0`. |

---

## 6. Included Arduino Examples

Open via **File -> Examples -> FNIRSI DSO-152**:
1. **`01_HelloWorld_GFX`**: Interactive `Adafruit_GFX` dashboard showing live button indicators, analog waveform bar, voltage range/coupling controls, and one-key reboot into USB bootloader mode.
2. **`02_Breakout_Arduino`**: Full 60 FPS Breakout arcade game in an Arduino sketch (`LEFT`/`RIGHT` to move paddle, `OK` to launch ball, `RUN` to pause).
3. **`03_FreeRTOS_Oscilloscope`**: Preemptive multi-tasking oscilloscope built with `<STM32FreeRTOS.h>` running 3 concurrent RTOS tasks (`TaskCalGen`, `TaskButtons`, `TaskScopeDisplay`) synchronized with a FreeRTOS mutex.
