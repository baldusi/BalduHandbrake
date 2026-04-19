/*  BalduHandrake
	Open Source Hydraulic Simracing Handbrake
	Copyright (c) 2026 Alejandro Belluscio
	Additional copyright holders listed inline below.
	This file is licensed under the Apache 2.0 license
	Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// ui.h — User Interface Module
// ============================================================================
// Owns the rotary encoder, menu state machine, and orchestrates display
// and storage operations. Runs entirely on Core 1.
// ============================================================================
#ifndef UI_H
#define UI_H

#include "config.h"
#include "display.h"

// ============================================================================
//  Public API
// ============================================================================

void uiInit();

void uiUpdate(const LiveData& liveData,
              const DeviceConfig& activeConfig,
              DeviceConfig& pendingConfig,
              volatile bool& configDirty,
              unsigned long nowMs);

#endif // UI_H
