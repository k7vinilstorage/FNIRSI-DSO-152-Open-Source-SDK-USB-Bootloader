/**
 * @file 01_HelloWorld_GFX.ino
 * @brief Arduino + Adafruit_GFX Demo for the FNIRSI DSO-152 Oscilloscope
 *
 * Board Setup in Arduino IDE:
 *   - Board: "Generic STM32F1 series"
 *   - Board part number: "FNIRSI DSO-152 (CH32F103C8)" (or "BluePill F103C8")
 *   - Bootloader: "FNIRSI DSO-152 USB Bootloader (16KB, 0x4000)"
 *   - Upload method: "FNIRSI DSO-152 USB Drive (DSO BOOT)"
 */

#include <FNIRSI_DSO152.h>

DSO152_Buttons prevBtns = {};
uint32_t lastCalToggle = 0;

void drawButtonBadge(int16_t x, int16_t y, const char *label, bool pressed) {
    uint16_t bg = pressed ? COLOR_GREEN : COLOR_DARKGRAY;
    uint16_t fg = pressed ? COLOR_BLACK : COLOR_WHITE;
    dso.fillRect(x, y, 42, 20, bg);
    dso.drawRect(x, y, 42, 20, pressed ? COLOR_WHITE : COLOR_GRAY);
    dso.drawString(x + 5, y + 6, label, fg, bg, 1);
}

void setup() {
    // 1. Initialize DSO-152 hardware, power latch, buttons, LCD, and AFE
    dso.begin(1); // Rotation 1 = 320x240 Landscape

    // 2. Draw Header Banner
    dso.fillRect(0, 0, 320, 26, COLOR_BLUE);
    dso.setCursor(16, 6);
    dso.setTextColor(COLOR_WHITE, COLOR_BLUE);
    dso.setTextSize(2);
    dso.print("ARDUINO ON DSO-152");

    // 3. System Info using standard Arduino Print / Adafruit_GFX API
    dso.setTextSize(1);
    dso.setTextColor(COLOR_CYAN, COLOR_BLACK);
    dso.setCursor(10, 34);
    dso.println("Framework: STM32duino + Adafruit_GFX");

    dso.setTextColor(COLOR_LIGHTGRAY, COLOR_BLACK);
    dso.setCursor(10, 48);
    dso.print("F_CPU: ");
    dso.print(F_CPU / 1000000UL);
    dso.println(" MHz | Display: 320x240 16-bit");

    // 4. Graphics Primitives Box
    dso.drawRect(10, 66, 145, 75, COLOR_GRAY);
    dso.drawString(16, 70, "Adafruit_GFX Shapes", COLOR_WHITE, COLOR_BLACK, 1);
    dso.fillRect(18, 88, 28, 28, COLOR_RED);
    dso.fillCircle(68, 102, 14, COLOR_GREEN);
    dso.drawCircle(104, 102, 14, COLOR_CYAN);
    dso.drawLine(124, 88, 148, 116, COLOR_YELLOW);
    dso.drawLine(124, 116, 148, 88, COLOR_MAGENTA);

    // 5. Live ADC Readout Box
    dso.drawRect(165, 66, 145, 75, COLOR_GRAY);
    dso.drawString(172, 70, "Live Probe Input", COLOR_WHITE, COLOR_BLACK, 1);

    // 6. Button Widgets
    dso.drawString(10, 152, "Interactive Button Test:", COLOR_WHITE, COLOR_BLACK, 1);
    drawButtonBadge(10,  170, "LEFT",  false);
    drawButtonBadge(58,  170, "OK",    false);
    drawButtonBadge(106, 170, "RIGHT", false);
    drawButtonBadge(154, 170, "UP",    false);
    drawButtonBadge(202, 170, "DOWN",  false);
    drawButtonBadge(250, 170, "MODE",  false);

    dso.drawString(10, 205, "Hold PWR (1s): Off | Hold OK (2s): Bootloader", COLOR_ORANGE, COLOR_BLACK, 1);
}

void loop() {
    // Handle Power button shutdown & OK button bootloader shortcut
    dso.updatePowerButton(true);

    // Toggle 1kHz calibration output pin (PA0)
    if (millis() - lastCalToggle >= 1) {
        lastCalToggle = millis();
        dso.toggleCalPin();
    }

    // Update buttons on change
    DSO152_Buttons b = dso.readButtons();
    if (b.left  != prevBtns.left)  drawButtonBadge(10,  170, "LEFT",  b.left);
    if (b.ok    != prevBtns.ok)    drawButtonBadge(58,  170, "OK",    b.ok);
    if (b.right != prevBtns.right) drawButtonBadge(106, 170, "RIGHT", b.right);
    if (b.up    != prevBtns.up)    drawButtonBadge(154, 170, "UP",    b.up);
    if (b.down  != prevBtns.down)  drawButtonBadge(202, 170, "DOWN",  b.down);
    if (b.mode  != prevBtns.mode)  drawButtonBadge(250, 170, "MODE",  b.mode);
    prevBtns = b;

    // Update live ADC reading every 100ms
    static uint32_t lastAdcUpdate = 0;
    if (millis() - lastAdcUpdate >= 100) {
        lastAdcUpdate = millis();
        char buf[24];
        snprintf(buf, sizeof(buf), "ADC: %4u (%4lu mV)", dso.readADC(), (unsigned long)dso.readMillivolts());
        dso.drawString(172, 96, buf, COLOR_YELLOW, COLOR_BLACK, 1);

        snprintf(buf, sizeof(buf), "Uptime: %6lu s", millis() / 1000UL);
        dso.drawString(172, 116, buf, COLOR_CYAN, COLOR_BLACK, 1);
    }

    delay(10);
}
