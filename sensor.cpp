/*  BalduHandrake
	Open Source Hydraulic Simracing Handbrake
	Copyright (c) 2026 Alejandro Belluscio
	Additional copyright holders listed inline below.
	This file is licensed under the Apache 2.0 license
	Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// sensor.cpp — Sensor Pipeline Implementation
// ============================================================================
// This module runs entirely on Core 0. It owns the I2C bus (ADS1115) and
// has no dependencies on SPI, OLED, or UI code.
// ============================================================================

#include "sensor.h"
#include "curves.h"
//#include <Wire.h>

// ============================================================================
//  ADC selection at 
// ============================================================================
#if defined(ADC_ADS1115)
    #include "devices/ads1115.h"
#elif defined(ADC_ADS122C04) && defined(SENSOR_LOAD_CELL)
    #include "devices/ads122c04_loadcell.h"
#elif defined(ADC_ADS122C04)
    #include "devices/ads122c04.h"
#elif defined(ADC_NAU7802)
    #include "devices/nau7802.h"
#elif defined(ADC_HX711)
    #include "devices/hx711.h"
#endif

// Safety check: NAU7802 is load-cell only
#if defined(ADC_NAU7802) && !defined(SENSOR_LOAD_CELL)
    #error "NAU7802 requires SENSOR_LOAD_CELL — it is a load cell amplifier, not a general-purpose ADC"
#endif
#if defined(ADC_NAU7802) && !defined(SENSOR_LOAD_CELL)
    #error "NAU7802 requires SENSOR_LOAD_CELL"
#endif
#if defined(ADC_HX711) && !defined(SENSOR_LOAD_CELL)
    #error "HX711 requires SENSOR_LOAD_CELL"
#endif

// ============================================================================
//  ADC rate table — common code to al shim drivers
// ============================================================================

const uint8_t  SENSOR_RATE_COUNT = sizeof(SENSOR_RATE_OPTIONS) / sizeof(SENSOR_RATE_OPTIONS[0]);

static uint8_t findRateIndex(uint16_t hz) {
    for (uint8_t i = 0; i < SENSOR_RATE_COUNT; i++)
        if (SENSOR_RATE_OPTIONS[i] == hz) return i;
    return SENSOR_RATE_COUNT - 1;
}



// ============================================================================
//  sensorBusInit() Wrapper function for bus initialization. 
// ============================================================================
// It's needed for the .ino file to see the adcBusInit() on the shim driver

void sensorBusInit() {
    adcBusInit();
}

// ============================================================================
//  Transducer spec constants (derived from config.h hardware defines)
// ============================================================================
// These define the transducer's physical voltage-to-pressure relationship.
// They are NOT affected by user calibration — PSI display always reflects
// the transducer's absolute reading.
// static const uint16_t SPEC_ADC_SPAN = ADC_SPEC_FULL - ADC_SPEC_ZERO;


// ============================================================================
//  sensorUpdateDataRate()
// ============================================================================
void sensorUpdateDataRate(uint16_t sampleRateHz) {
    uint8_t idx = findRateIndex(sampleRateHz);
    adcSetRate(ADC_REG_VALUES[idx]);
}

// ============================================================================
//  sensorInit()
// ============================================================================
bool sensorInit() {
    // Wire.begin() must be called before this function (in main setup).
 
    if (!adcBegin()) return false;
    adcConfigure();
    adcSetRate(ADC_REG_VALUES[SENSOR_RATE_COUNT - 1]);
    // Wait for ADC power-on and first conversion to stabilize.
    // The ADS1115 needs time after configuration before reliable reads.
    // This only runs once at boot, so a generous delay costs nothing.
    delay(300);

    // Flush first conversion (may be stale or transitional)
    int16_t firstRead = adcRead();
    (void)firstRead;  // intentionally discarded

    return true;
}

// ============================================================================
//  Pipeline internals
// ============================================================================

// Compute derived deadzone boundaries from config.
// Returns the effective ADC range after deadzones are applied.
struct EffectiveRange {
    uint16_t min;     // ADC value at the bottom of the active zone
    uint16_t max;     // ADC value at the top of the active zone
    uint16_t span;    // max - min
};

static EffectiveRange computeEffectiveRange(const DeviceConfig& cfg) {
    EffectiveRange r;
    uint16_t calSpan = cfg.calAdcMax - cfg.calAdcZero;

    // Deadzones are in tenths of percent (50 = 5.0%)
    // effectiveMin = calAdcZero + calSpan * deadzoneLow / 1000
    r.min = cfg.calAdcZero
            + (uint16_t)((uint32_t)calSpan * cfg.deadzoneLow / 1000UL);
    r.max = cfg.calAdcMax
            - (uint16_t)((uint32_t)calSpan * cfg.deadzoneHigh / 1000UL);

    // Guard against misconfiguration where deadzones overlap
    if (r.max <= r.min) {
        r.max = r.min + 1;
    }
    r.span = r.max - r.min;

    return r;
}

// ============================================================================
//  applyCurveCorrection()
// ============================================================================
// Takes a normalized value (0–Z_AXIS_MAX) and applies the selected curve.
// Linear: pass-through.
// Drift Snap: zero below threshold, then linear ramp to max.
// All others: LUT with 2-bit fractional interpolation.

uint16_t applyCurveCorrection(uint16_t normValue, uint8_t curveIndex,
                              uint16_t snapThreshold) {
    // --- Linear: no transformation ---
    if (curveIndex == CURVE_LINEAR) {
        return normValue;
    }

    // --- Drift Snap: dead zone then linear ramp ---
    if (curveIndex == CURVE_DRIFT_SNAP) {
        // snapThreshold is in tenths of percent (550 = 55.0%).
        // Convert to axis units: threshold = Z_AXIS_MAX * snapThreshold / 1000
        uint16_t threshAdc = (uint16_t)((uint32_t)Z_AXIS_MAX * snapThreshold / 1000UL);

        if (normValue < threshAdc) {
            return 0;
        }
        return (uint16_t)(((uint32_t)(normValue - threshAdc) * Z_AXIS_MAX)
               / (Z_AXIS_MAX - threshAdc));
    }

    // --- LUT-based curves ---
    uint8_t lutIndex = curveToLutIndex(curveIndex);
    if (lutIndex == 0xFF) {
        return normValue;   // Defensive fallback to linear
    }

    const uint16_t* lut = CURVE_LUTS[lutIndex];

    // 1024-entry table covers 0–4095 input:
    //   index = normValue >> 2    (0–1023)
    //   frac  = normValue & 0x03  (0–3, sub-entry interpolation)
    uint16_t index = normValue >> 2;
    uint16_t frac  = normValue & 0x03;

    uint16_t y0 = lut[index];
    uint16_t y1 = lut[index + 1];   // Safe: LUT has 1025 entries (sentinel)

    // Linear interpolation between adjacent LUT entries
    // result = y0 + (y1 - y0) * frac / 4
    return y0 + (uint16_t)(((uint32_t)(y1 - y0) * frac) >> 2);
}

// ============================================================================
//  sensorUpdate() — Full pipeline, called once per sample cycle
// ============================================================================
void sensorUpdate(const DeviceConfig& cfg, LiveData& out) {
    // ------------------------------------------------------------------
    // 1. Read ADC (cached value from continuous mode — no I2C wait)
    // ------------------------------------------------------------------
    //int16_t adcRaw = ads.getValue();
    static LiveData lastOut = {};
    
    if (!adcDataReady()) {
        out = lastOut;      // Reuse previous result
        return;
    }
    int16_t adcRaw = adcRead();

    // ------------------------------------------------------------------
    // 1a. Median-of-3 pre-filter (kills single-sample spikes)
    // ------------------------------------------------------------------
    static int16_t med0 = 0, med1 = 0, med2 = 0;
    med2 = med1;
    med1 = med0;
    med0 = adcRaw;
    int16_t adcMedian;
    if (med0 >= med1) {
        if (med1 >= med2)      adcMedian = med1;  // 0 >= 1 >= 2
        else if (med0 >= med2) adcMedian = med2;  // 0 >= 2 > 1
        else                   adcMedian = med0;  // 2 > 0 >= 1
    } else {
        if (med0 >= med2)      adcMedian = med0;  // 1 > 0 >= 2
        else if (med1 >= med2) adcMedian = med2;  // 1 >= 2 > 0
        else                   adcMedian = med1;  // 2 > 1 > 0
    }

    // ------------------------------------------------------------------
    // 1b. EMA temporal filter (K=8, ~8ms settling at 1000Hz)
    // ------------------------------------------------------------------
    static int32_t emaAccum = -1;
    if (emaAccum < 0) {
        emaAccum = (int32_t)adcMedian << 3;  // Seed on first read
    }
    emaAccum += (int32_t)adcMedian - (emaAccum >> 3);
    adcRaw = (int16_t)(emaAccum >> 3);

    // ------------------------------------------------------------------
    // 1c. Continue pipeline with filtered value
    // ------------------------------------------------------------------
    out.rawAdc = (adcRaw > 0) ? (uint16_t)adcRaw : 0;
    //out.centiVolts = rawToCentiVolts(adcRaw);
    out.centiVolts = adcRawToCentiVolts(adcRaw);

    // ------------------------------------------------------------------
    // 2. Fault detection (driver-specific)
    // ------------------------------------------------------------------
    uint8_t fault = adcCheckFault(adcRaw);
    out.sensorFail = (fault & FAULT_DISCONNECTED) != 0;
    out.sensorSaturation = (fault & FAULT_SATURATION) != 0;

    if (fault & FAULT_DISCONNECTED) {
        out.sensorLow      = false;
        out.centiUnit      = 0;
        out.centiPercent   = 0;
        out.axisOutput     = Z_AXIS_MIN;
        return;
    }

    if (fault & FAULT_SATURATION) {
        out.sensorLow      = false;
        out.centiUnit      = (uint32_t)SENSOR_UNIT_MAX * 108UL;
        out.centiPercent   = 10000;
        out.axisOutput     = Z_AXIS_MAX;
        return;
    }

    out.sensorFail = false;
    out.sensorSaturation = false;

    // ------------------------------------------------------------------
    // 3. Compute informational unit value (from sensor spec, not calibration)
    // ------------------------------------------------------------------
    out.centiUnit = adcRawToCentiUnit(adcRaw);

    // ------------------------------------------------------------------
    // 4. Check for low pressure/force warning
    // ------------------------------------------------------------------
    out.sensorLow  = (adcRaw < (int16_t)cfg.calAdcZero);

    // ------------------------------------------------------------------
    // 5. Apply deadzones and normalize to Z-axis range
    // ------------------------------------------------------------------
    EffectiveRange range = computeEffectiveRange(cfg);

    uint16_t axisRaw;
    if (adcRaw <= (int16_t)range.min) {
        // Below low deadzone: axis = 0
        axisRaw = Z_AXIS_MIN;
    } else if (adcRaw >= (int16_t)range.max) {
        // Above high deadzone: axis = max
        axisRaw = Z_AXIS_MAX;
    } else {
        // Normal range: linear map [range.min, range.max] → [0, Z_AXIS_MAX]
        axisRaw = (uint16_t)(
            (uint32_t)(adcRaw - range.min) * Z_AXIS_MAX / range.span
        );
    }

    // ------------------------------------------------------------------
    // 6. Apply curve correction
    // ------------------------------------------------------------------
    uint16_t axisCurved = applyCurveCorrection(axisRaw, cfg.curveIndex,
                                                cfg.snapThreshold);

    // ------------------------------------------------------------------
    // 7. Compute display percentage and write final output
    // ------------------------------------------------------------------
    out.axisOutput  = axisCurved;
    out.centiPercent = (uint16_t)((uint32_t)axisCurved * 10000UL / Z_AXIS_MAX);
    lastOut = out;
}
