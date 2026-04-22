/*  BalduHandrake
    Open Source Hydraulic Simracing Handbrake
    Copyright (c) 2026 Alejandro Belluscio
    Additional copyright holders listed inline below.
    This file is licensed under the Apache 2.0 license
    Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// devices/ads122c04.h — ADS122C04 hardware abstraction for sensor.cpp
// ============================================================================
// This file is #included directly into sensor.cpp. It is NOT a standalone
// header — do not include it from any other translation unit.
// ============================================================================
// SENSOR DEVICE DRIVER: ADS122C04
// ============================================================================
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

#include <SparkFun_ADS122C04_ADC_Arduino_Library.h>

static SFE_ADS122C04 ads;

// ============================================================================
//  ADC rate table — single source of truth for supported sample rates
// ============================================================================
static const uint8_t ADC_REG_VALUES[] = { 3,    4,    5,    6    };
const uint16_t SENSOR_RATE_OPTIONS[]  = { 175,  330,  600,  1000 };

static bool adcBegin() { return ads.begin(ADS122C04_ADDR, Wire); }

static void adcConfigure() {
    ads.setInputMultiplexer(ADS122C04_MUX_AIN0_AVSS);               // Single-ended on A0 vs AVSS
    ads.setGain(ADS122C04_GAIN_1);                                  // Do not change as the transducer is 5V and needs the full range
    ads.setDataCounter(ADS122C04_DCNT_DISABLE);                     // Disable the data counter (Note: the library does not currently support the data count)
    ads.setDataIntegrityCheck(ADS122C04_CRC_DISABLED);              // Disable CRC checking (Note: the library does not currently support data integrity checking)
    ads.setConversionMode(ADS122C04_CONVERSION_MODE_CONTINUOUS);    // Use continuous mode
    ads.setOperatingMode(ADS122C04_OP_MODE_NORMAL);                 // Disable turbo mode
    ads.setVoltageReference(ADS122C04_VREF_AVDD);                   // ADS122C04 must be fed fromt he same 5V rail as the transducer
    ads.enableInternalTempSensor(ADS122C04_TEMP_SENSOR_OFF);        // When using temp sensor on, you read the internal temp data, not the ADC
}

static bool adcDataReady() {
    #if ADC_DRDY_PIN >= 0
        return digitalRead(ADC_DRDY_PIN) == LOW;   // GPIO read: nanoseconds
    #else
        return ads.checkDataReady();                // I2C register poll: ~100µs
    #endif
}

static int16_t adcRead() {
    int32_t raw24 = ads.readADC();
    return (int16_t)(raw24 >> 8);
}

static void adcSetRate(uint8_t reg) { ads.setDataRate(reg); }

// Gain 1, AVDD reference (5V), 24-bit >> 8 = 16-bit
// LSB ≈ 5.0V / 65536 ≈ 76.3µV
// centivolts = raw * 500 / 65536 (with rounding)
static uint16_t adcRawToCentiVolts(int16_t raw) {
    if (raw <= 0) return 0;
    return (uint16_t)(((uint32_t)raw * 500UL + 32768UL) / 65536UL);
}

static const uint16_t SPEC_ADC_SPAN = ADC_SPEC_FULL - ADC_SPEC_ZERO;

static uint32_t adcRawToCentiUnit(int16_t raw) {
    if (raw <= (int16_t)ADC_SPEC_ZERO) return 0;
    uint32_t offset = (uint32_t)(raw - ADC_SPEC_ZERO);
    return (offset * (uint32_t)(SENSOR_UNIT_MAX * 100UL) + (SPEC_ADC_SPAN / 2))
           / SPEC_ADC_SPAN;
}

static uint8_t adcCheckFault(int16_t raw) {
    if (raw < (int16_t)ADC_FAIL_THRESHOLD)   return FAULT_DISCONNECTED;
    if (raw >= (int16_t)ADC_OVER_THRESHOLD)  return FAULT_SATURATION;
    return FAULT_NONE;
}
