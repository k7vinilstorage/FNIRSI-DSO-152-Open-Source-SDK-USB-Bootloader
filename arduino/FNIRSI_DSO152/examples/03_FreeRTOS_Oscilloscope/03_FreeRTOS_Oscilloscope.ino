/**
 * @file 03_FreeRTOS_Oscilloscope.ino
 * @brief Multi-tasking FreeRTOS Oscilloscope for FNIRSI DSO-152
 *
 * Demonstrates using STM32duino FreeRTOS (<STM32FreeRTOS.h>) alongside
 * the FNIRSI_DSO152 library and Adafruit_GFX.
 *
 * Tasks:
 *   1. TaskCalGen (Priority 1): Toggles PA0 to generate a 1 kHz square wave
 *      test signal on the top calibration tab.
 *   2. TaskButtons (Priority 2): Polls buttons at 100 Hz, manages voltage range
 *      (10mV/div .. 20V/div), AC/DC coupling, trigger pause, USB DFU entry,
 *      and soft power-off.
 *   3. TaskScopeDisplay (Priority 2): Captures 300-sample waveforms from the
 *      analog front-end, computes Vmin/Vmax/Vpp, and renders the graticule
 *      and waveform on the ST7789 display.
 *
 * Required Libraries (install via Arduino Library Manager):
 *   - "STM32duino FreeRTOS" by Richard Barry / STMicroelectronics
 *   - "Adafruit GFX Library" by Adafruit
 */

#include <FNIRSI_DSO152.h>
#include <STM32FreeRTOS.h>

// Shared oscilloscope state protected by a FreeRTOS mutex
struct ScopeState {
  dso152_range_t range;
  bool acCoupling;
  uint16_t delayLoops;
  bool paused;
  uint16_t trigLevel;
};

static ScopeState gState = {
  AFE_RANGE_1V,    // 1V/div default
  false,           // DC coupling
  20,              // Delay loops per sample
  false,           // Running
  2048             // Midpoint trigger level
};

static SemaphoreHandle_t xStateMutex = NULL;
static uint16_t rawSamples[300];
static uint8_t prevY[300];

static const uint16_t DELAY_STEPS[] = { 0, 5, 10, 20, 50, 100, 250 };
static const char* const TIMEBASE_NAMES[] = {
  "10us", "25us", "50us", "100us", "250us", "500us", "1ms"
};
static uint8_t timebaseIdx = 3;

// ---------------------------------------------------------------------------
// Task 1: Square Wave Calibration Signal Generator on PA0
// ---------------------------------------------------------------------------
void TaskCalGen(void *pvParameters) {
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    dso.toggleCalPin();
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
  }
}

// ---------------------------------------------------------------------------
// Task 2: Button Input & Power Management Task (100 Hz)
// ---------------------------------------------------------------------------
void TaskButtons(void *pvParameters) {
  (void)pvParameters;
  DSO152_Buttons prevBtn = {};
  uint8_t pwrHoldTicks = 0;

  for (;;) {
    DSO152_Buttons btn = dso.readButtons();

    // Power-off detection: Hold POWER (PC11) for ~1.0 second
    if (btn.pwr) {
      pwrHoldTicks++;
      if (pwrHoldTicks > 100) {
        vTaskSuspendAll();
        dso.fillRect(40, 90, 240, 50, COLOR_RED);
        dso.drawRect(40, 90, 240, 50, COLOR_WHITE);
        dso.drawString(72, 111, "SHUTTING DOWN...", COLOR_WHITE, COLOR_RED, 1);
        delay(300);
        dso.powerOff();
      }
    } else {
      pwrHoldTicks = 0;
    }

    // Hold OK/MENU + AUTO to jump to USB Mass Storage Bootloader ("DSO BOOT")
    if (btn.ok && btn.auto_btn) {
      vTaskSuspendAll();
      dso.fillRect(30, 90, 260, 50, COLOR_BLUE);
      dso.drawRect(30, 90, 260, 50, COLOR_WHITE);
      dso.drawString(52, 111, "REBOOTING TO DSO BOOT...", COLOR_WHITE, COLOR_BLUE, 1);
      delay(300);
      dso.rebootToBootloader();
    }

    // Rising-edge button actions protected by mutex
    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (btn.up && !prevBtn.up) {
        if (gState.range < AFE_RANGE_20V) {
          gState.range = (dso152_range_t)(gState.range + 1);
          dso.setRange(gState.range);
        }
      }
      if (btn.down && !prevBtn.down) {
        if (gState.range > AFE_RANGE_10mV) {
          gState.range = (dso152_range_t)(gState.range - 1);
          dso.setRange(gState.range);
        }
      }
      if (btn.left && !prevBtn.left) {
        if (timebaseIdx > 0) timebaseIdx--;
        gState.delayLoops = DELAY_STEPS[timebaseIdx];
      }
      if (btn.right && !prevBtn.right) {
        if (timebaseIdx < 6) timebaseIdx++;
        gState.delayLoops = DELAY_STEPS[timebaseIdx];
      }
      if (btn.mode && !prevBtn.mode) {
        gState.acCoupling = !gState.acCoupling;
        dso.setCoupling(gState.acCoupling);
      }
      if (btn.run && !prevBtn.run) {
        gState.paused = !gState.paused;
      }
      if (btn.auto_btn && !prevBtn.auto_btn) {
        gState.range = AFE_RANGE_1V;
        timebaseIdx = 3;
        gState.delayLoops = DELAY_STEPS[timebaseIdx];
        dso.setRange(gState.range);
      }
      xSemaphoreGive(xStateMutex);
    }

    prevBtn = btn;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------------------------------------------------------------------
// Task 3: Waveform Acquisition & Display Rendering Task
// ---------------------------------------------------------------------------
static void drawGridLinePixel(int16_t x, int16_t y) {
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

void TaskScopeDisplay(void *pvParameters) {
  (void)pvParameters;

  // Draw static frame once
  dso.fillScreen(COLOR_BLACK);
  dso.fillRect(0, 0, 320, 22, COLOR_NAVY);
  dso.fillRect(0, 214, 320, 26, COLOR_DARKGRAY);
  dso.drawRect(9, 27, 302, 182, COLOR_GRAY);

  for (int16_t x = 10; x < 310; x++) {
    for (int16_t y = 28; y < 208; y++) {
      drawGridLinePixel(x, y);
    }
  }
  for (uint16_t i = 0; i < 300; i++) {
    prevY[i] = 118;
  }

  for (;;) {
    ScopeState local;
    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      local = gState;
      xSemaphoreGive(xStateMutex);
    }

    if (!local.paused) {
      // Capture 300 triggered samples
      dso.captureWaveform(rawSamples, 300, local.delayLoops, local.trigLevel);

      uint16_t vMinRaw = 4095, vMaxRaw = 0;
      uint32_t sum = 0;
      for (uint16_t i = 0; i < 300; i++) {
        uint16_t s = rawSamples[i];
        if (s < vMinRaw) vMinRaw = s;
        if (s > vMaxRaw) vMaxRaw = s;
        sum += s;
      }
      uint16_t mid = sum / 300;

      // Erase old trace and compute new Y positions
      for (uint16_t i = 0; i < 300; i++) {
        int16_t x = 10 + i;
        uint8_t oldY0 = prevY[i];
        uint8_t oldY1 = (i + 1 < 300) ? prevY[i + 1] : oldY0;
        uint8_t yA = (oldY0 < oldY1) ? oldY0 : oldY1;
        uint8_t yB = (oldY0 > oldY1) ? oldY0 : oldY1;
        for (uint8_t y = yA; y <= yB; y++) {
          drawGridLinePixel(x, y);
        }

        int32_t centered = (int32_t)rawSamples[i] - (int32_t)mid;
        int32_t yNew = 118 - (centered * 80) / 2048;
        if (yNew < 29) yNew = 29;
        if (yNew > 206) yNew = 206;
        prevY[i] = (uint8_t)yNew;
      }

      // Draw new trace
      for (uint16_t i = 0; i < 299; i++) {
        int16_t x = 10 + i;
        uint8_t y0 = prevY[i];
        uint8_t y1 = prevY[i + 1];
        uint8_t yA = (y0 < y1) ? y0 : y1;
        uint8_t yB = (y0 > y1) ? y0 : y1;
        dso.drawFastVLine(x, yA, (yB - yA) + 1, COLOR_YELLOW);
      }

      int32_t vppMv = ((int32_t)(vMaxRaw - vMinRaw) * 3300) / 4095;
      int32_t vAvgMv = ((int32_t)mid * 3300) / 4095;

      char footerBuf[40];
      snprintf(footerBuf, sizeof(footerBuf), "Vpp:%4ldmV Vavg:%4ldmV RTOS:3Tasks",
               (long)vppMv, (long)vAvgMv);
      dso.drawString(8, 223, footerBuf, COLOR_CYAN, COLOR_DARKGRAY, 1);
    }

    // Header status bar
    char hdrBuf[40];
    snprintf(hdrBuf, sizeof(hdrBuf), "DSO152 %-7s %s %-5s %s",
             dso.getRangeName(local.range),
             local.acCoupling ? "AC" : "DC",
             TIMEBASE_NAMES[timebaseIdx],
             local.paused ? "HOLD" : "RUN ");
    dso.drawString(6, 7, hdrBuf, COLOR_WHITE, COLOR_NAVY, 1);

    vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS UI refresh
  }
}

void setup() {
  dso.begin(1);
  dso.setRange(gState.range);
  dso.setCoupling(gState.acCoupling);

  xStateMutex = xSemaphoreCreateMutex();

  // Create FreeRTOS tasks
  xTaskCreate(TaskCalGen,       "CalGen",  128, NULL, 1, NULL);
  xTaskCreate(TaskButtons,      "Buttons", 256, NULL, 2, NULL);
  xTaskCreate(TaskScopeDisplay, "Display", 512, NULL, 2, NULL);

  // Start FreeRTOS preemptive scheduler
  vTaskStartScheduler();
}

void loop() {
  // Execution is managed by the FreeRTOS scheduler
}
