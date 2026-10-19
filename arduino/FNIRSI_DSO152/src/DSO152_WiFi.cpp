#include "DSO152_WiFi.h"

/* ============================================================================
 * Hex Encoding / Decoding Helpers for Binary-Safe UART Framing
 * ============================================================================ */
static const char WIFI_HEX_LUT[] = "0123456789ABCDEF";

static String toHex(const char *str) {
    if (!str) return "";
    String out;
    size_t len = strlen(str);
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        uint8_t b = (uint8_t)str[i];
        out += WIFI_HEX_LUT[(b >> 4) & 0x0F];
        out += WIFI_HEX_LUT[b & 0x0F];
    }
    return out;
}

static String bytesToHex(const uint8_t *buf, size_t len) {
    String out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        uint8_t b = buf[i];
        out += WIFI_HEX_LUT[(b >> 4) & 0x0F];
        out += WIFI_HEX_LUT[b & 0x0F];
    }
    return out;
}

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static String fromHex(const String &hex) {
    String out;
    size_t n = hex.length() / 2;
    out.reserve(n);
    for (size_t i = 0; i < n; i++) {
        uint8_t b = (uint8_t)((hexNibble(hex[i * 2]) << 4) | hexNibble(hex[i * 2 + 1]));
        out += (char)b;
    }
    return out;
}

/* ============================================================================
 * DSO152_WiFiClass Implementation
 * ============================================================================ */
DSO152_WiFiClass::DSO152_WiFiClass() {
    _modem_init    = false;
    _cached_status = WL_DISCONNECTED;
    _cached_ip     = "0.0.0.0";
    _cached_ssid   = "";
    _cached_rssi   = 0;
    _scan_count    = 0;
}

bool DSO152_WiFiClass::initModem(uint32_t baud) {
    dso.swdSerialBegin(baud);
    _modem_init = true;
    return isModemReady();
}

String DSO152_WiFiClass::sendCommand(const char *cmd, uint32_t timeout_ms) {
    if (!_modem_init) {
        initModem(57600);
    }
    // Static buffer big enough for hex-encoded HTTP responses (up to ~1.5 KB)
    static char rxBuf[1600];
    uint16_t n = dso.swdSerialTransact(cmd, rxBuf, sizeof(rxBuf), timeout_ms);
    if (n == 0) return "";
    return String(rxBuf);
}

bool DSO152_WiFiClass::isModemReady() {
    for (int attempt = 0; attempt < 2; attempt++) {
        String r = sendCommand("AT+PING", 450);
        if (r.indexOf("OK|PONG") >= 0) return true;
        delay(20);
    }
    return false;
}

void DSO152_WiFiClass::_refreshStatus() {
    // Reply format: OK|<status>|<ip>|<ssid>|<rssi>|<ap_ip>|<mac>
    String r = sendCommand("AT+STATUS", 800);
    int okPos = r.indexOf("OK|");
    if (okPos < 0) return;
    if (okPos > 0) r = r.substring(okPos);

    int p1 = r.indexOf('|');
    int p2 = r.indexOf('|', p1 + 1);
    int p3 = r.indexOf('|', p2 + 1);
    int p4 = r.indexOf('|', p3 + 1);
    int p5 = r.indexOf('|', p4 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) return;

    _cached_status = (wl_status_t)r.substring(p1 + 1, p2).toInt();
    _cached_ip     = r.substring(p2 + 1, p3);
    _cached_ssid   = r.substring(p3 + 1, p4);
    if (p5 > 0) {
        _cached_rssi = r.substring(p4 + 1, p5).toInt();
    } else {
        _cached_rssi = r.substring(p4 + 1).toInt();
    }
}

wl_status_t DSO152_WiFiClass::begin(const char *ssid, const char *passphrase) {
    if (!_modem_init) initModem(57600);
    String cmd = "AT+BEGIN=" + toHex(ssid) + "," + toHex(passphrase ? passphrase : "");
    String r = sendCommand(cmd.c_str(), 12000); // Allow up to 12s for DHCP handshake
    _refreshStatus();
    return _cached_status;
}

wl_status_t DSO152_WiFiClass::beginSaved() {
    if (!_modem_init) initModem(57600);
    sendCommand("AT+BEGINSAVED", 10000);
    _refreshStatus();
    return _cached_status;
}

bool DSO152_WiFiClass::softAP(const char *ssid, const char *passphrase) {
    if (!_modem_init) initModem(57600);
    String cmd = "AT+AP=" + toHex(ssid) + "," + toHex(passphrase ? passphrase : "");
    String r = sendCommand(cmd.c_str(), 2500);
    return r.indexOf("OK|") >= 0;
}

bool DSO152_WiFiClass::startConfigPortal(const char *ap_ssid) {
    if (!_modem_init) initModem(57600);
    String cmd = "AT+PORTAL=" + toHex(ap_ssid ? ap_ssid : "DSO-152-Setup");
    String r = sendCommand(cmd.c_str(), 2500);
    return r.indexOf("OK|") >= 0;
}

void DSO152_WiFiClass::disconnect() {
    sendCommand("AT+DISC", 1000);
    _cached_status = WL_DISCONNECTED;
    _cached_ip = "0.0.0.0";
}

wl_status_t DSO152_WiFiClass::status() {
    _refreshStatus();
    return _cached_status;
}

String DSO152_WiFiClass::localIP() {
    _refreshStatus();
    return _cached_ip;
}

String DSO152_WiFiClass::softAPIP() {
    String r = sendCommand("AT+STATUS", 800);
    int p1 = r.indexOf('|');
    int p2 = r.indexOf('|', p1 + 1);
    int p3 = r.indexOf('|', p2 + 1);
    int p4 = r.indexOf('|', p3 + 1);
    int p5 = r.indexOf('|', p4 + 1);
    int p6 = r.indexOf('|', p5 + 1);
    if (p5 > 0 && p6 > 0) return r.substring(p5 + 1, p6);
    return "192.168.4.1";
}

String DSO152_WiFiClass::SSID() {
    _refreshStatus();
    return _cached_ssid;
}

int32_t DSO152_WiFiClass::RSSI() {
    _refreshStatus();
    return _cached_rssi;
}

String DSO152_WiFiClass::macAddress() {
    String r = sendCommand("AT+STATUS", 800);
    int lastPipe = r.lastIndexOf('|');
    if (lastPipe > 0) return r.substring(lastPipe + 1);
    return "00:00:00:00:00:00";
}

int16_t DSO152_WiFiClass::scanNetworks() {
    if (!_modem_init) initModem(57600);
    _scan_count = 0;
    // Reply format: OK|<count>|<ssid_hex>,<rssi>,<ch>,<enc>;...
    String r = sendCommand("AT+SCAN", 9000);
    int okPos = r.indexOf("OK|");
    if (okPos < 0) return 0;
    if (okPos > 0) r = r.substring(okPos);

    int p1 = r.indexOf('|');
    int p2 = r.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) return 0;

    String list = r.substring(p2 + 1);
    int start = 0;
    while (start < (int)list.length() && _scan_count < MAX_SCAN_ITEMS) {
        int semi = list.indexOf(';', start);
        String item = (semi >= 0) ? list.substring(start, semi) : list.substring(start);
        int c1 = item.indexOf(',');
        int c2 = item.indexOf(',', c1 + 1);
        int c3 = item.indexOf(',', c2 + 1);
        if (c1 > 0 && c2 > 0 && c3 > 0) {
            _scan_ssid[_scan_count] = fromHex(item.substring(0, c1));
            _scan_rssi[_scan_count] = item.substring(c1 + 1, c2).toInt();
            _scan_ch[_scan_count]   = item.substring(c2 + 1, c3).toInt();
            _scan_enc[_scan_count]  = (uint8_t)item.substring(c3 + 1).toInt();
            _scan_count++;
        }
        if (semi < 0) break;
        start = semi + 1;
    }
    return _scan_count;
}

String DSO152_WiFiClass::SSID(uint8_t i) const {
    return (i < _scan_count) ? _scan_ssid[i] : "";
}

int32_t DSO152_WiFiClass::RSSI(uint8_t i) const {
    return (i < _scan_count) ? _scan_rssi[i] : 0;
}

int32_t DSO152_WiFiClass::channel(uint8_t i) const {
    return (i < _scan_count) ? _scan_ch[i] : 0;
}

uint8_t DSO152_WiFiClass::encryptionType(uint8_t i) const {
    return (i < _scan_count) ? _scan_enc[i] : 0;
}

int32_t DSO152_WiFiClass::ping(const char *host, uint16_t port) {
    String cmd = "AT+HOSTPING=" + String(host) + "," + String(port);
    String r = sendCommand(cmd.c_str(), 3500);
    if (r.startsWith("OK|")) {
        return r.substring(3).toInt();
    }
    return -1;
}

String DSO152_WiFiClass::getNTPTime(long gmtOffset_sec) {
    String cmd = "AT+NTP=" + String(gmtOffset_sec);
    String r = sendCommand(cmd.c_str(), 4000);
    if (r.startsWith("OK|")) {
        return r.substring(3);
    }
    return "NTP Sync Pending";
}

/* ============================================================================
 * Interactive On-Screen Wi-Fi Picker & Virtual Keyboard on the DSO-152 LCD
 * ============================================================================ */
static const char* const KB_ROWS[4] = {
    "1234567890-=",
    "qwertyuiop[]",
    "asdfghjkl;._",
    "zxcvbnm!@#$?"
};

static bool enterPasswordOnScreen(const String &ssid, String &outPass) {
    outPass = "";
    int curR = 1, curC = 0;
    bool shift = false;
    DSO152_Buttons prev = dso.readButtons();

    auto redraw = [&]() {
        dso.fillScreen(COLOR_BLACK);
        dso.fillRect(0, 0, 320, 22, COLOR_NAVY);
        String title = "WPA2 PASS: " + ssid.substring(0, 18);
        dso.drawString(6, 7, title.c_str(), COLOR_YELLOW, COLOR_NAVY, 1);

        // Password box
        dso.drawRect(10, 30, 300, 24, COLOR_CYAN);
        dso.fillRect(11, 31, 298, 22, COLOR_DARKGRAY);
        dso.drawString(16, 38, outPass.c_str(), COLOR_WHITE, COLOR_DARKGRAY, 1);

        // 4x12 character grid
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 12; c++) {
                char ch = KB_ROWS[r][c];
                if (shift && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
                bool sel = (r == curR && c == curC);
                int16_t x = 14 + c * 24;
                int16_t y = 64 + r * 28;
                uint16_t bg = sel ? COLOR_GREEN : 0x18E3;
                uint16_t fg = sel ? COLOR_BLACK : COLOR_WHITE;
                dso.fillRect(x, y, 21, 24, bg);
                dso.drawCharFast(x + 6, y + 8, ch, fg, bg, 1);
            }
        }

        // Bottom row action buttons (curR == 4, curC = 0..3)
        const char* actions[4] = { shift ? "CAPS:ON" : "CAPS:off", " SPACE ", " [DEL] ", "CONNECT" };
        for (int a = 0; a < 4; a++) {
            bool sel = (curR == 4 && curC == a);
            int16_t x = 14 + a * 74;
            int16_t y = 180;
            uint16_t bg = sel ? COLOR_YELLOW : (a == 3 ? COLOR_BLUE : COLOR_DARKGRAY);
            uint16_t fg = sel ? COLOR_BLACK : COLOR_WHITE;
            dso.fillRect(x, y, 68, 24, bg);
            dso.drawRect(x, y, 68, 24, COLOR_GRAY);
            dso.drawString(x + 6, y + 8, actions[a], fg, bg, 1);
        }

        dso.drawString(8, 218, "D-Pad:Move  OK:Select  RUN:Connect  MODE:Back", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    };

    redraw();
    while (true) {
        dso.updatePowerButton(false);
        DSO152_Buttons b = dso.readButtons();
        bool changed = false;

        if (b.up && !prev.up) {
            if (curR > 0) { curR--; if (curR == 3 && curC > 11) curC = 11; changed = true; }
        }
        if (b.down && !prev.down) {
            if (curR < 4) {
                curR++;
                if (curR == 4) curC = curC / 3;
                changed = true;
            }
        }
        if (b.left && !prev.left) {
            int maxC = (curR == 4) ? 3 : 11;
            curC = (curC > 0) ? (curC - 1) : maxC;
            changed = true;
        }
        if (b.right && !prev.right) {
            int maxC = (curR == 4) ? 3 : 11;
            curC = (curC < maxC) ? (curC + 1) : 0;
            changed = true;
        }
        if (b.mode && !prev.mode) {
            return false; // Cancel
        }
        if (b.run && !prev.run) {
            return true;  // Connect immediately
        }
        if (b.ok && !prev.ok) {
            if (curR < 4) {
                char ch = KB_ROWS[curR][curC];
                if (shift && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
                if (outPass.length() < 34) outPass += ch;
                changed = true;
            } else {
                if (curC == 0) { shift = !shift; changed = true; }
                else if (curC == 1) { if (outPass.length() < 34) outPass += ' '; changed = true; }
                else if (curC == 2) { if (outPass.length() > 0) outPass.remove(outPass.length() - 1); changed = true; }
                else if (curC == 3) { return true; }
            }
        }

        prev = b;
        if (changed) redraw();
        delay(15);
    }
}

bool DSO152_WiFiClass::selectNetworkUI() {
    if (!_modem_init) initModem(57600);

    while (true) {
        dso.fillScreen(COLOR_BLACK);
        dso.fillRect(0, 0, 320, 24, COLOR_NAVY);
        dso.drawString(8, 8, "DSO-152 WI-FI CLIENT MANAGER", COLOR_WHITE, COLOR_NAVY, 1);
        dso.drawString(16, 85, "Checking ESP32-C3 UART link (PA13/14)...", COLOR_CYAN, COLOR_BLACK, 1);
        bool modemOk = isModemReady();
        dso.drawString(16, 105, modemOk ? "Modem ONLINE! Scanning 2.4GHz Wi-Fi..." : "No Ping reply, trying AT+SCAN...",
                       modemOk ? COLOR_GREEN : COLOR_YELLOW, COLOR_BLACK, 1);

        int16_t count = scanNetworks();
        if (count > 0) modemOk = true;
        int sel = 0;
        DSO152_Buttons prev = dso.readButtons();

        auto drawList = [&]() {
            dso.fillRect(0, 24, 320, 190, COLOR_BLACK);
            if (count <= 0) {
                if (modemOk) {
                    dso.drawString(12, 55, "ESP32-C3 Modem : ONLINE (UART OK!)", COLOR_GREEN, COLOR_BLACK, 1);
                    dso.drawString(12, 75, ("ESP32 MAC      : " + macAddress()).c_str(), COLOR_CYAN, COLOR_BLACK, 1);
                    dso.drawString(12, 105, "Scan returned 0 2.4GHz networks.", COLOR_YELLOW, COLOR_BLACK, 1);
                    dso.drawString(12, 125, "Press [RUN] to scan again.", COLOR_WHITE, COLOR_BLACK, 1);
                } else {
                    dso.drawString(12, 50, "ESP32-C3 Modem : NO REPLY (Timeout)", COLOR_RED, COLOR_BLACK, 1);
                    dso.drawString(12, 78, "Check 4-wire SWD header connection:", COLOR_YELLOW, COLOR_BLACK, 1);
                    dso.drawString(12, 98, "  DSO DIO (PA13) -> ESP32 RX (GPIO20)", COLOR_WHITE, COLOR_BLACK, 1);
                    dso.drawString(12, 114, "  DSO CLK (PA14) -> ESP32 TX (GPIO21)", COLOR_WHITE, COLOR_BLACK, 1);
                    dso.drawString(12, 130, "  DSO 3.3V & GND -> ESP32 3V3 & GND", COLOR_WHITE, COLOR_BLACK, 1);
                    dso.drawString(12, 156, "Press [RUN] to retry.", COLOR_CYAN, COLOR_BLACK, 1);
                }
            } else {
                int showCount = (count > 7) ? 7 : count;
                for (int i = 0; i < showCount; i++) {
                    int16_t y = 30 + i * 24;
                    uint16_t bg = (i == sel) ? COLOR_BLUE : COLOR_BLACK;
                    uint16_t fg = (i == sel) ? COLOR_YELLOW : COLOR_WHITE;
                    dso.fillRect(6, y, 308, 22, bg);
                    char rowBuf[40];
                    snprintf(rowBuf, sizeof(rowBuf), "%c %-20.20s %4lddBm %s",
                             (i == sel) ? '>' : ' ',
                             _scan_ssid[i].c_str(),
                             (long)_scan_rssi[i],
                             _scan_enc[i] ? "*" : " ");
                    dso.drawString(8, y + 7, rowBuf, fg, bg, 1);
                }
            }
            dso.fillRect(0, 214, 320, 26, COLOR_DARKGRAY);
            dso.drawString(6, 223, "OK:Pick RUN:Rescan AUTO:Saved MODE:Exit", COLOR_CYAN, COLOR_DARKGRAY, 1);
        };

        drawList();
        bool rescan = false;
        while (!rescan) {
            dso.updatePowerButton(false);
            DSO152_Buttons b = dso.readButtons();

            if (b.up && !prev.up && sel > 0) { sel--; drawList(); }
            if (b.down && !prev.down && sel + 1 < count && sel < 6) { sel++; drawList(); }
            if (b.run && !prev.run) { rescan = true; }
            if (b.mode && !prev.mode) { return (_cached_status == WL_CONNECTED); }
            if (b.auto_btn && !prev.auto_btn) {
                dso.fillRect(30, 90, 260, 50, COLOR_NAVY);
                dso.drawRect(30, 90, 260, 50, COLOR_WHITE);
                dso.drawString(45, 111, "Connecting to saved Wi-Fi...", COLOR_YELLOW, COLOR_NAVY, 1);
                if (beginSaved() == WL_CONNECTED) return true;
                drawList();
            }
            if (b.ok && !prev.ok && count > 0) {
                String chosenSSID = _scan_ssid[sel];
                String pass = "";
                bool proceed = true;
                if (_scan_enc[sel] != 0) {
                    proceed = enterPasswordOnScreen(chosenSSID, pass);
                }
                if (proceed) {
                    dso.fillScreen(COLOR_BLACK);
                    dso.fillRect(20, 85, 280, 60, COLOR_NAVY);
                    dso.drawRect(20, 85, 280, 60, COLOR_WHITE);
                    dso.drawString(36, 102, "Connecting to Wi-Fi...", COLOR_YELLOW, COLOR_NAVY, 1);
                    dso.drawString(36, 120, chosenSSID.c_str(), COLOR_WHITE, COLOR_NAVY, 1);
                    if (begin(chosenSSID.c_str(), pass.c_str()) == WL_CONNECTED) {
                        return true;
                    }
                    dso.fillRect(20, 85, 280, 60, COLOR_RED);
                    dso.drawString(46, 111, "Connection Failed!", COLOR_WHITE, COLOR_RED, 1);
                    delay(1200);
                }
                drawList();
            }
            prev = b;
            delay(15);
        }
    }
}

/* ============================================================================
 * DSO152_WiFiClient Implementation
 * ============================================================================ */
DSO152_WiFiClient::DSO152_WiFiClient() {
    _connected = false;
    _rx_buffer = "";
}

bool DSO152_WiFiClient::connect(const char *host, uint16_t port, bool ssl) {
    _rx_buffer = "";
    String cmd = "AT+TCPCONN=" + String(host) + "," + String(port) + "," + String(ssl ? 1 : 0);
    String r = WiFi.sendCommand(cmd.c_str(), 6000);
    _connected = r.startsWith("OK|1");
    return _connected;
}

size_t DSO152_WiFiClient::write(const uint8_t *buf, size_t size) {
    if (!_connected || !buf || size == 0) return 0;
    String cmd = "AT+TCPSEND=" + bytesToHex(buf, size);
    String r = WiFi.sendCommand(cmd.c_str(), 3000);
    if (r.startsWith("OK|")) {
        return (size_t)r.substring(3).toInt();
    }
    _connected = false;
    return 0;
}

size_t DSO152_WiFiClient::print(const char *str) {
    if (!str) return 0;
    return write((const uint8_t*)str, strlen(str));
}

size_t DSO152_WiFiClient::println(const char *str) {
    String s = String(str ? str : "") + "\r\n";
    return write((const uint8_t*)s.c_str(), s.length());
}

void DSO152_WiFiClient::_fetchSocketData(uint32_t timeout_ms) {
    String cmd = "AT+TCPRECV=512," + String(timeout_ms);
    String r = WiFi.sendCommand(cmd.c_str(), timeout_ms + 1000);
    // Reply: OK|<connected>|<data_hex>
    if (!r.startsWith("OK|")) return;
    int p1 = r.indexOf('|');
    int p2 = r.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) return;
    _connected = (r.substring(p1 + 1, p2).toInt() != 0);
    String hexData = r.substring(p2 + 1);
    if (hexData.length() >= 2) {
        _rx_buffer += fromHex(hexData);
    }
}

int DSO152_WiFiClient::available() {
    if (_rx_buffer.length() == 0 && _connected) {
        _fetchSocketData(100);
    }
    return (int)_rx_buffer.length();
}

int DSO152_WiFiClient::read() {
    if (available() <= 0) return -1;
    char c = _rx_buffer[0];
    _rx_buffer.remove(0, 1);
    return (uint8_t)c;
}

String DSO152_WiFiClient::readString(uint32_t timeout_ms) {
    _fetchSocketData(timeout_ms);
    String out = _rx_buffer;
    _rx_buffer = "";
    return out;
}

String DSO152_WiFiClient::readStringUntil(char terminator, uint32_t timeout_ms) {
    int idx = _rx_buffer.indexOf(terminator);
    if (idx < 0 && _connected) {
        _fetchSocketData(timeout_ms);
        idx = _rx_buffer.indexOf(terminator);
    }
    if (idx >= 0) {
        String out = _rx_buffer.substring(0, idx);
        _rx_buffer.remove(0, idx + 1);
        return out;
    }
    String out = _rx_buffer;
    _rx_buffer = "";
    return out;
}

bool DSO152_WiFiClient::connected() {
    if (_rx_buffer.length() > 0) return true;
    _fetchSocketData(20);
    return _connected || (_rx_buffer.length() > 0);
}

void DSO152_WiFiClient::stop() {
    WiFi.sendCommand("AT+TCPCLOSE", 500);
    _connected = false;
    _rx_buffer = "";
}

/* ============================================================================
 * DSO152_HTTPClient Implementation
 * ============================================================================ */
DSO152_HTTPClient::DSO152_HTTPClient() {
    _url = "";
    _content_type = "application/json";
    _auth_header = "";
    _response_body = "";
    _last_code = -1;
}

bool DSO152_HTTPClient::begin(const char *url) {
    _url = url ? url : "";
    _response_body = "";
    _last_code = -1;
    return _url.length() > 0;
}

void DSO152_HTTPClient::addHeader(const char *name, const char *value) {
    if (!name || !value) return;
    if (strcasecmp(name, "Content-Type") == 0) {
        _content_type = value;
    } else if (strcasecmp(name, "Authorization") == 0) {
        _auth_header = value;
    }
}

int DSO152_HTTPClient::GET() {
    if (_url.length() == 0) return -1;
    String cmd = "AT+HTTPGET=" + toHex(_url.c_str());
    String r = WiFi.sendCommand(cmd.c_str(), 10000);
    // Reply: OK|<http_code>|<body_hex>
    if (!r.startsWith("OK|")) return -1;
    int p1 = r.indexOf('|');
    int p2 = r.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) return -1;
    _last_code = r.substring(p1 + 1, p2).toInt();
    _response_body = fromHex(r.substring(p2 + 1));
    return _last_code;
}

int DSO152_HTTPClient::POST(const char *payload) {
    if (_url.length() == 0) return -1;
    String cmd = "AT+HTTPPOST=" + toHex(_url.c_str()) + "," +
                 toHex(_content_type.c_str()) + "," +
                 toHex(payload ? payload : "");
    String r = WiFi.sendCommand(cmd.c_str(), 10000);
    if (!r.startsWith("OK|")) return -1;
    int p1 = r.indexOf('|');
    int p2 = r.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) return -1;
    _last_code = r.substring(p1 + 1, p2).toInt();
    _response_body = fromHex(r.substring(p2 + 1));
    return _last_code;
}

void DSO152_HTTPClient::end() {
    _url = "";
}

/* Global Pre-Instantiated WiFi Object */
DSO152_WiFiClass WiFi;
