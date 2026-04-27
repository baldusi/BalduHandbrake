// ============================================================================
// BalduHandbrake.ino — Main Sketch
// ============================================================================
// Open Source Hydraulic Simracing Handbrake
// Copyright (c) 2026 Alejandro Belluscio
//
// Hardware: Waveshare ESP32-S3 Zero + ADS1115 ADC + SSD1351 OLED
//           + EC11 Rotary Encoder + Momentary Hold Button
//           + Ejoyous 0-500 PSI Transducer
//
// Architecture:
//   Core 0 — Sensor pipeline + HID output (1000 Hz deterministic)
//            Reads ADC, validates, applies deadzones + curves, sends USB HID
//   Core 1 — User interface (best-effort, ~30 Hz display refresh)
//            Encoder input, menu state machine, OLED drawing, NVS storage
//
// Shared state:
//   liveData      — Core 0 writes, Core 1 reads (display values)
//   activeConfig  — Core 0 reads (sensor parameters)
//   pendingConfig — Core 1 writes when user saves settings
//   configDirty   — flag: Core 0 picks up pendingConfig when true
//
// Project:  BalduHandbrake
// License:  Apache 2.0
// Author:   Alejandro Belluscio
// ============================================================================

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
//#include <Wire.h>

#include "config.h"
#include "sensor.h"
#include "hid.h"
#include "display.h"
#include "storage.h"
#include "ui.h"

// ============================================================================
//  Shared state between cores
// ============================================================================

// Core 0 writes every cycle, Core 1 reads for display.
// No mutex needed — single writer, cosmetic torn reads are harmless.
static volatile LiveData liveData;

// Active configuration — Core 0's working copy.
// Only updated from pendingConfig when configDirty is set.
static DeviceConfig activeConfig;

// Pending configuration — Core 1 writes here when user saves.
static DeviceConfig pendingConfig;

// Flag: set by Core 1, cleared by Core 0 after copying.
static volatile bool configDirty = false;

// ============================================================================
//  HID sampling rate monitor — pure integer math
// ============================================================================
void monitorHIDRate(uint32_t now_us) {
    static uint32_t last_us       = 0;
    static uint32_t deviant_count = 0;
    static uint32_t min_period    = 1000;
    static uint32_t max_period    = 1000;
    static uint32_t sample_count  = 0;

    if (last_us == 0) {
        last_us = now_us;
        return;
    }

    uint32_t delta_us = now_us - last_us;
    last_us = now_us;

    if (delta_us < 900 || delta_us > 1100) {
        deviant_count++;
    }

    if (delta_us < min_period) min_period = delta_us;
    if (delta_us > max_period) max_period = delta_us;

    sample_count++;

    if (sample_count >= 1000) {
        uint32_t max_hz = 1000000UL / min_period;
        uint32_t min_hz = 1000000UL / max_period;

        Serial.printf("[HID] Samples: %u | Deviations (>10%%): %u | Freq range: %u–%u Hz\n",
                      sample_count, deviant_count, min_hz, max_hz);

        deviant_count = 0;
        min_period    = 1000;
        max_period    = 1000;
        sample_count  = 0;
    }
}

// ============================================================================
//  Core 0 Task — Sensor Pipeline + HID (1000 Hz)
// ============================================================================
// This task is pinned to Core 0 and runs at the highest practical priority.
// It must not block, allocate memory, or call any SPI/OLED functions.

void sensorTask(void* param) {
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // --- Pick up config changes from UI (Core 1) ---
        if (configDirty) {
            activeConfig = pendingConfig;
            configDirty = false;
            sensorUpdateDataRate(activeConfig.sampleRateHz);
            // Note: if curveIndex changed, the next sensorUpdate() will
            // use the new curve automatically (LUT is indexed per-call).
        }

        // --- Run sensor pipeline ---
        // Cast away volatile for the function call — sensorUpdate writes
        // all fields atomically enough for display purposes.
        LiveData localData;
        sensorUpdate(activeConfig, localData);

        // --- Run HID update (includes hold button debounce) ---
        unsigned long nowMs = millis();
        hidUpdate(localData.axisOutput, activeConfig, localData, nowMs);
       
        //Quick function for checking consistency.
        //monitorHIDRate(micros());

        // --- Publish to shared state for Core 1 ---
        // Volatile write — field-by-field is fine, no mutex needed.
        LiveData* dest = (LiveData*)&liveData;
        dest->rawAdc           = localData.rawAdc;
        dest->centiVolts       = localData.centiVolts;
        dest->centiUnit        = localData.centiUnit;
        dest->centiPercent     = localData.centiPercent;
        dest->axisOutput       = localData.axisOutput;
        dest->holdActive       = localData.holdActive;
        dest->sensorLow        = localData.sensorLow;
        dest->sensorFail       = localData.sensorFail;
        dest->sensorSaturation = localData.sensorSaturation;

        // --- Precise 1ms timing (overflow-safe) ---
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
    }
}

// ============================================================================
//  Core 1 Task — User Interface (~30 Hz effective)
// ============================================================================
// This task is pinned to Core 1. It handles encoder input, menu navigation,
// OLED drawing, and NVS storage. Display redraws can take 10-50ms but
// this doesn't affect sensor timing because it's on a separate core.

void uiTask(void* param) {
    
    for (;;) {
        unsigned long nowMs = millis();

        // Read shared state (volatile → local copy for consistency)
        LiveData localData;
        const volatile LiveData* src = &liveData;
        localData.rawAdc           = src->rawAdc;
        localData.centiVolts       = src->centiVolts;
        localData.centiUnit        = src->centiUnit;
        localData.centiPercent     = src->centiPercent;
        localData.axisOutput       = src->axisOutput;
        localData.holdActive       = src->holdActive;
        localData.sensorLow        = src->sensorLow;
        localData.sensorFail       = src->sensorFail;
        localData.sensorSaturation = src->sensorSaturation;

        // Run UI cycle
        uiUpdate(localData, activeConfig, pendingConfig, configDirty, nowMs);

        // Yield — don't hog Core 1. 1ms gives the RTOS scheduler room
        // to run WiFi/BT tasks if ever needed, and matches encoder
        // polling rate.
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ============================================================================
//  setup() — Hardware initialization (runs on Core 1 by default)
// ============================================================================
// Initialization order matters:
//   1. HID (TinyUSB) — must be first, before any other USB activity
//   2. Serial — for debug output
//   3. I2C + ADC — sensor hardware
//   4. SPI + OLED — display hardware
//   5. NVS — load saved configuration
//   6. UI — encoder setup and state machine init
//   7. Create FreeRTOS tasks
//   8. Delete the Arduino loop task (we don't use it)

void setup() {
    // --- 1. HID (must be first) ---
    hidInit();

    // --- 2. Serial debug ---
    Serial.begin(115200);
    delay(2200);
    // Brief wait for serial — non-blocking, won't hang if no terminal
    unsigned long serialWait = millis();
    while (!Serial && (millis() - serialWait) < 500) { }
    Serial.println("\n===== BalduHandbrake Initializing =====");
	Serial.print(STR_BOOT_VERSION);
	Serial.print(FW_VERSION);
	Serial.print(" (build ");
	Serial.print(FW_BUILD);
	Serial.println(")");
    Serial.print("Sensor Mode: ");
    #ifdef SENSOR_PRESSURE_TRANSDUCER
        Serial.println("pressure transducer");
    #elif defined(SENSOR_LOAD_CELL)
        Serial.println("load cell");
    #endif

    // --- 3. I2C + ADC ---
    sensorBusInit();

    if (sensorInit()) {
        Serial.print(ADS_SENSOR_NAME);
        Serial.println(": OK");
    } else {
        Serial.print(ADS_SENSOR_NAME);
        Serial.println(": NOT FOUND");
    }

    // --- 4. SPI + OLED ---
	// Be carefull if you ever implement a SPI ADC, SPI2_HOST is used by the display, use SPI3_HOST for the ADC.
    displayInit();
    Serial.println("SSD1351: OK");

    // --- 5. NVS — load configuration ---
    if (storageInit()) {
        Serial.println("NVS: OK");
    } else {
        Serial.println("NVS: INIT FAILED");
    }

    // Load last-used profile, or defaults if none saved
    uint8_t lastProfile = storageGetLastProfile();
    if (!storageLoadProfile(lastProfile, activeConfig)) {
        activeConfig = getDefaultConfig();
        Serial.println("Config: using defaults");
    } else {
        Serial.print("Config: loaded profile ");
        Serial.println(lastProfile);
    }
    // Initialize pending to match active
    pendingConfig = activeConfig;
    sensorUpdateDataRate(activeConfig.sampleRateHz);

    // --- Show boot screen (uses loaded language setting) ---
    displayBootScreen(activeConfig.language);

    // --- 6. UI init ---
    uiInit();
    Serial.println("UI: OK");

    // --- Print startup summary ---
    Serial.print("Curve: ");
    Serial.println(CURVE_NAMES[activeConfig.curveIndex]);
    Serial.print("Hold mode: ");
    Serial.println(activeConfig.holdMode == HOLD_FIRMWARE ? "Firmware" : "Game");
    Serial.print("Deadzones: ");
    Serial.print(activeConfig.deadzoneLow);
    Serial.print(" / ");
    Serial.println(activeConfig.deadzoneHigh);
    Serial.print("Sample rate: ");
    Serial.print(activeConfig.sampleRateHz);
    Serial.println(" Hz");

    // --- 7. Create FreeRTOS tasks ---
    Serial.println("Starting FreeRTOS tasks...");

    xTaskCreatePinnedToCore(
        sensorTask,     // Task function
        "sensor",       // Name (for debugging)
        6144,           // Stack size (bytes) — sensor pipeline is lean
        NULL,           // Parameter (unused)
        6,              // Priority (high — time-critical)
        NULL,           // Task handle (unused)
        0               // Core 0
    );

    xTaskCreatePinnedToCore(
        uiTask,         // Task function
        "ui",           // Name
        8192,           // Stack size — UI + OLED lib needs more stack
        NULL,           // Parameter
        2,              // Priority (lower than sensor)
        NULL,           // Task handle
        1               // Core 1
    );

    Serial.println("===== BalduHandbrake Ready =====");

    // --- 8. Delete Arduino's loop task ---
    // setup() and loop() run as a FreeRTOS task on Core 1.
    // We've created our own tasks, so this one is no longer needed.
    vTaskDelete(NULL);
}

// ============================================================================
//  loop() — Not used (tasks handle everything)
// ============================================================================
void loop() {
    // Never reached — setup() deletes this task after creating
    // the sensor and UI tasks. If somehow reached, just yield.
    vTaskDelay(portMAX_DELAY);
}
