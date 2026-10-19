# ESP32-C3 SuperMini Wi-Fi Mod Guide (4-Wire SWD Header Method)

Now that the **`DSO BOOT` USB Bootloader** is installed on your FNIRSI DSO-152, the 4 SWD pads on the PCB (`VCC`, `GND`, `DIO/PA13`, `CLK/PA14`) are no longer needed for daily flashing. You can connect a tiny **ESP32-C3 SuperMini** directly to those 4 pads to give your oscilloscope **2.4 GHz Wi-Fi & BLE**!

---

## 1. 4-Wire Hardware Wiring Diagram

```text
  FNIRSI DSO-152 PCB (SWD Pads)              ESP32-C3 SuperMini
 +-----------------------------+            +-------------------------+
 |                             |            |                         |
 |   [ VCC ] (Switched 3.3V) --+----------->| 3V3                     |
 |                             |            |                         |
 |   [ GND ] ------------------+----------->| G (GND)                 |
 |                             |            |                         |
 |   [ DIO ] (PA13 / DSO TX) --+----------->| RX (GPIO20 / UART1_RX)  |
 |                             |            |                         |
 |   [ CLK ] (PA14 / DSO RX) <-+------------+ TX (GPIO21 / UART1_TX)  |
 |                             |            |                         |
 +-----------------------------+            +-------------------------+
```

| DSO-152 SWD Pad | MCU Pin | Direction | ESP32-C3 SuperMini Pin | Notes |
| :--- | :--- | :---: | :--- | :--- |
| **`VCC`** | Switched `3.3V` | $\rightarrow$ | **`3V3`** | Controlled by DSO-152 `PC10` power latch! Turns OFF automatically when DSO-152 shuts down. |
| **`GND`** | `GND` | $\leftrightarrow$ | **`G` (`GND`)** | Common ground. |
| **`DIO`** | `PA13` (`SWDIO`) | $\rightarrow$ | **`RX` (`GPIO20`)** | DSO-152 transmits waveforms & commands to ESP32-C3. |
| **`CLK`** | `PA14` (`SWCLK`) | $\leftarrow$ | **`TX` (`GPIO21`)** | ESP32-C3 sends IP status, remote Web UI commands & Wi-Fi scan results to DSO-152. |

> [!TIP]
> **Power Rail Tip:** The companion firmware automatically sets `WiFi.setTxPower(WIFI_POWER_11dBm)` so the ESP32-C3 draws only ~90–110 mA during Wi-Fi transmission instead of 300 mA spikes. If you have a small **10 µF – 47 µF capacitor**, soldering it across `3V3` and `G` on the ESP32-C3 SuperMini makes the 3.3V rail even smoother.
>
> **Important:** Do **not** connect USB-C power to the ESP32-C3 at the same time its `3V3` pin is wired to the DSO-152's `3.3V` pad (flash the ESP32-C3 over USB-C *before* soldering the `3V3` wire, or disconnect `3V3` while flashing the ESP32-C3).

---

## 2. Flashing the Two Boards in Arduino IDE

### Step A: Flash the ESP32-C3 SuperMini
1. Open [`arduino/esp32c3_wifi_companion/esp32c3_wifi_companion.ino`](../arduino/esp32c3_wifi_companion/esp32c3_wifi_companion.ino) in the Arduino IDE.
2. *(Optional)* At the top of the sketch, you can enter your home Wi-Fi `STA_SSID` and `STA_PASS` if you want it to join your home LAN in addition to broadcasting its own **`DSO-152-WiFi`** hotspot.
3. Select **Tools -> Board -> esp32 -> `ESP32C3 Dev Module`**.
4. Connect the ESP32-C3 SuperMini via USB-C and click **Upload**.

### Step B: Flash the FNIRSI DSO-152
1. Run `./arduino/install_arduino_ide.sh` from the repository root so the latest library and example are installed into `~/Arduino/libraries/FNIRSI_DSO152`.
2. Open **File -> Examples -> FNIRSI DSO-152 -> `04_WiFi_Web_Oscilloscope`** ([`04_WiFi_Web_Oscilloscope.ino`](../arduino/FNIRSI_DSO152/examples/04_WiFi_Web_Oscilloscope/04_WiFi_Web_Oscilloscope.ino)).
3. Select:
   - **Board:** `Generic STM32F1 series`
   - **Board part number:** `FNIRSI DSO-152 (AT32F403A / STM32F103RC)`
   - **Upload method:** `FNIRSI DSO-152 USB Drive (DSO BOOT @ 0x08004000)`
4. Put the DSO-152 into **`DSO BOOT`** mode (hold `OK` while turning on) and click **Upload**.

---

## 3. Using the Wireless Web Oscilloscope & Wi-Fi Scanner

### Mode 1: Wireless Web Oscilloscope (`http://192.168.4.1`)
1. Turn on the DSO-152. The bottom status bar on the LCD will show **`Web: 192.168.4.1`** (or your home LAN IP if `STA_SSID` was configured) in green once the ESP32-C3 replies over `PA14`.
2. On your phone or laptop, connect to the Wi-Fi network:
   - **SSID:** `DSO-152-WiFi`
   - **Password:** `dso152wifi`
3. Open **`http://192.168.4.1`** in any web browser:
   - View the live triggered waveform, `Vpp`, `Vavg`, Vertical Range, Coupling, and Timebase.
   - Click **`Range +`**, **`Range -`**, **`Timebase Fast/Slow`**, **`AC / DC`**, **`RUN / HOLD`**, or **`AUTO SET`** in the browser to remotely switch the hardware relays and muxes on the DSO-152!
   - Click **`Export CSV`** to download the captured waveform samples to your computer or phone.

### Mode 2: On-Screen 2.4 GHz Wi-Fi Scanner
- Press the **`OK / MENU`** button briefly on the DSO-152 to switch to the **2.4 GHz Wi-Fi Scanner** screen:
  - The DSO-152 sends `$SCAN` over `PA13` (`DIO`) to the ESP32-C3.
  - The ESP32-C3 scans all 2.4 GHz channels and returns the top networks (`SSID`, `Channel`, `RSSI dBm`), which are rendered with color-coded signal bars directly on the DSO-152's 320x240 IPS display!
  - Press **`RUN`** to rescan, or press **`OK / MENU`** again to return to the oscilloscope.
