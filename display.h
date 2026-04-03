// ============================================================================
// display.h — OLED Display Interface
// ============================================================================
// LIMITATION: 7-bit ASCII only. See notes in display.cpp.
//
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
// ============================================================================
#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

// ============================================================================
//  String IDs
// ============================================================================
enum StringID : uint8_t {
    STR_BOOT_TITLE = 0, STR_BOOT_VERSION, STR_BOOT_AUTHOR,
    STR_BOOT_PROJECT, STR_BOOT_STATUS,
    STR_HOLD, STR_LIVE, STR_PSI, STR_VOLTS, STR_PERCENT,
    STR_FAIL, STR_LOW, STR_OVER,
    STR_GAME, STR_FIRMWARE,
    STR_SAVE, STR_LOAD, STR_EMPTY, STR_SAVED, STR_PROFILE,
    STR_CAL_PUSH_DOWN, STR_CAL_PULL_UP, STR_CAL_SETTLING, STR_CAL_HOLD_STEADY,
    STR_CAL_SAMPLING, STR_CAL_DONE, STR_CAL_ZERO_OK, STR_CAL_MAX_OK,
    STR_CAL_ERROR, STR_CAL_RETRY, STR_CAL_PURGE, STR_CAL_OVERPRESS, STR_CAL_TOO_LOW,
    STR_TITLE_HOLD_MODE, STR_TITLE_DEADZONES, STR_TITLE_DEFAULT_CURVE,
    STR_TITLE_SNAP_THRESH, STR_TITLE_DEBOUNCE, STR_TITLE_REFRESH,
    STR_TITLE_CALIBRATE, STR_TITLE_SAVE_LOAD, STR_TITLE_LANGUAGE,
    STR_LABEL_LOW, STR_LABEL_HIGH, STR_LABEL_USB_ADC, STR_LABEL_DISPLAY,
    STR_LABEL_MS, STR_LABEL_HZ,
    STR_CAL_REDO, STR_CAL_NEXT,
    NUM_STRINGS
};

// ============================================================================
//  UI State
// ============================================================================
struct UIState {
    DisplayState state;
    uint8_t      menuScrollPos;
    uint8_t      editScreenIndex;
    uint8_t      liveScreen;
};

// ============================================================================
//  Initialization
// ============================================================================
void displayInit();
void displayBootScreen(uint8_t language);

// ============================================================================
//  Localization
// ============================================================================
const char* displayGetString(StringID id, uint8_t language);

// ============================================================================
//  Live Screens
// ============================================================================
void displaySetupLiveFull(const DeviceConfig& cfg);
void displayUpdateLiveFull(const LiveData& data, const DeviceConfig& cfg);

void displaySetupLiveClean(const DeviceConfig& cfg);
void displayUpdateLiveClean(const LiveData& data, const DeviceConfig& cfg);

void displaySetupLiveBar();
void displayUpdateLiveBar(const LiveData& data, const DeviceConfig& cfg);

void displaySetupLiveDark();
void displayShowCurveOverlay(uint8_t curveIndex);
void displayUpdateLiveDark(const LiveData& data, uint8_t language);

void displayUpdateHoldIndicator(bool holdActive, uint8_t language);

// ============================================================================
//  Navigation Bar
// ============================================================================
void displayDrawNavBar(const UIState& ui);
void displayUpdateNavCursor(const UIState& ui, uint8_t prevScrollPos);

// ============================================================================
//  Edit Screens
// ============================================================================
void displayDrawEditScreen(const UIState& ui, const DeviceConfig& cfg,
                           const LiveData& data);
void displayDrawHoldMode(const UIState& ui, const DeviceConfig& cfg);
void displayDrawDeadzones(const UIState& ui, const DeviceConfig& cfg);
void displayDrawDefaultCurve(const UIState& ui, const DeviceConfig& cfg);
void displayDrawSnapThreshold(const UIState& ui, const DeviceConfig& cfg);
void displayDrawButtonDebounce(const UIState& ui, const DeviceConfig& cfg);
void displayDrawRefreshRates(const UIState& ui, const DeviceConfig& cfg);
void displayDrawCalibrate(const UIState& ui, const CalibData& calib, uint8_t language);
void displayDrawSaveLoad(const UIState& ui, uint8_t selectedSlot,
                         const bool slotExists[NUM_NVS_PROFILES], uint8_t language);
void displayDrawLanguage(const UIState& ui, uint8_t currentLanguage);

// ============================================================================
//  Utility
// ============================================================================
void displayClearContentArea();
void displayDrawEditTitle(const char* title);

#endif // DISPLAY_H
