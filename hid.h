/*  BalduHandrake
    Open Source Hydraulic Simracing Handbrake
    Copyright (c) 2026 Alejandro Belluscio
    Additional copyright holders listed inline below.
    This file is licensed under the Apache 2.0 license
    Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// hid.h — USB HID Interface
// ============================================================================
// Manages the TinyUSB HID joystick device: descriptor, report building,
// report sending, and hold-button logic.
//
// The hold button debounce is co-located here for practical reasons:
// it runs on Core 0 alongside the sensor pipeline because the button
// state directly affects the HID report (hold mode can override the
// Z-axis value and suppress the button click). Keeping it here avoids
// an extra cross-module interface for a single GPIO + debounce counter.
//
// Usage:
//   hidInit()   — call once during setup, BEFORE any other init
//                  (TinyUSB requires early initialization)
//   hidUpdate() — call every sample cycle on Core 0
//
// ============================================================================
#ifndef HID_H
#define HID_H

#include "config.h"

// ----------------------------------------------------------------------------
//  Public API
// ----------------------------------------------------------------------------

// Initialize TinyUSB HID device. MUST be called before all other
// hardware initialization — TinyUSB requires early USB setup.
void hidInit();

// Process hold button + build and send HID report.
//
// axisValue:   Z-axis output from sensor pipeline (0–4095)
// cfg:         active device config (for hold mode and debounce timing)
// liveData:    shared struct — holdActive flag is written here for UI display
// nowMs:       current millis() timestamp (passed in to avoid redundant calls)
void hidUpdate(uint16_t axisValue, const DeviceConfig& cfg,
               LiveData& liveData, unsigned long nowMs);

#endif // HID_H
