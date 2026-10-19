/**
 * @file esp32c3_wifi_companion.ino
 * @brief Universal Wi-Fi / HTTPS / TCP / NTP Modem Bridge for FNIRSI DSO-152 (ESP32-C3 SuperMini)
 *
 * FLASH THIS ONCE TO THE ESP32-C3 SUPERMINI!
 * All Wi-Fi connections, scans, HTTP/HTTPS requests, TCP/SSL sockets, and NTP sync
 * are controlled dynamically from your DSO-152 Arduino sketches via <DSO152_WiFi.h>.
 *
 * USB Serial Monitor Debug (115200 baud):
 *   - IMPORTANT: On ESP32-C3 SuperMini, enable Tools -> USB CDC On Boot: "Enabled"
 *     in the Arduino IDE if you want to view live debug logs over the USB-C port!
 *   - Prints boot diagnostics, a startup Wi-Fi scan self-test, a 3-second heartbeat,
 *     and every AT command/response exchanged with the DSO-152.
 *   - You can also type AT commands (e.g. AT+PING, AT+SCAN, AT+STATUS) directly into
 *     the Arduino IDE Serial Monitor to test the ESP32-C3 independently.
 *
 * 4-Wire Connection to FNIRSI DSO-152 SWD Pads:
 *   ESP32-C3 SuperMini       DSO-152 SWD Header Pad
 *   ------------------       ----------------------
 *   3V3                  <-  VCC (3.3V, switched by DSO-152 power latch!)
 *   GND (G)              <-  GND
 *   RX  (GPIO20)         <-  DIO (PA13 / DSO TX)
 *   TX  (GPIO21)         ->  CLK (PA14 / DSO RX)
 */

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

static const int PIN_DSO_RX = 20; // GPIO20 (RX on SuperMini) <- DSO PA13 (DIO)
static const int PIN_DSO_TX = 21; // GPIO21 (TX on SuperMini) -> DSO PA14 (CLK)
static const int PIN_LED    = 8;  // GPIO8  (Onboard Blue LED on SuperMini, active-low)

Preferences prefs;
WebServer server(80);
bool portalRunning = false;

// Active TCP / SSL client socket for DSO152_WiFiClient
WiFiClient tcpClient;
WiFiClientSecure sslClient;
bool activeSocketIsSSL = false;

// Debug counters
static uint32_t dbgRxBytes   = 0;
static uint32_t dbgCmdCount  = 0;
static uint32_t lastBeatMs   = 0;

// ---------------------------------------------------------------------------
// Hex Encoding / Decoding Helpers
// ---------------------------------------------------------------------------
static const char HEX_LUT[] = "0123456789ABCDEF";

static String toHex(const String &s) {
  String out;
  out.reserve(s.length() * 2);
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t b = (uint8_t)s[i];
    out += HEX_LUT[(b >> 4) & 0x0F];
    out += HEX_LUT[b & 0x0F];
  }
  return out;
}

static String bytesToHex(const uint8_t *buf, size_t len) {
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    uint8_t b = buf[i];
    out += HEX_LUT[(b >> 4) & 0x0F];
    out += HEX_LUT[b & 0x0F];
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

static const char* wifiStatusName(wl_status_t st) {
  switch (st) {
    case WL_IDLE_STATUS:     return "WL_IDLE";
    case WL_NO_SSID_AVAIL:   return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:       return "WL_CONNECTED";
    case WL_CONNECT_FAILED:  return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:    return "WL_DISCONNECTED";
    default:                 return "WL_UNKNOWN";
  }
}

static void sendReply(const String &line) {
  Serial1.print(line);
  Serial1.print('\n');
  Serial1.flush();
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));

  Serial.print("[DSO-TX <- ESP32] (");
  Serial.print(line.length());
  Serial.print(" bytes): ");
  if (line.length() > 160) {
    Serial.println(line.substring(0, 160) + "...");
  } else {
    Serial.println(line);
  }
}

static String buildStatusString() {
  int st = (int)WiFi.status();
  String ip = (st == WL_CONNECTED) ? WiFi.localIP().toString() : "0.0.0.0";
  String ssid = (st == WL_CONNECTED) ? WiFi.SSID() : "";
  long rssi = (st == WL_CONNECTED) ? WiFi.RSSI() : 0;
  String apIp = WiFi.softAPIP().toString();
  String mac = WiFi.macAddress();
  return "OK|" + String(st) + "|" + ip + "|" + ssid + "|" + String(rssi) + "|" + apIp + "|" + mac;
}

// ---------------------------------------------------------------------------
// Reliable Wi-Fi Scan Helper (handles stuck STA_CONNECTING state on ESP32-C3)
// ---------------------------------------------------------------------------
static int performWiFiScan() {
  Serial.println("[WIFI-SCAN] Starting 2.4GHz Wi-Fi scan...");

  // Ensure STA interface is enabled
  if (!portalRunning && WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    delay(50);
  }

  // CRITICAL on ESP32 Arduino Core: if WiFi.begin() was previously called and is still
  // trying to connect in the background (status != WL_CONNECTED), WiFi.scanNetworks()
  // immediately fails with -1 (WIFI_SCAN_FAILED). Disconnect any pending attempt first!
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(false, false);
    delay(80);
  }

  WiFi.scanDelete();
  uint32_t t0 = millis();
  // Active scan, show_hidden=false, passive=false, max_ms_per_chan=200ms
  int n = WiFi.scanNetworks(false, false, false, 200);
  if (n < 0) {
    Serial.printf("[WIFI-SCAN] First scan returned %d (busy/failed), resetting STA and retrying...\n", n);
    WiFi.disconnect(true, false);
    delay(150);
    WiFi.mode(portalRunning ? WIFI_AP_STA : WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    delay(100);
    n = WiFi.scanNetworks(false, false, false, 250);
  }

  uint32_t dt = millis() - t0;
  Serial.printf("[WIFI-SCAN] Completed in %lu ms -> found %d networks\n", (unsigned long)dt, n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  [%02d] SSID='%s' | RSSI=%d dBm | Ch=%d | Auth=%s\n",
                  i + 1,
                  WiFi.SSID(i).c_str(),
                  (int)WiFi.RSSI(i),
                  (int)WiFi.channel(i),
                  (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURED");
  }
  return n;
}

// ---------------------------------------------------------------------------
// Captive Web Configuration Portal (http://192.168.4.1)
// ---------------------------------------------------------------------------
static void startWebConfigPortal(const String &apSsid) {
  Serial.printf("[PORTAL] Starting SoftAP '%s' at http://192.168.4.1\n", apSsid.c_str());
  WiFi.mode(WIFI_AP_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.softAP(apSsid.c_str());

  if (!portalRunning) {
    server.on("/", HTTP_GET, []() {
      String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<style>body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:20px;max-width:420px;margin:auto}"
                    "input,button{width:100%;padding:10px;margin:8px 0;border-radius:6px;border:1px solid #334155;background:#1e293b;color:#fff}"
                    "button{background:#0284c7;font-weight:bold;cursor:pointer}</style></head><body>"
                    "<h2>FNIRSI DSO-152 Wi-Fi Setup</h2>"
                    "<form action='/save' method='POST'>"
                    "<label>Wi-Fi SSID:</label><input name='s' required>"
                    "<label>Password:</label><input name='p' type='password'>"
                    "<button type='submit'>Save & Connect</button></form></body></html>";
      server.send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, []() {
      String s = server.arg("s");
      String p = server.arg("p");
      Serial.printf("[PORTAL] Received credentials via Web UI -> SSID='%s'\n", s.c_str());
      prefs.begin("dso_wifi", false);
      prefs.putString("ssid", s);
      prefs.putString("pass", p);
      prefs.end();
      server.send(200, "text/html", "<h3>Saved! Connecting to " + s + "... Check your DSO-152 screen.</h3>");
      delay(300);
      WiFi.begin(s.c_str(), p.c_str());
    });

    server.begin();
    portalRunning = true;
  }
}

// ---------------------------------------------------------------------------
// Command Dispatcher for <DSO152_WiFi.h>
// ---------------------------------------------------------------------------
static void processATCommand(const String &cmd) {
  dbgCmdCount++;
  Serial.printf("\n[DSO-RX -> ESP32] Cmd #%lu: '%s'\n", (unsigned long)dbgCmdCount, cmd.c_str());

  if (cmd == "AT+PING") {
    sendReply("OK|PONG");
    return;
  }

  if (cmd == "AT+STATUS") {
    sendReply(buildStatusString());
    return;
  }

  if (cmd == "AT+DISC") {
    WiFi.disconnect(false);
    sendReply("OK");
    return;
  }

  if (cmd.startsWith("AT+BEGIN=")) {
    int comma = cmd.indexOf(',', 9);
    if (comma < 0) { sendReply("ERR"); return; }
    String ssid = fromHex(cmd.substring(9, comma));
    String pass = fromHex(cmd.substring(comma + 1));

    Serial.printf("[WIFI] Connecting to SSID='%s'...\n", ssid.c_str());
    prefs.begin("dso_wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    WiFi.mode(portalRunning ? WIFI_AP_STA : WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    WiFi.disconnect(false);
    delay(50);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 9500) {
      delay(100);
      Serial.print(".");
    }
    Serial.println();
    Serial.printf("[WIFI] Result: %s (IP=%s)\n",
                  wifiStatusName(WiFi.status()),
                  WiFi.localIP().toString().c_str());
    sendReply(buildStatusString());
    return;
  }

  if (cmd == "AT+BEGINSAVED") {
    prefs.begin("dso_wifi", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    prefs.end();

    if (ssid.length() == 0) {
      Serial.println("[WIFI] No saved SSID in NVS preferences.");
      sendReply("ERR|NO_SAVED_WIFI");
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf("[WIFI] Reconnecting to saved SSID='%s'...\n", ssid.c_str());
      WiFi.mode(portalRunning ? WIFI_AP_STA : WIFI_STA);
      WiFi.setTxPower(WIFI_POWER_11dBm);
      WiFi.begin(ssid.c_str(), pass.c_str());
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 7500) {
        delay(100);
      }
    }
    sendReply(buildStatusString());
    return;
  }

  if (cmd.startsWith("AT+AP=")) {
    int comma = cmd.indexOf(',', 6);
    String ssid = (comma > 0) ? fromHex(cmd.substring(6, comma)) : fromHex(cmd.substring(6));
    String pass = (comma > 0) ? fromHex(cmd.substring(comma + 1)) : "";
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    if (pass.length() >= 8) WiFi.softAP(ssid.c_str(), pass.c_str());
    else WiFi.softAP(ssid.c_str());
    sendReply("OK|" + WiFi.softAPIP().toString());
    return;
  }

  if (cmd.startsWith("AT+PORTAL=")) {
    String apSsid = fromHex(cmd.substring(10));
    startWebConfigPortal(apSsid);
    sendReply("OK|" + WiFi.softAPIP().toString());
    return;
  }

  if (cmd == "AT+SCAN") {
    int n = performWiFiScan();
    int count = (n > 10) ? 10 : (n < 0 ? 0 : n);
    String out = "OK|" + String(count) + "|";
    for (int i = 0; i < count; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 24) ssid = ssid.substring(0, 24);
      int enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? 0 : 1;
      out += toHex(ssid) + "," + String(WiFi.RSSI(i)) + "," +
             String(WiFi.channel(i)) + "," + String(enc);
      if (i + 1 < count) out += ";";
    }
    WiFi.scanDelete();
    sendReply(out);
    return;
  }

  if (cmd.startsWith("AT+HOSTPING=")) {
    int comma = cmd.indexOf(',', 12);
    String host = (comma > 0) ? cmd.substring(12, comma) : cmd.substring(12);
    uint16_t port = (comma > 0) ? (uint16_t)cmd.substring(comma + 1).toInt() : 80;
    WiFiClient testClient;
    uint32_t t0 = millis();
    if (testClient.connect(host.c_str(), port, 2500)) {
      uint32_t dt = millis() - t0;
      testClient.stop();
      sendReply("OK|" + String(dt));
    } else {
      sendReply("ERR|-1");
    }
    return;
  }

  if (cmd.startsWith("AT+NTP=")) {
    long offsetSec = cmd.substring(7).toInt();
    configTime(offsetSec, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 3000)) {
      char buf[32];
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
      sendReply("OK|" + String(buf));
    } else {
      sendReply("ERR|NTP_TIMEOUT");
    }
    return;
  }

  if (cmd.startsWith("AT+HTTPGET=")) {
    String url = fromHex(cmd.substring(11));
    Serial.printf("[HTTP] GET %s\n", url.c_str());
    HTTPClient http;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    bool ok = false;
    if (url.startsWith("https://")) {
      secureClient.setInsecure();
      ok = http.begin(secureClient, url);
    } else {
      ok = http.begin(plainClient, url);
    }
    if (!ok) { sendReply("ERR|-1|"); return; }

    http.setTimeout(7000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    String body = (code > 0) ? http.getString() : "";
    if (body.length() > 650) body = body.substring(0, 650);
    http.end();
    sendReply("OK|" + String(code) + "|" + toHex(body));
    return;
  }

  if (cmd.startsWith("AT+HTTPPOST=")) {
    int c1 = cmd.indexOf(',', 12);
    int c2 = cmd.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) { sendReply("ERR"); return; }
    String url   = fromHex(cmd.substring(12, c1));
    String ctype = fromHex(cmd.substring(c1 + 1, c2));
    String body  = fromHex(cmd.substring(c2 + 1));

    Serial.printf("[HTTP] POST %s (%u bytes)\n", url.c_str(), (unsigned)body.length());
    HTTPClient http;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    bool ok = false;
    if (url.startsWith("https://")) {
      secureClient.setInsecure();
      ok = http.begin(secureClient, url);
    } else {
      ok = http.begin(plainClient, url);
    }
    if (!ok) { sendReply("ERR|-1|"); return; }

    http.addHeader("Content-Type", ctype);
    int code = http.POST(body);
    String resp = (code > 0) ? http.getString() : "";
    if (resp.length() > 650) resp = resp.substring(0, 650);
    http.end();
    sendReply("OK|" + String(code) + "|" + toHex(resp));
    return;
  }

  if (cmd.startsWith("AT+TCPCONN=")) {
    int c1 = cmd.indexOf(',', 11);
    int c2 = cmd.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) { sendReply("ERR|0"); return; }
    String host = cmd.substring(11, c1);
    uint16_t port = (uint16_t)cmd.substring(c1 + 1, c2).toInt();
    bool ssl = (cmd.substring(c2 + 1).toInt() != 0);

    Serial.printf("[TCP] Connecting to %s:%u (SSL=%d)...\n", host.c_str(), port, (int)ssl);
    tcpClient.stop();
    sslClient.stop();
    activeSocketIsSSL = ssl;

    bool connected = false;
    if (ssl) {
      sslClient.setInsecure();
      connected = sslClient.connect(host.c_str(), port, 4500);
    } else {
      connected = tcpClient.connect(host.c_str(), port, 4500);
    }
    sendReply(connected ? "OK|1" : "ERR|0");
    return;
  }

  if (cmd.startsWith("AT+TCPSEND=")) {
    String data = fromHex(cmd.substring(11));
    WiFiClient &c = activeSocketIsSSL ? (WiFiClient&)sslClient : tcpClient;
    if (!c.connected()) { sendReply("ERR|0"); return; }
    size_t n = c.write((const uint8_t*)data.c_str(), data.length());
    sendReply("OK|" + String(n));
    return;
  }

  if (cmd.startsWith("AT+TCPRECV=")) {
    int comma = cmd.indexOf(',', 11);
    int maxBytes = (comma > 0) ? cmd.substring(11, comma).toInt() : 512;
    uint32_t timeoutMs = (comma > 0) ? cmd.substring(comma + 1).toInt() : 500;
    if (maxBytes > 600) maxBytes = 600;

    WiFiClient &c = activeSocketIsSSL ? (WiFiClient&)sslClient : tcpClient;
    uint32_t t0 = millis();
    while (c.available() == 0 && c.connected() && (millis() - t0) < timeoutMs) {
      delay(5);
    }

    uint8_t buf[600];
    int avail = c.available();
    int toRead = (avail > maxBytes) ? maxBytes : avail;
    int got = (toRead > 0) ? c.read(buf, toRead) : 0;
    int isConn = (c.connected() || c.available() > 0) ? 1 : 0;
    sendReply("OK|" + String(isConn) + "|" + bytesToHex(buf, got > 0 ? got : 0));
    return;
  }

  if (cmd == "AT+TCPCLOSE") {
    tcpClient.stop();
    sslClient.stop();
    sendReply("OK");
    return;
  }

  Serial.printf("[WARN] Unrecognized AT command: '%s'\n", cmd.c_str());
  sendReply("ERR|UNKNOWN_CMD");
}

static void handleIncomingLine(String line, const char *sourceTag) {
  line.trim();
  if (line.length() == 0) return;

  // Tolerate any leading pin-mux/power-on glitch byte before "AT+"
  int atPos = line.indexOf("AT+");
  if (atPos >= 0) {
    if (atPos > 0) {
      Serial.printf("[%s] Stripped %d leading noise bytes before AT+\n", sourceTag, atPos);
    }
    processATCommand(line.substring(atPos));
    return;
  }

  int scanPos = line.indexOf("$SCAN");
  if (scanPos >= 0) {
    int n = performWiFiScan();
    String out = "$NETS|";
    int count = (n > 8) ? 8 : (n < 0 ? 0 : n);
    for (int i = 0; i < count; i++) {
      String s = WiFi.SSID(i);
      s.replace(",", " "); s.replace(";", " ");
      if (s.length() > 16) s = s.substring(0, 16);
      out += s + "," + String(WiFi.RSSI(i)) + "," + String(WiFi.channel(i));
      if (i + 1 < count) out += ";";
    }
    WiFi.scanDelete();
    sendReply(out);
    return;
  }

  // Log unexpected/garbled UART input in both ASCII and Hex to diagnose baud/wiring
  Serial.printf("[%s] Ignored non-AT input (%u bytes): ASCII='%s' HEX=",
                sourceTag, (unsigned)line.length(), line.c_str());
  for (size_t i = 0; i < line.length(); i++) {
    Serial.printf("%02X ", (uint8_t)line[i]);
  }
  Serial.println();
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW); // LED ON during setup

  Serial.begin(115200);
  // Wait briefly so USB CDC Serial Monitor can attach on boot without blocking standalone operation
  uint32_t usbWait = millis();
  while (!Serial && (millis() - usbWait) < 1500) {
    delay(10);
  }

  // Configure Serial1 on GPIO20 (RX <- DSO PA13/DIO) & GPIO21 (TX -> DSO PA14/CLK)
  // Using SERIAL_8N2 adds an extra stop bit on ESP32 TX so the DSO-152's bit-banged
  // UART receiver has 34.7us of idle HIGH time between back-to-back bytes!
  pinMode(PIN_DSO_RX, INPUT_PULLUP);
  Serial1.setRxBufferSize(2048);
  Serial1.begin(57600, SERIAL_8N2, PIN_DSO_RX, PIN_DSO_TX);

  Serial.println("\n============================================================");
  Serial.println(" FNIRSI DSO-152 Wi-Fi Companion Modem (ESP32-C3 SuperMini)");
  Serial.println("============================================================");
  Serial.printf(" UART Bridge : 57600 8N2 | RX=GPIO%d (<- DSO DIO/PA13) | TX=GPIO%d (-> DSO CLK/PA14)\n",
                PIN_DSO_RX, PIN_DSO_TX);

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm); // Prevents 3.3V LDO brownout on DSO-152!
  WiFi.setAutoReconnect(false);      // Don't block scans with background reconnect loops

  Serial.printf(" MAC Address : %s\n", WiFi.macAddress().c_str());

  // Run a quick self-test Wi-Fi scan on boot and print to USB Serial
  performWiFiScan();

  prefs.begin("dso_wifi", true);
  String savedSsid = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();

  if (savedSsid.length() > 0) {
    Serial.printf("[BOOT] Found saved Wi-Fi SSID='%s', attempting initial connection (4s timeout)...\n",
                  savedSsid.c_str());
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 4000) {
      delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[BOOT] Saved Wi-Fi not in range right now; disconnecting so scanner stays ready.");
      WiFi.disconnect(false);
    } else {
      Serial.printf("[BOOT] Connected! IP=%s\n", WiFi.localIP().toString().c_str());
    }
  } else {
    Serial.println("[BOOT] No saved Wi-Fi in NVS. Ready for commands from DSO-152 or USB Serial.");
  }

  digitalWrite(PIN_LED, HIGH); // LED OFF (blinks on every packet)
  lastBeatMs = millis();
}

static String serial1Buf;
static String usbSerialBuf;

void loop() {
  if (portalRunning) {
    server.handleClient();
  }

  // 1. Read incoming bytes from DSO-152 over Serial1 (GPIO20/21)
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    dbgRxBytes++;
    if (c == '\n' || c == '\r') {
      if (serial1Buf.length() > 0) {
        handleIncomingLine(serial1Buf, "UART-DSO");
        serial1Buf = "";
      }
    } else if (serial1Buf.length() < 1500) {
      serial1Buf += c;
    }
  }

  // 2. Also allow typing AT commands directly in the Arduino IDE USB Serial Monitor!
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (usbSerialBuf.length() > 0) {
        handleIncomingLine(usbSerialBuf, "USB-DBG");
        usbSerialBuf = "";
      }
    } else if (usbSerialBuf.length() < 1500) {
      usbSerialBuf += c;
    }
  }

  // 3. Periodic 3-second debug heartbeat on USB Serial Monitor
  if (millis() - lastBeatMs >= 3000) {
    lastBeatMs = millis();
    Serial.printf("[HEARTBEAT] Uptime=%lus | Serial1 RX=%lu bytes | Cmds=%lu | GPIO20(RX)=%d | WiFi=%s (%s)\n",
                  (unsigned long)(millis() / 1000),
                  (unsigned long)dbgRxBytes,
                  (unsigned long)dbgCmdCount,
                  digitalRead(PIN_DSO_RX),
                  wifiStatusName(WiFi.status()),
                  (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString().c_str() : "no-ip");
  }
}
