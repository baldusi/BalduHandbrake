# BalduHandbrake

Originally an hydraulic handbrake device for simracing, that is fully open sourced. The hardware is based on actual competition hydraulic handbrake with a pressure transducer for a fully realistic feel. The electronics are designed around an ESP32, an OLED display and a rotary encoder to offer full device customization and control even while playing the game. In addition, the architecture is designed to be easily ported to pressure cells and potential as a controller for pedals and handbrake.

## What Is This?

BalduHandbrake turns a real hydraulic handbrake mechanism into a high-performance USB HID joystick input for racing simulators. The handle operates a master brake cylinder against a spring or elastic bushings, and a pressure transducer on the fluid line provides precise, analog position sensing — no potentiometers, no hall sensors, no mechanical wear on the sensing element.

The device presents itself to the PC as a standard HID joystick with one Z-axis (0–4095 resolution) and one button, generally used for the hold function, compatible with any simracing title that supports analog handbrake input.

### Key Features

- **1000 Hz USB polling rate** — deterministic sensor-to-USB pipeline on a dedicated CPU core, matching the maximum USB HID polling rate.
- **Six selectable response curves** — switchable live during gameplay via the rotary encoder, so you can adjust feel on the fly for different conditions (rally stages, drift, wet weather).
- **Hydraulic pressure sensing** — Implemented on a 0–500 PSI transducer on the brake fluid line provides smooth, repeatable input with natural damping. Easily adapted to other pressure ranges or even a pressure cell design with the correct ADC change.
- **On-device configuration** — 128×128 OLED screen with rotary encoder for full device setup without any PC software.
- **5 saveable profiles** — store and recall complete configurations for different cars or disciplines
- **Firmware hold mode** — optional axis lock at 100% on button press, handled either by the game or by the device firmware.
- **Configurable deadzones, debounce, and refresh rates** — all adjustable from the on-device menu.
- **On-device calibration** — guided calibration routine with settling detection and outlier rejection.
- **Multi-language UI** — string table architecture ready for localization (English and Spanish included).

## Hardware

### Components

| Component | Model | Interface |
|---|---|---|
| Microcontroller | Waveshare ESP32-S3 Zero | — |
| ADC | ADS1115 (16-bit, 860 SPS) | I²C |
| OLED Display | SSD1351 (128×128, RGB) | SPI |
| Pressure Transducer | Ejoyous 0–500 PSI, 1/8 NPT | Analog (0.5V–4.5V) |
| Rotary Encoder | EC11 with pushbutton | GPIO |
| Hold Button | Momentary pushbutton (pommel-mounted) | GPIO |

### Pin Assignments (Waveshare ESP32-S3 Zero)

| Function | GPIO |
|---|---|
| Hold Button | 4 |
| Encoder Channel A | 5 |
| Encoder Channel B | 13 |
| Encoder Button | 3 |
| OLED CS | 10 |
| OLED DC | 6 |
| OLED RST | 7 |
| OLED SCK | 12 |
| OLED MOSI | 11 |
| I²C SDA | 8 |
| I²C SCL | 9 |

Pin assignments can be changed in `config.h`.

## Software Architecture

### Dual-Core Design

The ESP32-S3's two CPU cores are used to completely isolate the time-critical sensor pipeline from the UI:

**Core 0 — Sensor + USB (1000 Hz, deterministic)**
- Reads ADC (cached continuous mode value via I²C).
- Validates reading (transducer failure, over-pressure, low pressure detection).
- Computes informational PSI and voltage from transducer spec.
- Applies configurable deadzones.
- Applies selected response curve via precomputed lookup table with interpolation.
- Normalizes to 0–4095 Z-axis output.
- Reads and debounces the hold button.
- Sends USB HID report.

**Core 1 — User Interface (best-effort, ~10–30 Hz display refresh)**
- Polls rotary encoder (rotation + button with debounce).
- Runs the menu state machine.
- Renders the OLED display (partial updates to minimize SPI traffic).
- Manages NVS profile storage.

The cores communicate through a shared `LiveData` struct (Core 0 writes, Core 1 reads for display) and a `pendingConfig` + `configDirty` flag (Core 1 writes when user saves settings, Core 0 picks up changes). No mutex is needed — the data flow is unidirectional and torn reads are cosmetically harmless at display refresh rates.

### Response Curves

Six response curves are available, switchable live during gameplay:

| Curve | Formula | Character |
|---|---|---|
| Linear | passthrough | Direct 1:1 mapping |
| Rally Soft | t^2.2 | Gentle initial response, progressive buildup |
| Rally Aggressive | t^0.75 | Strong initial bite, fine top-end control |
| Drift Snap | zero below threshold, then linear | Dead zone then immediate response |
| Wet | t^3.5 | Very soft initial response, prevents lockup |
| S-Curve | sigmoid | Soft at extremes, steep in the middle |

Curves that use mathematical functions are precomputed as 1025-entry lookup tables (1024 segments + sentinel for interpolation). The per-sample cost is a single array lookup with 2-bit fractional linear interpolation — no floating-point math in the sensor pipeline.

### Menu System

The device is configured entirely through the rotary encoder and OLED, with no PC software required.

**LIVE Mode** — Displays real-time sensor data. Four display styles:
- *Full Data* — PSI, voltage, percentage, and progress bar.
- *Clean* — Large centered percentage.
- *Bar* — Arc gauge with percentage.
- *Dark* — Black screen, briefly shows curve name on change.

In LIVE mode, rotating the encoder switches curves instantly. Holding the encoder button while rotating switches between LIVE display modes.

**Menu Navigation** — Click the encoder to enter. Four arrow buttons navigate between parameter screens (left/right) or return to LIVE (up) or enter editing (down).

**Value Editing** — The up/down arrows become Save (S) and Discard (X). Rotate to highlight adjustment buttons, click to change values. Save commits changes to the sensor pipeline immediately. Discard restores the previous values.

### Configuration Screens

- **Hold Mode** — Game (button click sent to PC) or Firmware (axis locked at 100% on click).
- **Deadzones** — Low and high deadzones, 0.0–20.0% in 0.1% increments.
- **Default Curve** — Boot-time curve selection.
- **Snap Threshold** — Drift Snap curve zero-region size (0.0–100.0%).
- **Button Debounce** — 5–200 ms.
- **Refresh Rates** — USB/ADC rate (250/475/860/1000 Hz) and display rate (10/15/20/30 Hz).
- **Calibrate** — Guided zero and max calibration with settling detection and outlier rejection.
- **Save & Load** — 5 profile slots with save, load, and status indication.
- **Language** — English / Spanish (extensible via string table).

### Persistence

Configuration is stored in the ESP32's Non-Volatile Storage (NVS) using a data-driven field descriptor table. Adding a new configuration field requires exactly three changes:
1. Add the field to `DeviceConfig` in `config.h`
2. Set its default in `getDefaultConfig()`
3. Add one line to `CONFIG_FIELDS[]` in `storage.cpp`

Old saved profiles remain compatible — new fields load with their default values.

## File Structure

```
BalduHandbrake/
  assets.h            — icons, buttons and custom fonts are stored here as code
  BalduHandbrake.ino  — setup(), FreeRTOS task creation, shared state
  config.h            — types, enums, structs, pin maps, defaults, tuning constants
  curves.h            — precomputed LUT data for response curves
  display.h/.cpp      — OLED rendering + localization string table
  hid.h/.cpp          — USB HID descriptor + hold button logic (Core 0)
  sensor.h/.cpp       — ADC pipeline (Core 0)
  storage.h/.cpp      — NVS profile persistence
  ui.h/.cpp           — encoder input, menu state machine, calibration (Core 1)
```

## Building

### Requirements

- Arduino IDE 2.x
- ESP32 board package by Espressif (Board Manager)
- Board selection: "Waveshare ESP32-S3-Zero"

### Libraries (install via Library Manager)

- **Adafruit TinyUSB** — USB HID
- **ADS1X15** by Rob Tillaart — ADC driver
- **LovyanGFX** — OLED driver and graphic primitives
- **RotaryEncoder** by Matthias Hertel — Encoder input

### Arduino IDE Settings

| Setting | Value |
|---|---|
| Board | Waveshare ESP32-S3-Zero |
| USB CDC On Boot | Enabled |
| CPU Frequency | 240Mhz (WiFi)
| USB DFU On Boot | Disabled |
| Events Run On | Core 1 |
| Flash Mode | QIO 80 Mhz |
| Arduino Runs On | Core 1 |
| USB Firmware MSC on Boot | Disabled |
| Partition Scheme | Default 4MB with spiffs |
| PSRAM | Enabled |
| Upload Mode | UART0 / Hardware DCD |
| Upload Speed | 921600 |
| USB Mode | USB-OTG (TinyUSB) |

### Upload Procedure

The Waveshare ESP32-S3 Zero uses native USB (no UART-to-USB chip). As we are reprogramming the USB definition to an HID joystick, windows will recognize it as a different device, and thus change the assigned COM port between programming mode and joystick mode. It will fail to upload after compiling and timing out due to it listening on the other COM port. Just select the other port (the one it originally assigned as ESP32-S3 Zero before uploading this sketch), and upload again. It will upload and reconfigure it to the joystick port.

Alternatively, to upload without having to wait, but you need physical access:

1. Disconnect the USB cable
2. Hold the BOOT button
3. Plug in the USB cable while holding BOOT
4. Release BOOT
5. Select the bootloader COM port in Arduino IDE
6. Click Upload
7. Press RESET after upload completes

## Customization

### Adding a Response Curve

1. Add a `CurveID` enum value in `config.h`
2. Add the curve name to `CURVE_NAMES[]` in `config.h`
3. Generate the LUT using the Python script in `curves.h` header comments
4. Add the LUT array to `curves.h` and update `CURVE_LUTS[]`
5. Update `curveToLutIndex()` in `config.h`
6. Handle the new case in `applyCurveCorrection()` in `sensor.cpp`

### Adding a Language

1. Add the `Language` enum value in `config.h`
2. Add a complete row to `STRING_TABLE[][]` in `display.cpp`
3. Add the language name to `LANG_NAMES[]` in `display.cpp`

Note: The SSD1351 with LovyanGFX does supports UTF-8, but most fonts only support 7-bit ASCII. Use unaccented approximations for non-English translations. Full Unicode support would require replacing the font.

### Adding a Configuration Parameter

1. Add the field to `DeviceConfig` in `config.h`
2. Set its default in `getDefaultConfig()` in `config.h`
3. Add one line to `CONFIG_FIELDS[]` in `storage.cpp`

Existing saved profiles will load the new field with its default value automatically.

## Hardware Notes

### Transducer Specifications

The Ejoyous 0–500 PSI transducer outputs 0.5V at 0 PSI and 4.5V at 500 PSI (ratiometric, 5V supply). It can withstand 750 PSI over-pressure without losing calibration, but cannot output voltage above VCC. Thus when the firmware detects readings above 4.82V, it will flag them as potential over-pressure/voltage saturation. Any 0-5V transducer could be added to this hardware, you'd just need to edit the ADC / TRANSDUCER HARDWARE CONSTANTS on `config.h`. The maximum pressure rating should depend on the maximum achievable pressure on your hardware. And please take into consideration that transducers with improved precision tend to be rated to lower SPS.

### ADC Upgrade Path

The ADS1115 is limited to 860 samples per second. For true 1000 Hz sampling, consider upgrading to a faster I²C ADC, the two planned hardware upgrades are the ADS1119 just to get the actual 1000SPS. It would require a new library, with some port efforts on `sensor.h` and `sensor.cpp`, and ideally some extra hardware and/or register setup to enable differential sampling. Please take care that the ADS1119 only has gain 1, so the raw readings for 0psi and max pressure will differ even when they are the same voltage. 
But the best solution would be the ADS122C04, able to achieve 24Bit samples at 2000SPS. It will not only take some extra port on on `sensor.h` and `sensor.cpp`, but since the samples are 24bits, on anything that touches `LiveData` and `getDefaultConfig`. On the other hand, the ADS122C04 can take differential smapling on two channels at 1000SPS simultaneously with up to 128 gain. This would enable to use pressure cells. In addition, you can easily configure two ADS122C04 on the same I²C using different addresses, which would enable you to control three pressure cell pedals on top of the handbrake, from a single controller, if you want to enable that.

### Calibration

The on-device calibration routine:
1. Prompts the user to release the handle, detects settling (readings stable within a configurable band)
2. Collects 500 samples over 5 seconds, sorts them, rejects the top and bottom 5% as outliers, and averages the remainder to establish the zero point
3. Repeats the process with the handle pulled to maximum to establish the full-scale point
4. PSI display always uses the transducer's physical specification — calibration only affects the Z-axis mapping

## License

Apache 2.0 — see [LICENSE](LICENSE) for details.

This license includes an explicit patent grant. You are free to use, modify, and distribute this project. If you build one, I'd love to hear about it.

## Author

Alejandro Belluscio ([@baldusi](https://github.com/baldusi))

