/**
 * @file esp32c3_wifi_companion.ino
 * @brief ESP32-C3 SuperMini Wi-Fi Companion Firmware for FNIRSI DSO-152
 *
 * Arduino IDE Board Settings for ESP32-C3 SuperMini:
 *   - Board: "ESP32C3 Dev Module" (under esp32 -> ESP32 Arduino)
 *   - USB CDC On Boot: "Enabled"
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
#include <WebServer.h>

// ---------------------------------------------------------------------------
// Wi-Fi Configuration
// ---------------------------------------------------------------------------
// 1. Built-in Wi-Fi Hotspot (Always active so you can connect anywhere):
static const char* AP_SSID = "DSO-152-WiFi";
static const char* AP_PASS = "dso152wifi"; // Minimum 8 chars, or "" for open network

// 2. Optional: Connect to your home Wi-Fi network as well (leave "" for AP-only):
static const char* STA_SSID = "";
static const char* STA_PASS = "";

// ESP32-C3 SuperMini Pin Definitions
static const int PIN_DSO_RX  = 20; // GPIO20 (labeled RX on SuperMini) <- DSO PA13 (DIO)
static const int PIN_DSO_TX  = 21; // GPIO21 (labeled TX on SuperMini) -> DSO PA14 (CLK)
static const int PIN_LED     = 8;  // GPIO8  (Onboard Blue LED on SuperMini, active-low)

WebServer server(80);

// Latest oscilloscope state received from DSO-152
struct TelemetryState {
  String range     = "1V";
  String coupling  = "DC";
  String timebase  = "100us";
  int    paused    = 0;
  long   vppMv     = 0;
  long   vAvgMv    = 0;
  String hexWave   = "";
  uint32_t lastUpdateMs = 0;
};

static TelemetryState gScope;
static String gPendingCmd = "";

// ---------------------------------------------------------------------------
// Embedded HTML5 Canvas Web Oscilloscope & Remote Control UI
// ---------------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>FNIRSI DSO-152 Wireless Web Oscilloscope</title>
<style>
  :root {
    --bg: #0b0f19;
    --panel: #161f30;
    --border: #2a3950;
    --accent: #38bdf8;
    --trace: #facc15;
    --text: #f1f5f9;
    --muted: #94a3b8;
    --green: #22c55e;
    --red: #ef4444;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: system-ui, -apple-system, sans-serif; }
  body { background: var(--bg); color: var(--text); display: flex; flex-direction: column; align-items: center; padding: 16px; }
  .container { width: 100%; max-width: 820px; background: var(--panel); border: 1px solid var(--border); border-radius: 12px; padding: 16px; box-shadow: 0 12px 32px rgba(0,0,0,0.45); }
  header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; flex-wrap: wrap; gap: 8px; }
  h1 { font-size: 1.25rem; color: var(--accent); letter-spacing: 0.5px; }
  .badge { padding: 4px 10px; border-radius: 999px; font-size: 0.8rem; font-weight: 700; background: #1e293b; border: 1px solid var(--border); }
  .badge.run { color: var(--green); border-color: var(--green); }
  .badge.hold { color: var(--red); border-color: var(--red); }
  .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); gap: 10px; margin-bottom: 14px; }
  .stat-card { background: #0f172a; border: 1px solid var(--border); border-radius: 8px; padding: 10px 12px; }
  .stat-label { font-size: 0.72rem; color: var(--muted); text-transform: uppercase; }
  .stat-val { font-size: 1.15rem; font-weight: 700; margin-top: 2px; color: var(--text); }
  canvas { width: 100%; height: auto; aspect-ratio: 5 / 3; background: #050811; border: 1px solid var(--border); border-radius: 8px; display: block; margin-bottom: 14px; }
  .controls { display: grid; grid-template-columns: repeat(auto-fit, minmax(105px, 1fr)); gap: 8px; }
  button { background: #1e293b; color: var(--text); border: 1px solid var(--border); border-radius: 8px; padding: 10px 12px; font-size: 0.88rem; font-weight: 600; cursor: pointer; transition: 0.15s; }
  button:hover { background: #334155; border-color: var(--accent); }
  button:active { transform: scale(0.97); }
  button.primary { background: #0284c7; border-color: #38bdf8; }
  button.warn { background: #b45309; border-color: #f59e0b; }
</style>
</head>
<body>
<div class="container">
  <header>
    <h1>FNIRSI DSO-152 // WIRELESS WEB SCOPE</h1>
    <span id="statusBadge" class="badge run">LIVE</span>
  </header>

  <div class="stats">
    <div class="stat-card"><div class="stat-label">Vertical Range</div><div class="stat-val" id="valRange">1V/div</div></div>
    <div class="stat-card"><div class="stat-label">Coupling</div><div class="stat-val" id="valCouple">DC</div></div>
    <div class="stat-card"><div class="stat-label">Timebase</div><div class="stat-val" id="valTime">100us</div></div>
    <div class="stat-card"><div class="stat-label">Vpp</div><div class="stat-val" id="valVpp" style="color:#facc15">0 mV</div></div>
    <div class="stat-card"><div class="stat-label">Vavg</div><div class="stat-val" id="valVavg" style="color:#38bdf8">0 mV</div></div>
  </div>

  <canvas id="scopeCanvas" width="600" height="360"></canvas>

  <div class="controls">
    <button onclick="sendCmd('RNG+')">Range +</button>
    <button onclick="sendCmd('RNG-')">Range -</button>
    <button onclick="sendCmd('TIM-')">Timebase Fast</button>
    <button onclick="sendCmd('TIM+')">Timebase Slow</button>
    <button onclick="sendCmd('ACDC')">AC / DC</button>
    <button class="warn" onclick="sendCmd('HOLD')">RUN / HOLD</button>
    <button class="primary" onclick="sendCmd('AUTO')">AUTO SET</button>
    <button onclick="exportCSV()">Export CSV</button>
  </div>
</div>

<script>
const canvas = document.getElementById('scopeCanvas');
const ctx = canvas.getContext('2d');
let lastSamples = [];

function drawGrid() {
  const w = canvas.width, h = canvas.height;
  ctx.fillStyle = '#050811';
  ctx.fillRect(0, 0, w, h);

  ctx.strokeStyle = '#162032';
  ctx.lineWidth = 1;
  for (let x = 0; x <= w; x += 60) {
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
  }
  for (let y = 0; y <= h; y += 60) {
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
  }

  ctx.strokeStyle = '#2c3e5a';
  ctx.beginPath();
  ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2);
  ctx.moveTo(w / 2, 0); ctx.lineTo(w / 2, h);
  ctx.stroke();
}

function renderWaveform(hex) {
  drawGrid();
  if (!hex || hex.length < 4) return;
  const n = Math.floor(hex.length / 2);
  const samples = [];
  let sum = 0;
  for (let i = 0; i < n; i++) {
    const v = parseInt(hex.substr(i * 2, 2), 16);
    samples.push(v);
    sum += v;
  }
  lastSamples = samples;
  const mid = sum / n;

  ctx.strokeStyle = '#facc15';
  ctx.lineWidth = 2.2;
  ctx.shadowColor = '#facc15';
  ctx.shadowBlur = 6;
  ctx.beginPath();
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * canvas.width;
    const y = (canvas.height / 2) - ((samples[i] - mid) / 128.0) * (canvas.height * 0.42);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.stroke();
  ctx.shadowBlur = 0;
}

async function pollState() {
  try {
    const r = await fetch('/api/state');
    const d = await r.json();
    document.getElementById('valRange').textContent = d.range + '/div';
    document.getElementById('valCouple').textContent = d.coupling;
    document.getElementById('valTime').textContent = d.timebase;
    document.getElementById('valVpp').textContent = d.vpp + ' mV';
    document.getElementById('valVavg').textContent = d.vavg + ' mV';
    const badge = document.getElementById('statusBadge');
    if (d.paused) {
      badge.textContent = 'HOLD';
      badge.className = 'badge hold';
    } else {
      badge.textContent = 'RUN (' + d.age_ms + 'ms)';
      badge.className = 'badge run';
    }
    renderWaveform(d.wave);
  } catch (e) {}
}

function sendCmd(c) {
  fetch('/api/cmd?c=' + encodeURIComponent(c));
}

function exportCSV() {
  if (!lastSamples.length) return;
  let csv = 'index,adc_8bit\n';
  lastSamples.forEach((v, i) => { csv += i + ',' + v + '\n'; });
  const blob = new Blob([csv], { type: 'text/csv' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'dso152_waveform.csv';
  a.click();
}

drawGrid();
setInterval(pollState, 100);
</script>
</body>
</html>
)rawliteral";

static String getActiveIPString() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return WiFi.softAPIP().toString();
}

// Parse incoming "$W,<range>,<acdc>,<timebase>,<paused>,<vpp>,<vavg>,<hex>"
static void handleWaveLine(const String &line) {
  int p1 = line.indexOf(',');
  int p2 = line.indexOf(',', p1 + 1);
  int p3 = line.indexOf(',', p2 + 1);
  int p4 = line.indexOf(',', p3 + 1);
  int p5 = line.indexOf(',', p4 + 1);
  int p6 = line.indexOf(',', p5 + 1);
  int p7 = line.indexOf(',', p6 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0 || p6 < 0 || p7 < 0) return;

  gScope.range        = line.substring(p1 + 1, p2);
  gScope.coupling     = line.substring(p2 + 1, p3);
  gScope.timebase     = line.substring(p3 + 1, p4);
  gScope.paused       = line.substring(p4 + 1, p5).toInt();
  gScope.vppMv        = line.substring(p5 + 1, p6).toInt();
  gScope.vAvgMv       = line.substring(p6 + 1, p7).toInt();
  gScope.hexWave      = line.substring(p7 + 1);
  gScope.lastUpdateMs = millis();

  // Immediately send compact reply back to DSO-152 on GPIO21 (TX -> PA14/CLK)
  String reply = "OK|" + getActiveIPString() + "|" + gPendingCmd + "\n";
  Serial1.print(reply);
  gPendingCmd = "";

  // Toggle SuperMini onboard LED to indicate active telemetry
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}

// Handle "$SCAN" request from DSO-152 and reply with "$NETS|ssid,rssi,ch;..."
static void handleScanRequest() {
  int n = WiFi.scanNetworks(false, false);
  String out = "$NETS|";
  int count = (n > 8) ? 8 : n;
  for (int i = 0; i < count; i++) {
    String ssid = WiFi.SSID(i);
    ssid.replace(",", " ");
    ssid.replace(";", " ");
    if (ssid.length() > 16) ssid = ssid.substring(0, 16);
    out += ssid + "," + String(WiFi.RSSI(i)) + "," + String(WiFi.channel(i));
    if (i + 1 < count) out += ";";
  }
  out += "\n";
  Serial1.print(out);
  WiFi.scanDelete();
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW); // LED ON during init

  Serial.begin(115200);
  // UART1 on GPIO20 (RX <- DSO PA13/DIO) and GPIO21 (TX -> DSO PA14/CLK)
  Serial1.setRxBufferSize(1024);
  Serial1.begin(57600, SERIAL_8N1, PIN_DSO_RX, PIN_DSO_TX);

  // Start Wi-Fi AP (+ optional STA) with reduced TX power (11 dBm)
  // so peak current spikes never brown out the DSO-152's 3.3V regulator!
  if (strlen(STA_SSID) > 0) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_AP);
  }
  WiFi.setTxPower(WIFI_POWER_11dBm);

  if (strlen(AP_PASS) >= 8) {
    WiFi.softAP(AP_SSID, AP_PASS);
  } else {
    WiFi.softAP(AP_SSID);
  }

  if (strlen(STA_SSID) > 0) {
    WiFi.begin(STA_SSID, STA_PASS);
  }

  // Web Server Routes
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/state", HTTP_GET, []() {
    uint32_t age = millis() - gScope.lastUpdateMs;
    String json = "{";
    json += "\"range\":\"" + gScope.range + "\",";
    json += "\"coupling\":\"" + gScope.coupling + "\",";
    json += "\"timebase\":\"" + gScope.timebase + "\",";
    json += "\"paused\":" + String(gScope.paused) + ",";
    json += "\"vpp\":" + String(gScope.vppMv) + ",";
    json += "\"vavg\":" + String(gScope.vAvgMv) + ",";
    json += "\"age_ms\":" + String(age) + ",";
    json += "\"wave\":\"" + gScope.hexWave + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/cmd", HTTP_GET, []() {
    if (server.hasArg("c")) {
      gPendingCmd = server.arg("c");
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  digitalWrite(PIN_LED, HIGH);
}

void loop() {
  server.handleClient();

  while (Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    line.trim();
    if (line.startsWith("$W,")) {
      handleWaveLine(line);
    } else if (line.startsWith("$SCAN")) {
      handleScanRequest();
    }
  }
}
