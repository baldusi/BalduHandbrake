/*  BalduHandrake
    Open Source Hydraulic Simracing Handbrake
    Copyright (c) 2026 Alejandro Belluscio
    Additional copyright holders listed inline below.
    This file is licensed under the Apache 2.0 license
    Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// devices/ads1115.h — ADS1115 hardware abstraction for sensor.cpp
// ============================================================================
// This file is #included directly into sensor.cpp. It is NOT a standalone
// header — do not include it from any other translation unit.
// ============================================================================
// SENSOR DEVICE DRIVER: ADS1115
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
// ============================================================================


//Rob Tillaart ADS1X15 library
#include <Wire.h>
#include <ADS1X15.h>

static ADS1115 ads(ADS1115_ADDR);

// ============================================================================
//  ADC rate table — single source of truth for supported sample rates
// ============================================================================
static const uint8_t ADC_REG_VALUES[] = { 5,   6,   7,   7    };
const uint16_t SENSOR_RATE_OPTIONS[]  = { 250, 475, 860, 1000 };


static void adcBusInit() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);		// Initialize the I²C bus
    Wire.setClock(I2C_WIRE_SPEED);				// Set the I²C speed. The ESP32-S3 Zero only supports up to 400kHz
}

static bool adcBegin() {
	return ads.begin();
}	// Obtain the ADC object handle

static void adcConfigure() {
    ads.setGain(ADS1X15_GAIN_6144MV);			// Only gain compatible with 5V.
    ads.setMode(ADS1X15_MODE_CONTINUOUS);		// Sets continuous mode
    ads.requestADC(0);							// Sets the reads on channel 0.
}

static bool adcDataReady() { return true; }  // No DRDY, accept duplicates

static int16_t adcRead() { return ads.getValue(); }

static void adcSetRate(uint8_t reg) { ads.setDataRate(reg); }

// ADS1115 at gain 0 (±6.144V): LSB = 0.1875 mV
// centivolts = raw * 1875 / 100000 (with rounding)
static uint16_t adcRawToCentiVolts(int16_t raw) {
    if (raw <= 0) return 0;
    return (uint16_t)(((uint32_t)raw * 1875UL + 50000UL) / 100000UL);
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