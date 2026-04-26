// ============================================================================
// devices/ads122c04_loadcell.h — ADS122C04 load cell mode for sensor.cpp
// ============================================================================
// This file is #included directly into sensor.cpp. It is NOT a standalone
// header — do not include it from any other translation unit.
//
// IMPORTANT: Connect BOTH ADC and laod cell to 3.3V rail!
//
// Differences from transducer mode:
//   - Gain 128, PGA enabled (required for gain > 4)
//   - Differential input (AIN0-AIN1)
//   - AVDD reference for truly ratiometric measuring.
//   - Unit conversion uses full 24-bit raw for display precision
//   - Fault detection checks both rail directions (open bridge)
// ============================================================================

#include <Wire.h>
#include <SparkFun_ADS122C04_ADC_Arduino_Library.h>

static SFE_ADS122C04 ads;

// Map config.h gain value to ADS122C04 register enum
#if SENSOR_ADC_GAIN == 1
    #define ADS_GAIN_REG ADS122C04_GAIN_1
#elif SENSOR_ADC_GAIN == 2
    #define ADS_GAIN_REG ADS122C04_GAIN_2
#elif SENSOR_ADC_GAIN == 4
    #define ADS_GAIN_REG ADS122C04_GAIN_4
#elif SENSOR_ADC_GAIN == 8
    #define ADS_GAIN_REG ADS122C04_GAIN_8
#elif SENSOR_ADC_GAIN == 16
    #define ADS_GAIN_REG ADS122C04_GAIN_16
#elif SENSOR_ADC_GAIN == 32
    #define ADS_GAIN_REG ADS122C04_GAIN_32
#elif SENSOR_ADC_GAIN == 64
    #define ADS_GAIN_REG ADS122C04_GAIN_64
#elif SENSOR_ADC_GAIN == 128
    #define ADS_GAIN_REG ADS122C04_GAIN_128
#else
    #error "Invalid SENSOR_ADC_GAIN for ADS122C04. Valid: 1, 2, 4, 8, 16, 32, 64, 128"
#endif

static const uint8_t ADC_REG_VALUES[] = { 3,    4,    5,    6    };
const uint16_t SENSOR_RATE_OPTIONS[]  = { 175,  330,  600,  1000 };

// Full-resolution capture for unit conversion
static int32_t lastRaw24 = 0;

static void adcBusInit() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);		// Initialize the I²C bus
    Wire.setClock(I2C_WIRE_SPEED);				// Set the I²C speed. The ESP32-S3 Zero only supports up to 400kHz
}

static bool adcBegin() { return ads.begin(ADS122C04_ADDR, Wire); }

static void adcConfigure() {
    ads.setGain(ADS_GAIN_REG);
    ads.setInputMultiplexer(ADS122C04_MUX_AIN0_AIN1);
    // PGA required for gain > 4
    #if SENSOR_ADC_GAIN > 4
        ads.enablePGA(ADS122C04_PGA_ENABLED);
    #else
        ads.enablePGA(ADS122C04_PGA_DISABLED);
    #endif
    ads.setDataCounter(ADS122C04_DCNT_DISABLE);
    ads.setDataIntegrityCheck(ADS122C04_CRC_DISABLED);
    ads.setConversionMode(ADS122C04_CONVERSION_MODE_CONTINUOUS);
    ads.setOperatingMode(ADS122C04_OP_MODE_NORMAL);
    ads.setVoltageReference(ADS122C04_VREF_AVDD);      // Ratiometric with cell excitation
}

static int16_t adcRead() {
    lastRaw24 = (int32_t)ads.readADC();
    return (int16_t)(lastRaw24 >> 8);
}

static void adcSetRate(uint8_t reg) { ads.setDataRate(reg); }

static bool adcDataReady() {
    #if ADC_DRDY_PIN > 0
        return digitalRead(ADC_DRDY_PIN) == LOW;
    #else
        return ads.checkDataReady();
    #endif
}

// Input range = ±Vref / gain
// At AVDD 3.3V, gain from config: range = ±3300mV / gain
// centi-mV per count = (3300 * 2 / gain) * 10000 / 65536
// Simplified: raw * (66000 / SENSOR_ADC_GAIN) / 65536
static uint16_t adcRawToCentiVolts(int16_t raw) {
    if (raw <= 0) return 0;
    return (uint16_t)(((uint32_t)raw * (66000UL / SENSOR_ADC_GAIN) + 32768UL) / 65536UL);
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