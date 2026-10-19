/**
 * @file 04_WiFi_Web_Oscilloscope.ino
 * @brief Wireless Web Oscilloscope & 2.4GHz Wi-Fi Scanner for FNIRSI DSO-152 + ESP32-C3 SuperMini
 *
 * Hardware Wiring (Method 1 - 4-Pin SWD Header Pads on DSO-152 PCB):
 *   DSO-152 SWD Pad      ESP32-C3 SuperMini Pin
 *   ---------------      ----------------------
 *   VCC (3.3V)       ->  3V3
 *   GND              ->  GND (G)
 *   DIO (PA13 / TX)  ->  RX  (GPIO20)
 *   CLK (PA14 / RX)  ->  TX  (GPIO21)
 *
 * Features:
 *   1. SCOPE + WEB STREAM MODE:
 *      - Captures live waveforms from the probe (PA1) and renders on the 320x240 IPS LCD
 *      - Streams waveforms + telemetry at 57600 baud over PA13 (DIO) to the ESP32-C3
 *      - Displays the ESP32-C3's Wi-Fi IP address on the DSO-152 screen
 *      - Accepts remote control commands clicked in the Web Browser UI (Range, Timebase, AC/DC, Hold)
 *   2. 2.4 GHz WI-FI SCANNER MODE (Press OK/MENU briefly to toggle):
 *      - Commands the ESP32-C3 to scan 2.4 GHz Wi-Fi channels and renders SSIDs,
 *        channels, dBm values, and color-coded RSSI bars on the DSO-152 display!
 */

#include <FNIRSI_DSO152.h>

static dso152_range_t currentRange = AFE_RANGE_1V;
static bool acCoupling = false;
static bool paused = false;
static bool wifiScannerMode = false;

static const uint16_t DELAY_STEPS[] = { 0, 5, 10, 20, 50, 100, 250 };
static const char* const TIMEBASE_NAMES[] = {
    "10us", "25us", "50us", "100us", "250us", "500us", "1ms"
};
static uint8_t timebaseIdx = 3;

static uint16_t rawSamples[300];
static uint8_t prevY[300];
static DSO152_Buttons prevBtn = {};

static char espIpStr[20] = "Connecting...";
static bool espConnected = false;
static uint32_t lastWifiTx = 0;
static uint32_t lastCalToggle = 0;

static const char HEX_CHARS[] = "0123456789ABCDEF";

static void drawGridPixel(int16_t x, int16_t y) {
    int16_t gx = x - 10;
    int16_t gy = y - 28;
    if (gy == 90 || gx == 150) {
        dso.drawPixel(x, y, COLOR_GRAY);
    } else if ((gx % 30 == 0 && (gy % 4 == 0)) || (gy % 30 == 0 && (gx % 4 == 0))) {
        dso.drawPixel(x, y, COLOR_DARKGRAY);
    } else {
        dso.drawPixel(x, y, COLOR_BLACK);
    }
}

static void drawScopeFrame() {
    dso.fillScreen(COLOR_BLACK);
    dso.fillRect(0, 0, 320, 22, COLOR_NAVY);
    dso.fillRect(0, 214, 320, 26, COLOR_DARKGRAY);
    dso.drawRect(9, 27, 302, 182, COLOR_GRAY);

    for (int16_t x = 10; x < 310; x++) {
        for (int16_t y = 28; y < 208; y++) {
            drawGridPixel(x, y);
        }
    }
    for (uint16_t i = 0; i < 300; i++) {
        prevY[i] = 118;
    }
}

static void applyRemoteCommand(const char *cmd) {
    if (!cmd || !cmd[0]) return;
    if (strcmp(cmd, "RNG+") == 0 && currentRange < AFE_RANGE_20V) {
        currentRange = (dso152_range_t)(currentRange + 1);
        dso.setRange(currentRange);
    } else if (strcmp(cmd, "RNG-") == 0 && currentRange > AFE_RANGE_10mV) {
        currentRange = (dso152_range_t)(currentRange - 1);
        dso.setRange(currentRange);
    } else if (strcmp(cmd, "TIM-") == 0 && timebaseIdx > 0) {
        timebaseIdx--;
    } else if (strcmp(cmd, "TIM+") == 0 && timebaseIdx < 6) {
        timebaseIdx++;
    } else if (strcmp(cmd, "ACDC") == 0) {
        acCoupling = !acCoupling;
        dso.setCoupling(acCoupling);
    } else if (strcmp(cmd, "HOLD") == 0) {
        paused = !paused;
    } else if (strcmp(cmd, "AUTO") == 0) {
        currentRange = AFE_RANGE_1V;
        timebaseIdx = 3;
        paused = false;
        dso.setRange(currentRange);
    }
}

static void streamWaveformToESP32(int32_t vppMv, int32_t vAvgMv) {
    // Format: $W,<range>,<acdc>,<timebase>,<paused>,<vpp>,<vavg>,<150 hex bytes (00..FF)>\n
    char hdr[80];
    snprintf(hdr, sizeof(hdr), "$W,%s,%s,%s,%d,%ld,%ld,",
             dso.getRangeName(currentRange),
             acCoupling ? "AC" : "DC",
             TIMEBASE_NAMES[timebaseIdx],
             paused ? 1 : 0,
             (long)vppMv,
             (long)vAvgMv);

    dso.swdSerialPrint(hdr);

    // Send 150 decimated 8-bit samples as 300 hex chars
    for (uint16_t i = 0; i < 300; i += 2) {
        uint8_t s8 = (uint8_t)(rawSamples[i] >> 4); // 12-bit (0..4095) -> 8-bit (0..255)
        dso.swdSerialWrite(HEX_CHARS[(s8 >> 4) & 0x0F]);
        dso.swdSerialWrite(HEX_CHARS[s8 & 0x0F]);
    }
    dso.swdSerialWrite('\n');

    // Read immediate status/command reply from ESP32-C3: "OK|<ip>|<cmd>"
    char rxBuf[64];
    uint16_t len = dso.swdSerialReadLine(rxBuf, sizeof(rxBuf), 20);
    if (len >= 3 && strncmp(rxBuf, "OK|", 3) == 0) {
        espConnected = true;
        char *ipPtr = rxBuf + 3;
        char *sep = strchr(ipPtr, '|');
        if (sep) {
            *sep = '\0';
            char *cmdPtr = sep + 1;
            strncpy(espIpStr, ipPtr, sizeof(espIpStr) - 1);
            espIpStr[sizeof(espIpStr) - 1] = '\0';
            applyRemoteCommand(cmdPtr);
        } else {
            strncpy(espIpStr, ipPtr, sizeof(espIpStr) - 1);
            espIpStr[sizeof(espIpStr) - 1] = '\0';
        }
    }
}

static void runWiFiScannerScreen() {
    dso.fillScreen(COLOR_BLACK);
    dso.fillRect(0, 0, 320, 24, COLOR_BLUE);
    dso.drawString(8, 8, "ESP32-C3 2.4GHz WI-FI SCANNER", COLOR_WHITE, COLOR_BLUE, 1);
    dso.fillRect(0, 216, 320, 24, COLOR_DARKGRAY);
    dso.drawString(8, 224, "OK: Back to Scope | RUN: Rescan Wi-Fi", COLOR_YELLOW, COLOR_DARKGRAY, 1);

    dso.drawString(16, 100, "Scanning 2.4 GHz networks via ESP32-C3...", COLOR_CYAN, COLOR_BLACK, 1);

    // Send $SCAN and wait up to 3.5 seconds for ESP32-C3 scan results
    char rxLine[240];
    uint16_t n = dso.swdSerialTransact("$SCAN", rxLine, sizeof(rxLine), 3500);
    dso.fillRect(0, 26, 320, 188, COLOR_BLACK);

    if (n == 0 || strncmp(rxLine, "$NETS|", 6) != 0) {
        dso.drawString(16, 90, "No response from ESP32-C3 on PA13/PA14!", COLOR_RED, COLOR_BLACK, 1);
        dso.drawString(16, 112, "Check SWD wiring: DIO->RX(20), CLK->TX(21)", COLOR_WHITE, COLOR_BLACK, 1);
        return;
    }

    // Parse "$NETS|ssid1,rssi1,ch1;ssid2,rssi2,ch2;..."
    char *p = rxLine + 6;
    int row = 0;
    while (*p && row < 8) {
        char *endEntry = strchr(p, ';');
        if (endEntry) *endEntry = '\0';

        char *c1 = strchr(p, ',');
        char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
        if (c1 && c2) {
            *c1 = '\0';
            *c2 = '\0';
            const char *ssid = p;
            int rssi = atoi(c1 + 1);
            int ch   = atoi(c2 + 1);

            int16_t y = 32 + row * 22;
            char lineBuf[32];
            snprintf(lineBuf, sizeof(lineBuf), "%d. %-16.16s Ch%-2d %4ddBm", row + 1, ssid, ch, rssi);
            dso.drawString(6, y, lineBuf, COLOR_WHITE, COLOR_BLACK, 1);

            // Draw RSSI bar (-90 dBm .. -30 dBm -> 0 .. 56 px)
            int16_t barW = (rssi + 95) * 56 / 65;
            if (barW < 4) barW = 4;
            if (barW > 56) barW = 56;
            uint16_t barColor = (rssi > -60) ? COLOR_GREEN : (rssi > -75 ? COLOR_YELLOW : COLOR_RED);
            dso.fillRect(256, y, barW, 10, barColor);
            dso.drawRect(256, y, 56, 10, COLOR_GRAY);
            row++;
        }

        if (!endEntry) break;
        p = endEntry + 1;
    }

    if (row == 0) {
        dso.drawString(16, 100, "No 2.4 GHz networks found.", COLOR_ORANGE, COLOR_BLACK, 1);
    }
}

void setup() {
    dso.begin(1); // 320x240 Landscape
    dso.setRange(currentRange);
    dso.setCoupling(acCoupling);

    // Initialize SWD Header UART Bridge (PA13/DIO = TX, PA14/CLK = RX) at 57600 baud
    dso.swdSerialBegin(57600);

    drawScopeFrame();
}

void loop() {
    // Handle Power button (hold 800ms to power off) and OK button (hold 2s for DSO BOOT)
    dso.updatePowerButton(true);

    // Toggle 1kHz calibration wave on PA0
    if (millis() - lastCalToggle >= 1) {
        lastCalToggle = millis();
        dso.toggleCalPin();
    }

    DSO152_Buttons btn = dso.readButtons();

    // Short press on OK/MENU switches between Oscilloscope and Wi-Fi Scanner
    if (!btn.ok && prevBtn.ok) {
        wifiScannerMode = !wifiScannerMode;
        if (wifiScannerMode) {
            runWiFiScannerScreen();
        } else {
            drawScopeFrame();
        }
    }

    if (wifiScannerMode) {
        if (btn.run && !prevBtn.run) {
            runWiFiScannerScreen();
        }
        prevBtn = btn;
        delay(15);
        return;
    }

    // Scope controls
    if (btn.up && !prevBtn.up && currentRange < AFE_RANGE_20V) {
        currentRange = (dso152_range_t)(currentRange + 1);
        dso.setRange(currentRange);
    }
    if (btn.down && !prevBtn.down && currentRange > AFE_RANGE_10mV) {
        currentRange = (dso152_range_t)(currentRange - 1);
        dso.setRange(currentRange);
    }
    if (btn.left && !prevBtn.left && timebaseIdx > 0) {
        timebaseIdx--;
    }
    if (btn.right && !prevBtn.right && timebaseIdx < 6) {
        timebaseIdx++;
    }
    if (btn.mode && !prevBtn.mode) {
        acCoupling = !acCoupling;
        dso.setCoupling(acCoupling);
    }
    if (btn.run && !prevBtn.run) {
        paused = !paused;
    }
    if (btn.auto_btn && !prevBtn.auto_btn) {
        currentRange = AFE_RANGE_1V;
        timebaseIdx = 3;
        paused = false;
        dso.setRange(currentRange);
    }
    prevBtn = btn;

    static int32_t vppMv = 0;
    static int32_t vAvgMv = 0;

    if (!paused) {
        dso.captureWaveform(rawSamples, 300, DELAY_STEPS[timebaseIdx], 2048);

        uint16_t vMinRaw = 4095, vMaxRaw = 0;
        uint32_t sum = 0;
        for (uint16_t i = 0; i < 300; i++) {
            uint16_t s = rawSamples[i];
            if (s < vMinRaw) vMinRaw = s;
            if (s > vMaxRaw) vMaxRaw = s;
            sum += s;
        }
        uint16_t mid = sum / 300;

        for (uint16_t i = 0; i < 300; i++) {
            int16_t x = 10 + i;
            uint8_t oldY0 = prevY[i];
            uint8_t oldY1 = (i + 1 < 300) ? prevY[i + 1] : oldY0;
            uint8_t yA = (oldY0 < oldY1) ? oldY0 : oldY1;
            uint8_t yB = (oldY0 > oldY1) ? oldY0 : oldY1;
            for (uint8_t y = yA; y <= yB; y++) {
                drawGridPixel(x, y);
            }

            int32_t centered = (int32_t)rawSamples[i] - (int32_t)mid;
            int32_t yNew = 118 - (centered * 80) / 2048;
            if (yNew < 29) yNew = 29;
            if (yNew > 206) yNew = 206;
            prevY[i] = (uint8_t)yNew;
        }

        for (uint16_t i = 0; i < 299; i++) {
            int16_t x = 10 + i;
            uint8_t y0 = prevY[i];
            uint8_t y1 = prevY[i + 1];
            uint8_t yA = (y0 < y1) ? y0 : y1;
            uint8_t yB = (y0 > y1) ? y0 : y1;
            dso.drawFastVLine(x, yA, (yB - yA) + 1, COLOR_YELLOW);
        }

        vppMv  = ((int32_t)(vMaxRaw - vMinRaw) * 3300) / 4095;
        vAvgMv = ((int32_t)mid * 3300) / 4095;
    }

    // Stream waveform to ESP32-C3 every 80ms (~12.5 Hz web update rate)
    if (millis() - lastWifiTx >= 80) {
        lastWifiTx = millis();
        streamWaveformToESP32(vppMv, vAvgMv);
    }

    // Header bar
    char hdrBuf[40];
    snprintf(hdrBuf, sizeof(hdrBuf), "WIFI-DSO %-6s %s %-5s %s",
             dso.getRangeName(currentRange),
             acCoupling ? "AC" : "DC",
             TIMEBASE_NAMES[timebaseIdx],
             paused ? "HOLD" : "RUN ");
    dso.drawString(6, 7, hdrBuf, COLOR_WHITE, COLOR_NAVY, 1);

    // Footer bar with live measurements + ESP32-C3 IP address
    char footBuf[40];
    snprintf(footBuf, sizeof(footBuf), "Vpp:%4ldmV | Web: %-15.15s",
             (long)vppMv, espIpStr);
    dso.drawString(6, 223, footBuf, espConnected ? COLOR_GREEN : COLOR_ORANGE, COLOR_DARKGRAY, 1);

    delay(10);
}
