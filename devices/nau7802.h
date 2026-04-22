/*  BalduHandrake
    Open Source Hydraulic Simracing Handbrake
    Copyright (c) 2026 Alejandro Belluscio
    This file is licensed under the Apache 2.0 license
    Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// devices/nau7802.h — NAU7802 load cell mode for sensor.cpp
// ============================================================================
// This file is #included directly into sensor.cpp. It is NOT a standalone
// header — do not include it from any other translation unit.
// ============================================================================
// SENSOR DEVICE DRIVER: NAU7802
// ============================================================================
// The NAU7802 is a 24-bit ADC designed specifically for load cell / 
// Wheatstone bridge applications. It includes a built-in PGA (up to 128x),
// an internal LDO, and differential inputs. Max sample rate is 320 SPS.
//
// Fixed I2C address: 0x2A (hardware-defined, cannot be changed).
// Multiple NAU7802 on the same bus require an I2C multiplexer.
//
// IMPORTANT: The NAU7802 requires an internal AFE calibration after any
// change to gain, sample rate, or channel. adcSetRate() handles this
// automatically.
// ============================================================================
// All sensor device drivers must define:
//  Preprocessor:
//		a) #include <DEVICE LIBRARY>
//	Variables
//		1) ads 						//Handle of the sensor
// 		1) ADC_REG_VALUES			//Code parameters for the allowed sample rates
// 		2) SENSOR_RATE_OPTIONS		//SPS in numbers
//  Helper functions:
//      a) adcBegin()               // Initialization code
//      b) adcConfigure()           // Configuration code
//      c) adcDataReady()           // Duplicate read avoidance
//      d) adcRead()                // Value read, must be normalized to int16_t
//      e) adcSetRate()             // Sample rate setting of the sensor
//      f) adcRawToCentiVolts()     // Raw to true input voltage (gain-compensated)
//      g) adcRawToCentiUnit()      // Raw to physical unit (spec-based)
//      h) adcCheckFault()          // Sensor-specific fault detection

#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>

static NAU7802 ads;

// ============================================================================
//  ADC rate table — single source of truth for supported sample rates
// ============================================================================
// NAU7802 supports: 10, 20, 40, 80, 320 SPS
// Register values are the NAU7802_SPS_* enum values from the library
static const uint8_t ADC_REG_VALUES[] = { NAU7802_SPS_10,  NAU7802_SPS_20,
                                          NAU7802_SPS_40,  NAU7802_SPS_80,
                                          NAU7802_SPS_320 };
const uint16_t SENSOR_RATE_OPTIONS[]  = { 10, 20, 40, 80, 320 };

// Full-resolution capture for unit conversion
static int32_t lastRaw24 = 0;

static bool adcBegin() { return ads.begin(Wire); }

static void adcConfigure() {
    ads.setGain(NAU7802_GAIN_128);
    ads.setLDO(NAU7802_LDO_3V3);
    ads.setSampleRate(NAU7802_SPS_320);
    ads.setChannel(1);
    ads.calibrateAFE();
}

static bool adcDataReady() { return ads.available(); }

static int16_t adcRead() {
    lastRaw24 = ads.getReading();
    return (int16_t)(lastRaw24 >> 8);
}

static void adcSetRate(uint8_t reg) {
    ads.setSampleRate(reg);
    ads.calibrateAFE();     // Required after sample rate change
}

// NAU7802 at gain 128, internal reference ~1.2V
// Input range = ±1.2V / 128 ≈ ±9.375 mV
// Report true bridge voltage (before gain) in centivolts
// At 16-bit (24>>8): full scale ≈ 9.375mV → 937 centi-mV
// centivolts = raw * 1875 / 65536 (units of 0.01 mV)
static uint16_t adcRawToCentiVolts(int16_t raw) {
    if (raw <= 0) return 0;
    return (uint16_t)(((uint32_t)raw * 1875UL + 32768UL) / 65536UL);
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
    if (raw > (int16_t)ADC_FAIL_HIGH_THRESHOLD)  return FAULT_DISCONNECTED;
    if (raw < (int16_t)ADC_FAIL_LOW_THRESHOLD)   return FAULT_DISCONNECTED;
    return FAULT_NONE;
}