/*  BalduHandrake
    Open Source Hydraulic Simracing Handbrake
    Copyright (c) 2026 Alejandro Belluscio
    Additional copyright holders listed inline below.
    This file is licensed under the Apache 2.0 license
    Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// sensor.h — Sensor Pipeline Interface
// ============================================================================
// Handles ADC reading, signal validation, deadzone application, curve
// correction, and normalization. Runs on Core 0 at ~1000 Hz.
//
// Usage:
//   sensorInit()   — call once during setup, after Wire.begin()
//   sensorUpdate() — call every sample cycle; reads ADC, runs pipeline,
//                     writes results to the shared LiveData struct
// ============================================================================
#ifndef SENSOR_H
#define SENSOR_H

#include "config.h"

#ifdef ADC_ADS1115
    #define ADS_SENSOR_NAME "ADS1115"
#elif defined(ADC_ADS122C04)
    #define ADS_SENSOR_NAME "ADS122C04"
#elif defined(ADC_NAU7802)
    #define ADS_SENSOR_NAME "NAU7802"
#endif

// ----------------------------------------------------------------------------
//  Public API
// ----------------------------------------------------------------------------

// Initialize ADS1115: sets gain, data rate, continuous mode, first read.
// Returns true if the ADC was found and responded.
bool sensorInit();

// Run the full sensor pipeline once:
//   1. Read cached ADC value (continuous mode)
//   2. Validate reading (fail / low / over / normal)
//   3. Compute informational values (voltage, PSI)
//   4. Apply deadzones to determine effective range
//   5. Apply curve correction via LUT
//   6. Normalize to Z-axis output (0–4095)
//   7. Compute display percentage
//   8. Write all results to the provided LiveData struct
//
// Takes the active config by const reference (Core 0's copy).
void sensorUpdate(const DeviceConfig& cfg, LiveData& out);

// Apply curve correction to a normalized value (0–4095).
// Exposed separately so it can be unit-tested or reused.
uint16_t applyCurveCorrection(uint16_t normValue, uint8_t curveIndex,
                              uint16_t snapThreshold);

// --- ADC rate abstraction ---
extern const uint16_t SENSOR_RATE_OPTIONS[];
extern const uint8_t  SENSOR_RATE_COUNT;

void sensorUpdateDataRate(uint16_t sampleRateHz);

#endif // SENSOR_H
