/*  BalduHandrake
	Open Source Hydraulic Simracing Handbrake
	Copyright (c) 2026 Alejandro Belluscio
	Additional copyright holders listed inline below.
	This file is licensed under the Apache 2.0 license
	Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// display.h — OLED Display Interface
// ============================================================================
// LIMITATION: 7-bit ASCII only. See notes in display.cpp.

#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "strtable.h"

// ============================================================================
//  UI State
// ============================================================================
struct UIState {
    DisplayState state;
    uint8_t      menuScrollPos;
    uint8_t      editScreenIndex;
    uint8_t      liveScreen;
};

// ============================================================================
//  Initialization
// ============================================================================
void displayInit();
void displayBootScreen(uint8_t language);

// ============================================================================
//  Live Screens
// ============================================================================
void displaySetupLiveFull(const DeviceConfig& cfg);
void displayUpdateLiveFull(const LiveData& data, const DeviceConfig& cfg);

void displaySetupLiveClean(const DeviceConfig& cfg);
void displayUpdateLiveClean(const LiveData& data, const DeviceConfig& cfg);

void displaySetupLiveBar();
void displayUpdateLiveBar(const LiveData& data, const DeviceConfig& cfg);

void displaySetupLiveDark();
void displayShowCurveOverlay(uint8_t curveIndex);
void displayUpdateLiveDark(const LiveData& data, uint8_t language);

void displayUpdateHoldIndicator(bool holdActive, uint8_t language);

// ============================================================================
//  Navigation Bar
// ============================================================================
void displayDrawNavBar(const UIState& ui);
void displayUpdateNavCursor(const UIState& ui, uint8_t prevScrollPos);

// ============================================================================
//  Edit Screens
// ============================================================================
void displayDrawEditScreen(const UIState& ui, const DeviceConfig& cfg,
                           const LiveData& data);
void displayDrawHoldMode(const UIState& ui, const DeviceConfig& cfg);
void displayDrawDeadzones(const UIState& ui, const DeviceConfig& cfg);
void displayDrawDefaultCurve(const UIState& ui, const DeviceConfig& cfg);
void displayDrawSnapThreshold(const UIState& ui, const DeviceConfig& cfg);
void displayDrawButtonDebounce(const UIState& ui, const DeviceConfig& cfg);
void displayDrawRefreshRates(const UIState& ui, const DeviceConfig& cfg);
void displayDrawCalibrate(const UIState& ui, const CalibData& calib, uint8_t language);
void displayDrawLanguage(const UIState& ui, uint8_t currentLanguage);
void displayDrawQuickSave(const UIState& ui, const DeviceConfig& cfg,
                          uint8_t profileSlot, bool justSaved);
void displayDrawSaveLoad(const UIState& ui, uint8_t selectedSlot,
                         const bool slotExists[NUM_NVS_PROFILES], uint8_t language);

// ============================================================================
//  Utility
// ============================================================================
void displayClearContentArea();
void displayDrawEditTitle(const char* title);

#endif // DISPLAY_H
