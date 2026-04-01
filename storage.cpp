// ============================================================================
// storage.cpp — NVS Profile Storage Implementation
// ============================================================================
// Uses the ESP32 Preferences library (NVS wrapper) to store DeviceConfig
// profiles as individual key-value pairs within a namespace.
//
// Storage layout:
//   Namespace: NVS_NAMESPACE ("bhbrake")
//   Keys per profile:  "pN_curve", "pN_snap", "pN_dzLo", etc.
//                      where N is the profile index (0–4)
//   Global keys:       "lastProf" — index of last-used profile
//                      "pN_valid" — flag indicating profile N has saved data
//
// The field descriptor table CONFIG_FIELDS[] defines the mapping between
// DeviceConfig struct fields and NVS keys. Adding a new field to
// DeviceConfig requires adding exactly one line to this table.
//
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
// ============================================================================

#include "storage.h"
#include <Preferences.h>
#include <stddef.h>     // offsetof()
#include <string.h>     // memcpy()

// ============================================================================
//  Field Descriptor Table
// ============================================================================
// Maps each DeviceConfig field to its NVS key suffix, byte offset in the
// struct, and storage size.
//
// TO ADD A NEW FIELD:
//   1. Add the field to DeviceConfig in config.h
//   2. Set its default in getDefaultConfig() in config.h
//   3. Add one line here — that's it, save/load update automatically
//
// Supported sizes:
//   1 = uint8_t / bool  (stored via getUChar / putUChar)
//   2 = uint16_t         (stored via getUShort / putUShort)
//   4 = uint32_t         (stored via getULong / putULong)

struct FieldDescriptor {
    const char* suffix;     // *char up to 12 chars long
    uint16_t    offset;     // uint16_t — supports structs up to 65535 bytes
    uint8_t     size;       // 1, 2, or 4
};

static const FieldDescriptor CONFIG_FIELDS[] = {
    // suffix     offsetof(DeviceConfig, field)                       size
    { "curve",    (uint16_t)offsetof(DeviceConfig, curveIndex),        1 },
    { "snap",     (uint16_t)offsetof(DeviceConfig, snapThreshold),     2 },
    { "dzLo",     (uint16_t)offsetof(DeviceConfig, deadzoneLow),       2 },
    { "dzHi",     (uint16_t)offsetof(DeviceConfig, deadzoneHigh),      2 },
    { "hold",     (uint16_t)offsetof(DeviceConfig, holdMode),          1 },
    { "livScr",   (uint16_t)offsetof(DeviceConfig, liveScreen),        1 },
    { "dkTout",   (uint16_t)offsetof(DeviceConfig, liveDarkTimeoutMs), 2 },
    { "lang",     (uint16_t)offsetof(DeviceConfig, language),          1 },
    { "sRate",    (uint16_t)offsetof(DeviceConfig, sampleRateHz),      2 },
    { "dRate",    (uint16_t)offsetof(DeviceConfig, displayRateHz),     2 },
    { "dbnc",     (uint16_t)offsetof(DeviceConfig, debounceMs),        1 },
    { "calZ",     (uint16_t)offsetof(DeviceConfig, calAdcZero),        2 },
    { "calM",     (uint16_t)offsetof(DeviceConfig, calAdcMax),         2 },
};
#define NUM_CONFIG_FIELDS (sizeof(CONFIG_FIELDS) / sizeof(CONFIG_FIELDS[0]))

// ============================================================================
//  Module-local state
// ============================================================================
static Preferences prefs;

// ============================================================================
//  Key generation helper
// ============================================================================
// Builds a profile-specific key like "p2_curve" from index 2 and suffix "curve".
// Buffer must be at least KEY_BUF_SIZE bytes.

static const uint8_t KEY_BUF_SIZE = 16;

static void makeKey(char* buf, uint8_t profileIndex, const char* suffix) {
    buf[0] = 'p';
    buf[1] = '0' + profileIndex;
    buf[2] = '_';
    uint8_t i = 0;
    while (suffix[i] != '\0' && i < KEY_BUF_SIZE - 4) {
        buf[3 + i] = suffix[i];
        i++;
    }
    buf[3 + i] = '\0';
}

// ============================================================================
//  Generic load/save helpers (driven by CONFIG_FIELDS table)
// ============================================================================

static void loadAllFields(uint8_t profileIndex, DeviceConfig& cfg) {
    DeviceConfig defaults = getDefaultConfig();
    uint8_t* cfgBytes = (uint8_t*)&cfg;
    uint8_t* defBytes = (uint8_t*)&defaults;
    char key[KEY_BUF_SIZE];

    for (uint8_t i = 0; i < NUM_CONFIG_FIELDS; i++) {
        const FieldDescriptor& fd = CONFIG_FIELDS[i];
        makeKey(key, profileIndex, fd.suffix);

        if (fd.size == 1) {
            cfgBytes[fd.offset] = prefs.getUChar(key, defBytes[fd.offset]);
        } else if (fd.size == 2) {
            uint16_t defVal;
            memcpy(&defVal, &defBytes[fd.offset], sizeof(uint16_t));
            uint16_t val = prefs.getUShort(key, defVal);
            memcpy(&cfgBytes[fd.offset], &val, sizeof(uint16_t));
        } else {
            uint32_t defVal;
            memcpy(&defVal, &defBytes[fd.offset], sizeof(uint32_t));
            uint32_t val = prefs.getULong(key, defVal);
            memcpy(&cfgBytes[fd.offset], &val, sizeof(uint32_t));
        }
    }
}

static void saveAllFields(uint8_t profileIndex, const DeviceConfig& cfg) {
    const uint8_t* cfgBytes = (const uint8_t*)&cfg;
    char key[KEY_BUF_SIZE];

    for (uint8_t i = 0; i < NUM_CONFIG_FIELDS; i++) {
        const FieldDescriptor& fd = CONFIG_FIELDS[i];
        makeKey(key, profileIndex, fd.suffix);

        if (fd.size == 1) {
            prefs.putUChar(key, cfgBytes[fd.offset]);
        } else if (fd.size == 2) {
            uint16_t val;
            memcpy(&val, &cfgBytes[fd.offset], sizeof(uint16_t));
            prefs.putUShort(key, val);
        } else {
            uint32_t val;
            memcpy(&val, &cfgBytes[fd.offset], sizeof(uint32_t));
            prefs.putULong(key, val);
        }
    }
}

// ============================================================================
//  storageInit()
// ============================================================================
bool storageInit() {
    bool ok = prefs.begin(NVS_NAMESPACE, false);
    if (ok) {
        prefs.end();
    }
    return ok;
}

// ============================================================================
//  storageLoadProfile()
// ============================================================================
bool storageLoadProfile(uint8_t profileIndex, DeviceConfig& cfg) {
    if (profileIndex >= NUM_NVS_PROFILES) {
        cfg = getDefaultConfig();
        return false;
    }

    if (!prefs.begin(NVS_NAMESPACE, true)) {
        cfg = getDefaultConfig();
        return false;
    }

    char key[KEY_BUF_SIZE];
    makeKey(key, profileIndex, "valid");
    bool exists = prefs.getBool(key, false);

    if (!exists) {
        prefs.end();
        cfg = getDefaultConfig();
        return false;
    }

    loadAllFields(profileIndex, cfg);

    prefs.end();

    storageSetLastProfile(profileIndex);

    return true;
}

// ============================================================================
//  storageSaveProfile()
// ============================================================================
bool storageSaveProfile(uint8_t profileIndex, const DeviceConfig& cfg) {
    if (profileIndex >= NUM_NVS_PROFILES) return false;

    if (!prefs.begin(NVS_NAMESPACE, false)) return false;

    char key[KEY_BUF_SIZE];
    makeKey(key, profileIndex, "valid");
    prefs.putBool(key, true);

    saveAllFields(profileIndex, cfg);

    prefs.end();

    storageSetLastProfile(profileIndex);

    return true;
}

// ============================================================================
//  storageProfileExists()
// ============================================================================
bool storageProfileExists(uint8_t profileIndex) {
    if (profileIndex >= NUM_NVS_PROFILES) return false;

    if (!prefs.begin(NVS_NAMESPACE, true)) return false;

    char key[KEY_BUF_SIZE];
    makeKey(key, profileIndex, "valid");
    bool exists = prefs.getBool(key, false);

    prefs.end();
    return exists;
}

// ============================================================================
//  storageGetLastProfile()
// ============================================================================
uint8_t storageGetLastProfile() {
    if (!prefs.begin(NVS_NAMESPACE, true)) return 0;

    uint8_t idx = prefs.getUChar("lastProf", 0);

    prefs.end();

    return (idx < NUM_NVS_PROFILES) ? idx : 0;
}

// ============================================================================
//  storageSetLastProfile()
// ============================================================================
void storageSetLastProfile(uint8_t profileIndex) {
    if (profileIndex >= NUM_NVS_PROFILES) return;

    if (!prefs.begin(NVS_NAMESPACE, false)) return;

    prefs.putUChar("lastProf", profileIndex);

    prefs.end();
}

// ============================================================================
//  storageEraseProfile()
// ============================================================================
bool storageEraseProfile(uint8_t profileIndex) {
    if (profileIndex >= NUM_NVS_PROFILES) return false;

    if (!prefs.begin(NVS_NAMESPACE, false)) return false;

    char key[KEY_BUF_SIZE];
    makeKey(key, profileIndex, "valid");
    prefs.remove(key);

    prefs.end();
    return true;
}

// ============================================================================
//  storageFactoryReset()
// ============================================================================
void storageFactoryReset() {
    if (!prefs.begin(NVS_NAMESPACE, false)) return;

    prefs.clear();

    prefs.end();
}
