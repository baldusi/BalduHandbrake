// ============================================================================
// display.h — OLED Display Interface
// ============================================================================
// All SSD1351 OLED drawing functions. Runs on Core 1.
//
// LIMITATION: The SSD1351 with Adafruit_GFX only supports 7-bit ASCII.
// Accented characters and non-Latin scripts are not available.
// Translations should use unaccented approximations.
// To add full Unicode support, replace Adafruit_GFX with u8g2 or a
// custom font renderer.
//
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
// ============================================================================
#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

// ============================================================================
//  String IDs — index into the localization string table
// ============================================================================
enum StringID : uint8_t {
    // Boot screen
    STR_BOOT_TITLE = 0,
    STR_BOOT_VERSION,
    STR_BOOT_AUTHOR,
    STR_BOOT_PROJECT,
    STR_BOOT_STATUS,

    // Live view
    STR_HOLD,
    STR_LIVE,
    STR_PSI,
    STR_VOLTS,
    STR_PERCENT,

    // Sensor status
    STR_FAIL,
    STR_LOW,
    STR_OVER,

    // Hold mode screen
    STR_GAME,
    STR_FIRMWARE,

    // Save/Load screen
    STR_SAVE,
    STR_LOAD,
    STR_EMPTY,
    STR_SAVED,
    STR_PROFILE,

    // Calibration
    STR_CAL_PUSH_DOWN,
    STR_CAL_PULL_UP,
    STR_CAL_SETTLING,
    STR_CAL_HOLD_STEADY,
    STR_CAL_SAMPLING,
    STR_CAL_DONE,
    STR_CAL_ZERO_OK,
    STR_CAL_MAX_OK,
    STR_CAL_ERROR,
    STR_CAL_RETRY,
    STR_CAL_PURGE,
    STR_CAL_OVERPRESS,
    STR_CAL_TOO_LOW,

    // Edit screen titles
    STR_TITLE_HOLD_MODE,
    STR_TITLE_DEADZONES,
    STR_TITLE_DEFAULT_CURVE,
    STR_TITLE_SNAP_THRESH,
    STR_TITLE_DEBOUNCE,
    STR_TITLE_REFRESH,
    STR_TITLE_CALIBRATE,
    STR_TITLE_SAVE_LOAD,
    STR_TITLE_LANGUAGE,

    // Labels
    STR_LABEL_LOW,
    STR_LABEL_HIGH,
    STR_LABEL_USB_ADC,
    STR_LABEL_DISPLAY,
    STR_LABEL_MS,
    STR_LABEL_HZ,

    NUM_STRINGS
};

// ============================================================================
//  UI State — passed from ui.cpp to display functions
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
void displayUpdateLiveFull(const LiveData& data, uint8_t language);

void displaySetupLiveClean(uint8_t language);
void displayUpdateLiveClean(const LiveData& data, uint8_t language);

void displaySetupLiveBar();
void displayUpdateLiveBar(const LiveData& data, uint8_t language);

void displaySetupLiveDark();
void displayShowCurveOverlay(uint8_t curveIndex);

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
void displayDrawCalibrate(const UIState& ui, const CalibData& calib,
                          uint8_t language);
void displayDrawSaveLoad(const UIState& ui, uint8_t selectedSlot,
                         const bool slotExists[NUM_NVS_PROFILES],
                         uint8_t language);
void displayDrawLanguage(const UIState& ui, uint8_t currentLanguage);

// ============================================================================
//  Utility
// ============================================================================
void displayClearContentArea();
void displayDrawEditTitle(const char* title);

#endif // DISPLAY_H
