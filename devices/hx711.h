/*  BalduHandrake
    Open Source Hydraulic Simracing Handbrake
    Copyright (c) 2026 Alejandro Belluscio
    This file is licensed under the Apache 2.0 license
    Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// devices/hx711.h — HX711 load cell mode for sensor.cpp
// ============================================================================
// This file is #included directly into sensor.cpp. It is NOT a standalone
// header — do not include it from any other translation unit.
// ============================================================================
// SENSOR DEVICE DRIVER: HX711
// ============================================================================
// The HX711 is a 24-bit ADC designed for load cell / Wheatstone bridge
// applications. Unlike the other ADCs in this project, it uses a
// proprietary 2-wire GPIO protocol (DOUT + SCK), NOT I2C.
//
// Key limitations vs ADS122C04 / NAU7802:
//   - Fixed sample rate: 10 or 80 SPS (hardware pin, not software)
//   - No software-selectable sample rate — adcSetRate() is a no-op
//   - Gain is fixed at initialization (128 on channel A by default)
//   - No built-in LDO or voltage reference
//   - Only one HX711 per set of GPIO pins
//
// Despite these limitations, the HX711 is by far the most common and
// cheapest load cell amplifier available, making it a good entry point
// for hobbyists building a load cell handbrake on a budget.
// ============================================================================
// All sensor device drivers must define:
//  Preprocessor:
//      a) #include <DEVICE LIBRARY>
//  Variables:
//      1) ads                      // Handle of the sensor
//      2) ADC_REG_VALUES           // Code parameters for the allowed sample rates
//      3) SENSOR_RATE_OPTIONS      // SPS in numbers
//  Helper functions:
//      a) adcBusInit()             // Bus initialization (GPIO for HX711)
//      b) adcBegin()               // Initialization code
//      c) adcConfigure()           // Configuration code
//      d) adcDataReady()           // Duplicate read avoidance
//      e) adcRead()                // Value read, must be normalized to int16_t
//      f) adcSetRate()             // Sample rate setting (no-op for HX711)
//      g) adcRawToCentiVolts()     // Raw to true input voltage (gain-compensated)
//      h) adcRawToCentiUnit()      // Raw to physical unit (spec-based)
//      i) adcCheckFault()          // Sensor-specific fault detection

// Bogdan Necula HX711 library
#include <HX711.h>

static HX711 ads;

#if SENSOR_ADC_GAIN == 128
    #define HX711_GAIN_REG 128    // Channel A, gain 128, ±20mV
#elif SENSOR_ADC_GAIN == 64
    #define HX711_GAIN_REG 64     // Channel A, gain 64, ±40mV
#elif SENSOR_ADC_GAIN == 32
    #define HX711_GAIN_REG 32     // Channel B, gain 32, ±80mV
#else
    #error "Invalid SENSOR_ADC_GAIN for HX711. Valid: 32 (ch B), 64 (ch A), 128 (ch A)"
#endif


// ============================================================================
//  ADC rate table — single source of truth for supported sample rates
// ============================================================================
// HX711 rate is hardware-selected (RATE pin: LOW = 10 SPS, HIGH = 80 SPS).
// Software cannot change it. We expose only the expected rate so the UI
// shows something meaningful, but adcSetRate() is a no-op.
static const uint8_t ADC_REG_VALUES[] = { 0 };
const uint16_t SENSOR_RATE_OPTIONS[]  = { 80 };

// Full-resolution capture for unit conversion
static int32_t lastRaw24 = 0;

static void adcBusInit() {
    // HX711 uses GPIO, not I2C — pin setup happens in adcBegin()
}

static bool adcBegin() {
    // Initialize with DOUT and SCK pins, default gain 128 (channel A)
    ads.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    // Wait up to 1 second for the HX711 to become ready
    return ads.wait_ready_timeout(1000);
}

static void adcConfigure() {
    // Gain 128 on channel A is set by default in begin().
    // set_gain() also selects the channel:
    //   128 = channel A, gain 128 (±20mV input, default)
    //    64 = channel A, gain 64  (±40mV input)
    //    32 = channel B, gain 32  (±80mV input)
    ads.set_gain(HX711_GAIN_REG);
}

static bool adcDataReady() { return ads.is_ready(); }

static int16_t adcRead() {
    lastRaw24 = ads.read();
    return (int16_t)(lastRaw24 >> 8);
}

static void adcSetRate(uint8_t reg) {
    (void)reg;  // HX711 rate is hardware-selected, cannot be changed
}

// HX711 internal reference ~2.56V (fixed, independent of AVDD)
// Gain 128: ±20mV, Gain 64: ±40mV, Gain 32: ±80mV
// All satisfy: range = 2560mV / gain
// centi-mV = raw * (512000 / SENSOR_ADC_GAIN) / 65536
static uint16_t adcRawToCentiVolts(int16_t raw) {
    if (raw <= 0) return 0;
    return (uint16_t)(((uint32_t)raw * (512000UL / SENSOR_ADC_GAIN) + 32768UL) / 65536UL);
}

// 24-bit spec constants derived from 16-bit user config
static const int32_t SPEC_ZERO_24 = (int32_t)ADC_SPEC_ZERO << 8;
static const int32_t SPEC_FULL_24 = (int32_t)ADC_SPEC_FULL << 8;
static const int32_t SPEC_SPAN_24 = SPEC_FULL_24 - SPEC_ZERO_24;

static uint32_t adcRawToCentiUnit(int16_t raw) {
    (void)raw;  // use full-resolution capture instead
    if (lastRaw24 <= SPEC_ZERO_24) return 0;
    uint32_t offset = (uint32_t)(lastRaw24 - SPEC_ZERO_24);
    return (offset * (uint32_t)(SENSOR_UNIT_MAX * 100UL) + (SPEC_SPAN_24 / 2))
           / SPEC_SPAN_24;
}

static uint8_t adcCheckFault(int16_t raw) {
    // HX711 doesn't have rail detection like the ADS122C04.
    // If DOUT stays high indefinitely, is_ready() never returns true,
    // and adcDataReady() returns false — sensorUpdate reuses lastOut.
    // For active fault detection, check if raw is near the 24-bit rails.
    if (raw > (int16_t)ADC_FAIL_HIGH_THRESHOLD)  return FAULT_DISCONNECTED;
    if (raw < (int16_t)ADC_FAIL_LOW_THRESHOLD)   return FAULT_DISCONNECTED;
    return FAULT_NONE;
}