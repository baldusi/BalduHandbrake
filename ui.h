// ============================================================================
// ui.h — User Interface Module
// ============================================================================
// Owns the rotary encoder, menu state machine, and orchestrates display
// and storage operations. Runs entirely on Core 1.
//
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
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
