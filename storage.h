/*  BalduHandrake
    Open Source Hydraulic Simracing Handbrake
    Copyright (c) 2026 Alejandro Belluscio
    Additional copyright holders listed inline below.
    This file is licensed under the Apache 2.0 license
    Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// storage.h — NVS Profile Storage Interface
// ============================================================================
// Manages persistent storage of DeviceConfig profiles using the ESP32's
// Non-Volatile Storage (NVS) system. Supports multiple named profiles
// with save, load, and factory reset operations.
//
// NVS is wear-leveled flash storage designed for small key-value pairs —
// ideal for configuration data that changes infrequently (user saves).
// It survives power cycles, OTA updates, and sketch re-uploads.
//
// ADDING A NEW FIELD TO DeviceConfig:
//   1. Add the field to DeviceConfig in config.h
//   2. Set its default in getDefaultConfig() in config.h
//   3. Add one line to CONFIG_FIELDS[] in storage.cpp
//   That's it. Save and load update automatically.
//
// Usage:
//   storageInit()           — call once during setup
//   storageLoadProfile(n)   — load profile n into a DeviceConfig
//   storageSaveProfile(n)   — save a DeviceConfig to profile n
//   storageGetLastProfile() — get the index of the last-used profile
//
// This module has no dependencies on display, UI, or hardware — only
// on config.h for the DeviceConfig struct and <Preferences.h> for NVS.
// A future companion desktop app could reuse this module directly.
// ============================================================================
#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"

// ----------------------------------------------------------------------------
//  Public API
// ----------------------------------------------------------------------------

// Initialize NVS storage. Call once during setup().
// Creates the namespace and verifies NVS is accessible.
// Returns true if NVS initialized successfully.
bool storageInit();

// Load a profile from NVS into the provided DeviceConfig struct.
// profileIndex: 0 to NUM_NVS_PROFILES-1
// Returns true if the profile existed and was loaded successfully.
// If the profile doesn't exist, cfg is filled with factory defaults.
bool storageLoadProfile(uint8_t profileIndex, DeviceConfig& cfg);

// Save the provided DeviceConfig to a profile slot in NVS.
// profileIndex: 0 to NUM_NVS_PROFILES-1
// Returns true if the save completed successfully.
bool storageSaveProfile(uint8_t profileIndex, const DeviceConfig& cfg);

// Check whether a profile slot contains saved data.
// profileIndex: 0 to NUM_NVS_PROFILES-1
bool storageProfileExists(uint8_t profileIndex);

// Get the index of the last-used profile (saved on every load/save).
// Returns 0 if no profile has been used yet.
uint8_t storageGetLastProfile();

// Save which profile was last used (called internally by load/save,
// but exposed in case the UI needs to set it explicitly).
void storageSetLastProfile(uint8_t profileIndex);

// Erase a single profile slot.
// profileIndex: 0 to NUM_NVS_PROFILES-1
// Returns true if the erase completed successfully.
bool storageEraseProfile(uint8_t profileIndex);

// Erase ALL profiles and reset to factory state.
// After this call, storageLoadProfile() will return defaults for all slots.
void storageFactoryReset();

#endif // STORAGE_H
