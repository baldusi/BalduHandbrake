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
#include <Wire.h>

// ============================================================================
//  ADC selection at 
// ============================================================================
#ifdef ADC_ADS1115
    #include "devices/ads1115.h"
#elif defined(ADC_ADS122C04)
    #include "devices/ads122c04.h"
#endif

/*
// ============================================================================
//  ADC rate table — single source of truth for supported sample rates
// ============================================================================
#ifdef ADC_ADS1115
  static const uint8_t ADC_REG_VALUES[] = { 5,   6,   7,   7    };
  const uint16_t SENSOR_RATE_OPTIONS[]  = { 250, 475, 860, 1000 };
#elif defined(ADC_ADS122C04)
  static const uint8_t ADC_REG_VALUES[] = { 3,    4,    5,    6    };
  const uint16_t SENSOR_RATE_OPTIONS[]  = { 175,  330,  600,  1000 };
#endif
*/

const uint8_t  SENSOR_RATE_COUNT = sizeof(SENSOR_RATE_OPTIONS) / sizeof(SENSOR_RATE_OPTIONS[0]);

static uint8_t findRateIndex(uint16_t hz) {
    for (uint8_t i = 0; i < SENSOR_RATE_COUNT; i++)
        if (SENSOR_RATE_OPTIONS[i] == hz) return i;
    return SENSOR_RATE_COUNT - 1;
}

/*
// ============================================================================
//  Module-local ADC instance
// ============================================================================
#ifdef ADC_ADS1115
    static ADS1115 ads(ADS1115_ADDR);
#elif defined(ADC_ADS122C04)
    static SFE_ADS122C04 ads;
#endif */

// ============================================================================
//  Transducer spec constants (derived from config.h hardware defines)
// ============================================================================
// These define the transducer's physical voltage-to-pressure relationship.
// They are NOT affected by user calibration — PSI display always reflects
// the transducer's absolute reading.
static const uint16_t SPEC_ADC_SPAN = ADC_SPEC_FULL - ADC_SPEC_ZERO;


// ============================================================================
//  ensorUpdateDataRate()
// ============================================================================
void sensorUpdateDataRate(uint16_t sampleRateHz) {
    uint8_t idx = findRateIndex(sampleRateHz);
    //ads.setDataRate(ADC_REG_VALUES[idx]);
    adcSetRate(ADC_REG_VALUES[idx]);
}

// ============================================================================
//  sensorInit()
// ============================================================================
bool sensorInit() {
    // Wire.begin() must be called before this function (in main setup).
    /*
    #ifdef ADC_ADS1115
        if (!ads.begin()) {
    #elif defined(ADC_ADS122C04)
        if (!ads.begin(ADS122C04_ADDR, Wire)) {
    #endif
        return false;
    }
    #ifdef ADC_ADS1115
        ads.setGain(0);      // ±6.144V range
        ads.setDataRate(ADC_REG_VALUES[SENSOR_RATE_COUNT - 1]); // 860 SPS (max for ADS1115)
        //ads.setDataRate(7);   // 860 SPS (max for ADS1115)
        ads.setMode(0);       // Continuous conversion
        ads.requestADC(0);    // Start continuous on channel 0
    #elif defined(ADC_ADS122C04)
        ads.setInputMultiplexer(ADS122C04_MUX_AIN0_AVSS);               // Single-ended on A0 vs AVSS
        ads.setGain(ADS122C04_GAIN_1);                                  // Do not change as the transducer is 5V and needs the full range
        ads.setDataCounter(ADS122C04_DCNT_DISABLE);                     // Disable the data counter (Note: the library does not currently support the data count)
        ads.setDataIntegrityCheck(ADS122C04_CRC_DISABLED);              // Disable CRC checking (Note: the library does not currently support data integrity checking)
        ads.setConversionMode(ADS122C04_CONVERSION_MODE_CONTINUOUS);    // Use continuous mode
        ads.setOperatingMode(ADS122C04_OP_MODE_NORMAL);                 // Disable turbo mode
        ads.setVoltageReference(ADS122C04_VREF_AVDD);                   // ADS122C04 must be fed fromt he same 5V rail as the transducer
        ads.enableInternalTempSensor(ADS122C04_TEMP_SENSOR_OFF);        // When using temp sensor on, you read the internal temp data, not the ADC
        ads.setDataRate(ADC_REG_VALUES[SENSOR_RATE_COUNT - 1]);         // 1000 SPS (max non turbo mode)
    #endif
    */
    if (!adcBegin()) return false;
    adcConfigure();
    adcSetRate(ADC_REG_VALUES[SENSOR_RATE_COUNT - 1]);
    // Wait for ADC power-on and first conversion to stabilize.
    // The ADS1115 needs time after configuration before reliable reads.
    // This only runs once at boot, so a generous delay costs nothing.
    delay(300);

    // Flush first conversion (may be stale or transitional)
    /* #ifdef ADC_ADS1115
        int16_t firstRead = ads.getValue();
    #elif defined(ADC_ADS122C04)
        int32_t raw24 = ads.getConversionData();
        int16_t firstRead = (int16_t)(raw24 >> 8);
    #endif  */
    int16_t firstRead = adcRead();
    (void)firstRead;  // intentionally discarded

    return true;
}

// ============================================================================
//  Pipeline internals
// ============================================================================

// Convert raw ADC count to centivolts (V × 100, e.g. 457 = 4.57V).
// ADS1115 at gain=0: LSB = 0.1875 mV = 1875 / 10000000 V
// centivolts = raw * 1875 / 1000000, scaled with rounding:
static uint16_t rawToCentiVolts(int16_t raw) {
    if (raw <= 0) return 0;
    return (uint16_t)(((uint32_t)raw * 1875UL + 50000UL) / 100000UL);
}

// Convert raw ADC count to centiPsi (PSI × 100, e.g. 12345 = 123.45 PSI).
// Uses the transducer's physical spec: ADC_SPEC_ZERO = 0 PSI, ADC_SPEC_FULL = 500 PSI.
// This is purely informational — NOT affected by user calibration.
static uint32_t rawToCentiPsi(int16_t raw) {
    if (raw <= (int16_t)ADC_SPEC_ZERO) return 0;

    uint32_t offset = (uint32_t)(raw - ADC_SPEC_ZERO);
    // centiPsi = offset * (PSI_MAX * 100) / SPEC_ADC_SPAN, with rounding
    return (offset * (uint32_t)(TRANSDUCER_PSI_MAX * 100UL) + (SPEC_ADC_SPAN / 2))
           / SPEC_ADC_SPAN;
}

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
    out.centiVolts = rawToCentiVolts(adcRaw);

    // ------------------------------------------------------------------
    // 2. Validate reading — check hardware safety limits
    // ------------------------------------------------------------------

    // Transducer disconnected or dead (< 0.25V)
    if (adcRaw < (int16_t)ADC_FAIL_THRESHOLD) {
        out.transducerFail = true;
        out.pressureLow    = false;
        out.saturationFail = false;
        out.centiPsi       = 0;
        out.centiPercent    = 0;
        out.axisOutput     = Z_AXIS_MIN;
        return;
    }

    // Overpressure / voltage saturation (> 4.82V)
    if (adcRaw >= (int16_t)ADC_OVER_THRESHOLD) {
        out.transducerFail = false;
        out.pressureLow    = false;
        out.saturationFail = true;
        out.centiPsi       = (uint32_t)TRANSDUCER_PSI_MAX * 108UL;  // Show ~540 PSI
        out.centiPercent    = 10000;   // 100.00%
        out.axisOutput     = Z_AXIS_MAX;
        return;
    }

    // Clear error flags for normal readings
    out.transducerFail = false;
    out.saturationFail = false;

    // ------------------------------------------------------------------
    // 3. Compute informational PSI (always from transducer spec)
    // ------------------------------------------------------------------
    out.centiPsi = rawToCentiPsi(adcRaw);

    // ------------------------------------------------------------------
    // 4. Check for low pressure warning
    // ------------------------------------------------------------------
    if (adcRaw < (int16_t)cfg.calAdcZero) {
        out.pressureLow = true;
    } else {
        out.pressureLow = false;
    }

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
}
