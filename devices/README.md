# Devices .h files

To simplify adding devices a dedicated .h file is used to abstract them. Here we define the requirements.

## How to use one.

Each device type has a particular set of requirements. In the initial release only ADC (those functions abstracted in `sensor.h/cpp`) are defined.

Drivers are defined as single .h files with the name definined by the device name.

For now, everything hardware, including GPIO connection, bus addresses and such, is defined in `config.h`.

## ADC Abstraction

All ADC sensor device drivers must define:

### Preprocessor

- #include < DEVICE LIBRARY >
- #define ADS_SENSOR_NAME "User readable device name"

### Variables
 - ads //Handle of the sensor
 - ADC_REG_VALUES			//Code parameters for the allowed sample rates
 - SENSOR_RATE_OPTIONS		//SPS in numbers

### Helper functions
 - adcBegin()				//Initialization code
 - adcConfigure()			//Configuration code
 - adcRead()				//Value read, must be normalized to int16_t
 - adcSetRate()				//Sample rate setting of the sensor

### Example

```
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
```