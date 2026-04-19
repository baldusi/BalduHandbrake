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
//		b) #define ADS_SENSOR_NAME "User readable device name"
//	Variables
//		1) ads 						//Handle of the sensor
// 		1) ADC_REG_VALUES			//Code parameters for the allowed sample rates
// 		2) SENSOR_RATE_OPTIONS		//SPS in numbers
//	Helper functions:
// 		a) adcBegin()				//Initialization code
// 		b) adcConfigure()			//Configuration code
// 		c) adcRead()				//Value read, must be normalized to int16_t
// 		d) adcSetRate()				//Sample rate setting of the sensor


//Rob Tillaart ADS1X15 library
#include <ADS1X15.h>

#define ADS_SENSOR_NAME "ADS1115"

static ADS1115 ads(ADS1115_ADDR);

// ============================================================================
//  ADC rate table — single source of truth for supported sample rates
// ============================================================================
static const uint8_t ADC_REG_VALUES[] = { 5,   6,   7,   7    };
const uint16_t SENSOR_RATE_OPTIONS[]  = { 250, 475, 860, 1000 };

static bool adcBegin() { return ads.begin(); }

static void adcConfigure() {
    ads.setGain(0);
    ads.setMode(0);
    ads.requestADC(0);
}

static int16_t adcRead() { return ads.getValue(); }

static void adcSetRate(uint8_t reg) { ads.setDataRate(reg); }