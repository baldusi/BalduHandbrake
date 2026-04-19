/*  BalduHandrake
	Open Source Hydraulic Simracing Handbrake
	Copyright (c) 2026 Alejandro Belluscio
	Additional copyright holders listed inline below.
	This file is licensed under the Apache 2.0 license
	Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// hid.cpp — USB HID Implementation
// ============================================================================
// This module runs entirely on Core 0. It owns the USB HID interface and
// the hold button GPIO. No other module should call TinyUSB functions or
// read the hold button pin.
// ============================================================================

#include "hid.h"
#include <Adafruit_TinyUSB.h>

// ============================================================================
//  USB HID Report Descriptor
// ============================================================================
// Defines a joystick with:
//   - 1 button  (1 bit data + 7 bits padding = 1 byte)
//   - 1 Z-axis  (16 bits, logical range 0–4095)
// Total report: 3 bytes (report ID 3 is prepended by TinyUSB automatically)

static const uint8_t HID_DESCRIPTOR[] = {
    0x05, 0x01,         // USAGE_PAGE (Generic Desktop)
    0x09, 0x04,         // USAGE (Joystick)
    0xA1, 0x01,         // COLLECTION (Application)
      0x85, 0x03,       //   REPORT_ID (3)
      // --- Button (1 bit + 7 padding) ---
      0x05, 0x09,       //   USAGE_PAGE (Button)
      0x19, 0x01,       //   USAGE_MINIMUM (Button 1)
      0x29, 0x01,       //   USAGE_MAXIMUM (Button 1)
      0x15, 0x00,       //   LOGICAL_MINIMUM (0)
      0x25, 0x01,       //   LOGICAL_MAXIMUM (1)
      0x75, 0x01,       //   REPORT_SIZE (1 bit)
      0x95, 0x01,       //   REPORT_COUNT (1)
      0x81, 0x02,       //   INPUT (Data, Variable, Absolute)
      0x75, 0x07,       //   REPORT_SIZE (7 bits padding)
      0x95, 0x01,       //   REPORT_COUNT (1)
      0x81, 0x03,       //   INPUT (Constant, Variable, Absolute)
      // --- Z-Axis (16 bits) ---
      0x05, 0x01,       //   USAGE_PAGE (Generic Desktop)
      0x09, 0x32,       //   USAGE (Z)
      0x15, 0x00,       //   LOGICAL_MINIMUM (0)
      0x26, 0xFF, 0x0F, //   LOGICAL_MAXIMUM (4095)
      0x75, 0x10,       //   REPORT_SIZE (16 bits)
      0x95, 0x01,       //   REPORT_COUNT (1)
      0x81, 0x02,       //   INPUT (Data, Variable, Absolute)
    0xC0                // END_COLLECTION
};

// ============================================================================
//  Module-local state
// ============================================================================
static Adafruit_USBD_HID usbHid;

// Hold button debounce state
static unsigned long lastHoldDebounceTime = 0;
static int           lastHoldReading      = HIGH;
static bool          holdButtonState      = false;  // debounced state (LOW=pressed)
static bool          holdToggle           = false;  // toggled on each press


// ============================================================================
//  hidInit()
// ============================================================================
void hidInit() {
    // TinyUSB HID setup — must happen before TinyUSBDevice configuration
    usbHid.setStringDescriptor(USB_PRODUCT_STR);
    usbHid.setReportDescriptor(HID_DESCRIPTOR, sizeof(HID_DESCRIPTOR));
    usbHid.begin();

    // USB device identity strings
    TinyUSBDevice.setSerialDescriptor(USB_SERIAL_STR);
    TinyUSBDevice.setProductDescriptor(USB_PRODUCT_STR);
    TinyUSBDevice.setManufacturerDescriptor(USB_MANUFACTURER_STR);

    // Hold button GPIO — internal pull-up, active LOW
    pinMode(HOLD_BUTTON_PIN, INPUT_PULLUP);
	
	// Allow time for USB enumeration. Only runs once at boot.
    delay(1200);
}

// ============================================================================
//  hidUpdate() — debounce hold button, build and send HID report
// ============================================================================
void hidUpdate(uint16_t axisValue, const DeviceConfig& cfg,
               LiveData& liveData, unsigned long nowMs) {

    // ------------------------------------------------------------------
    // 1. Hold button debounce (overflow-safe subtraction pattern)
    // ------------------------------------------------------------------
    int currentReading = digitalRead(HOLD_BUTTON_PIN);

    if (currentReading != lastHoldReading) {
        lastHoldDebounceTime = nowMs;
    }

    if ((nowMs - lastHoldDebounceTime) >= cfg.debounceMs) {
        // Reading has been stable long enough
        if (currentReading != holdButtonState) {
            holdButtonState = currentReading;

            // Toggle hold on press (LOW = pressed, active low)
            if (holdButtonState == LOW) {
                holdToggle = !holdToggle;
            }
        }
    }

    lastHoldReading = currentReading;

    // Update shared state for UI display
    liveData.holdActive = holdToggle;

    // ------------------------------------------------------------------
    // 2. Build HID report (3 bytes: button + Z-axis low + Z-axis high)
    // ------------------------------------------------------------------
    uint8_t report[3];

    // Button byte: depends on hold mode
    if (cfg.holdMode == HOLD_FIRMWARE) {
        // Firmware hold mode: button click is NOT sent to the game.
        // The axis is locked at max while hold is active.
        report[0] = 0x00;
    } else {
        // Game hold mode: physical button state is passed through.
        // Button is active LOW on the GPIO, but HID expects 1 = pressed.
        report[0] = (holdButtonState == LOW) ? 0x01 : 0x00;
    }

    // Z-Axis: apply firmware hold override if active
    uint16_t reportAxis = axisValue;
    if (cfg.holdMode == HOLD_FIRMWARE && holdToggle) {
        reportAxis = Z_AXIS_MAX;
    }

    report[1] = lowByte(reportAxis);
    report[2] = highByte(reportAxis);

    // ------------------------------------------------------------------
    // 3. Send report
    // ------------------------------------------------------------------
    if (usbHid.ready()) {
        usbHid.sendReport(3, report, sizeof(report));
    }
}
