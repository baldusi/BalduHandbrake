/*  BalduHandrake
	Open Source Hydraulic Simracing Handbrake
	Copyright (c) 2026 Alejandro Belluscio
	Additional copyright holders listed inline below.
	This file is licensed under the Apache 2.0 license
	Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// config.h — BalduHandbrake System Configuration
// ============================================================================
// This file contains ALL hardware definitions, default parameters, and shared
// data types for the BalduHandbrake project. If you are building your own
// handbrake, this is the primary file you need to edit to match your hardware.
// Hardware: Waveshare ESP32-S3 Zero + [ADS1115 || ADS122C04] + SSD1351 + EC11 Encoder
// ============================================================================
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
//  FIRMWARE VERSION
// ============================================================================
// Update this for every release. Format: MAJOR.MINOR.PATCH
#define FW_VERSION          "v1.5.2"
#define FW_BUILD            __DATE__ " " __TIME__   // or use a CI build number later

// ============================================================================
//  USB IDENTITY
// ============================================================================
#define USB_SERIAL_STR       "BalduHandbrake-v1"
#define USB_PRODUCT_STR      "Baldu Handbrake"
#define USB_MANUFACTURER_STR "Baldu Handbrake Project"

// ============================================================================
//  ADC HARDWARE SELECTION — uncomment exactly one
// ============================================================================
// ADC
#define ADC_ADS1115
//#define ADC_ADS122C04  //Preliminary work to support the ADS122C04
//#define ADC_NAU7802    //Preliminary support of NAU7802 for load cells
//#define ADC_HX711      //Preliminary support of HX711 for load cells

// Sensor type
#define SENSOR_PRESSURE_TRANSDUCER
//#define SENSOR_LOAD_CELL

// ============================================================================
//  PIN ASSIGNMENTS — Waveshare ESP32-S3 Zero
// ============================================================================

//Hardware Version 1.0
//#define HOLD_BUTTON_PIN       4
//#define ROTARY_CHANNEL_A_PIN  5
//#define ROTARY_CHANNEL_B_PIN  13
//#define ROTARY_BUTTON_PIN     3
//#define OLED_CS               10
//#define OLED_DC               6
//#define OLED_RST              7
//#define OLED_SCK              12
//#define OLED_MOSI             11
//#define OLED_MISO             -1
//#define I2C_SDA_PIN           8
//#define I2C_SCL_PIN           9
//#define ADS_DRDY_PIN          -1 //For ADS122C04 or other ADS with a Data Ready line. -1 means not connected

/*
//Hardware Version 1.1
//This version of the connection was made in such a way that no signal
//trace crosses. If you have separate VCC and GND planes, you can use a
//single layer for signals. And in a 2 layer PCB, you minimize your need
//for vias and using the bottom plane to cross the VCC.
*/

#ifdef ADC_ADS122C04
    #define ADS_DRDY_PIN            1
#else
    #define ADS_DRDY_PIN            -1
#endif

#define I2C_SCL_PIN           2
#define I2C_SDA_PIN           3
#define ROTARY_CHANNEL_A_PIN  4
#define ROTARY_CHANNEL_B_PIN  5
#define ROTARY_BUTTON_PIN     6
#define OLED_MOSI             13
#define OLED_SCK              12
#define OLED_MISO             -1 //11
#define OLED_CS               10
#define OLED_DC               9
#define OLED_RST              8
#define HOLD_BUTTON_PIN       7



// ============================================================================
//  ADC ELECTRONIC PARAMETERS
// ============================================================================
//Address parameters defined here to keep all hardware configuration here.
#ifdef ADC_ADS1115
    #define ADS1115_ADDR          0x48
#elif defined(ADC_ADS122C04)
    #define ADS122C04_ADDR        0x40
#elif defined(ADC_NAU7802)
    // NAU7802 has fixed hardware address 0x2A — no configuration needed
#elif defined(ADC_HX711)
    // HX711 uses GPIO, not I2C — no address needed
    // H711 GPIO assignments for Hardware V1.0
    #define HX711_DOUT_PIN        8     // Data out (was I2C_SDA_PIN)
    #define HX711_SCK_PIN         9     // Clock (was I2C_SCL_PIN)
    // H711 GPIO assignments for Hardware V1.1
    //#define HX711_SCK_PIN         3     // Clock (was I2C_SCL_PIN) 
    //#define HX711_DOUT_PIN        2     // Data out (was I2C_SDA_PIN)
#endif

#define I2C_WIRE_SPEED        400000

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
//  SENSOR PHYSICAL CONSTANTS
// ============================================================================
// Configure these to match your hardware. Only one block is active at a time.

#ifdef SENSOR_LOAD_CELL
    // ADC_SPEC_FULL depends on gain — see calculation guide below.
    // counts = sensitivity_V_per_V × gain × 32768  (ratiometric mode)
    // Example: 0.002 V/V × 128 × 32768 = 8389
    //
    // CHECK FOR OVERFLOW:
    //   amplified_mV = sensitivity_mV_per_V × excitation_V × gain
    //   This must be LESS than the ADC reference voltage:
    //     ADS122C04 AVDD ref:  amplified_mV < AVDD (3300 or 5000 mV)
    //     ADS122C04 internal:  amplified_mV < 2048 mV
    //     NAU7802 LDO ref:     amplified_mV < LDO setting (3300 mV)
    //     HX711 channel A:     amplified_mV < AVDD (always, gain is fixed)
    //
    //   Example: 3 mV/V cell, 3.3V excitation, gain 128
    //     3 × 3.3 × 128 = 1267 mV — OK (< 3300 mV)
    //
    //   Example: 3 mV/V cell, 5V excitation, gain 128
    //     3 × 5.0 × 128 = 1920 mV — OK if AVDD ref (< 5000 mV)
    //                             — OVERFLOW if internal 2.048V ref!
    //
    //   If overflow: reduce gain. Try 64:
    //     3 × 5.0 × 64 = 960 mV — OK for all references
    #define SENSOR_ADC_GAIN             128     // PGA gain (1, 2, 4, 8, 16, 32, 64, 128)

    // ====================================================================
    //  LOAD CELL CONFIGURATION
    // ====================================================================
    // To calculate ADC_SPEC_FULL for your load cell and ADC combination:
    //
    // Step 1: Find your cell's full-scale output voltage
    //   full_scale_mV = sensitivity_mV_per_V × excitation_V
    //   Example: 2 mV/V cell with 5V excitation → 2 × 5 = 10 mV
    //
    // Step 2: Find your ADC's 16-bit input range (after >>8 from 24-bit)
    //   The 16-bit signed range (±32767) maps to the ADC's full input range.
    //   The input range depends on the ADC's reference voltage and gain:
    //
    //   ADS122C04 (gain 128, AVDD ratiometric reference):
    //     Ratiometric mode — counts are independent of supply voltage.
    //     counts = sensitivity_V_per_V × gain × 32768
    //     Example: 0.002 V/V × 128 × 32768 = 8389
    //     This value is the same whether AVDD is 3.3V or 5V,
    //     as long as the load cell excitation comes from AVDD.
    //
    //   NAU7802 (gain 128, AVDD ref via LDO 3.3V):
    //     input_range = ±3.3V / 128 = ±25.78 mV
    //     counts = full_scale_mV / 25.78 × 32768
    //     Example: 6.6 mV / 25.78 × 32768 = 8389
    //     NOTE: If cell is powered from AVDD, measurement is ratiometric
    //     and the counts formula simplifies to:
    //     counts = sensitivity_mV_per_V × 32768 / 128 = sensitivity × 256
    //
    //   HX711 (gain 128, no internal ref, channel A):
    //     input_range = ±20 mV
    //     counts = full_scale_mV / 20.0 × 32768
    //     Example: 10 mV / 20.0 × 32768 = 16384
    //
    // Step 3: Set ADC_SPEC_FULL to the value from Step 2.
    //   Set ADC_SPEC_ZERO to 0 (bridge at rest outputs ~0 mV).
    //
    // Step 4: Set fault thresholds near the 16-bit rails (±32767).
    //   These detect open-circuit conditions (disconnected cell).
    //   Keep them above ADC_SPEC_FULL with enough margin for overload.
    //
    // Step 5: Set SENSOR_UNIT_MAX to your cell's rated capacity.
    //   This is purely for the informational display — the game axis
    //   is driven by calibration (calAdcZero / calAdcMax), not this value.
    // ====================================================================
    #define SENSOR_UNIT_MAX             200     // kgf full scale
    #define ADC_SPEC_ZERO                 0     // bridge at rest (0 mV)
    #define ADC_SPEC_FULL              8389     // bridge at rated load
    #define ADC_FAIL_HIGH_THRESHOLD   31000     // positive rail: open circuit
    #define ADC_FAIL_LOW_THRESHOLD   -31000     // negative rail: open circuit
#else
    // Pressure transducer spec — Ejoyous 0–500 PSI
    // 0.5V = 0 PSI, 4.5V = 500 PSI (ratiometric, 5V supply)
    // ADS1115 at gain=0 (±6.144V): LSB = 0.1875 mV
    #define SENSOR_UNIT_MAX             500     // PSI full scale
    #define ADC_SPEC_ZERO              2667     // 0.50V: transducer zero
    #define ADC_SPEC_FULL             24000     // 4.50V: transducer full scale
    #define ADC_FAIL_THRESHOLD         1333     // below 0.25V: disconnected
    #define ADC_OVER_THRESHOLD        25707     // near VCC: saturation
#endif

// ============================================================================
//  CALIBRATION VALIDATION THRESHOLDS (used by ui.cpp)
// ============================================================================
// Unified names so calibration code doesn't need #ifdefs
#ifdef SENSOR_LOAD_CELL
    #define CALIB_REJECT_ZERO_LOW     ADC_FAIL_LOW_THRESHOLD
    #define CALIB_REJECT_MAX_HIGH     ADC_FAIL_HIGH_THRESHOLD
#else
    #define CALIB_REJECT_ZERO_LOW     ADC_FAIL_THRESHOLD
    #define CALIB_REJECT_MAX_HIGH     ADC_OVER_THRESHOLD
#endif

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
//  SENSOR FAULT FLAGS
// ============================================================================
enum SensorFault : uint8_t {
    FAULT_NONE         = 0x00,
    FAULT_DISCONNECTED = 0x01,
    FAULT_LOW          = 0x02,
    FAULT_SATURATION   = 0x04,
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
// NOTE: ADS1115 datasheet maximum is 860 SPS. The 1000 Hz entry is accepted
// for consistency with other ADCs but is silently mapped to 860 SPS (register 7).
// The sensor pipeline gracefully handles this via the "reuse last value" path
// when adcDataReady() returns true (always for ADS1115). This does not affect
// functionality thanks to the median + EMA filter.

#ifdef ADC_NAU7802
    #define DEFAULT_SAMPLE_RATE_HZ    320	//Maximum hardware capability
#elif defined(ADC_HX711)
    #define DEFAULT_SAMPLE_RATE_HZ     80	//Only supported sample rate
#else
    #define DEFAULT_SAMPLE_RATE_HZ   1000	//We consider this the standard
#endif
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
    uint32_t centiUnit;
    uint16_t centiPercent;
    uint16_t axisOutput;
    bool     holdActive;
    bool     sensorLow;         // Below calibrated zero (low pressure / negative force)
    bool     sensorFail;        // Sensor disconnected or open circuit
    bool     sensorSaturation;  // Over range (overpressure / rail saturation)
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
