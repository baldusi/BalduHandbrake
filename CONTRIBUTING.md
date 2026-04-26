# Contributing to BalduHandbrake

Thank you for considering contributing to **BalduHandbrake** — an open-source hydraulic handbrake peripheral for professional simracing.

This document outlines the process for reporting issues, suggesting features, and submitting code or hardware extensions. All contributions are welcome, provided they follow the project's technical and licensing standards.

## Code of Conduct

By participating, you agree to respect the [Apache License 2.0](LICENSE) terms and maintain a professional, technical tone. We value competence, clarity, and biological realism over social constructs.

## How to Report a Bug

1. Check the [existing issues](https://github.com/baldusi/BalduHandbrake/issues) and the latest tagged release.
2. Open a new issue using the **Bug Report** template (if available) or manually include:
   - BalduHandbrake version (from OLED or `VERSION` define)
   - ESP32-S3 board / Arduino IDE version
   - ADC type (ADS1115, NAU7802, HX711, etc.)
   - Exact reproduction steps
   - Sensor raw values / calibration data if relevant
   - Any serial output or crash logs

## How to Suggest a Feature or Enhancement

Open an issue labeled **enhancement** with:
- Clear use-case (e.g., “add support for XYZ transducer” or “new response curve type”)
- Expected behaviour vs. current behaviour
- Any supporting formulas, schematics, or reference hardware

## Development Workflow

1. Fork the repository.
2. Create a feature branch: `git checkout -b feature/amazing-thing`
3. Make your changes.
4. Test on real hardware (1000 Hz HID path is performance-critical).
5. Commit with clear, present-tense messages.
6. Open a Pull Request against the `main` branch.

### Branching Policy
- `main` — always stable, reflects latest released tag
- Feature branches — short-lived, descriptive names
- No direct pushes to `main`

## Adding New ADC Support (Hardware Abstraction)

The project is deliberately designed for easy sensor extension via the `/devices` directory.

See the full interface contract in [`devices/README.md`](devices/README.md).

**Quick checklist for a new ADC shim:**
- Implement all functions defined in the contract (`adcBegin`, `adcRead`, `adcRawToCentiUnit`, fault detection, etc.)
- Add a new `#define DEVICE_xxx` in `config.h`
- Provide a complete example in `/devices/` (copy the ADS1115 or HX711 structure)
- Update `sensor.cpp` selection logic and `hardware/` diagrams + BOM
- Add any required library to the Arduino IDE library list in README.md
- Test calibration, 1000 Hz polling, and outlier rejection

PRs that follow this pattern will be merged rapidly.

## Other Contributions Welcome
- New response curves (with formulas + precomputed LUTs)
- Additional UI languages (via `strtable.cpp/h`)
- Mechanical CAD / 3D-print improvements (reference only — not compiled)
- Documentation fixes or translations
- GitHub Actions / CI (Arduino CLI build + unit tests would be excellent)

## Coding Style
- Follow existing patterns (FreeRTOS tasks, LovyanGFX, NVS storage)
- Prefer explicit variable names over abbreviations
- Comment sensor math and any magic numbers
- Keep the 1000 Hz sensor → HID path lean
- Use metric units everywhere

## Legal
- All contributions are licensed under the same [Apache License 2.0 with patent grant](LICENSE)
- By submitting a PR you confirm you have the right to license your work under these terms

Questions? Open an issue or tag `@baldusi` — happy to discuss architecture or hardware details.

Thanks for helping make BalduHandbrake the reference open-source hydraulic handbrake platform.