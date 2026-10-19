/**
 * @file 05_WiFi_Cyberdeck_Client.ino
 * @brief ESP32-Style WiFi, WiFiClient & HTTPClient Cyberdeck Suite for FNIRSI DSO-152
 *
 * Demonstrates using <DSO152_WiFi.h> on the DSO-152 just like standard ESP32
 * <WiFi.h> and <HTTPClient.h> — all controlled dynamically from the DSO-152!
 *
 * Controls:
 *   - LEFT / RIGHT : Switch between 4 tabs:
 *                    [1. Dashboard & HTTPS] [2. Wi-Fi Scanner] [3. Ping Monitor] [4. TCP Client]
 *   - OK / RUN     : Refresh / Trigger action on current tab
 *   - AUTO         : Open Interactive On-Screen Wi-Fi Network Picker & Keyboard!
 *   - Hold POWER   : Clean battery shutdown (also cuts power to ESP32-C3)
 */

#include <FNIRSI_DSO152.h>
#include <DSO152_WiFi.h>

// Optional: Put default Wi-Fi credentials here, OR leave "" to use saved Wi-Fi
// or pick a network interactively on screen using the AUTO button!
static const char* DEFAULT_SSID = "";
static const char* DEFAULT_PASS = "";

static int currentTab = 0; // 0=Dashboard, 1=WiFi Scan, 2=Ping, 3=TCP Client
static DSO152_Buttons prevBtn = {};

static void drawTopBar() {
    dso.fillRect(0, 0, 320, 22, COLOR_NAVY);
    const char* tabs[4] = { "1:DASH", "2:SCAN", "3:PING", "4:TCP" };
    for (int i = 0; i < 4; i++) {
        bool active = (i == currentTab);
        int16_t x = 4 + i * 54;
        uint16_t bg = active ? COLOR_CYAN : COLOR_NAVY;
        uint16_t fg = active ? COLOR_BLACK : COLOR_WHITE;
        dso.fillRect(x, 3, 50, 16, bg);
        dso.drawString(x + 4, 7, tabs[i], fg, bg, 1);
    }

    // Connection badge on the right
    bool conn = (WiFi.status() == WL_CONNECTED);
    dso.drawString(228, 7, conn ? "WIFI:ON " : "WIFI:OFF", conn ? COLOR_GREEN : COLOR_RED, COLOR_NAVY, 1);

    // Footer bar
    dso.fillRect(0, 216, 320, 24, COLOR_DARKGRAY);
    dso.drawString(6, 224, "L/R:Tab  OK/RUN:Refresh  AUTO:Wi-Fi Setup", COLOR_YELLOW, COLOR_DARKGRAY, 1);
}

// ---------------------------------------------------------------------------
// Tab 0: Live Weather, Public IP, & NTP Clock via HTTPClient
// ---------------------------------------------------------------------------
static void renderDashboardTab() {
    drawTopBar();
    dso.fillRect(0, 22, 320, 194, COLOR_BLACK);

    if (WiFi.status() != WL_CONNECTED) {
        dso.drawString(16, 70, "Not connected to Wi-Fi!", COLOR_RED, COLOR_BLACK, 1);
        dso.drawString(16, 95, "Press [AUTO] to open the on-screen", COLOR_WHITE, COLOR_BLACK, 1);
        dso.drawString(16, 112, "Wi-Fi Network Picker & Keyboard.", COLOR_CYAN, COLOR_BLACK, 1);
        return;
    }

    dso.drawString(10, 30, ("SSID : " + WiFi.SSID()).c_str(), COLOR_WHITE, COLOR_BLACK, 1);
    dso.drawString(10, 44, ("IP   : " + WiFi.localIP() + " (" + String(WiFi.RSSI()) + " dBm)").c_str(), COLOR_GREEN, COLOR_BLACK, 1);
    dso.drawString(10, 58, ("Time : " + WiFi.getNTPTime(-10800)).c_str(), COLOR_CYAN, COLOR_BLACK, 1);

    dso.drawFastHLine(10, 74, 300, COLOR_GRAY);
    dso.drawString(10, 82, "Fetching HTTPS Weather & IP Info...", COLOR_YELLOW, COLOR_BLACK, 1);

    // 1. HTTP GET Public IP & Location
    HTTPClient http;
    if (http.begin("http://ip-api.com/line/?fields=query,city,country,isp")) {
        int code = http.GET();
        if (code == 200) {
            String body = http.getString();
            int l1 = body.indexOf('\n');
            int l2 = body.indexOf('\n', l1 + 1);
            int l3 = body.indexOf('\n', l2 + 1);
            String pubIp   = (l1 > 0) ? body.substring(0, l1) : "";
            String city    = (l2 > l1) ? body.substring(l1 + 1, l2) : "";
            String country = (l3 > l2) ? body.substring(l2 + 1, l3) : "";
            String isp     = (l3 > 0) ? body.substring(l3 + 1) : "";
            isp.trim();

            dso.fillRect(10, 82, 300, 16, COLOR_BLACK);
            dso.drawString(10, 82,  ("WAN IP : " + pubIp).c_str(), COLOR_ORANGE, COLOR_BLACK, 1);
            dso.drawString(10, 98,  ("Loc    : " + city + ", " + country).c_str(), COLOR_WHITE, COLOR_BLACK, 1);
            dso.drawString(10, 114, ("ISP    : " + isp.substring(0, 30)).c_str(), COLOR_LIGHTGRAY, COLOR_BLACK, 1);
        }
        http.end();
    }

    // 2. HTTPS GET Weather from wttr.in
    if (http.begin("https://wttr.in/?format=%l:+%C+%t+%h+%w")) {
        int code = http.GET();
        if (code == 200) {
            String w = http.getString();
            w.trim();
            dso.drawRect(10, 136, 300, 44, COLOR_CYAN);
            dso.drawString(16, 144, "LIVE WEATHER (HTTPS):", COLOR_CYAN, COLOR_BLACK, 1);
            dso.drawString(16, 162, w.substring(0, 35).c_str(), COLOR_YELLOW, COLOR_BLACK, 1);
        }
        http.end();
    }
}

// ---------------------------------------------------------------------------
// Tab 1: 2.4 GHz Wi-Fi Scanner using ESP32-Style WiFi.scanNetworks()
// ---------------------------------------------------------------------------
static void renderScannerTab() {
    drawTopBar();
    dso.fillRect(0, 22, 320, 194, COLOR_BLACK);
    dso.drawString(16, 100, "Calling WiFi.scanNetworks()...", COLOR_CYAN, COLOR_BLACK, 1);

    int n = WiFi.scanNetworks();
    dso.fillRect(0, 22, 320, 194, COLOR_BLACK);

    if (n <= 0) {
        dso.drawString(16, 100, "No 2.4 GHz networks found.", COLOR_RED, COLOR_BLACK, 1);
        return;
    }

    int show = (n > 8) ? 8 : n;
    for (int i = 0; i < show; i++) {
        int16_t y = 28 + i * 22;
        int32_t rssi = WiFi.RSSI(i);
        char line[36];
        snprintf(line, sizeof(line), "%d.%-15.15s Ch%-2ld %4lddBm",
                 i + 1, WiFi.SSID(i).c_str(), (long)WiFi.channel(i), (long)rssi);
        dso.drawString(6, y, line, COLOR_WHITE, COLOR_BLACK, 1);

        int16_t w = (rssi + 95) * 54 / 65;
        if (w < 4) w = 4;
        if (w > 54) w = 54;
        uint16_t col = (rssi > -60) ? COLOR_GREEN : (rssi > -75 ? COLOR_YELLOW : COLOR_RED);
        dso.fillRect(258, y, w, 10, col);
        dso.drawRect(258, y, 54, 10, COLOR_GRAY);
    }
}

// ---------------------------------------------------------------------------
// Tab 2: Live Latency / Ping Monitor using WiFi.ping()
// ---------------------------------------------------------------------------
static void renderPingTab() {
    drawTopBar();
    dso.fillRect(0, 22, 320, 194, COLOR_BLACK);

    const char* hosts[5] = {
        "1.1.1.1",
        "8.8.8.8",
        "github.com",
        "google.com",
        "api.open-meteo.com"
    };

    dso.drawString(10, 30, "HOST TCP REACHABILITY & LATENCY:", COLOR_CYAN, COLOR_BLACK, 1);

    for (int i = 0; i < 5; i++) {
        int16_t y = 52 + i * 30;
        dso.drawString(10, y, hosts[i], COLOR_WHITE, COLOR_BLACK, 1);
        int32_t ms = WiFi.ping(hosts[i], 80);

        if (ms >= 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%4ld ms", (long)ms);
            uint16_t col = (ms < 60) ? COLOR_GREEN : (ms < 180 ? COLOR_YELLOW : COLOR_ORANGE);
            dso.drawString(155, y, buf, col, COLOR_BLACK, 1);

            int16_t barW = (ms > 300) ? 90 : (ms * 90 / 300);
            if (barW < 4) barW = 4;
            dso.fillRect(220, y, barW, 10, col);
            dso.drawRect(220, y, 90, 10, COLOR_GRAY);
        } else {
            dso.drawString(155, y, "TIMEOUT", COLOR_RED, COLOR_BLACK, 1);
        }
    }
}

// ---------------------------------------------------------------------------
// Tab 3: Raw TCP Socket Demo using WiFiClient
// ---------------------------------------------------------------------------
static void renderTCPClientTab() {
    drawTopBar();
    dso.fillRect(0, 22, 320, 194, COLOR_BLACK);
    dso.drawString(10, 30, "WiFiClient Raw TCP Socket Demo:", COLOR_CYAN, COLOR_BLACK, 1);
    dso.drawString(10, 48, "Connecting to tcpbin.com / example.com:80...", COLOR_YELLOW, COLOR_BLACK, 1);

    WiFiClient client;
    if (client.connect("example.com", 80)) {
        dso.drawString(10, 66, "Connected! Sending HTTP/1.1 request...", COLOR_GREEN, COLOR_BLACK, 1);
        client.print("GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");

        String resp = client.readString(2500);
        client.stop();

        dso.drawRect(8, 84, 304, 124, COLOR_GRAY);
        int pos = 0;
        for (int row = 0; row < 7 && pos < (int)resp.length(); row++) {
            int nl = resp.indexOf('\n', pos);
            String line = (nl >= 0) ? resp.substring(pos, nl) : resp.substring(pos);
            line.trim();
            dso.drawString(14, 90 + row * 16, line.substring(0, 36).c_str(), COLOR_WHITE, COLOR_BLACK, 1);
            if (nl < 0) break;
            pos = nl + 1;
        }
    } else {
        dso.drawString(10, 70, "Socket connect failed (Check Wi-Fi)!", COLOR_RED, COLOR_BLACK, 1);
    }
}

static void renderCurrentTab() {
    if (currentTab == 0)      renderDashboardTab();
    else if (currentTab == 1) renderScannerTab();
    else if (currentTab == 2) renderPingTab();
    else if (currentTab == 3) renderTCPClientTab();
}

void setup() {
    dso.begin(1); // 320x240 Landscape
    dso.fillScreen(COLOR_BLACK);
    dso.fillRect(0, 0, 320, 24, COLOR_NAVY);
    dso.drawString(8, 8, "DSO-152 WI-FI CYBERDECK CLIENT", COLOR_WHITE, COLOR_NAVY, 1);

    dso.drawString(12, 50, "Initializing ESP32-C3 Modem (PA13/PA14)...", COLOR_CYAN, COLOR_BLACK, 1);
    if (WiFi.initModem(57600)) {
        dso.drawString(12, 70, "Modem Ready! Connecting to Wi-Fi...", COLOR_GREEN, COLOR_BLACK, 1);
        if (strlen(DEFAULT_SSID) > 0) {
            WiFi.begin(DEFAULT_SSID, DEFAULT_PASS);
        } else {
            WiFi.beginSaved();
        }
    } else {
        dso.drawString(12, 70, "ESP32-C3 not detected on PA13/PA14!", COLOR_RED, COLOR_BLACK, 1);
        delay(1200);
    }

    renderCurrentTab();
}

void loop() {
    dso.updatePowerButton(true);
    DSO152_Buttons b = dso.readButtons();

    if (b.left && !prevBtn.left) {
        currentTab = (currentTab > 0) ? (currentTab - 1) : 3;
        renderCurrentTab();
    }
    if (b.right && !prevBtn.right) {
        currentTab = (currentTab < 3) ? (currentTab + 1) : 0;
        renderCurrentTab();
    }
    if ((b.ok && !prevBtn.ok) || (b.run && !prevBtn.run)) {
        renderCurrentTab();
    }
    if (b.auto_btn && !prevBtn.auto_btn) {
        // Launch interactive on-screen Wi-Fi Network Picker & Virtual Keyboard!
        WiFi.selectNetworkUI();
        renderCurrentTab();
    }

    prevBtn = b;
    delay(20);
}
