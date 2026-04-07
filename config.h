// ============================================================================
// config.h — BalduHandbrake System Configuration
// ============================================================================
// This file contains ALL hardware definitions, default parameters, and shared
// data types for the BalduHandbrake project. If you are building your own
// handbrake, this is the primary file you need to edit to match your hardware.
//
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
// Hardware: Waveshare ESP32-S3 Zero + ADS1115 + SSD1351 + EC11 Encoder
// ============================================================================
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
//  USB IDENTITY
// ============================================================================
#define USB_SERIAL_STR       "BalduHandbrake-v1"
#define USB_PRODUCT_STR      "Baldu Handbrake"
#define USB_MANUFACTURER_STR "Baldu Handbrake Project"

// ============================================================================
//  PIN ASSIGNMENTS — Waveshare ESP32-S3 Zero
// ============================================================================
#define HOLD_BUTTON_PIN       4
#define ROTARY_CHANNEL_A_PIN  5
#define ROTARY_CHANNEL_B_PIN  13
#define ROTARY_BUTTON_PIN     3
#define OLED_CS               10
#define OLED_DC               6
#define OLED_RST              7
#define OLED_SCK              12
#define OLED_MOSI             11
#define I2C_SDA_PIN           8
#define I2C_SCL_PIN           9
#define ADS1115_ADDR          0x48

// ============================================================================
//  DISPLAY
// ============================================================================
#define SCREEN_WIDTH          128
#define SCREEN_HEIGHT         128

// ============================================================================
//  Z-AXIS (HID output range)
// ============================================================================
#define Z_AXIS_MIN            0
#define Z_AXIS_MAX            4095

// ============================================================================
//  ADC / TRANSDUCER HARDWARE CONSTANTS
// ============================================================================
// Ejoyous 0-500 psi transducer: 0.5V = 0 psi, 4.5V = 500 psi (ratiometric).
// ADS1115 at gain=0 (±6.144V): LSB = 0.1875 mV
#define TRANSDUCER_PSI_MIN        0
#define TRANSDUCER_PSI_MAX      500
#define TRANSDUCER_MV_PER_PSI     8

#define ADC_FAIL_THRESHOLD     1333     // Below 0.25V: transducer disconnected
#define ADC_SPEC_ZERO          2667     // 0.50V: transducer theoretical zero
#define ADC_SPEC_FULL         24000     // 4.50V: transducer theoretical full scale
#define ADC_OVER_THRESHOLD    25707     // 4.82V: near VCC, unreliable above

// ============================================================================
//  CURVE DEFINITIONS
// ============================================================================
static const char* const CURVE_NAMES[] = {
    "Linear",
    "Rally Soft",
    "Rally Aggr",
    "Drift Snap",
    "Wet",
    "S-Curve"
};
#define NUM_CURVES (sizeof(CURVE_NAMES) / sizeof(CURVE_NAMES[0]))

enum CurveID : uint8_t {
    CURVE_LINEAR      = 0,
    CURVE_RALLY_SOFT  = 1,
    CURVE_RALLY_AGGR  = 2,
    CURVE_DRIFT_SNAP  = 3,
    CURVE_WET         = 4,
    CURVE_S_CURVE     = 5
};

inline uint8_t curveToLutIndex(uint8_t curveId) {
    switch (curveId) {
        case CURVE_RALLY_SOFT:  return 0;
        case CURVE_RALLY_AGGR:  return 1;
        case CURVE_WET:         return 2;
        case CURVE_S_CURVE:     return 3;
        default:                return 0xFF;
    }
}

// ============================================================================
//  HOLD MODE
// ============================================================================
enum HoldMode : uint8_t {
    HOLD_GAME     = 0,
    HOLD_FIRMWARE = 1
};

// ============================================================================
//  LIVE DISPLAY MODES
// ============================================================================
enum LiveScreen : uint8_t {
    LIVE_FULL_DATA = 0,
    LIVE_CLEAN     = 1,
    LIVE_BAR_ONLY  = 2,
    LIVE_DARK      = 3
};
#define NUM_LIVE_SCREENS 4

// ============================================================================
//  LANGUAGE
// ============================================================================
enum Language : uint8_t {
    LANG_EN = 0,
    LANG_ES = 1,
    NUM_LANGUAGES
};
#define DEFAULT_LANGUAGE  LANG_EN

// ============================================================================
//  DISPLAY STATES (Menu State Machine)
// ============================================================================
enum DisplayState : uint8_t {
    DISPLAY_LIVE       = 0,
    DISPLAY_MENU_LIST  = 1,
    DISPLAY_EDIT_VALUE = 2
};

// ============================================================================
//  DEFAULT BEHAVIOR
// ============================================================================
#define DEFAULT_SAMPLE_RATE_HZ    1000
#define DEFAULT_DISPLAY_RATE_HZ     30
#define DEFAULT_DEBOUNCE_MS         50
#define DEFAULT_ERROR_RESET_MS     250
#define DEFAULT_DEADZONE_LOW        50
#define DEFAULT_DEADZONE_HIGH       50
#define DEFAULT_CURVE_INDEX          0
#define DEFAULT_SNAP_THRESHOLD     550
#define DEFAULT_HOLD_MODE            0
#define DEFAULT_LIVE_SCREEN          0
#define DEFAULT_LIVE_DARK_TIMEOUT  5000

// ============================================================================
//  GENERAL COLORS (RGB565 for SSD1351)
// ============================================================================
#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
#define COLOR_CYAN     0x07FF
#define COLOR_YELLOW   0xFFE0
#define COLOR_MAGENTA  0xF81F
#define COLOR_ORANGE   0xFD20

// ============================================================================
//  SEMANTIC COLORS — Override these to theme the display
// ============================================================================
#define LIVE_BG_COLOR       COLOR_BLACK
#define LIVE_FG_COLOR       COLOR_WHITE
#define LIVE_VALUE_COLOR    COLOR_CYAN
#define LIVE_LABEL_COLOR    COLOR_WHITE
#define LIVE_OK_COLOR       COLOR_GREEN
#define LIVE_WARN_COLOR     COLOR_YELLOW
#define LIVE_ERROR_COLOR    COLOR_RED

#define EDIT_BG_COLOR       COLOR_BLACK
#define EDIT_TITLE_COLOR    NAV_NORMAL_FG
#define EDIT_VALUE_COLOR    COLOR_CYAN
#define EDIT_LABEL_COLOR    COLOR_WHITE

// ============================================================================
//  NAV BAR LAYOUT
// ============================================================================
#define NAV_BOX_COUNT      4
#define NAV_BOX_W         32
#define NAV_BOX_H         20
#define NAV_SEPARATOR_PX   2

static const int16_t NAV_BOX_X[NAV_BOX_COUNT] = { 0, 96, 64, 32 };
static const int16_t NAV_BOX_Y[NAV_BOX_COUNT] = { 0,  0,  0,  0 };

#define NAV_NORMAL_BG    0x08A5
#define NAV_SELECTED_BG  0x03DF
#define NAV_NORMAL_FG    0x1B39
#define NAV_SELECTED_FG  0xFFFF

// ============================================================================
//  DEVICE CONFIGURATION (persisted to NVS per profile)
// ============================================================================
struct DeviceConfig {
    uint8_t  curveIndex;
    uint16_t snapThreshold;
    uint16_t deadzoneLow;
    uint16_t deadzoneHigh;
    uint8_t  holdMode;
    uint8_t  liveScreen;
    uint16_t liveDarkTimeoutMs;
    uint8_t  language;
    uint16_t sampleRateHz;
    uint16_t displayRateHz;
    uint8_t  debounceMs;
    uint16_t calAdcZero;
    uint16_t calAdcMax;
};

inline DeviceConfig getDefaultConfig() {
    DeviceConfig cfg;
    cfg.curveIndex        = DEFAULT_CURVE_INDEX;
    cfg.snapThreshold     = DEFAULT_SNAP_THRESHOLD;
    cfg.deadzoneLow       = DEFAULT_DEADZONE_LOW;
    cfg.deadzoneHigh      = DEFAULT_DEADZONE_HIGH;
    cfg.holdMode          = DEFAULT_HOLD_MODE;
    cfg.liveScreen        = DEFAULT_LIVE_SCREEN;
    cfg.liveDarkTimeoutMs = DEFAULT_LIVE_DARK_TIMEOUT;
    cfg.language          = DEFAULT_LANGUAGE;
    cfg.sampleRateHz      = DEFAULT_SAMPLE_RATE_HZ;
    cfg.displayRateHz     = DEFAULT_DISPLAY_RATE_HZ;
    cfg.debounceMs        = DEFAULT_DEBOUNCE_MS;
    cfg.calAdcZero        = ADC_SPEC_ZERO;
    cfg.calAdcMax         = ADC_SPEC_FULL;
    return cfg;
}

// ============================================================================
//  LIVE DATA (shared Core 0 → Core 1)
// ============================================================================
struct LiveData {
    uint16_t rawAdc;
    uint16_t centiVolts;
    uint32_t centiPsi;
    uint16_t centiPercent;
    uint16_t axisOutput;
    bool     holdActive;
    bool     pressureLow;
    bool     transducerFail;
    bool     saturationFail;
};

// ============================================================================
//  EDIT SCREEN INFRASTRUCTURE
// ============================================================================
enum ParamType : uint8_t {
    PARAM_NONE   = 0,
    PARAM_BOOL   = 1,
    PARAM_UINT8  = 2,
    PARAM_UINT16 = 3,
    PARAM_UINT32 = 4
};

#define MAX_EDIT_PARAMS 6

typedef void (*DrawFunc)(void);

struct EditScreen {
    const char* title;
    uint8_t     numParams;
    void*       paramPtrs[MAX_EDIT_PARAMS];
    ParamType   paramTypes[MAX_EDIT_PARAMS];
    uint8_t     numButtons;
    DrawFunc    drawFunc;
};

// ============================================================================
//  NVS PROFILE STORAGE
// ============================================================================
#define NUM_NVS_PROFILES  5
#define NVS_NAMESPACE     "bhbrake"

// ============================================================================
//  CALIBRATION STATE MACHINE
// ============================================================================
// The calibration routine runs across multiple UI update cycles.
// ui.cpp drives the state transitions; display.cpp renders the current step.
// Lives in config.h as a compromise — conceptually CalibState belongs to UI
// and CalibData to sensor, but both display and UI need them.

enum CalibState : uint8_t {
    CALIB_IDLE            = 0,   // Not calibrating
    CALIB_PROMPT_ZERO     = 1,   // "Push handle down" prompt
    CALIB_SETTLING_ZERO   = 2,   // Waiting for readings to stabilize at rest
    CALIB_SAMPLING_ZERO   = 3,   // Collecting 500 samples (5 seconds)
    CALIB_RESULT_ZERO     = 4,   // Show zero result, continue or retry
    CALIB_PROMPT_MAX      = 5,   // "Pull handle up" prompt
    CALIB_SETTLING_MAX    = 6,   // Waiting for readings to stabilize at max pull
    CALIB_SAMPLING_MAX    = 7,   // Collecting 500 samples (5 seconds)
    CALIB_RESULT_MAX      = 8,   // Show max result
    CALIB_DONE            = 9,   // Calibration complete, show summary
    CALIB_ERROR           = 10   // Error condition (bad readings)
};

// ============================================================================
//  CALIBRATION TUNING CONSTANTS
// ============================================================================
// These control the settling detection and sample collection behavior.
// Marked as PLACEHOLDER — tune these empirically with real hardware.
//
// Settling detection works as follows:
//   1. Readings must be in the correct zone (near zero for zero-cal,
//      above a minimum for max-cal)
//   2. A rolling window of the last CALIB_STABILITY_COUNT samples must
//      all fall within CALIB_STABILITY_BAND_PCT of their running average
//   3. Once both conditions are met, sampling begins automatically
//
// For zero calibration:
//   - "Correct zone" means below calAdcZero + CALIB_ZERO_CEILING_PCT of span
//   - This catches the case where the user hasn't fully released the handle
//
// For max calibration:
//   - "Correct zone" means above CALIB_MAX_FLOOR_PCT of span
//   - This ensures the user is actually pulling, not resting

// PLACEHOLDER values — adjust after testing with real hardware
#define CALIB_SAMPLE_COUNT        500   // Total samples to collect
#define CALIB_SAMPLE_DURATION_MS  5000  // Duration of sample collection window
#define CALIB_OUTLIER_REJECT_PCT  0.05f // Reject top and bottom 5% of samples

// Settling detection parameters
#define CALIB_STABILITY_COUNT     50    // Consecutive stable samples required (~300ms)
#define CALIB_STABILITY_BAND_PCT  5     // Max deviation as % of ADC span (PLACEHOLDER)
#define CALIB_ZERO_CEILING_PCT    15    // Zero readings must be below this % of span (PLACEHOLDER)
#define CALIB_MAX_FLOOR_PCT       30    // Max readings must be above this % of span (PLACEHOLDER)

struct CalibData {
    CalibState    state;

    // Sample collection
    uint16_t      samples[CALIB_SAMPLE_COUNT];
    uint16_t      sampleCount;
    unsigned long samplingStartMs;

    // Settling detection
    uint16_t      settleRingBuf[CALIB_STABILITY_COUNT];  // Rolling window
    uint8_t       settleRingIdx;                          // Current write position
    uint8_t       settleStableCount;                      // Consecutive stable readings
    uint32_t      settleRunningSum;                       // Sum of ring buffer values
    bool          settleBufferFull;                        // Ring buffer filled at least once

    // Results
    uint16_t      resultAdcZero;
    uint16_t      resultAdcMax;
    uint16_t      resultCentiVolts;   // Voltage of last result for display

    // Error
    const char*   errorMsg;           // Points into STRING_TABLE or NULL
};

#endif // CONFIG_H
