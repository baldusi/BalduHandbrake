// ============================================================================
// strings.h — Localized String Table
// ============================================================================
// Extracted from display.h/.cpp to allow non-display modules to access
// localized strings without depending on the display subsystem.
//
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
// ============================================================================
#ifndef STRTABLE_H
#define STRTABLE_H

#include "config.h"

// ============================================================================
//  String IDs
// ============================================================================
enum StringID : uint8_t {
  STR_BOOT_TITLE = 0,
	STR_BOOT_VERSION,
	STR_BOOT_AUTHOR,
  STR_BOOT_PROJECT,
	STR_BOOT_STATUS,
  STR_HOLD,
	STR_LIVE,
	STR_PSI,
  STR_FORCE,
	STR_VOLTS,
	STR_PERCENT,
  STR_FAIL,
	STR_LOW,
	STR_OVER,
  STR_GAME,
	STR_FIRMWARE,
  STR_SAVE,
	STR_LOAD,
	STR_EMPTY,
	STR_SAVED,
	STR_PROFILE,
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
  STR_TITLE_HOLD_MODE,
	STR_TITLE_DEADZONES,
	STR_TITLE_DEFAULT_CURVE,
  STR_TITLE_SNAP_THRESH,
	STR_TITLE_DEBOUNCE,
	STR_TITLE_REFRESH,
  STR_TITLE_CALIBRATE,
	STR_TITLE_LANGUAGE,
	STR_TITLE_QUICK_SAVE,
	STR_TITLE_SAVE_LOAD,
  STR_QUICK_SAVE_HINT,
	STR_QUICK_SAVE_DONE,
  STR_LABEL_LOW,
	STR_LABEL_HIGH,
	STR_LABEL_USB_ADC,
	STR_LABEL_DISPLAY,
  STR_LABEL_MS,
	STR_LABEL_HZ,
  STR_CAL_REDO,
	STR_CAL_NEXT,
  NUM_STRINGS
};

// ============================================================================
//  Unit abstraction — display code uses STR_UNIT everywhere
// ============================================================================
#ifdef SENSOR_LOAD_CELL
    #define STR_UNIT  STR_FORCE
#else
    #define STR_UNIT  STR_PSI
#endif

// ============================================================================
//  Language names (for the language selection screen)
// ============================================================================
extern const char* const LANG_NAMES[NUM_LANGUAGES];

// ============================================================================
//  Public API
// ============================================================================
const char* strGet(StringID id, uint8_t language);

#endif // STRINGS_H