# Devices .h files

To simplify adding devices a dedicated .h file is used to abstract them. Here we define the requirements.

## How to use one.

Each device type has a particular set of requirements. In the initial release only ADC (those functions abstracted in `sensor.h/cpp`) are defined.

Drivers are defined as single .h files with the name definined by the device name.

For now, everything hardware, including GPIO connection, bus addresses and such, is defined in `config.h`.

## ADC Abstraction

All ADC sensor shim drivers must define:

### Preprocessor

- #include < DEVICE LIBRARY >

### Variables
 - ads						//Handle of the sensor
 - ADC_REG_VALUES			//Code parameters for the allowed sample rates
 - SENSOR_RATE_OPTIONS		//SPS in numbers

### Functions

| Function | Signature | Purpose |
|---|---|---|
| `adcBegin` | `bool ()` | Init hardware |
| `adcConfigure` | `void ()` | Set gain, mux, mode, ref |
| `adcRead` | `int16_t ()` | Return raw as 16-bit |
| `adcSetRate` | `void (uint8_t reg)` | Change sample rate |
| `adcDataReady` | `bool ()` | DRDY / always true |
| `adcRawToCentiVolts` | `uint16_t (int16_t raw)` | True input voltage |
| `adcRawToCentiUnit` | `uint32_t (int16_t raw)` | Spec-based physical unit |
| `adcCheckFault` | `uint8_t (int16_t raw)` | Sensor fault detection |
| `SENSOR_RATE_OPTIONS[]` | `const uint16_t[]` | Available Hz values |
| `ADC_REG_VALUES[]` | `const uint8_t[]` | Register codes |FunctionSignaturePurposeadcBeginbool ()Init hardwareadcConfigurevoid ()Set gain, mux, mode, refadcReadint16_t ()Return raw as 16-bitadcSetRatevoid (uint8_t reg)Change sample rateadcDataReadybool ()DRDY / always trueadcRawToCentiVoltsuint16_t (int16_t raw)True input voltageadcRawToCentiUnituint32_t (int16_t raw)Spec-based physical unitadcCheckFaultuint8_t (int16_t raw)Sensor fault detectionSENSOR_RATE_OPTIONS[]const uint16_t[]Available Hz valuesADC_REG_VALUES[]const uint8_t[]Register codes

### Example

```
//Rob Tillaart ADS1X15 library
#include <ADS1X15.h>

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
```