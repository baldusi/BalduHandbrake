# Quick Start Guide

This guide gets your BalduHandbrake from zero to a working 1000 Hz HID device in no time.

## 1. Hardware Assembly (5–10 min)

1. Wire the ESP32-S3 Zero, OLED (SSD1351 128×128), rotary encoder + button, hold button, and chosen ADC according to the reference diagrams in `hardware/`.
2. Use the V1.1 pinout (recommended). Full schematics and BOM are in the `hardware/` directory.
3. For 5 V pressure transducers, insert a logic-level converter on the signal line.
4. Add the recommended 104/106 capacitors and 220 Ω protection resistors shown in the diagrams.

## 2. Software Setup (5 min)

1. Install **Arduino IDE 2.x**.
2. Add the ESP32 board manager URL:  
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Install board **ESP32S3 Dev Module** (or exact Waveshare ESP32-S3 Zero equivalent).
4. Critical board settings:
   - USB CDC On Boot: **Enabled**
   - CPU Frequency: **240 MHz**
   - USB DFU On Boot: **Disabled**
   - Events Run On: **Core 1**
   - Flash Mode: **QIO 80 MHz**
   - Arduino Runs On: **Core 1**
   - USB Firmware MSC on Boot: **Disabled**
   - Partition Scheme: **Default 4MB with spiffs**
   - PSRAM: **Enabled**
   - Upload Mode: **UART0 / Hardware DCD**
   - Upload Speed: **921600**
   - USB Mode: **USB-OTG (TinyUSB)**
5. Install required libraries and their dependencies via Library Manager:
   - Adafruit TinyUSB
   - LovyanGFX
   - RotaryEncoder by Matthias Hertel
   Plus your sensor library:
   - ADS1X15 by Rob Tillaart — For the ADS1115 ADC
   - SparkFun_ADS122C04_ADC_Arduino_Library — For the ADS122C04 ADC
   - SparkFun_Qwiic_Scale_NAU7802_Arduino_Library — For the NAU7802 ADC
   - HX711 by Bogdan Necula — For the HX711 ADC
   - Any additional ADC library you chose in `config.h`
6. Grab the latest release from the [Official Repository](https://github.com/baldusi/BalduHandbrake/releases) and unpack it in the directory you want to use. Unpack the BalduHandbrake directory, not only it's contents, as Arduino IDE requires the .ino and directory names to match.

## 3. Configure and Flash (3 min)

1. Start up Arduino IDE and open `BalduHandbrake.ino`.
2. Edit `config.h`:
   - Choose your ADC (`#define SENSOR_ADC_ADS1115`, or `ADC_ADS122C04` or `ADC_NAU7802` or `ADC_HX711`)
   - Set `SENSOR_PRESSURE_TRANSDUCER` or `SENSOR_LOAD_CELL`
   - Set correct GPIO pins if you deviated from the reference.
   - Set your sensor's `SENSOR_UNIT_MAX`, `ADC_SPEC_ZERO`, `ADC_SPEC_FULL` and either (`ADC_FAIL_THRESHOLD`, `ADC_OVER_THRESHOLD`) or (`ADC_FAIL_HIGH_THRESHOLD` or `ADC_FAIL_LOW_THRESHOLD`). Review `README.md` **Configuring the sensor** section for calculating those values.
3. Select the correct COM port for connection.
4. Verify → Upload.
5. Upload might fail. When the upload puts ESP32 into firmware programming mode, it goes sets it USB back to ESP32-S3 Zero, and Windows re-enumerates with a different port. Just select the new COM port and hit Upload again.
4. After upload, the device will reboot and show the LIVE screen on the OLED.

## 4. First Calibration (5 min)

1. Power the unit via USB.
2. The OLED will enter **LIVE** mode showing raw sensor value and current curve.
3. Press rotary encoder to enter Menu mode.
4. Navigate to **Calibrate** → press to start.
5. Follow the on-screen prompts:
   - Release the lever completely (zero point).
   - Apply full force slowly (full-scale point).
   - The routine includes settling detection and automatic outlier rejection.
6. Save the calibration to the current profile.

You now have a fully functional handbrake at 1000 Hz with native HID Z-axis (0–4095).

## Next Steps

- Read `USAGE.md` for daily operation, profile switching, live curve changes, and hold mode.
- Customise Hold mode, deadzones, Drift Curve snap, sampling rate, language and default curve.
- Contribute to the project adding new languages or ADC shims following `devices/README.md`.

---

**You are now ready to race.**