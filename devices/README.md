# Devices .h files

To simplify adding devices a dedicated .h file is used to abstract them. Here we define the requirements.

## How to use one.

Each device type has a particular set of requirements. In the initial release only ADC (those functions abstracted in `sensor.h/cpp`) are defined.

Drivers are defined as single .h files with the name defined by the device name.

Most parameters, including GPIO connection, bus addresses and such, are defined in `config.h`, but abstracted in the shim.

We use different shims for pressure transducers and load cells, even for the exact same ADC. One reason is that pressure transducers work at 5V, which mean a low gain, and need a logic level converter. Also, the ranges and failure modes are different, and even the channel configuration differs. As such, in `config.h` is defined the `SENSOR_LOAD_CELL` or `SENSOR_PRESSURE_TRANSDUCER` to configure the variables and modes.

## ADC Abstraction

### User Definable Variables

All user definable variables, like GPIO pins, bus address, gain, etc, are exposed in the `config.h` file. One should strive to reuse preexisting variables. And if you need to add a variable or function it should be done in the most general way as to extend the interface.

### Preprocessor
|#include| Definition|
|-|-|
| **< BUS LIBRARY >** | I²C or SPI, generally custom buses like the HX711's are included in the same library.|
| **< DEVICE LIBRARY >** | The ADC handling library. You should chose a reliable, well supported library. But special care must be taken to chose those that use mostly integrar math. Most of the efforts on data transformation functions went into avoiding floating point functions.|

### Variables
|Name | type | definition|
|-|-|-|
 | private | any type | Private variables are allowed for internal use as long as they stay strictly within this file's context. |
 | `ads` | Library object | Handle of the sensor. |
 | `ADC_REG_VALUES[]` | `const uint8_t[]` | Library code parameters for the allowed sample rates. |
 | `SENSOR_RATE_OPTIONS[]` | `const uint16_t[]` | Samples per second in integer numbers. |
 | `ADS_GAIN_REG` | `#define` | **(Only for Load Cell case)** A series of defines to translate SENSOR_ADS_GAIN to the actual gain parameter of the library.|

### Functions

| Function | Signature | Purpose |
|---|---|---|
| `adcBusInit` | `void ()` | Bus initalization code, like I2C, SPI, etc. Must take pinout and speed variables from `config.h`. |
| `adcBegin` | `bool ()` | Init hardware. Should take the addresses from `config.h`. |
| `adcConfigure` | `void ()` | Set gain, mux, mode, ref, channel. Must take GAIN from `config.h`. |
| `adcRead` | `int16_t ()` | Return raw as 16-bit |
| `adcSetRate` | `void (uint8_t reg)` | Change sample rate using  |
| `adcDataReady` | `bool ()` | Poll ADC if new data ready. If no method is available, always return true. |
| `adcRawToCentiVolts` | `uint16_t (int16_t raw)` | Return true input voltage * 100 |
| `adcRawToCentiUnit` | `uint32_t (int16_t raw)` | Spec-based physical unit |
| `adcCheckFault` | `uint8_t (int16_t raw)` | Sensor fault detection |

### Example

#### Pressure Transducer

```
// ============================================================================
// devices/ads1115.h — ADS1115 hardware abstraction for sensor.cpp
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
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_WIRE_SPEED);
}

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
#### Load Cell
````
// ============================================================================
// devices/nau7802.h — NAU7802 load cell mode for sensor.cpp
// ============================================================================

#include <Wire.h>
#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>

static NAU7802 ads;

#if SENSOR_ADC_GAIN == 1
    #define ADS_GAIN_REG NAU7802_GAIN_1
#elif SENSOR_ADC_GAIN == 2
    #define ADS_GAIN_REG NAU7802_GAIN_2
#elif SENSOR_ADC_GAIN == 4
    #define ADS_GAIN_REG NAU7802_GAIN_4
#elif SENSOR_ADC_GAIN == 8
    #define ADS_GAIN_REG NAU7802_GAIN_8
#elif SENSOR_ADC_GAIN == 16
    #define ADS_GAIN_REG NAU7802_GAIN_16
#elif SENSOR_ADC_GAIN == 32
    #define ADS_GAIN_REG NAU7802_GAIN_32
#elif SENSOR_ADC_GAIN == 64
    #define ADS_GAIN_REG NAU7802_GAIN_64
#elif SENSOR_ADC_GAIN == 128
    #define ADS_GAIN_REG NAU7802_GAIN_128
#else
    #error "Invalid SENSOR_ADC_GAIN for NAU7802. Valid: 1, 2, 4, 8, 16, 32, 64, 128"
#endif


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

static void adcBusInit() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_WIRE_SPEED);
}

static bool adcBegin() { return ads.begin(Wire); }

static void adcConfigure() {
    ads.setGain(ADS_GAIN_REG);
    ads.setLDO(NAU7802_LDO_3V3);
    ads.setSampleRate(NAU7802_SPS_320);
    ads.setChannel(1);
    ads.calibrateAFE();
}


static int16_t adcRead() {
    lastRaw24 = ads.getReading();
    return (int16_t)(lastRaw24 >> 8);
}

static void adcSetRate(uint8_t reg) {
    ads.setSampleRate(reg);
    ads.calibrateAFE();     // Required after sample rate change
}

static bool adcDataReady() { return ads.available(); }

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
```