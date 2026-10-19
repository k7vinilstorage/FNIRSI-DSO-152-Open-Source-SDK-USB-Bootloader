#ifndef DSO152_WIFI_H
#define DSO152_WIFI_H

#include "FNIRSI_DSO152.h"

/* ============================================================================
 * Standard Arduino / ESP32 Wi-Fi Status Codes
 * ============================================================================ */
typedef enum {
    WL_NO_SHIELD       = 255,
    WL_IDLE_STATUS     = 0,
    WL_NO_SSID_AVAIL   = 1,
    WL_SCAN_COMPLETED  = 2,
    WL_CONNECTED       = 3,
    WL_CONNECT_FAILED  = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED    = 6
} wl_status_t;

/* ============================================================================
 * DSO152_WiFiClass — ESP32-Style WiFi Controller for FNIRSI DSO-152
 * Talks dynamically to the ESP32-C3 SuperMini over PA13 (DIO) & PA14 (CLK)
 * ============================================================================ */
class DSO152_WiFiClass {
public:
    DSO152_WiFiClass();

    /**
     * @brief Initialize the SWD UART bridge to the ESP32-C3 modem and check connectivity.
     * @return true if the ESP32-C3 modem responded with PONG.
     */
    bool initModem(uint32_t baud = 57600);

    /**
     * @brief Check if the internal ESP32-C3 modem is alive and responding.
     */
    bool isModemReady();

    /**
     * @brief Connect to a 2.4 GHz Wi-Fi network (just like ESP32 WiFi.begin(ssid, pass)).
     *        Credentials are also persisted in ESP32-C3 NVS Flash.
     */
    wl_status_t begin(const char *ssid, const char *passphrase = "");

    /**
     * @brief Reconnect using the last Wi-Fi credentials saved in the ESP32-C3's NVS Flash.
     */
    wl_status_t beginSaved();

    /**
     * @brief Start a Wi-Fi Access Point on the ESP32-C3 dynamically from the DSO-152.
     */
    bool softAP(const char *ssid, const char *passphrase = "");

    /**
     * @brief Start a Captive Configuration Portal AP on the ESP32-C3 so you can
     *        enter Wi-Fi credentials from your phone browser at http://192.168.4.1
     */
    bool startConfigPortal(const char *ap_ssid = "DSO-152-Setup");

    /**
     * @brief Interactive on-screen Wi-Fi Network Picker & Virtual Keyboard on the DSO-152 LCD!
     *        Scans networks, lets you pick an SSID and enter the password using the
     *        DSO-152 buttons (UP/DOWN/LEFT/RIGHT/OK/RUN), and connects!
     */
    bool selectNetworkUI();

    /**
     * @brief Disconnect from the current Wi-Fi network.
     */
    void disconnect();

    /**
     * @brief Query current connection status (WL_CONNECTED, WL_DISCONNECTED, etc.).
     */
    wl_status_t status();

    /**
     * @brief Get local IPv4 address as a String (e.g. "192.168.1.42").
     */
    String localIP();

    /**
     * @brief Get SoftAP IPv4 address as a String (default "192.168.4.1").
     */
    String softAPIP();

    /**
     * @brief Get currently connected SSID.
     */
    String SSID();

    /**
     * @brief Get current Wi-Fi RSSI in dBm.
     */
    int32_t RSSI();

    /**
     * @brief Get ESP32-C3 Wi-Fi MAC address string.
     */
    String macAddress();

    /* --- Wi-Fi Scanner API (Matches ESP32 WiFi.scanNetworks()) --- */
    int16_t scanNetworks();
    String  SSID(uint8_t networkItem) const;
    int32_t RSSI(uint8_t networkItem) const;
    int32_t channel(uint8_t networkItem) const;
    uint8_t encryptionType(uint8_t networkItem) const;

    /* --- Network Utilities (Handled by ESP32-C3 Co-Processor) --- */
    /**
     * @brief Resolve hostname or test reachability via TCP/ICMP on the ESP32-C3.
     * @return Connect/ping time in ms, or -1 on failure.
     */
    int32_t ping(const char *host, uint16_t port = 80);

    /**
     * @brief Fetch current formatted date/time via NTP on the ESP32-C3.
     * @param gmtOffset_sec GMT offset in seconds (e.g. -10800 for BRT / UTC-3)
     */
    String getNTPTime(long gmtOffset_sec = -10800);

    /* --- Low-Level Command Transaction with ESP32-C3 Modem --- */
    String sendCommand(const char *cmd, uint32_t timeout_ms = 2000);

private:
    bool _modem_init;
    wl_status_t _cached_status;
    String _cached_ip;
    String _cached_ssid;
    int32_t _cached_rssi;

    static const uint8_t MAX_SCAN_ITEMS = 10;
    uint8_t _scan_count;
    String  _scan_ssid[MAX_SCAN_ITEMS];
    int32_t _scan_rssi[MAX_SCAN_ITEMS];
    int32_t _scan_ch[MAX_SCAN_ITEMS];
    uint8_t _scan_enc[MAX_SCAN_ITEMS];

    void _refreshStatus();
};

/* ============================================================================
 * DSO152_WiFiClient — ESP32-Style TCP / SSL Socket Client
 * ============================================================================ */
class DSO152_WiFiClient {
public:
    DSO152_WiFiClient();

    bool connect(const char *host, uint16_t port, bool ssl = false);
    bool connectSSL(const char *host, uint16_t port = 443) { return connect(host, port, true); }

    size_t write(const uint8_t *buf, size_t size);
    size_t print(const char *str);
    size_t print(const String &str) { return print(str.c_str()); }
    size_t println(const char *str = "");
    size_t println(const String &str) { return println(str.c_str()); }

    int available();
    int read();
    String readString(uint32_t timeout_ms = 2000);
    String readStringUntil(char terminator, uint32_t timeout_ms = 2000);

    bool connected();
    void stop();

private:
    bool _connected;
    String _rx_buffer;
    void _fetchSocketData(uint32_t timeout_ms = 500);
};

/* ============================================================================
 * DSO152_HTTPClient — ESP32-Style HTTP / HTTPS Client (Supports TLS!)
 * ============================================================================ */
class DSO152_HTTPClient {
public:
    DSO152_HTTPClient();

    bool begin(const char *url);
    bool begin(const String &url) { return begin(url.c_str()); }
    void addHeader(const char *name, const char *value);

    int GET();
    int POST(const char *payload);
    int POST(const String &payload) { return POST(payload.c_str()); }

    const String& getString() const { return _response_body; }
    int getSize() const { return (int)_response_body.length(); }
    void end();

private:
    String _url;
    String _content_type;
    String _auth_header;
    String _response_body;
    int _last_code;
};

/* Global Pre-Instantiated WiFi Object & Standard Type Aliases */
extern DSO152_WiFiClass WiFi;
typedef DSO152_WiFiClient WiFiClient;
typedef DSO152_HTTPClient HTTPClient;

#endif /* DSO152_WIFI_H */
