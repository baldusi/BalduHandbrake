// ============================================================================
// display.cpp — OLED Display Implementation
// ============================================================================
// All SSD1351 drawing code. Runs entirely on Core 1. Owns the SPI bus.
//
// LIMITATION: 7-bit ASCII only. See display.h header for details.
//
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
// ============================================================================

#include "display.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

// ============================================================================
//  Localization String Table
// ============================================================================
// Indexed by [Language][StringID].
// IMPORTANT: Every row must have exactly NUM_STRINGS entries in StringID order.
// 7-bit ASCII only — use unaccented approximations for non-English.

static const char* const STRING_TABLE[NUM_LANGUAGES][NUM_STRINGS] = {
    // ---- LANG_EN (English) ----
    {
        "E-BRAKE",              // STR_BOOT_TITLE
        "v1.0",                 // STR_BOOT_VERSION
        "A.Belluscio",          // STR_BOOT_AUTHOR
        "BalduHandbrake",       // STR_BOOT_PROJECT
        "Initializing...",      // STR_BOOT_STATUS
        "HOLD",                 // STR_HOLD
        "LIVE",                 // STR_LIVE
        "PSI",                  // STR_PSI
        "V",                    // STR_VOLTS
        "%",                    // STR_PERCENT
        "FAIL",                 // STR_FAIL
        "LOW",                  // STR_LOW
        "OVER",                 // STR_OVER
        "Game",                 // STR_GAME
        "Firmware",             // STR_FIRMWARE
        "Save",                 // STR_SAVE
        "Load",                 // STR_LOAD
        "Empty",                // STR_EMPTY
        "Saved",                // STR_SAVED
        "Prof",                 // STR_PROFILE
        "Push handle down",     // STR_CAL_PUSH_DOWN
        "Pull handle up",       // STR_CAL_PULL_UP
        "Settling...",          // STR_CAL_SETTLING
        "Hold steady...",       // STR_CAL_HOLD_STEADY
        "Sampling...",          // STR_CAL_SAMPLING
        "Calibration OK",       // STR_CAL_DONE
        "Zero:",                // STR_CAL_ZERO_OK
        "Max:",                 // STR_CAL_MAX_OK
        "ERROR",                // STR_CAL_ERROR
        "X=retry S=keep",       // STR_CAL_RETRY
        "Purge fluid!",         // STR_CAL_PURGE
        "Overpressure!",        // STR_CAL_OVERPRESS
        "Too close to zero",    // STR_CAL_TOO_LOW
        "Hold Mode",            // STR_TITLE_HOLD_MODE
        "Deadzones",            // STR_TITLE_DEADZONES
        "Default Curve",        // STR_TITLE_DEFAULT_CURVE
        "Snap Threshold",       // STR_TITLE_SNAP_THRESH
        "Btn Debounce",         // STR_TITLE_DEBOUNCE
        "Refresh Rates",        // STR_TITLE_REFRESH
        "Recalibrate",          // STR_TITLE_CALIBRATE
        "Save & Load",          // STR_TITLE_SAVE_LOAD
        "Language",             // STR_TITLE_LANGUAGE
        "Low:",                 // STR_LABEL_LOW
        "High:",                // STR_LABEL_HIGH
        "USB/ADC:",             // STR_LABEL_USB_ADC
        "Display:",             // STR_LABEL_DISPLAY
        "ms",                   // STR_LABEL_MS
        "Hz",                   // STR_LABEL_HZ
    },

    // ---- LANG_ES (Espanol) ----
    {
        "E-BRAKE",              // STR_BOOT_TITLE
        "v1.0",                 // STR_BOOT_VERSION
        "A.Belluscio",          // STR_BOOT_AUTHOR
        "BalduHandbrake",       // STR_BOOT_PROJECT
        "Iniciando...",         // STR_BOOT_STATUS
        "ESPERA",               // STR_HOLD
        "VIVO",                 // STR_LIVE
        "PSI",                  // STR_PSI
        "V",                    // STR_VOLTS
        "%",                    // STR_PERCENT
        "FALLO",                // STR_FAIL
        "BAJA",                 // STR_LOW
        "SOBRE",                // STR_OVER
        "Juego",                // STR_GAME
        "Firmware",             // STR_FIRMWARE
        "Guardar",              // STR_SAVE
        "Cargar",               // STR_LOAD
        "Vacio",                // STR_EMPTY
        "Guardado",             // STR_SAVED
        "Perf",                 // STR_PROFILE
        "Empuje palanca",       // STR_CAL_PUSH_DOWN
        "Tire palanca",         // STR_CAL_PULL_UP
        "Estabilizando...",     // STR_CAL_SETTLING
        "Mantenga firme...",    // STR_CAL_HOLD_STEADY
        "Muestreando...",       // STR_CAL_SAMPLING
        "Calibracion OK",       // STR_CAL_DONE
        "Cero:",                // STR_CAL_ZERO_OK
        "Max:",                 // STR_CAL_MAX_OK
        "ERROR",                // STR_CAL_ERROR
        "X=rein S=guard",       // STR_CAL_RETRY
        "Purgar fluido!",       // STR_CAL_PURGE
        "Sobrepresion!",        // STR_CAL_OVERPRESS
        "Muy cerca de cero",    // STR_CAL_TOO_LOW
        "Modo Espera",          // STR_TITLE_HOLD_MODE
        "Zonas Muertas",        // STR_TITLE_DEADZONES
        "Curva Default",        // STR_TITLE_DEFAULT_CURVE
        "Umbral Snap",          // STR_TITLE_SNAP_THRESH
        "Antirrebote",          // STR_TITLE_DEBOUNCE
        "Tasas Refresco",       // STR_TITLE_REFRESH
        "Recalibrar",           // STR_TITLE_CALIBRATE
        "Guard & Cargar",       // STR_TITLE_SAVE_LOAD
        "Idioma",               // STR_TITLE_LANGUAGE
        "Bajo:",                // STR_LABEL_LOW
        "Alto:",                // STR_LABEL_HIGH
        "USB/ADC:",             // STR_LABEL_USB_ADC
        "Pantalla:",            // STR_LABEL_DISPLAY
        "ms",                   // STR_LABEL_MS
        "Hz",                   // STR_LABEL_HZ
    },
};

static const char* const LANG_NAMES[NUM_LANGUAGES] = {
    "English",
    "Espanol",
};

// ============================================================================
//  Module-local OLED instance
// ============================================================================
static Adafruit_SSD1351 tft = Adafruit_SSD1351(
    SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_CS, OLED_DC, OLED_RST
);

static uint8_t activeLang = LANG_EN;

// ============================================================================
//  Layout constants
// ============================================================================
static const int16_t CONTENT_Y    = NAV_BOX_H + NAV_SEPARATOR_PX;
static const int16_t CONTENT_H    = SCREEN_HEIGHT - CONTENT_Y;

// Arc gauge parameters
static const int16_t ARC_CX       = SCREEN_WIDTH / 2;
static const int16_t ARC_CY       = SCREEN_HEIGHT / 2 + 8;
static const int16_t ARC_OUTER_R  = 55;
static const int16_t ARC_INNER_R  = 42;
static const float   ARC_START    = 135.0f;
static const float   ARC_SWEEP    = 270.0f;
// DEG_TO_RAD is already defined in Arduino.h — we just use it directly.
//static const float   DEG_TO_RAD   = 3.14159265f / 180.0f;

static uint16_t prevArcPercent = 0xFFFF;

// ============================================================================
//  String lookup
// ============================================================================
static const char* str(StringID id) {
    if (activeLang >= NUM_LANGUAGES) return STRING_TABLE[LANG_EN][id];
    if (id >= NUM_STRINGS) return "";
    return STRING_TABLE[activeLang][id];
}

const char* displayGetString(StringID id, uint8_t language) {
    if (language >= NUM_LANGUAGES) language = LANG_EN;
    if (id >= NUM_STRINGS) return "";
    return STRING_TABLE[language][id];
}

// ============================================================================
//  displayInit()
// ============================================================================
void displayInit() {
    SPI.begin(OLED_SCK, -1, OLED_MOSI);
    tft.begin();
    tft.setRotation(0);
    delay(200);
}

// ============================================================================
//  displayBootScreen()
// ============================================================================
void displayBootScreen(uint8_t language) {
    activeLang = language;
    tft.fillScreen(LIVE_BG_COLOR);
    tft.setTextColor(LIVE_FG_COLOR);
    tft.setTextSize(2);
    tft.setCursor(16, 24);
    tft.print(str(STR_BOOT_TITLE));
    tft.setTextSize(1);
    tft.setTextColor(LIVE_VALUE_COLOR);
    tft.setCursor(16, 52);
    tft.print(str(STR_BOOT_PROJECT));
    tft.setTextColor(LIVE_LABEL_COLOR);
    tft.setCursor(16, 66);
    tft.print(str(STR_BOOT_VERSION));
    tft.setCursor(16, 80);
    tft.print(str(STR_BOOT_AUTHOR));
    tft.setTextColor(LIVE_OK_COLOR);
    tft.setCursor(16, 100);
    tft.print(str(STR_BOOT_STATUS));
    delay(2000);
}

// ============================================================================
//  Helpers
// ============================================================================
static void printCentiValue(int16_t x, int16_t y, uint32_t centiVal,
                            uint16_t color, uint8_t textSize) {
    tft.setTextColor(color);
    tft.setTextSize(textSize);
    tft.setCursor(x, y);
    tft.print(centiVal / 100u);
    tft.print(".");
    uint16_t frac = centiVal % 100u;
    if (frac < 10) tft.print("0");
    tft.print(frac);
}

static void printSensorStatus(int16_t x, int16_t y, const LiveData& data,
                               uint8_t textSize) {
    tft.setTextSize(textSize);
    tft.setCursor(x, y);
    if (data.transducerFail) {
        tft.setTextColor(LIVE_ERROR_COLOR);
        tft.print(str(STR_FAIL));
    } else if (data.pressureLow) {
        tft.setTextColor(LIVE_WARN_COLOR);
        tft.print(str(STR_LOW));
    } else if (data.saturationFail) {
        tft.setTextColor(LIVE_ERROR_COLOR);
        tft.print(str(STR_OVER));
    }
}

static void drawSmallArrowUp(int16_t cx, int16_t y, uint16_t color) {
    tft.fillTriangle(cx - 4, y + 8, cx + 4, y + 8, cx, y, color);
}
static void drawSmallArrowDown(int16_t cx, int16_t y, uint16_t color) {
    tft.fillTriangle(cx - 4, y, cx + 4, y, cx, y + 8, color);
}
static void drawDoubleArrowUp(int16_t cx, int16_t y, uint16_t color) {
    tft.fillTriangle(cx - 4, y + 8, cx + 4, y + 8, cx, y + 2, color);
    tft.fillTriangle(cx - 4, y + 12, cx + 4, y + 12, cx, y + 6, color);
}
static void drawDoubleArrowDown(int16_t cx, int16_t y, uint16_t color) {
    tft.fillTriangle(cx - 4, y, cx + 4, y, cx, y + 6, color);
    tft.fillTriangle(cx - 4, y + 4, cx + 4, y + 4, cx, y + 10, color);
}
static void drawButtonHighlight(int16_t x, int16_t y, int16_t w, int16_t h,
                                bool isSelected) {
    tft.fillRect(x, y, w, h, isSelected ? NAV_SELECTED_BG : EDIT_BG_COLOR);
}

// ============================================================================
//  LIVE_FULL_DATA
// ============================================================================
void displaySetupLiveFull(const DeviceConfig& cfg) {
    activeLang = cfg.language;
    tft.fillScreen(LIVE_BG_COLOR);
    tft.setTextColor(LIVE_FG_COLOR);
    tft.setTextSize(2);
    tft.setCursor(4, 2);
    if (cfg.curveIndex < NUM_CURVES) tft.print(CURVE_NAMES[cfg.curveIndex]);
    tft.setTextSize(2);
    tft.setTextColor(LIVE_LABEL_COLOR);
    tft.setCursor(90, 32); tft.print(str(STR_PSI));
    tft.setCursor(90, 54); tft.print(str(STR_VOLTS));
    tft.setCursor(90, 76); tft.print(str(STR_PERCENT));
    tft.drawRect(4, 116, 122, 12, LIVE_FG_COLOR);
}

void displayUpdateLiveFull(const LiveData& data, uint8_t language) {
    activeLang = language;
    tft.fillRect(12, 32, 70, 16, LIVE_BG_COLOR);
    if (data.transducerFail || data.pressureLow || data.saturationFail)
        printSensorStatus(12, 32, data, 2);
    else
        printCentiValue(12, 32, data.centiPsi, LIVE_OK_COLOR, 2);
    tft.fillRect(12, 54, 70, 16, LIVE_BG_COLOR);
    printCentiValue(12, 54, data.centiVolts, LIVE_ERROR_COLOR, 2);
    tft.fillRect(12, 76, 70, 16, LIVE_BG_COLOR);
    printCentiValue(12, 76, data.centiPercent, LIVE_VALUE_COLOR, 2);
    int barWidth = (int)((uint32_t)data.centiPercent * 120UL / 10000UL);
    tft.fillRect(5, 117, barWidth, 10, LIVE_VALUE_COLOR);
    tft.fillRect(5 + barWidth, 117, 120 - barWidth, 10, LIVE_BG_COLOR);
}

// ============================================================================
//  LIVE_CLEAN
// ============================================================================
void displaySetupLiveClean(uint8_t language) {
    activeLang = language;
    tft.fillScreen(LIVE_BG_COLOR);
    tft.setTextColor(LIVE_LABEL_COLOR);
    tft.setTextSize(1);
    tft.setCursor(100, 120);
    tft.print(str(STR_PERCENT));
}

void displayUpdateLiveClean(const LiveData& data, uint8_t language) {
    activeLang = language;
    tft.fillRect(4, 40, 120, 40, LIVE_BG_COLOR);
    if (data.transducerFail || data.pressureLow || data.saturationFail)
        printSensorStatus(4, 48, data, 3);
    else
        printCentiValue(4, 48, data.centiPercent, LIVE_VALUE_COLOR, 3);
}

// ============================================================================
//  LIVE_BAR_ONLY (Arc Gauge)
// ============================================================================
static void drawArcSegment(float startDeg, float endDeg,
                           int16_t innerR, int16_t outerR,
                           int16_t cx, int16_t cy, uint16_t color) {
    const float step = 3.0f;
    for (float angle = startDeg; angle < endDeg; angle += step) {
        float a1 = angle * DEG_TO_RAD;
        float a2 = ((angle + step > endDeg) ? endDeg : angle + step) * DEG_TO_RAD;
        float cos1 = cos(a1), sin1 = sin(a1);
        float cos2 = cos(a2), sin2 = sin(a2);
        int16_t ix1 = cx + (int16_t)(innerR * cos1);
        int16_t iy1 = cy + (int16_t)(innerR * sin1);
        int16_t ox1 = cx + (int16_t)(outerR * cos1);
        int16_t oy1 = cy + (int16_t)(outerR * sin1);
        int16_t ix2 = cx + (int16_t)(innerR * cos2);
        int16_t iy2 = cy + (int16_t)(innerR * sin2);
        int16_t ox2 = cx + (int16_t)(outerR * cos2);
        int16_t oy2 = cy + (int16_t)(outerR * sin2);
        tft.fillTriangle(ix1, iy1, ox1, oy1, ox2, oy2, color);
        tft.fillTriangle(ix1, iy1, ix2, iy2, ox2, oy2, color);
    }
}

void displaySetupLiveBar() {
    tft.fillScreen(LIVE_BG_COLOR);
    prevArcPercent = 0xFFFF;
    drawArcSegment(ARC_START, ARC_START + ARC_SWEEP,
                   ARC_INNER_R, ARC_OUTER_R, ARC_CX, ARC_CY, NAV_NORMAL_BG);
}

void displayUpdateLiveBar(const LiveData& data, uint8_t language) {
    activeLang = language;
    uint16_t pct = data.centiPercent;
    uint16_t quantized = pct / 100;
    uint16_t prevQuantized = (prevArcPercent == 0xFFFF) ? 0xFFFF : prevArcPercent / 100;
    if (quantized != prevQuantized) {
        float fillEnd = ARC_START + (ARC_SWEEP * pct / 10000.0f);
        if (pct > prevArcPercent || prevArcPercent == 0xFFFF) {
            float prevEnd = (prevArcPercent == 0xFFFF) ? ARC_START
                            : ARC_START + (ARC_SWEEP * prevArcPercent / 10000.0f);
            if (fillEnd > prevEnd)
                drawArcSegment(prevEnd, fillEnd, ARC_INNER_R, ARC_OUTER_R,
                               ARC_CX, ARC_CY, LIVE_VALUE_COLOR);
        } else {
            float oldEnd = ARC_START + (ARC_SWEEP * prevArcPercent / 10000.0f);
            if (oldEnd > fillEnd)
                drawArcSegment(fillEnd, oldEnd, ARC_INNER_R, ARC_OUTER_R,
                               ARC_CX, ARC_CY, NAV_NORMAL_BG);
        }
        prevArcPercent = pct;
    }
    tft.fillRect(ARC_CX - 30, ARC_CY - 8, 60, 20, LIVE_BG_COLOR);
    if (data.transducerFail || data.pressureLow || data.saturationFail) {
        printSensorStatus(ARC_CX - 24, ARC_CY - 8, data, 2);
    } else {
        tft.setTextColor(LIVE_FG_COLOR);
        tft.setTextSize(2);
        uint16_t intPct = pct / 100;
        int16_t tx = (intPct < 10) ? ARC_CX - 6 :
                     (intPct < 100) ? ARC_CX - 12 : ARC_CX - 18;
        tft.setCursor(tx, ARC_CY - 8);
        tft.print(intPct);
        tft.print(str(STR_PERCENT));
    }
}

// ============================================================================
//  LIVE_DARK
// ============================================================================
void displaySetupLiveDark() { tft.fillScreen(LIVE_BG_COLOR); }

void displayShowCurveOverlay(uint8_t curveIndex) {
    tft.fillRect(0, 40, SCREEN_WIDTH, 48, LIVE_BG_COLOR);
    tft.setTextColor(LIVE_VALUE_COLOR);
    tft.setTextSize(2);
    tft.setCursor(4, 56);
    if (curveIndex < NUM_CURVES) tft.print(CURVE_NAMES[curveIndex]);
}

// ============================================================================
//  Hold Mode Indicator
// ============================================================================
void displayUpdateHoldIndicator(bool holdActive, uint8_t language) {
    activeLang = language;
    tft.fillRect(36, 98, 56, 16, LIVE_BG_COLOR);
    tft.setTextSize(2);
    tft.setCursor(36, 98);
    if (holdActive) {
        tft.setTextColor(LIVE_ERROR_COLOR);
        tft.print(str(STR_HOLD));
    } else {
        tft.setTextColor(LIVE_OK_COLOR);
        tft.print(str(STR_LIVE));
    }
}

// ============================================================================
//  Navigation Bar
// ============================================================================
static bool isNavBoxVisible(const UIState& ui, uint8_t index) {
    if (ui.state == DISPLAY_LIVE) return false;
    if (ui.state == DISPLAY_MENU_LIST) return true;
    if (ui.state == DISPLAY_EDIT_VALUE) return (index == 2 || index == 3);
    return false;
}

static void drawNavBox(const UIState& ui, uint8_t index, bool isSelected) {
    if (index > 3) return;
    int16_t x = NAV_BOX_X[index];
    int16_t y = NAV_BOX_Y[index];
    uint16_t bg = isSelected ? NAV_SELECTED_BG : NAV_NORMAL_BG;
    tft.fillRect(x, y, NAV_BOX_W, NAV_BOX_H, bg);
    uint16_t fg = isSelected ? NAV_SELECTED_FG : NAV_NORMAL_FG;
    int16_t cx = x + NAV_BOX_W / 2;
    int16_t cy = y + NAV_BOX_H / 2;
    if (ui.state == DISPLAY_EDIT_VALUE && (index == 2 || index == 3)) {
        tft.setTextSize(2);
        tft.setTextColor(fg);
        tft.setCursor(x + 10, y + 1);
        tft.print((index == 2) ? 'S' : 'X');
    } else {
        switch (index) {
            case 0: tft.fillTriangle(x+NAV_BOX_W-5,cy-6, x+NAV_BOX_W-5,cy+6, x+8,cy, fg); break;
            case 1: tft.fillTriangle(x+8,cy-6, x+8,cy+6, x+NAV_BOX_W-5,cy, fg); break;
            case 2: tft.fillTriangle(cx-7,y+NAV_BOX_H-5, cx+7,y+NAV_BOX_H-5, cx,y+5, fg); break;
            case 3: tft.fillTriangle(cx-7,y+5, cx+7,y+5, cx,y+NAV_BOX_H-5, fg); break;
        }
    }
}

void displayDrawNavBar(const UIState& ui) {
    if (ui.state == DISPLAY_LIVE) return;
    tft.fillRect(0, NAV_BOX_Y[0], SCREEN_WIDTH, NAV_BOX_H, NAV_NORMAL_BG);
    tft.fillRect(0, NAV_BOX_H, SCREEN_WIDTH, NAV_SEPARATOR_PX, EDIT_BG_COLOR);
    for (uint8_t i = 0; i < NAV_BOX_COUNT; i++) {
        if (isNavBoxVisible(ui, i))
            drawNavBox(ui, i, ui.menuScrollPos == i);
    }
}

void displayUpdateNavCursor(const UIState& ui, uint8_t prevScrollPos) {
    if (prevScrollPos < NAV_BOX_COUNT && isNavBoxVisible(ui, prevScrollPos))
        drawNavBox(ui, prevScrollPos, false);
    if (ui.menuScrollPos < NAV_BOX_COUNT && isNavBoxVisible(ui, ui.menuScrollPos))
        drawNavBox(ui, ui.menuScrollPos, true);
}

// ============================================================================
//  Edit Screen Utilities
// ============================================================================
void displayClearContentArea() {
    tft.fillRect(0, CONTENT_Y, SCREEN_WIDTH, CONTENT_H, EDIT_BG_COLOR);
}

void displayDrawEditTitle(const char* title) {
    tft.setTextSize(1);
    tft.setTextColor(EDIT_TITLE_COLOR);
    tft.setCursor(4, CONTENT_Y + 4);
    tft.print(title);
}

// ============================================================================
//  Edit Screens
// ============================================================================
void displayDrawHoldMode(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_HOLD_MODE));
    bool isFW = (cfg.holdMode == HOLD_FIRMWARE);
    int16_t baseY = CONTENT_Y + 20;
    drawButtonHighlight(4, baseY, 120, 22, ui.menuScrollPos == 4);
    tft.setTextSize(2);
    tft.setTextColor(isFW ? NAV_NORMAL_FG : EDIT_VALUE_COLOR);
    tft.setCursor(8, baseY + 3); tft.print(str(STR_GAME));
    if (!isFW) { tft.setCursor(108, baseY + 3); tft.print("*"); }
    drawButtonHighlight(4, baseY + 30, 120, 22, ui.menuScrollPos == 5);
    tft.setTextColor(isFW ? EDIT_VALUE_COLOR : NAV_NORMAL_FG);
    tft.setCursor(8, baseY + 33); tft.print(str(STR_FIRMWARE));
    if (isFW) { tft.setCursor(108, baseY + 33); tft.print("*"); }
}

void displayDrawDeadzones(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_DEADZONES));
    int16_t baseY = CONTENT_Y + 16, arrowX = 100;

    tft.setTextSize(1); tft.setTextColor(EDIT_LABEL_COLOR);
    tft.setCursor(8, baseY); tft.print(str(STR_LABEL_LOW));
    tft.setTextSize(2); tft.setCursor(8, baseY + 12); tft.setTextColor(EDIT_VALUE_COLOR);
    tft.print(cfg.deadzoneLow / 10u); tft.print("."); tft.print(cfg.deadzoneLow % 10u); tft.print(str(STR_PERCENT));
    drawDoubleArrowUp(arrowX, baseY + 2, (ui.menuScrollPos == 4) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowUp(arrowX + 18, baseY + 2, (ui.menuScrollPos == 5) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowDown(arrowX + 18, baseY + 18, (ui.menuScrollPos == 6) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawDoubleArrowDown(arrowX, baseY + 18, (ui.menuScrollPos == 7) ? NAV_SELECTED_FG : NAV_NORMAL_FG);

    int16_t hiY = baseY + 44;
    tft.setTextSize(1); tft.setTextColor(EDIT_LABEL_COLOR);
    tft.setCursor(8, hiY); tft.print(str(STR_LABEL_HIGH));
    tft.setTextSize(2); tft.setCursor(8, hiY + 12); tft.setTextColor(EDIT_VALUE_COLOR);
    tft.print(cfg.deadzoneHigh / 10u); tft.print("."); tft.print(cfg.deadzoneHigh % 10u); tft.print(str(STR_PERCENT));
    drawDoubleArrowUp(arrowX, hiY + 2, (ui.menuScrollPos == 8) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowUp(arrowX + 18, hiY + 2, (ui.menuScrollPos == 9) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowDown(arrowX + 18, hiY + 18, (ui.menuScrollPos == 10) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawDoubleArrowDown(arrowX, hiY + 18, (ui.menuScrollPos == 11) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
}

void displayDrawDefaultCurve(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_DEFAULT_CURVE));
    int16_t baseY = CONTENT_Y + 30;
    tft.setTextSize(2); tft.setTextColor(EDIT_VALUE_COLOR);
    tft.setCursor(8, baseY + 10);
    if (cfg.curveIndex < NUM_CURVES) tft.print(CURVE_NAMES[cfg.curveIndex]);
    drawSmallArrowUp(108, baseY, (ui.menuScrollPos == 4) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowDown(108, baseY + 22, (ui.menuScrollPos == 5) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
}

void displayDrawSnapThreshold(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_SNAP_THRESH));
    int16_t baseY = CONTENT_Y + 24;
    tft.setTextSize(2); tft.setTextColor(EDIT_VALUE_COLOR);
    tft.setCursor(8, baseY + 10);
    tft.print(cfg.snapThreshold / 10u); tft.print("."); tft.print(cfg.snapThreshold % 10u); tft.print(str(STR_PERCENT));
    int16_t ax = 100;
    drawDoubleArrowUp(ax, baseY, (ui.menuScrollPos == 4) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowUp(ax + 18, baseY, (ui.menuScrollPos == 5) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowDown(ax + 18, baseY + 22, (ui.menuScrollPos == 6) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawDoubleArrowDown(ax, baseY + 22, (ui.menuScrollPos == 7) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
}

void displayDrawButtonDebounce(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_DEBOUNCE));
    int16_t baseY = CONTENT_Y + 24;
    tft.setTextSize(2); tft.setTextColor(EDIT_VALUE_COLOR);
    tft.setCursor(8, baseY + 10);
    tft.print(cfg.debounceMs); tft.print(" "); tft.print(str(STR_LABEL_MS));
    drawSmallArrowUp(108, baseY, (ui.menuScrollPos == 4) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowDown(108, baseY + 22, (ui.menuScrollPos == 5) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
}

void displayDrawRefreshRates(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_REFRESH));
    int16_t baseY = CONTENT_Y + 16, ax = 100;
    tft.setTextSize(1); tft.setTextColor(EDIT_LABEL_COLOR);
    tft.setCursor(8, baseY); tft.print(str(STR_LABEL_USB_ADC));
    tft.setTextSize(2); tft.setCursor(8, baseY + 12); tft.setTextColor(EDIT_VALUE_COLOR);
    tft.print(cfg.sampleRateHz); tft.print(str(STR_LABEL_HZ));
    drawSmallArrowUp(ax, baseY + 2, (ui.menuScrollPos == 4) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowDown(ax, baseY + 18, (ui.menuScrollPos == 5) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    int16_t dY = baseY + 44;
    tft.setTextSize(1); tft.setTextColor(EDIT_LABEL_COLOR);
    tft.setCursor(8, dY); tft.print(str(STR_LABEL_DISPLAY));
    tft.setTextSize(2); tft.setCursor(8, dY + 12); tft.setTextColor(EDIT_VALUE_COLOR);
    tft.print(cfg.displayRateHz); tft.print(str(STR_LABEL_HZ));
    drawSmallArrowUp(ax, dY + 2, (ui.menuScrollPos == 6) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
    drawSmallArrowDown(ax, dY + 18, (ui.menuScrollPos == 7) ? NAV_SELECTED_FG : NAV_NORMAL_FG);
}

// ============================================================================
//  Edit Screen: Calibration
// ============================================================================
void displayDrawCalibrate(const UIState& ui, const CalibData& calib,
                          uint8_t language) {
    activeLang = language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_CALIBRATE));
    int16_t baseY = CONTENT_Y + 16;

    switch (calib.state) {
        case CALIB_IDLE:
        case CALIB_PROMPT_ZERO:
            tft.setTextSize(1);
            tft.setTextColor((calib.state == CALIB_IDLE) ? EDIT_LABEL_COLOR : LIVE_WARN_COLOR);
            tft.setCursor(8, baseY + 10); tft.print(str(STR_CAL_PUSH_DOWN));
            tft.setCursor(8, baseY + 26); tft.print(str(STR_CAL_HOLD_STEADY));
            break;

        case CALIB_SETTLING_ZERO:
        case CALIB_SETTLING_MAX:
            tft.setTextSize(1);
            tft.setTextColor(LIVE_VALUE_COLOR);
            tft.setCursor(8, baseY + 10); tft.print(str(STR_CAL_SETTLING));
            tft.setCursor(8, baseY + 26); tft.print(str(STR_CAL_HOLD_STEADY));
            // Show stability progress as count of consecutive stable samples
            tft.setTextColor(EDIT_LABEL_COLOR);
            tft.setCursor(8, baseY + 48);
            tft.print(calib.settleStableCount);
            tft.print("/");
            tft.print(CALIB_STABILITY_COUNT);
            break;

        case CALIB_SAMPLING_ZERO:
        case CALIB_SAMPLING_MAX: {
            tft.setTextSize(1);
            tft.setTextColor(LIVE_VALUE_COLOR);
            tft.setCursor(8, baseY + 10); tft.print(str(STR_CAL_SAMPLING));
            int16_t barW = (int16_t)((uint32_t)calib.sampleCount * 100UL / CALIB_SAMPLE_COUNT);
            tft.fillRect(8, baseY + 30, barW, 10, LIVE_VALUE_COLOR);
            tft.fillRect(8 + barW, baseY + 30, 100 - barW, 10, NAV_NORMAL_BG);
            tft.setTextColor(EDIT_LABEL_COLOR);
            tft.setCursor(8, baseY + 48);
            tft.print(calib.sampleCount); tft.print("/"); tft.print(CALIB_SAMPLE_COUNT);
            break;
        }

        case CALIB_RESULT_ZERO:
            tft.setTextSize(1); tft.setTextColor(LIVE_OK_COLOR);
            tft.setCursor(8, baseY + 10); tft.print(str(STR_CAL_ZERO_OK));
            printCentiValue(8, baseY + 24, calib.resultCentiVolts, EDIT_VALUE_COLOR, 2);
            tft.setTextSize(1); tft.setCursor(80, baseY + 28); tft.print(str(STR_VOLTS));
            tft.setTextColor(EDIT_LABEL_COLOR);
            tft.setCursor(8, baseY + 50); tft.print(str(STR_CAL_RETRY));
            break;

        case CALIB_PROMPT_MAX:
            tft.setTextSize(1); tft.setTextColor(LIVE_WARN_COLOR);
            tft.setCursor(8, baseY + 10); tft.print(str(STR_CAL_PULL_UP));
            tft.setCursor(8, baseY + 26); tft.print(str(STR_CAL_HOLD_STEADY));
            break;

        case CALIB_RESULT_MAX:
            tft.setTextSize(1); tft.setTextColor(LIVE_OK_COLOR);
            tft.setCursor(8, baseY + 10); tft.print(str(STR_CAL_MAX_OK));
            printCentiValue(8, baseY + 24, calib.resultCentiVolts, EDIT_VALUE_COLOR, 2);
            tft.setTextSize(1); tft.setCursor(80, baseY + 28); tft.print(str(STR_VOLTS));
            tft.setTextColor(EDIT_LABEL_COLOR);
            tft.setCursor(8, baseY + 50); tft.print(str(STR_CAL_RETRY));
            break;

        case CALIB_DONE:
            tft.setTextSize(1); tft.setTextColor(LIVE_OK_COLOR);
            tft.setCursor(8, baseY + 10); tft.print(str(STR_CAL_DONE));
            tft.setTextColor(EDIT_LABEL_COLOR);
            tft.setCursor(8, baseY + 28);
            tft.print(str(STR_CAL_ZERO_OK)); tft.print(" ADC "); tft.print(calib.resultAdcZero);
            tft.setCursor(8, baseY + 42);
            tft.print(str(STR_CAL_MAX_OK)); tft.print(" ADC "); tft.print(calib.resultAdcMax);
            break;

        case CALIB_ERROR:
            tft.setTextSize(2); tft.setTextColor(LIVE_ERROR_COLOR);
            tft.setCursor(8, baseY + 10); tft.print(str(STR_CAL_ERROR));
            tft.setTextSize(1); tft.setTextColor(LIVE_WARN_COLOR);
            tft.setCursor(8, baseY + 34);
            if (calib.errorMsg) tft.print(calib.errorMsg);
            tft.setTextColor(EDIT_LABEL_COLOR);
            tft.setCursor(8, baseY + 52); tft.print(str(STR_CAL_RETRY));
            break;
    }
}

// ============================================================================
//  Edit Screen: Save & Load
// ============================================================================
void displayDrawSaveLoad(const UIState& ui, uint8_t selectedSlot,
                         const bool slotExists[NUM_NVS_PROFILES],
                         uint8_t language) {
    activeLang = language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_SAVE_LOAD));
    int16_t baseY = CONTENT_Y + 16;
    for (uint8_t i = 0; i < NUM_NVS_PROFILES; i++) {
        int16_t slotY = baseY + i * 18;
        bool isSel = (ui.menuScrollPos == (4 + i));
        drawButtonHighlight(4, slotY, 120, 16, isSel);
        tft.setTextSize(1);
        tft.setTextColor(isSel ? NAV_SELECTED_FG : EDIT_LABEL_COLOR);
        tft.setCursor(8, slotY + 4);
        tft.print(str(STR_PROFILE)); tft.print(" "); tft.print(i + 1); tft.print(": ");
        if (slotExists[i]) {
            tft.setTextColor(isSel ? NAV_SELECTED_FG : LIVE_OK_COLOR);
            tft.print(str(STR_SAVED));
        } else {
            tft.setTextColor(isSel ? NAV_SELECTED_FG : NAV_NORMAL_FG);
            tft.print(str(STR_EMPTY));
        }
    }
    int16_t actY = baseY + NUM_NVS_PROFILES * 18 + 4;
    uint8_t saveBtn = 4 + NUM_NVS_PROFILES, loadBtn = saveBtn + 1;
    drawButtonHighlight(4, actY, 56, 16, ui.menuScrollPos == saveBtn);
    tft.setTextSize(1);
    tft.setTextColor((ui.menuScrollPos == saveBtn) ? NAV_SELECTED_FG : EDIT_LABEL_COLOR);
    tft.setCursor(12, actY + 4); tft.print(str(STR_SAVE));
    drawButtonHighlight(66, actY, 56, 16, ui.menuScrollPos == loadBtn);
    tft.setTextColor((ui.menuScrollPos == loadBtn) ? NAV_SELECTED_FG : EDIT_LABEL_COLOR);
    tft.setCursor(76, actY + 4); tft.print(str(STR_LOAD));
}

// ============================================================================
//  Edit Screen: Language
// ============================================================================
void displayDrawLanguage(const UIState& ui, uint8_t currentLanguage) {
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_LANGUAGE));
    int16_t baseY = CONTENT_Y + 20;
    for (uint8_t i = 0; i < NUM_LANGUAGES; i++) {
        int16_t slotY = baseY + i * 22;
        bool isSel = (ui.menuScrollPos == (4 + i));
        bool isCur = (i == currentLanguage);
        drawButtonHighlight(4, slotY, 120, 20, isSel);
        tft.setTextSize(2);
        tft.setTextColor(isCur ? EDIT_VALUE_COLOR : NAV_NORMAL_FG);
        tft.setCursor(8, slotY + 2); tft.print(LANG_NAMES[i]);
        if (isCur) { tft.setCursor(108, slotY + 2); tft.print("*"); }
    }
}

// ============================================================================
//  Edit Screen Dispatcher
// ============================================================================
void displayDrawEditScreen(const UIState& ui, const DeviceConfig& cfg,
                           const LiveData& data) {
    activeLang = cfg.language;
    switch (ui.editScreenIndex) {
        case 0: displayDrawHoldMode(ui, cfg);        break;
        case 1: displayDrawDeadzones(ui, cfg);        break;
        case 2: displayDrawDefaultCurve(ui, cfg);     break;
        case 3: displayDrawSnapThreshold(ui, cfg);    break;
        case 4: displayDrawButtonDebounce(ui, cfg);   break;
        case 5: displayDrawRefreshRates(ui, cfg);     break;
        default: break;
    }
}
