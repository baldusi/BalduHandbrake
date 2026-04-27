# Changelog

All notable changes to **BalduHandbrake** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),  
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.5.5] - 2026-04-27
### Changed
- `ads122c04.h` renamed `ads122c04` from capital C to lowercase c

## [1.5.4] - 2026-04-27
### Changed
- `config.h` commented out all ADC defines and sensort type defines to enable automation of build

## [1.5.3] - 2026-04-26
### Fixed
- `.github/workflows/release.yml` give write permission for the release

## [1.5.2] - 2026-04-26
### Fixed
- `.github/workflows/release.yml` make build paths predictable

## [1.5.1] - 2026-04-26
### Fixed
- `.github/workflows/release.yml` to install dependencies

## [1.5.0] - 2026-04-26
### Added
- `tools/BalduHandbrake-LUT-Generator.py`
- `FW_VERSION` and `FW_BUILD` in `BalduHandbrake.ino`, `config.h` and `display.cpp`
- `.github/workflows/build.yml` and `.github/workflows/release.yml` for automation
- Version 1.2 hardware schematic with ESP32-S3 Nano and ADS1220 on SPI3, for future support.

### Changed
- Minor version as it changed some strings and enabled the new version and build and release automation.

### Fixed
- Typos and inconsistencies on README.md, QUICKSTART.md, `sensor.cpp`, 

## [1.4.1] - 2026-04-25
### Added
- `USAGE.md` (daily operation manual)
- `QUICKSTART.md` (first-30-minutes builder guide)
- `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`
- Full `.github/ISSUE_TEMPLATE/` set (bug report + feature request + config)
- `.github/pull_request_template.md`
- Added screenshots
- Added graphics of the response curves.

### Changed
- Bug fixes and interface consistency across all ADC shims in `devices/`
- Corrected the display of Volts to milliVolts for the load cell sensor case and added the new strings to the tables.
- Renamed LiveDate.pressureLow/.transducerFail/.saturationFail to sensor neutral .sensorLow/.sensorFail/.sensorSaturation. Checked that functionality works on load cell case, too.
- Updated `.gitignore` (maintenance)
- BOM files
- Switched default pinout to V1.1

### Fixed
- Minor inconsistencies in device abstraction layer (affects HX711, NAU7802, ADS1115, ADS122C04 paths)


## [1.4] - 2026-04-25
### Added
- HX711 (HX) support via new ADC shim in `devices/`.
- Full hardware abstraction layer for ADCs (`devices/README.md` + formal interface contract with `adcBegin`, `adcRead`, `adcRawToCentiUnit`, fault detection, etc.).
- Additional hardware reference files in `hardware/` (HX711 variants, updated diagrams and BOM entries).
- Enhanced documentation (README.md updates, device shim examples).

### Fixed
- GAIN configuration bugs in sensor path (sensor.cpp/h and config.h).

### Changed
- Refactored multiple modules (BalduHandbrake.ino, sensor.cpp/h, strtable.cpp, assets.h).

## [1.3] - 2026-04-22
### Added
- NAU7802 load-cell ADC shim support.
- Preliminary (untested) load-cell operating mode.

### Changed
- Moved all UI strings to `strtable.cpp/h` (preparation for multi-language and future UTF-8 work).
- Updated display and UI modules (display.cpp/h, ui.cpp).

## [1.2] - 2026-04-19
### Added
- Preliminary support for ADS122C04 ADC and associated hardware reference files.
- Updated hardware/ directory with new connection diagrams.

### Fixed
- Various bugs (HID, storage, UI, and curve handling).

### Changed
- Refactored assets.h, curves.h, hid.cpp/h, storage.cpp/h, ui.h.

## [1.1.2] - 2026-04-08
### Changed
- Version string typo correction (internal only).

## [1.1.1] - 2026-04-08
### Changed
- Minor release housekeeping (version bump).

## [1.1] - 2026-04-07
### Added
- Full port of display subsystem to LovyanGFX.
- UTF-8 support and antialiasing on OLED.

### Changed
- Major refactoring of display rendering code.

## [1.0.1] - 2026-04-03
### Added
- Sensor filtering improvements.
- Copyright notice updates.
- Expanded README.md with additional build and calibration details.

### Changed
- Minor sensor path tweaks.

## [1.0] - 2026-04-03
### Added
- Persistent parameter save/load via NVS (5 profiles).
- Full navigation and parameter loading while in menus.
- Calibration routine with outlier rejection.

### Changed
- Core FreeRTOS task structure stabilized.

## [0.9] - 2026-04-01
### Added
- Multi-file FreeRTOS refactor.
- Clean compilation across all modules.

## [0.8] - 2026-03-29
### Changed
- Initial merge and baseline structure (early development snapshot).

---

**Notes**  
- Early versions (v0.8–v0.9) represent the initial FreeRTOS refactoring phase.  
- From v1.0 onward the project follows semantic versioning more strictly.  
- Full commit history and file-level diffs are available via Git tags.
