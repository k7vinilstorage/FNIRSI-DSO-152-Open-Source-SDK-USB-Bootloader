# FNIRSI DSO-152 Pinout & GPIO Reference

This document provides a comprehensive mapping of every microcontroller pin on the FNIRSI DSO-152 (CH32F103C8T6 / STM32F103C8T6 48-pin LQFP).

---

## 1. Complete GPIO Pin Allocation

| Pin | GPIO | Direction | Reset State | Active Level | Connected Peripheral / Signal |
|---|---|---|---|---|---|
| **PA0** | `GPIOA_0` | Output Push-Pull | LOW | HIGH | 1 kHz Calibration Test Point Output |
| **PA1** | `GPIOA_1` | Analog Input | High-Z | — | Scope Analog Input (`ADC1_IN1`) |
| **PA2** | `GPIOA_2` | Output Push-Pull | LOW | HIGH | AFE Attenuation Relay (0 = Direct, 1 = Attenuated) |
| **PA3** | `GPIOA_3` | Output Push-Pull | LOW | HIGH | AFE Coupling Select (0 = DC, 1 = AC) |
| **PA4** | `GPIOA_4` | Output Push-Pull | LOW | HIGH | CD4051 Analog MUX Address `A0` |
| **PA5** | `GPIOA_5` | Output Push-Pull | LOW | HIGH | CD4051 Analog MUX Address `A1` |
| **PA6** | `GPIOA_6` | Output Push-Pull | LOW | HIGH | CD4051 Analog MUX Address `A2` |
| **PA7** | `GPIOA_7` | Input / Unused | High-Z | — | Auxiliary / Unconnected |
| **PA8** | `GPIOA_8` | Output Push-Pull | HIGH | LOW | ST7789 Display Reset (`RST`) |
| **PA9** | `GPIOA_9` | Input with Pull-Up | HIGH | LOW | **DOWN Button** |
| **PA10**| `GPIOA_10`| Input with Pull-Up | HIGH | LOW | **RUN/STOP Button** |
| **PA11**| `GPIOA_11`| Bidirectional | High-Z | — | USB Full-Speed Data Minus (`USB_DM`) |
| **PA12**| `GPIOA_12`| Bidirectional | High-Z | — | USB Full-Speed Data Plus (`USB_DP`) |
| **PA13**| `GPIOA_13`| Debug Input/Output | Pull-Up | — | SWD Debug Data (`SWDIO`) |
| **PA14**| `GPIOA_14`| Debug Clock | Pull-Down | — | SWD Debug Clock (`SWCLK`) |
| **PA15**| `GPIOA_15`| Output Push-Pull | HIGH | LOW | ST7789 Display Chip Select (`CS`) *(Requires JTAG disable)* |

---

## 2. Port B: Display 16-Bit Parallel Data Bus

Port B (`PB0`..`PB15`) is dedicated entirely to the ST7789 16-bit 8080-II parallel data interface.

> [!NOTE]
> `PB3` and `PB4` default to JTAG `JTDO` and `JNTRST` on power-on reset.
> To use Port B as a full 16-bit GPIO bus, AFIO remap must disable JTAG while keeping SWD enabled:
> ```c
> AFIO->MAPR = (AFIO->MAPR & ~(7 << 24)) | (2 << 24); // SWJ_CFG = 010 (JTAG-DP Disabled, SW-DP Enabled)
> ```

| Port B Pin | ST7789 Data Line | Bit Weight | Color Channel (RGB565) |
|---|---|:---:|---|
| `PB0`  | `DB0`  | `0x0001` | Blue 0 (LSB) |
| `PB1`  | `DB1`  | `0x0002` | Blue 1 |
| `PB2`  | `DB2`  | `0x0003` | Blue 2 |
| `PB3`  | `DB3`  | `0x0004` | Blue 3 |
| `PB4`  | `DB4`  | `0x0008` | Blue 4 (MSB) |
| `PB5`  | `DB5`  | `0x0010` | Green 0 (LSB) |
| `PB6`  | `DB6`  | `0x0020` | Green 1 |
| `PB7`  | `DB7`  | `0x0040` | Green 2 |
| `PB8`  | `DB8`  | `0x0080` | Green 3 |
| `PB9`  | `DB9`  | `0x0100` | Green 4 |
| `PB10` | `DB10` | `0x0200` | Green 5 (MSB) |
| `PB11` | `DB11` | `0x0400` | Red 0 (LSB) |
| `PB12` | `DB12` | `0x0800` | Red 1 |
| `PB13` | `DB13` | `0x1000` | Red 2 |
| `PB14` | `DB14` | `0x2000` | Red 3 |
| `PB15` | `DB15` | `0x4000` | Red 4 (MSB) |

---

## 3. Port C: Control Lines & Buttons

| Pin | GPIO | Direction | Function / Peripheral |
|---|---|---|---|
| **PC3**  | `GPIOC_3`  | Input with Pull-Up | **LEFT Button** |
| **PC4**  | `GPIOC_4`  | Input with Pull-Up | **OK / MENU Button** *(Held on boot = Enter Bootloader)* |
| **PC5**  | `GPIOC_5`  | Input with Pull-Up | **RIGHT Button** |
| **PC6**  | `GPIOC_6`  | Input with Pull-Up | **UP Button** |
| **PC7**  | `GPIOC_7`  | Input with Pull-Up | **MODE Button** |
| **PC8**  | `GPIOC_8`  | Input with Pull-Up | **AUTO Button** |
| **PC9**  | `GPIOC_9`  | Output Push-Pull   | **LCD Backlight Control** (`0` = ON, `1` = OFF) |
| **PC10** | `GPIOC_10` | Output Push-Pull   | **Battery Power Latch** (`1` = ON, `0` = Shut Down) |
| **PC11** | `GPIOC_11` | Input with Pull-Up | **POWER Button** *(Hold 800ms to power off)* |
| **PC13** | `GPIOC_13` | Output Push-Pull   | ST7789 Register Select (`RS`, `0` = Cmd, `1` = Data) |
| **PC14** | `GPIOC_14` | Output Push-Pull   | ST7789 Write Strobe (`WR`, active-low) |
| **PC15** | `GPIOC_15` | Output Push-Pull   | ST7789 Read Strobe (`RD`, active-low) |

---

## 4. Pushbutton Physical Layout

```
                  +---------------------------+
                  |    [ USB-C / Charging ]   |
                  +---------------------------+
                  |                           |
                  |     320x240 Color LCD     |
                  |                           |
                  +---------------------------+

   [ RUN/STOP ]            [ UP ]             [ AUTO ]
      (PA10)               (PC6)               (PC8)

  [ LEFT ]         [ MENU / OK ]         [ RIGHT ]
   (PC3)               (PC4)               (PC5)

   [ MODE ]               [ DOWN ]            [ POWER ]
   (PC7)                   (PA9)               (PC11)
```

---

## 5. SWD Programming Header

The DSO-152 PCB includes standard unpopulated SWD test pads on the rear of the board:

| Pad / Pin | Signal | MCU Pin | Description |
|---|---|---|---|
| **VCC** | 3.3V | VDD | Power rail (target reference voltage) |
| **DIO** | SWDIO | `PA13` | Bidirectional data line |
| **CLK** | SWCLK | `PA14` | Serial wire clock |
| **GND** | GND | VSS | Ground reference |
