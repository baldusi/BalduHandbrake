// ============================================================================
// ui.cpp — User Interface Implementation
// ============================================================================
// Runs entirely on Core 1. Owns the rotary encoder and menu state machine.
//
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
// ============================================================================

#include "ui.h"
#include "display.h"
#include "storage.h"
#include <RotaryEncoder.h>

// ============================================================================
//  Module-local state
// ============================================================================

// Encoder hardware
static RotaryEncoder encoder(ROTARY_CHANNEL_A_PIN, ROTARY_CHANNEL_B_PIN,
                              RotaryEncoder::LatchMode::FOUR3);
static int16_t lastEncoderPos = 0;

// Encoder button debounce
static unsigned long lastEncBtnDebounceTime = 0;
static int           lastEncBtnReading      = HIGH;
static bool          encBtnStable           = false;
static bool          encBtnPrevStable       = false;

// Button-as-modifier
static bool          encBtnHeld             = false;
static bool          btnUsedAsModifier      = false;

// UI navigation state
static UIState       uiState;
static UIState       prevUiState;
static bool          forceFullRedraw        = true;

// Working copy of config for editing
static DeviceConfig  editConfig;

// Edit screen count
static const uint8_t TOTAL_EDIT_SCREENS = 9;

static const uint8_t EDIT_SCREEN_BUTTONS[] = {
    2,   // 0: Hold Mode
    8,   // 1: Deadzones
    2,   // 2: Default Curve
    4,   // 3: Snap Threshold
    2,   // 4: Button Debounce
    4,   // 5: Refresh Rates
    2,   // 6: Calibrate
    7,   // 7: Save & Load
    2,   // 8: Language
};

// Display refresh timing
static unsigned long lastDisplayUpdate = 0;

// LIVE_DARK overlay timeout
static unsigned long curveOverlayStartMs = 0;
static bool          curveOverlayActive  = false;

// Save/Load screen state
static uint8_t       selectedProfileSlot = 0;
static bool          profileSlotExists[NUM_NVS_PROFILES];

// Calibration state
static CalibData     calibData;
static unsigned long calibSampleInterval = 0;
static unsigned long lastCalibSampleMs   = 0;

// ============================================================================
//  uiInit()
// ============================================================================
void uiInit() {
    encoder.setPosition(0);
    lastEncoderPos = 0;
    pinMode(ROTARY_BUTTON_PIN, INPUT_PULLUP);

    uiState.state           = DISPLAY_LIVE;
    uiState.menuScrollPos   = 0;
    uiState.editScreenIndex = 0;
    uiState.liveScreen      = LIVE_FULL_DATA;

    prevUiState = uiState;
    forceFullRedraw = true;

    calibData.state = CALIB_IDLE;
    calibData.sampleCount = 0;
    calibData.errorMsg = NULL;
    calibData.settleRingIdx = 0;
    calibData.settleStableCount = 0;
    calibData.settleRunningSum = 0;
    calibData.settleBufferFull = false;
}

// ============================================================================
//  Encoder button debounce
// ============================================================================
static void updateEncoderButton(unsigned long nowMs, uint8_t debounceMs) {
    int reading = digitalRead(ROTARY_BUTTON_PIN);
    if (reading != lastEncBtnReading) lastEncBtnDebounceTime = nowMs;
    lastEncBtnReading = reading;
    encBtnPrevStable = encBtnStable;
    if ((nowMs - lastEncBtnDebounceTime) >= debounceMs)
        encBtnStable = (reading == LOW);
    encBtnHeld = encBtnStable;
}

static bool encBtnJustReleased() {
    return !encBtnStable && encBtnPrevStable;
}

// ============================================================================
//  Parameter editing
// ============================================================================
static void backupEditParams(const DeviceConfig& cfg) {
    editConfig = cfg;
}

static void commitEditParams(DeviceConfig& pendingConfig,
                              volatile bool& configDirty) {
    pendingConfig = editConfig;
    configDirty = true;
}

static uint16_t clampU16(int32_t val, uint16_t lo, uint16_t hi) {
    if (val < (int32_t)lo) return lo;
    if (val > (int32_t)hi) return hi;
    return (uint16_t)val;
}

static uint8_t clampU8(int16_t val, uint8_t lo, uint8_t hi) {
    if (val < (int16_t)lo) return lo;
    if (val > (int16_t)hi) return hi;
    return (uint8_t)val;
}

static bool handleAdjustment(uint8_t screenIndex, uint8_t buttonIndex) {
    switch (screenIndex) {
        case 0:
            if (buttonIndex == 4) { editConfig.holdMode = HOLD_GAME; return true; }
            if (buttonIndex == 5) { editConfig.holdMode = HOLD_FIRMWARE; return true; }
            break;
        case 1:
            if (buttonIndex == 4)  { editConfig.deadzoneLow  = clampU16(editConfig.deadzoneLow  + 10, 0, 200); return true; }
            if (buttonIndex == 5)  { editConfig.deadzoneLow  = clampU16(editConfig.deadzoneLow  +  1, 0, 200); return true; }
            if (buttonIndex == 6)  { editConfig.deadzoneLow  = clampU16((int32_t)editConfig.deadzoneLow  -  1, 0, 200); return true; }
            if (buttonIndex == 7)  { editConfig.deadzoneLow  = clampU16((int32_t)editConfig.deadzoneLow  - 10, 0, 200); return true; }
            if (buttonIndex == 8)  { editConfig.deadzoneHigh = clampU16(editConfig.deadzoneHigh + 10, 0, 200); return true; }
            if (buttonIndex == 9)  { editConfig.deadzoneHigh = clampU16(editConfig.deadzoneHigh +  1, 0, 200); return true; }
            if (buttonIndex == 10) { editConfig.deadzoneHigh = clampU16((int32_t)editConfig.deadzoneHigh -  1, 0, 200); return true; }
            if (buttonIndex == 11) { editConfig.deadzoneHigh = clampU16((int32_t)editConfig.deadzoneHigh - 10, 0, 200); return true; }
            break;
        case 2:
            if (buttonIndex == 4) { editConfig.curveIndex = (editConfig.curveIndex + NUM_CURVES - 1) % NUM_CURVES; return true; }
            if (buttonIndex == 5) { editConfig.curveIndex = (editConfig.curveIndex + 1) % NUM_CURVES; return true; }
            break;
        case 3:
            if (buttonIndex == 4) { editConfig.snapThreshold = clampU16(editConfig.snapThreshold + 10, 0, 1000); return true; }
            if (buttonIndex == 5) { editConfig.snapThreshold = clampU16(editConfig.snapThreshold +  1, 0, 1000); return true; }
            if (buttonIndex == 6) { editConfig.snapThreshold = clampU16((int32_t)editConfig.snapThreshold -  1, 0, 1000); return true; }
            if (buttonIndex == 7) { editConfig.snapThreshold = clampU16((int32_t)editConfig.snapThreshold - 10, 0, 1000); return true; }
            break;
        case 4:
            if (buttonIndex == 4) { editConfig.debounceMs = clampU8(editConfig.debounceMs + 1, 5, 200); return true; }
            if (buttonIndex == 5) { editConfig.debounceMs = clampU8(editConfig.debounceMs - 1, 5, 200); return true; }
            break;
        case 5: {
            static const uint16_t sRates[] = { 250, 475, 860, 1000 };
            static const uint8_t nSR = sizeof(sRates) / sizeof(sRates[0]);
            static const uint16_t dRates[] = { 10, 15, 20, 30 };
            static const uint8_t nDR = sizeof(dRates) / sizeof(dRates[0]);
            if (buttonIndex == 4 || buttonIndex == 5) {
                uint8_t idx = 0;
                for (uint8_t i = 0; i < nSR; i++) if (sRates[i] == editConfig.sampleRateHz) { idx = i; break; }
                idx = (buttonIndex == 4) ? (idx + 1) % nSR : (idx + nSR - 1) % nSR;
                editConfig.sampleRateHz = sRates[idx]; return true;
            }
            if (buttonIndex == 6 || buttonIndex == 7) {
                uint8_t idx = 0;
                for (uint8_t i = 0; i < nDR; i++) if (dRates[i] == editConfig.displayRateHz) { idx = i; break; }
                idx = (buttonIndex == 6) ? (idx + 1) % nDR : (idx + nDR - 1) % nDR;
                editConfig.displayRateHz = dRates[idx]; return true;
            }
            break;
        }
        case 7:
            if (buttonIndex >= 4 && buttonIndex < 4 + NUM_NVS_PROFILES) {
                selectedProfileSlot = buttonIndex - 4; return true;
            }
            break;
        case 8:
            if (buttonIndex >= 4 && buttonIndex < 4 + NUM_LANGUAGES) {
                editConfig.language = buttonIndex - 4; return true;
            }
            break;
    }
    return false;
}

// ============================================================================
//  Calibration — settling detection
// ============================================================================
// Uses a ring buffer to track the last CALIB_STABILITY_COUNT readings.
// Stability is declared when all readings in the buffer fall within
// CALIB_STABILITY_BAND_PCT of their average, AND the average is in
// the correct zone (near zero for zero-cal, above floor for max-cal).

static void calibResetSettling() {
    calibData.settleRingIdx = 0;
    calibData.settleStableCount = 0;
    calibData.settleRunningSum = 0;
    calibData.settleBufferFull = false;
}

static void calibReset() {
    calibData.state = CALIB_IDLE;
    calibData.sampleCount = 0;
    calibData.errorMsg = NULL;
    calibResetSettling();
}

// Feed a new ADC reading into the settling detector.
// Returns true when stability has been achieved.
static bool calibSettleFeed(uint16_t adcVal, bool isZero,
                             const DeviceConfig& cfg) {
    // Compute span from current calibration (or defaults)
    uint16_t span = cfg.calAdcMax - cfg.calAdcZero;
    if (span == 0) span = 1;  // safety

    // Zone check: is the reading in the expected region?
    bool inZone;
    if (isZero) {
        // Zero cal: reading must be below calAdcZero + ceiling% of span
        uint16_t ceiling = cfg.calAdcZero
                           + (uint16_t)((uint32_t)span * CALIB_ZERO_CEILING_PCT / 100);
        inZone = (adcVal < ceiling);
    } else {
        // Max cal: reading must be above calAdcZero + floor% of span
        uint16_t floor = cfg.calAdcZero
                         + (uint16_t)((uint32_t)span * CALIB_MAX_FLOOR_PCT / 100);
        inZone = (adcVal > floor);
    }

    if (!inZone) {
        // Reset stability counter — user isn't in the right zone yet
        calibData.settleStableCount = 0;
        return false;
    }

    // Update ring buffer
    uint8_t idx = calibData.settleRingIdx;

    // If buffer is full, subtract the value we're about to overwrite
    if (calibData.settleBufferFull) {
        calibData.settleRunningSum -= calibData.settleRingBuf[idx];
    }

    calibData.settleRingBuf[idx] = adcVal;
    calibData.settleRunningSum += adcVal;
    calibData.settleRingIdx = (idx + 1) % CALIB_STABILITY_COUNT;

    if (!calibData.settleBufferFull && calibData.settleRingIdx == 0) {
        calibData.settleBufferFull = true;
    }

    // Need a full buffer before we can check stability
    if (!calibData.settleBufferFull) return false;

    // Check stability: all values in buffer must be within band of average
    uint16_t avg = (uint16_t)(calibData.settleRunningSum / CALIB_STABILITY_COUNT);
    uint16_t band = (uint16_t)((uint32_t)span * CALIB_STABILITY_BAND_PCT / 100);

    bool allStable = true;
    for (uint8_t i = 0; i < CALIB_STABILITY_COUNT; i++) {
        uint16_t val = calibData.settleRingBuf[i];
        if (val > avg + band || val < avg - band) {
            allStable = false;
            break;
        }
    }

    if (allStable) {
        calibData.settleStableCount++;
    } else {
        calibData.settleStableCount = 0;
    }

    return (calibData.settleStableCount >= 1);
    // Note: settleStableCount >= 1 means the ENTIRE buffer is within band.
    // The buffer itself represents CALIB_STABILITY_COUNT consecutive readings.
    // So this is already requiring CALIB_STABILITY_COUNT stable samples.
}

// ============================================================================
//  Calibration — sample processing
// ============================================================================
static void sortU16(uint16_t* arr, uint16_t count) {
    for (uint16_t i = 1; i < count; i++) {
        uint16_t key = arr[i];
        int16_t j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static uint16_t processCalibSamples(uint16_t* samples, uint16_t count,
                                     float rejectPct) {
    sortU16(samples, count);
    uint16_t reject = (uint16_t)(count * rejectPct);
    uint16_t start = reject;
    uint16_t end = count - reject;
    if (end <= start) { start = 0; end = count; }
    uint32_t sum = 0;
    for (uint16_t i = start; i < end; i++) sum += samples[i];
    return (uint16_t)(sum / (end - start));
}

// ============================================================================
//  Calibration — state machine update
// ============================================================================
static void calibStartSettling(bool isZero) {
    calibResetSettling();
    calibData.state = isZero ? CALIB_SETTLING_ZERO : CALIB_SETTLING_MAX;
    forceFullRedraw = true;
}

static void calibStartSampling(bool isZero, unsigned long nowMs) {
    calibData.sampleCount = 0;
    calibData.samplingStartMs = nowMs;
    calibSampleInterval = CALIB_SAMPLE_DURATION_MS / CALIB_SAMPLE_COUNT;
    lastCalibSampleMs = nowMs;
    calibData.state = isZero ? CALIB_SAMPLING_ZERO : CALIB_SAMPLING_MAX;
    forceFullRedraw = true;
}

static void calibUpdate(const LiveData& liveData,
                         const DeviceConfig& activeConfig,
                         unsigned long nowMs) {
    switch (calibData.state) {
        // --- Settling phases ---
        case CALIB_SETTLING_ZERO:
        case CALIB_SETTLING_MAX: {
            bool isZero = (calibData.state == CALIB_SETTLING_ZERO);
            if (calibSettleFeed(liveData.rawAdc, isZero, editConfig)) {
                // Settled — begin sample collection
                calibStartSampling(isZero, nowMs);
            }
            // Periodically update display to show stability progress
            static unsigned long lastSettleRedraw = 0;
            if ((nowMs - lastSettleRedraw) >= 200) {
                lastSettleRedraw = nowMs;
                forceFullRedraw = true;
            }
            break;
        }

        // --- Sampling phases ---
        case CALIB_SAMPLING_ZERO:
        case CALIB_SAMPLING_MAX: {
            unsigned long elapsed = nowMs - calibData.samplingStartMs;
            bool isZero = (calibData.state == CALIB_SAMPLING_ZERO);

            if (elapsed >= CALIB_SAMPLE_DURATION_MS) {
                // Sampling complete — process
                uint16_t result = processCalibSamples(
                    calibData.samples, calibData.sampleCount,
                    CALIB_OUTLIER_REJECT_PCT);

                calibData.resultCentiVolts =
                    (uint16_t)(((uint32_t)result * 1875UL + 50000UL) / 100000UL);

                if (isZero) {
                    if (result < ADC_FAIL_THRESHOLD) {
                        calibData.state = CALIB_ERROR;
                        calibData.errorMsg = displayGetString(STR_CAL_PURGE,
                                                               editConfig.language);
                    } else {
                        calibData.resultAdcZero = result;
                        calibData.state = CALIB_RESULT_ZERO;
                    }
                } else {
                    if (result > ADC_OVER_THRESHOLD) {
                        calibData.state = CALIB_ERROR;
                        calibData.errorMsg = displayGetString(STR_CAL_OVERPRESS,
                                                               editConfig.language);
                    } else if (result <= calibData.resultAdcZero + 500) {
                        calibData.state = CALIB_ERROR;
                        calibData.errorMsg = displayGetString(STR_CAL_TOO_LOW,
                                                               editConfig.language);
                    } else {
                        calibData.resultAdcMax = result;
                        calibData.state = CALIB_RESULT_MAX;
                    }
                }
                forceFullRedraw = true;
            } else if (calibData.sampleCount < CALIB_SAMPLE_COUNT) {
                if ((nowMs - lastCalibSampleMs) >= calibSampleInterval) {
                    calibData.samples[calibData.sampleCount] = liveData.rawAdc;
                    calibData.sampleCount++;
                    lastCalibSampleMs = nowMs;
                    forceFullRedraw = true;
                }
            }
            break;
        }

        default:
            break;
    }
}

// ============================================================================
//  State machine: encoder click handler
// ============================================================================
static void handleEncoderClick(const DeviceConfig& activeConfig,
                                DeviceConfig& pendingConfig,
                                volatile bool& configDirty,
                                unsigned long nowMs) {
    switch (uiState.state) {
        case DISPLAY_LIVE:
            uiState.state = DISPLAY_MENU_LIST;
            uiState.menuScrollPos = 3;
            forceFullRedraw = true;
            break;

        case DISPLAY_MENU_LIST:
            if (uiState.menuScrollPos == 0) {
                uiState.editScreenIndex = (uiState.editScreenIndex + TOTAL_EDIT_SCREENS - 1) % TOTAL_EDIT_SCREENS;
                forceFullRedraw = true;
            } else if (uiState.menuScrollPos == 1) {
                uiState.editScreenIndex = (uiState.editScreenIndex + 1) % TOTAL_EDIT_SCREENS;
                forceFullRedraw = true;
            } else if (uiState.menuScrollPos == 2) {
                uiState.state = DISPLAY_LIVE;
                forceFullRedraw = true;
            } else if (uiState.menuScrollPos == 3) {
                uiState.state = DISPLAY_EDIT_VALUE;
                uiState.menuScrollPos = 4;
                backupEditParams(activeConfig);
                if (uiState.editScreenIndex == 6) calibReset();
                else if (uiState.editScreenIndex == 7) {
                    selectedProfileSlot = 0;
                    for (uint8_t i = 0; i < NUM_NVS_PROFILES; i++)
                        profileSlotExists[i] = storageProfileExists(i);
                }
                forceFullRedraw = true;
            }
            break;

        case DISPLAY_EDIT_VALUE:
            if (uiState.menuScrollPos == 2) {
                // S (Save)
                if (uiState.editScreenIndex == 6 && calibData.state == CALIB_DONE) {
                    editConfig.calAdcZero = calibData.resultAdcZero;
                    editConfig.calAdcMax  = calibData.resultAdcMax;
                }
                commitEditParams(pendingConfig, configDirty);
                uiState.state = DISPLAY_MENU_LIST;
                uiState.menuScrollPos = 3;
                forceFullRedraw = true;
            } else if (uiState.menuScrollPos == 3) {
                // X (Discard)
                if (uiState.editScreenIndex == 6) calibReset();
                uiState.state = DISPLAY_MENU_LIST;
                uiState.menuScrollPos = 3;
                forceFullRedraw = true;
            } else {
                uint8_t btnIdx = uiState.menuScrollPos;

                if (uiState.editScreenIndex == 6) {
                    // Calibration state transitions
                    switch (calibData.state) {
                        case CALIB_IDLE:
                        case CALIB_PROMPT_ZERO:
                            calibStartSettling(true);
                            break;
                        case CALIB_RESULT_ZERO:
                            calibData.state = CALIB_PROMPT_MAX;
                            forceFullRedraw = true;
                            break;
                        case CALIB_PROMPT_MAX:
                            calibStartSettling(false);
                            break;
                        case CALIB_RESULT_MAX:
                            calibData.state = CALIB_DONE;
                            forceFullRedraw = true;
                            break;
                        case CALIB_ERROR:
                            calibReset();
                            forceFullRedraw = true;
                            break;
                        default: break;
                    }
                } else if (uiState.editScreenIndex == 7) {
                    if (btnIdx == 4 + NUM_NVS_PROFILES) {
                        storageSaveProfile(selectedProfileSlot, editConfig);
                        profileSlotExists[selectedProfileSlot] = true;
                        forceFullRedraw = true;
                    } else if (btnIdx == 4 + NUM_NVS_PROFILES + 1) {
                        if (storageLoadProfile(selectedProfileSlot, editConfig))
                            forceFullRedraw = true;
                    } else {
                        handleAdjustment(uiState.editScreenIndex, btnIdx);
                        forceFullRedraw = true;
                    }
                } else {
                    if (handleAdjustment(uiState.editScreenIndex, btnIdx))
                        forceFullRedraw = true;
                }
            }
            break;
    }
}

// ============================================================================
//  State machine: encoder rotation handler
// ============================================================================
static void handleEncoderRotation(int16_t delta,
                                   const DeviceConfig& activeConfig,
                                   DeviceConfig& pendingConfig,
                                   volatile bool& configDirty) {
    switch (uiState.state) {
        case DISPLAY_LIVE:
            if (encBtnHeld) {
                int16_t ns = (int16_t)uiState.liveScreen + delta;
                uiState.liveScreen = (uint8_t)((ns + NUM_LIVE_SCREENS) % NUM_LIVE_SCREENS);
                editConfig = activeConfig;
                editConfig.liveScreen = uiState.liveScreen;
                pendingConfig = editConfig;
                configDirty = true;
                btnUsedAsModifier = true;
                forceFullRedraw = true;
            } else {
                int16_t nc = (int16_t)activeConfig.curveIndex + delta;
                uint8_t next = (uint8_t)((nc + NUM_CURVES) % NUM_CURVES);
                editConfig = activeConfig;
                editConfig.curveIndex = next;
                pendingConfig = editConfig;
                configDirty = true;
                if (uiState.liveScreen == LIVE_DARK) {
                    displayShowCurveOverlay(next);
                    curveOverlayActive = true;
                    curveOverlayStartMs = millis();
                } else {
                    forceFullRedraw = true;
                }
            }
            break;

        case DISPLAY_MENU_LIST:
            uiState.menuScrollPos = (uint8_t)(
                ((int16_t)uiState.menuScrollPos + delta + NAV_BOX_COUNT) % NAV_BOX_COUNT);
            break;

        case DISPLAY_EDIT_VALUE: {
            uint8_t numBtns = EDIT_SCREEN_BUTTONS[uiState.editScreenIndex];
            uint8_t total = numBtns + 2;
            int16_t pos = (int16_t)uiState.menuScrollPos - 2;
            pos = ((pos + delta) % total + total) % total;
            uiState.menuScrollPos = (uint8_t)(pos + 2);
            break;
        }
    }
}

// ============================================================================
//  Display update dispatcher
// ============================================================================
static void updateDisplay(const LiveData& liveData,
                           const DeviceConfig& activeConfig,
                           unsigned long nowMs) {
    // LIVE_DARK overlay timeout
    if (curveOverlayActive && uiState.liveScreen == LIVE_DARK) {
        if ((nowMs - curveOverlayStartMs) >= activeConfig.liveDarkTimeoutMs) {
            displaySetupLiveDark();
            curveOverlayActive = false;
        }
    }

    // Full redraw
    if (forceFullRedraw) {
        forceFullRedraw = false;
        switch (uiState.state) {
            case DISPLAY_LIVE:
                switch (uiState.liveScreen) {
                    case LIVE_FULL_DATA:
                        displaySetupLiveFull(activeConfig);
                        displayUpdateHoldIndicator(liveData.holdActive, activeConfig.language);
                        break;
                    case LIVE_CLEAN:  displaySetupLiveClean(activeConfig.language); break;
                    case LIVE_BAR_ONLY: displaySetupLiveBar(); break;
                    case LIVE_DARK:   displaySetupLiveDark(); break;
                }
                break;
            case DISPLAY_MENU_LIST:
                displayDrawNavBar(uiState);
                displayDrawEditScreen(uiState, editConfig, liveData);
                break;
            case DISPLAY_EDIT_VALUE:
                displayDrawNavBar(uiState);
                if (uiState.editScreenIndex == 6)
                    displayDrawCalibrate(uiState, calibData, editConfig.language);
                else if (uiState.editScreenIndex == 7)
                    displayDrawSaveLoad(uiState, selectedProfileSlot, profileSlotExists, editConfig.language);
                else if (uiState.editScreenIndex == 8)
                    displayDrawLanguage(uiState, editConfig.language);
                else
                    displayDrawEditScreen(uiState, editConfig, liveData);
                break;
        }
        return;
    }

    // Incremental nav cursor update
    if (uiState.state != DISPLAY_LIVE &&
        uiState.menuScrollPos != prevUiState.menuScrollPos)
        displayUpdateNavCursor(uiState, prevUiState.menuScrollPos);

    // Periodic live data refresh
    if (uiState.state == DISPLAY_LIVE) {
        uint16_t interval = 1000 / activeConfig.displayRateHz;
        if ((nowMs - lastDisplayUpdate) >= interval) {
            lastDisplayUpdate = nowMs;
            switch (uiState.liveScreen) {
                case LIVE_FULL_DATA: displayUpdateLiveFull(liveData, activeConfig.language); break;
                case LIVE_CLEAN:     displayUpdateLiveClean(liveData, activeConfig.language); break;
                case LIVE_BAR_ONLY:  displayUpdateLiveBar(liveData, activeConfig.language); break;
                case LIVE_DARK:      break;
            }
        }
    }
}

// ============================================================================
//  uiUpdate() — main entry point
// ============================================================================
void uiUpdate(const LiveData& liveData,
              const DeviceConfig& activeConfig,
              DeviceConfig& pendingConfig,
              volatile bool& configDirty,
              unsigned long nowMs) {

    prevUiState = uiState;

    // Sync liveScreen from config
    if (uiState.state == DISPLAY_LIVE && uiState.liveScreen != activeConfig.liveScreen) {
        uiState.liveScreen = activeConfig.liveScreen;
        forceFullRedraw = true;
    }

    // Encoder rotation
    encoder.tick();
    int16_t currentPos = encoder.getPosition();
    int16_t delta = currentPos - lastEncoderPos;
    if (delta != 0) {
        lastEncoderPos = currentPos;
        handleEncoderRotation(delta, activeConfig, pendingConfig, configDirty);
    }

    // Encoder button
    updateEncoderButton(nowMs, activeConfig.debounceMs);
    if (encBtnJustReleased()) {
        if (btnUsedAsModifier)
            btnUsedAsModifier = false;
        else
            handleEncoderClick(activeConfig, pendingConfig, configDirty, nowMs);
    }

    // Calibration update
    if (uiState.editScreenIndex == 6 && uiState.state == DISPLAY_EDIT_VALUE)
        calibUpdate(liveData, activeConfig, nowMs);

    // Display
    updateDisplay(liveData, activeConfig, nowMs);
}
