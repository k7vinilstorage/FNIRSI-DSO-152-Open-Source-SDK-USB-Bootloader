#ifndef FNIRSI_DSO152_H
#define FNIRSI_DSO152_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__has_include)
  #if __has_include(<Adafruit_GFX.h>)
    #include <Adafruit_GFX.h>
    #define DSO152_HAS_ADAFRUIT_GFX 1
  #endif
#endif

/* ============================================================================
 * RGB565 Color Definitions
 * ============================================================================ */
#ifndef COLOR_BLACK
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_YELLOW      0xFFE0
#define COLOR_ORANGE      0xFD20
#define COLOR_DARKBLUE    0x000F
#define COLOR_NAVY        0x000F
#define COLOR_GRAY        0x8410
#define COLOR_DARKGRAY    0x31A6
#define COLOR_LIGHTGRAY   0xC618
#endif

/* ============================================================================
 * Analog Front-End (AFE) Vertical Sensitivity Ranges
 * ============================================================================ */
typedef enum {
    AFE_RANGE_10mV  = 0,
    AFE_RANGE_20mV  = 1,
    AFE_RANGE_50mV  = 2,
    AFE_RANGE_100mV = 3,
    AFE_RANGE_200mV = 4,
    AFE_RANGE_500mV = 5,
    AFE_RANGE_1V    = 6,
    AFE_RANGE_2V    = 7,
    AFE_RANGE_5V    = 8,
    AFE_RANGE_10V   = 9,
    AFE_RANGE_20V   = 10
} dso152_range_t;

/* ============================================================================
 * Front-Panel 9-Button State Structure
 * ============================================================================ */
struct DSO152_Buttons {
    bool up;        // PC6
    bool down;      // PA9
    bool left;      // PC3
    bool right;     // PC5
    bool ok;        // PC4 (Menu / OK)
    bool run;       // PA10 (Run / Stop)
    bool auto_btn;  // PC8 (Auto)
    bool mode;      // PC7 (Mode)
    bool pwr;       // PC11 (Power button)

    bool anyPressed() const {
        return up || down || left || right || ok || run || auto_btn || mode;
    }
};

/* ============================================================================
 * FNIRSI DSO-152 Main Hardware & Display Class
 * Inherits from Adafruit_GFX when available (or Print as fallback)
 * ============================================================================ */
#ifdef DSO152_HAS_ADAFRUIT_GFX
class FNIRSI_DSO152 : public Adafruit_GFX {
#else
class FNIRSI_DSO152 : public Print {
#endif
public:
    FNIRSI_DSO152();

    /**
     * @brief Initialize all DSO-152 hardware:
     *        Power latch (PC10), Backlight (PC9), 9 Buttons,
     *        16-bit Parallel ST7789 IPS LCD (320x240), AFE & 12-bit ADC1.
     */
    void begin(uint8_t default_rotation = 1);

    /* --- Power & Bootloader Control --- */
    void setPowerLatch(bool on);
    void powerOff();
    void setBacklight(bool on);
    void rebootToBootloader();

    /**
     * @brief Call periodically in loop() or a FreeRTOS task.
     *        - Holds Power (PC11) > 800ms -> Clean shutdown with UI prompt
     *        - Holds OK (PC4) > 2000ms (if allow_bootloader_shortcut=true) -> Reboots to DSO BOOT USB drive
     */
    void updatePowerButton(bool allow_bootloader_shortcut = true);

    /* --- Buttons --- */
    DSO152_Buttons readButtons();

    /* --- Analog Front-End (AFE) & ADC --- */
    void setRange(dso152_range_t range);
    dso152_range_t getRange() const { return _current_range; }
    const char* getRangeName(dso152_range_t range) const;
    const char* getRangeName() const { return getRangeName(_current_range); }

    void setCoupling(bool ac_coupling);
    bool isACCoupling() const { return _ac_coupling; }

    uint16_t readADC();
    uint32_t readMillivolts();

    /**
     * @brief Capture a triggered waveform buffer from ADC1 (PA1).
     * @param buffer      Destination array of 12-bit ADC samples (0..4095)
     * @param length      Number of samples to record
     * @param delay_loops Delay iterations between samples (0 = fastest ~800 kSPS)
     * @param trig_level  Rising-edge trigger level (default 2048 = midpoint)
     */
    void captureWaveform(uint16_t *buffer, uint16_t length, uint16_t delay_loops = 20, uint16_t trig_level = 2048);

    /* --- 1kHz Calibration Output (PA0) --- */
    void setCalPin(bool high);
    void toggleCalPin();

    /* --- Display & Graphics Primitives (Adafruit_GFX Overrides + Direct Fast API) --- */
    void setRotation(uint8_t r);
    void invertDisplay(bool i);

    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void fillScreen(uint16_t color);
    void fill(uint16_t color) { fillScreen(color); }

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

    void drawCharFast(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size = 1);
    void drawString(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size = 1);

    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

#ifndef DSO152_HAS_ADAFRUIT_GFX
    int16_t width() const { return _width; }
    int16_t height() const { return _height; }
    void setCursor(int16_t x, int16_t y) { _cursor_x = x; _cursor_y = y; }
    void setTextColor(uint16_t fg) { _text_fg = fg; _text_bg = fg; }
    void setTextColor(uint16_t fg, uint16_t bg) { _text_fg = fg; _text_bg = bg; }
    void setTextSize(uint8_t s) { _text_size = (s > 0) ? s : 1; }
    virtual size_t write(uint8_t c) override;
#endif

private:
#ifndef DSO152_HAS_ADAFRUIT_GFX
    int16_t _width;
    int16_t _height;
    int16_t _cursor_x;
    int16_t _cursor_y;
    uint16_t _text_fg;
    uint16_t _text_bg;
    uint8_t _text_size;
#endif
    dso152_range_t _current_range;
    bool _ac_coupling;
    uint32_t _pwr_press_start;
    uint32_t _ok_press_start;

    void _initClocksAndGPIO();
    void _initLCD(uint8_t rotation);
    void _initAFE();
};

extern FNIRSI_DSO152 dso;

#endif /* FNIRSI_DSO152_H */
