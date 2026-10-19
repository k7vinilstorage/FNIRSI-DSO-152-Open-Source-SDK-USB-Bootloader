#include "dso152.h"
#include <stdio.h>

#define GRID_X      10
#define GRID_Y      28
#define GRID_W      300
#define GRID_H      160

#define SAMPLES_NUM 300

static const char *range_str[] = {
    "10mV", "20mV", "50mV", "100mV", "200mV",
    "500mV", "1V", "2V", "5V", "10V", "20V"
};

static void format_val_4d(int32_t val, char *buf) {
    if (val < 0) val = 0;
    if (val > 9999) val = 9999;
    buf[0] = (val >= 1000) ? ('0' + (val / 1000) % 10) : ' ';
    buf[1] = (val >= 100)  ? ('0' + (val / 100) % 10)  : ' ';
    buf[2] = (val >= 10)   ? ('0' + (val / 10) % 10)   : ' ';
    buf[3] = '0' + (val % 10);
    buf[4] = '\0';
}

static void draw_grid(void) {
    // Fill scope background
    st7789_fill_rect(GRID_X, GRID_Y, GRID_W, GRID_H, COLOR_NAVY);
    st7789_draw_rect(GRID_X, GRID_Y, GRID_W, GRID_H, COLOR_CYAN);

    // Horizontal center line
    st7789_draw_line(GRID_X + 1, GRID_Y + GRID_H / 2, GRID_X + GRID_W - 2, GRID_Y + GRID_H / 2, COLOR_DARKGRAY);

    // Vertical center line
    st7789_draw_line(GRID_X + GRID_W / 2, GRID_Y + 1, GRID_X + GRID_W / 2, GRID_Y + GRID_H - 2, COLOR_DARKGRAY);

    // Grid dots every 30 pixels
    for (int x = GRID_X + 30; x < GRID_X + GRID_W; x += 30) {
        for (int y = GRID_Y + 20; y < GRID_Y + GRID_H; y += 20) {
            st7789_draw_pixel(x, y, COLOR_GRAY);
        }
    }
}

int main(void) {
    // 1. Initialize DSO152 SDK
    dso152_init();

    // 2. Set Landscape orientation (320x240)
    st7789_set_rotation(1);
    st7789_fill(COLOR_BLACK);

    // 3. Draw Header Status Bar
    st7789_fill_rect(0, 0, 320, 24, COLOR_BLUE);
    st7789_draw_string(8, 7, "DSO-152 SCOPE", COLOR_WHITE, COLOR_BLUE, 1);

    // 4. Draw Oscilloscope Grid
    draw_grid();

    // 5. Draw Footer Help Bar
    st7789_fill_rect(0, 192, 320, 48, COLOR_BLACK);
    st7789_draw_string(10, 196, "UP/DN: Range | MODE: AC/DC | RUN: Hold", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    st7789_draw_string(10, 210, "AUTO: Center | Hold PWR: Off | Hold OK: Bootloader", COLOR_ORANGE, COLOR_BLACK, 1);

    // 6. Application State
    dso152_range_t cur_range = AFE_RANGE_500mV;
    bool ac_coupling = false;
    bool is_running = true;

    dso152_afe_set_range(cur_range);
    dso152_afe_set_coupling(ac_coupling);

    dso152_buttons_t last_b = {0};
    uint8_t prev_y[GRID_W];
    for (int i = 0; i < GRID_W; i++) prev_y[i] = GRID_H / 2;

    uint32_t last_status_update = 0;
    uint32_t last_cal_toggle = 0;
    uint32_t ok_hold_start = 0;

    int8_t prev_drawn_range = -1;
    int8_t prev_drawn_coupling = -1;
    int8_t prev_drawn_running = -1;

    static uint16_t adc_buffer[SAMPLES_NUM];

    while (1) {
        dso152_buttons_t b = dso152_btn_read();
        dso152_btn_check_power_hold();

        // Check OK hold for 2s to reboot to bootloader
        if (b.ok) {
            if (ok_hold_start == 0) {
                ok_hold_start = get_ticks();
            } else if ((get_ticks() - ok_hold_start) > 2000) {
                st7789_fill(COLOR_BLUE);
                st7789_draw_string(50, 110, "REBOOTING TO BOOTLOADER...", COLOR_WHITE, COLOR_BLUE, 1);
                delay_ms(500);
                dso152_reboot_to_bootloader();
            }
        } else {
            ok_hold_start = 0;
        }

        // Toggle calibration pin PA0 every 1ms (~500Hz square wave)
        uint32_t now = get_ticks();
        if ((now - last_cal_toggle) >= 1) {
            last_cal_toggle = now;
            dso152_cal_toggle();
        }

        // Handle Button Presses:
        // UP: Decrease sensitivity (higher voltage range)
        if (b.up && !last_b.up) {
            if (cur_range < AFE_RANGE_20V) {
                cur_range++;
                dso152_afe_set_range(cur_range);
            }
        }
        // DOWN: Increase sensitivity (lower voltage range)
        if (b.down && !last_b.down) {
            if (cur_range > AFE_RANGE_10mV) {
                cur_range--;
                dso152_afe_set_range(cur_range);
            }
        }
        // MODE: Toggle AC / DC coupling
        if (b.mode && !last_b.mode) {
            ac_coupling = !ac_coupling;
            dso152_afe_set_coupling(ac_coupling);
        }
        // RUN: Pause / Resume sampling
        if (b.run && !last_b.run) {
            is_running = !is_running;
        }
        // AUTO: Reset to 1V Range DC
        if (b.auto_btn && !last_b.auto_btn) {
            cur_range = AFE_RANGE_1V;
            ac_coupling = false;
            dso152_afe_set_range(cur_range);
            dso152_afe_set_coupling(ac_coupling);
        }

        last_b = b;

        // Update Top Status Bar UI (only if changed)
        if ((int8_t)is_running != prev_drawn_running) {
            if (is_running) {
                st7789_fill_rect(120, 4, 38, 16, COLOR_GREEN);
                st7789_draw_string(124, 8, "RUN", COLOR_BLACK, COLOR_GREEN, 1);
            } else {
                st7789_fill_rect(120, 4, 38, 16, COLOR_RED);
                st7789_draw_string(124, 8, "STOP", COLOR_WHITE, COLOR_RED, 1);
            }
            prev_drawn_running = (int8_t)is_running;
        }

        if ((int8_t)cur_range != prev_drawn_range) {
            st7789_fill_rect(165, 4, 60, 16, COLOR_DARKBLUE);
            st7789_draw_string(168, 8, range_str[cur_range], COLOR_YELLOW, COLOR_DARKBLUE, 1);
            prev_drawn_range = (int8_t)cur_range;
        }

        if ((int8_t)ac_coupling != prev_drawn_coupling) {
            st7789_fill_rect(235, 4, 30, 16, COLOR_DARKBLUE);
            st7789_draw_string(240, 8, ac_coupling ? "AC" : "DC", COLOR_CYAN, COLOR_DARKBLUE, 1);
            prev_drawn_coupling = (int8_t)ac_coupling;
        }

        // Waveform Acquisition & Rendering
        if (is_running) {
            uint16_t min_v = 4095;
            uint16_t max_v = 0;
            uint32_t sum_v = 0;

            // Trigger Search: wait for signal crossing upward through midpoint
            uint16_t trig_level = 2048;
            uint32_t trig_to = 2000;
            uint16_t prev_sample = dso152_adc_read_single();
            while (--trig_to) {
                uint16_t cur_sample = dso152_adc_read_single();
                if (prev_sample < trig_level && cur_sample >= trig_level) break;
                prev_sample = cur_sample;
            }

            // Sample waveform buffer
            for (int i = 0; i < (GRID_W - 2); i++) {
                uint16_t val = dso152_adc_read_single();
                adc_buffer[i] = val;
                if (val < min_v) min_v = val;
                if (val > max_v) max_v = val;
                sum_v += val;

                // Software sweep delay (~500 kSPS)
                for (volatile int d = 0; d < 30; d++);
            }

            // Draw waveform with zero-flicker pixel replacement
            for (int i = 0; i < (GRID_W - 2); i++) {
                uint16_t raw = adc_buffer[i];
                uint8_t cur_y = (GRID_H - 4) - ((raw * (GRID_H - 4)) / 4095);
                int16_t px = GRID_X + 1 + i;

                if (cur_y != prev_y[i]) {
                    // Erase old point, restore grid color if on center line
                    uint16_t bg = (prev_y[i] == (GRID_H / 2)) ? COLOR_DARKGRAY : COLOR_NAVY;
                    st7789_draw_pixel(px, GRID_Y + 2 + prev_y[i], bg);

                    // Draw new point
                    st7789_draw_pixel(px, GRID_Y + 2 + cur_y, COLOR_YELLOW);
                    prev_y[i] = cur_y;
                }
            }

            // Periodically refresh Voltage Measurements (10Hz)
            if ((now - last_status_update) >= 100) {
                last_status_update = now;
                uint16_t avg_v = sum_v / (GRID_W - 2);
                uint32_t mv_avg = (avg_v * 3300) / 4095;
                uint32_t mv_pp = ((max_v - min_v) * 3300) / 4095;

                char buf[8];
                st7789_draw_string(275, 4, "Vpp:", COLOR_WHITE, COLOR_BLUE, 1);
                format_val_4d(mv_pp, buf);
                st7789_draw_string(275, 14, buf, COLOR_CYAN, COLOR_BLUE, 1);

                st7789_draw_string(230, 225, "Vavg:", COLOR_WHITE, COLOR_BLACK, 1);
                format_val_4d(mv_avg, buf);
                st7789_draw_string(270, 225, buf, COLOR_YELLOW, COLOR_BLACK, 1);
            }
        }
    }

    return 0;
}
