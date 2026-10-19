# ESP32-C3 SuperMini Wi-Fi Client & Modem Bridge Guide (`<DSO152_WiFi.h>`)

By connecting an **ESP32-C3 SuperMini** to the 4 SWD header pads inside the FNIRSI DSO-152 (`VCC`, `GND`, `DIO/PA13`, `CLK/PA14`), the ESP32-C3 acts as an **internal Wi-Fi / HTTPS / TCP / NTP modem co-processor**.

**You only flash the ESP32-C3 SuperMini ONCE** with [`esp32c3_wifi_companion.ino`](../arduino/esp32c3_wifi_companion/esp32c3_wifi_companion.ino) and close the case! From then on, you do **all** Wi-Fi configuration, scanning, HTTP/HTTPS requests, and TCP sockets dynamically on the DSO-152 using **`#include <DSO152_WiFi.h>`**—with the exact same API style as standard ESP32 `WiFi.h`, `WiFiClient.h`, and `HTTPClient.h`!

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
| **`DIO`** | `PA13` (`SWDIO`) | $\rightarrow$ | **`RX` (`GPIO20`)** | DSO-152 transmits commands & data to ESP32-C3. |
| **`CLK`** | `PA14` (`SWCLK`) | $\leftarrow$ | **`TX` (`GPIO21`)** | ESP32-C3 returns responses, HTTP payloads, and socket data to DSO-152. |

---

## 2. One-Time ESP32-C3 Modem Setup

1. Open [`arduino/esp32c3_wifi_companion/esp32c3_wifi_companion.ino`](../arduino/esp32c3_wifi_companion/esp32c3_wifi_companion.ino) in the Arduino IDE.
2. Select **Tools -> Board -> esp32 -> `ESP32C3 Dev Module`** and click **Upload**.
3. Wire the 4 pins (`3V3`, `G`, `RX/20`, `TX/21`) to the DSO-152 SWD pads and close the case—you never need to re-flash the ESP32-C3 again!

---

## 3. Using `<DSO152_WiFi.h>` in Your DSO-152 Sketches

### Example 1: Dynamic `WiFi.begin()`, `HTTPClient` (HTTPS!), and `WiFiClient`

```cpp
#include <FNIRSI_DSO152.h>
#include <DSO152_WiFi.h>

void setup() {
    dso.begin(1);

    // 1. Connect dynamically from the DSO-152 (also saves to ESP32-C3 NVS Flash!)
    //    Or call WiFi.selectNetworkUI() to pick a network on-screen with the buttons!
    WiFi.begin("MyHomeWiFi", "MyPassword123");

    if (WiFi.status() == WL_CONNECTED) {
        dso.print("IP: ");
        dso.println(WiFi.localIP());

        // 2. Make HTTP or HTTPS (TLS) GET/POST requests directly!
        HTTPClient http;
        if (http.begin("https://wttr.in/?format=3")) {
            if (http.GET() == 200) {
                dso.println(http.getString());
            }
            http.end();
        }

        // 3. Or use raw TCP / SSL sockets just like on an ESP32!
        WiFiClient client;
        if (client.connect("example.com", 80)) {
            client.print("GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");
            String response = client.readString();
            client.stop();
        }
    }
}

void loop() {
    dso.updatePowerButton(true);
}
```

### Example 2: Interactive On-Screen Wi-Fi Picker & Virtual Keyboard

Don't want to hardcode your Wi-Fi SSID/password in your sketch? Just call:

```cpp
WiFi.selectNetworkUI();
```

This opens a full interactive **Wi-Fi Network Scanner & On-Screen Virtual Keyboard** on the DSO-152's 320x240 IPS display:
- Use **`UP` / `DOWN`** to pick a Wi-Fi network and press **`OK`**.
- Use the D-Pad + **`OK`** to type the WPA2 password on the on-screen keyboard and press **`RUN`** to connect.
- Once connected, the ESP32-C3 saves the credentials in its NVS Flash so future boots can simply call `WiFi.beginSaved()`!

---

## 4. `<DSO152_WiFi.h>` API Summary

| Class / Method | Description |
| :--- | :--- |
| **`WiFi.begin(ssid, pass)`** | Connects to a 2.4 GHz Wi-Fi network and saves credentials in ESP32-C3 NVS. |
| **`WiFi.beginSaved()`** | Connects using the last saved Wi-Fi credentials in ESP32-C3 NVS. |
| **`WiFi.selectNetworkUI()`** | Launches the on-screen Wi-Fi scanner & virtual keyboard UI on the DSO-152 LCD. |
| **`WiFi.startConfigPortal("DSO-Setup")`** | Starts a Captive Web Portal on the ESP32-C3 (`http://192.168.4.1`) to configure Wi-Fi from a phone. |
| **`WiFi.softAP(ssid, pass)`** | Starts a Wi-Fi Access Point dynamically from your DSO-152 sketch. |
| **`WiFi.status()`** | Returns `WL_CONNECTED` (`3`) or `WL_DISCONNECTED` (`6`). |
| **`WiFi.localIP()` / `WiFi.SSID()` / `WiFi.RSSI()`** | Returns current IPv4 `String`, SSID `String`, and signal strength in `dBm`. |
| **`WiFi.scanNetworks()`** | Scans 2.4 GHz networks; read results via `WiFi.SSID(i)`, `WiFi.RSSI(i)`, `WiFi.channel(i)`. |
| **`WiFi.ping(host, port)`** | Measures TCP reachability/latency in milliseconds (`-1` on timeout). |
| **`WiFi.getNTPTime(gmtOffsetSec)`** | Syncs via NTP and returns formatted `"YYYY-MM-DD HH:MM:SS"`. |
| **`HTTPClient`** | `http.begin(url)` (supports `http://` and `https://`), `http.GET()`, `http.POST(body)`, `http.getString()`. |
| **`WiFiClient`** | `client.connect(host, port)`, `client.connectSSL(host, 443)`, `client.print()`, `client.readString()`, `client.stop()`. |
