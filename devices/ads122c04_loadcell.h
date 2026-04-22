// ============================================================================
// devices/ads122c04_loadcell.h — ADS122C04 load cell mode for sensor.cpp
// ============================================================================
// This file is #included directly into sensor.cpp. It is NOT a standalone
// header — do not include it from any other translation unit.
//
// Differences from transducer mode:
//   - Gain 128, PGA enabled (required for gain > 4)
//   - Differential input (AIN0-AIN1)
//   - Internal 2.048V reference (precision, not supply-dependent)
//   - Unit conversion uses full 24-bit raw for display precision
//   - Fault detection checks both rail directions (open bridge)
// ============================================================================
#include <SparkFun_ADS122C04_ADC_Arduino_Library.h>

static SFE_ADS122C04 ads;

static const uint8_t ADC_REG_VALUES[] = { 3,    4,    5,    6    };
const uint16_t SENSOR_RATE_OPTIONS[]  = { 175,  330,  600,  1000 };

// Full-resolution capture for unit conversion
static int32_t lastRaw24 = 0;

static bool adcBegin() { return ads.begin(ADS122C04_ADDR, Wire); }

static void adcConfigure() {
    ads.setGain(ADS122C04_GAIN_128);
    ads.setInputMultiplexer(ADS122C04_MUX_AIN0_AIN1);
    ads.enablePGA(ADS122C04_PGA_ENABLED);
    ads.setDataCounter(ADS122C04_DCNT_DISABLE);
    ads.setDataIntegrityCheck(ADS122C04_CRC_DISABLED);
    ads.setConversionMode(ADS122C04_CONVERSION_MODE_CONTINUOUS);
    ads.setOperatingMode(ADS122C04_OP_MODE_NORMAL);
    ads.setVoltageReference(ADS122C04_VREF_INTERNAL);
}

static int16_t adcRead() {
    lastRaw24 = (int32_t)ads.readADC();
    return (int16_t)(lastRaw24 >> 8);
}

static void adcSetRate(uint8_t reg) { ads.setDataRate(reg); }

static bool adcDataReady() {
    #if ADC_DRDY_PIN >= 0
        return digitalRead(ADC_DRDY_PIN) == LOW;
    #else
        return ads.checkDataReady();
    #endif
}

// Gain 128, internal 2.048V reference
// Input range = ±2.048V / 128 = ±16 mV
// Report true bridge voltage (before gain) in centivolts
// At 16-bit (24>>8): LSB ≈ 32mV / 65536 ≈ 0.488µV
// centivolts = raw * 3200 / 65536 (in units of 0.01 mV)
static uint16_t adcRawToCentiVolts(int16_t raw) {
    if (raw <= 0) return 0;
    return (uint16_t)(((uint32_t)raw * 3200UL + 32768UL) / 65536UL);
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