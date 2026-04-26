# Security Policy

## Supported Versions

Only the latest tagged release is considered supported for security updates.

| Version | Supported          |
|---------|--------------------|
| 1.4.x   | :white_check_mark: |
| < 1.4   | :x:                |

## Reporting a Vulnerability

**Please do NOT report security issues via public GitHub issues.**

If you discover a security vulnerability in BalduHandbrake (firmware, hardware abstraction, HID implementation, or build process), please report it privately to the maintainer.

**Preferred reporting method:**

- Send an email to: **balduHandbrake.security [at] proton.me**  
  (PGP key available on request or via GitHub profile)

Include as much of the following as possible:
- Version and commit hash
- Exact reproduction steps (including hardware/ADC used)
- Potential impact (e.g. USB descriptor crash, sensor data leak, DoS on ESP32, etc.)
- Any suggested mitigation

We aim to acknowledge reports within 48 hours and provide a fix timeline within 7 days for critical issues.

## Disclosure Policy

- We follow **responsible disclosure**: the reporter is credited (unless requested otherwise) once the issue is fixed and released.
- Public disclosure will only occur after a patch is available and users have had reasonable time to update.

## Scope

This policy covers the BalduHandbrake firmware and its immediate dependencies (FreeRTOS tasks, LovyanGFX, NVS storage, ADC shims).  
It does **not** cover:
- Third-party Arduino libraries or toolchains
- Mechanical hardware / 3D prints / hydraulic components
- User-modified code or custom ADC shims

## Thank You

Security is taken seriously even in hobby-grade simracing hardware. Thank you for helping keep BalduHandbrake reliable and safe.