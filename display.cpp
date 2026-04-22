/*  BalduHandrake
	Open Source Hydraulic Simracing Handbrake
	Copyright (c) 2026 Alejandro Belluscio
	Additional copyright holders listed inline below.
	This file is licensed under the Apache 2.0 license
	Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// display.cpp — OLED Display Implementation
// ============================================================================

#include "display.h"
#include "storage.h"
#include "assets.h"
#include "strtable.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ============================================================================
//  LGFX custom class — exact physical wiring
// ============================================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus_instance;
  lgfx::Panel_SSD1351 _panel_instance;

public:
  LGFX(void) {
    Serial.println("[LGFX] Constructor started");

    auto bus_cfg = _bus_instance.config();
    bus_cfg.spi_host = SPI2_HOST;
    bus_cfg.spi_mode = 0;
    bus_cfg.freq_write = 24000000;
    bus_cfg.freq_read  = 16000000;
    bus_cfg.spi_3wire  = false;
    bus_cfg.use_lock   = true;
    bus_cfg.dma_channel = SPI_DMA_CH_AUTO;

    bus_cfg.pin_mosi = OLED_MOSI;   // DIN
    bus_cfg.pin_miso = OLED_MISO;
    bus_cfg.pin_sclk = OLED_SCK;   // CLK
    bus_cfg.pin_dc   = OLED_DC;    // DC
    _bus_instance.config(bus_cfg);

    _panel_instance.setBus(&_bus_instance);

    auto panel_cfg = _panel_instance.config();
    panel_cfg.pin_cs     = OLED_CS;
    panel_cfg.pin_rst    = OLED_RST;
    panel_cfg.pin_busy   = -1;
    panel_cfg.offset_x   = 0;
    panel_cfg.offset_y   = 0;
    panel_cfg.offset_rotation = 0;
    panel_cfg.dlen_16bit = false;
    panel_cfg.bus_shared = false;
    panel_cfg.invert     = false;
    _panel_instance.config(panel_cfg);

    setPanel(&_panel_instance);
    Serial.println("[LGFX] Constructor finished");
  }
};

static LGFX lcd;    // global display object

static uint8_t activeLang = LANG_EN;

// Display hysteresis trackers
static uint16_t lastDispdeciUnit     = 0xFFFF;
static uint16_t lastDispDeciVolts   = 0xFFFF;
static uint16_t lastDispDeciPercent = 0xFFFF;
static bool     lastDispHold        = false;
static bool     lastDispHoldInit    = false;
static uint8_t  lastDispErrorState  = 0xFF;
static uint8_t  lastDispCurveIndex  = 0xFF;

//Used for the Up Down arrows helper functions
struct IconPair { const AssetDesc* normal; const AssetDesc* selected; };

// ============================================================================
//  Layout constants
// ============================================================================
static const int16_t CONTENT_Y = NAV_BOX_H + NAV_SEPARATOR_PX;
static const int16_t CONTENT_H = SCREEN_HEIGHT - CONTENT_Y;

// Arc gauge
static const int16_t ARC_CX      = SCREEN_WIDTH / 2;
static const int16_t ARC_CY      = 72;              // Shifted up to leave room below
static const int16_t ARC_OUTER_R = 50;
static const int16_t ARC_INNER_R = 38;
static const float   ARC_START   = 135.0f;           // Start angle (degrees)
static const float   ARC_SWEEP   = 270.0f;           // Total sweep
static const float   ARC_END     = ARC_START + ARC_SWEEP;  // End angle

static uint16_t prevArcPercent = 0xFFFF;

// ============================================================================
//  String lookup
// ============================================================================
static const char* str(StringID id) {
    return strGet(id, activeLang);
}

// ============================================================================
//  Helpers
// ============================================================================
static void resetDisplayHysteresis() {
    lastDispdeciUnit     = 0xFFFF;
    lastDispDeciVolts   = 0xFFFF;
    lastDispDeciPercent = 0xFFFF;
    lastDispHold        = false;
    lastDispHoldInit    = false;
    lastDispErrorState  = 0xFF;
    lastDispCurveIndex  = 0xFF;
}

static uint8_t getErrorState(const LiveData& data) {
    return (data.transducerFail ? 0x01 : 0x00)
         | (data.pressureLow   ? 0x02 : 0x00)
         | (data.saturationFail? 0x04 : 0x00);
}

static void printDeciValue(int16_t x, int16_t y, uint16_t deciVal,
                           uint16_t fg_color,  uint16_t bg_color, const lgfx::IFont* font = &fonts::DejaVu12) {
    lcd.setFont(font);
    lcd.setTextColor(fg_color, bg_color);
    lcd.setCursor(x, y);
    lcd.print(deciVal / 10u);
    lcd.print(".");
    lcd.print(deciVal % 10u);
}

static void printSensorStatus(int16_t x, int16_t y, const LiveData& data,
                               uint16_t bg_color, const lgfx::IFont* font = &fonts::DejaVu12) {
    lcd.setFont(font);
    lcd.setCursor(x, y);
    if (data.transducerFail) {
        lcd.setTextColor(LIVE_ERROR_COLOR, bg_color); lcd.print(str(STR_FAIL));
    } else if (data.pressureLow) {
        lcd.setTextColor(LIVE_WARN_COLOR, bg_color); lcd.print(str(STR_LOW));
    } else if (data.saturationFail) {
        lcd.setTextColor(LIVE_ERROR_COLOR, bg_color); lcd.print(str(STR_OVER));
    }
}

// --- Hold indicator ---
static void drawHoldIndicator(int16_t x, int16_t y, bool holdActive) {
	if (holdActive) {
        // Use the 64x20 pre-rendered logo (centered in the old 56x16 area)
        lcd.pushImage(x, y, 60, 20, (const lgfx::rgb565_t*)icon_live_hold.data);
    } else {
		//Just blank it, we should move to just push the HOLD icon.
        lcd.fillRect(x, y, 60, 20, LIVE_BG_COLOR);
    }
}

static void updateHoldIfChanged(int16_t x, int16_t y, bool holdActive) {
    if (!lastDispHoldInit || holdActive != lastDispHold) {
        drawHoldIndicator(x, y, holdActive);
        lastDispHold = holdActive;
        lastDispHoldInit = true;
    }
}

// --- Curve name ---
static void drawCurveName(int16_t x, int16_t y, uint8_t curveIndex,
                          uint16_t fg_color,  uint16_t bg_color, const lgfx::IFont* font = &fonts::DejaVu18) {
    lcd.setFont(font);
    lcd.setTextColor(fg_color, bg_color);
    lcd.setCursor(x, y);
    if (curveIndex < NUM_CURVES) lcd.print(CURVE_NAMES[curveIndex]);
}

static void updateCurveIfChanged(int16_t x, int16_t y, uint8_t curveIndex,
                                  int16_t clearW) {
    if (curveIndex != lastDispCurveIndex) {
        lcd.fillRect(x, y, clearW, 18, LIVE_BG_COLOR);
        drawCurveName(x, y, curveIndex, LIVE_FG_COLOR, LIVE_BG_COLOR);
        lastDispCurveIndex = curveIndex;
    }
}

// --- Edit screen arrows ---
static void drawSmallArrowUp(int16_t cx, int16_t y, uint16_t color) {
    lcd.fillTriangle(cx-4, y+8, cx+4, y+8, cx, y, color);
}
static void drawSmallArrowDown(int16_t cx, int16_t y, uint16_t color) {
    lcd.fillTriangle(cx-4, y, cx+4, y, cx, y+8, color);
}
static void drawDoubleArrowUp(int16_t cx, int16_t y, uint16_t color) {
    lcd.fillTriangle(cx-4, y+8, cx+4, y+8, cx, y+2, color);
    lcd.fillTriangle(cx-4, y+12, cx+4, y+12, cx, y+6, color);
}
static void drawDoubleArrowDown(int16_t cx, int16_t y, uint16_t color) {
    lcd.fillTriangle(cx-4, y, cx+4, y, cx, y+6, color);
    lcd.fillTriangle(cx-4, y+4, cx+4, y+4, cx, y+10, color);
}
static void drawButtonHighlight(int16_t x, int16_t y, int16_t w, int16_t h,
                                bool isSelected) {
    lcd.fillRect(x, y, w, h, isSelected ? NAV_SELECTED_BG : EDIT_BG_COLOR);
}

// --- Draws selectable text for menus ---
//Draws a selectable row, but allows for a differentiated color while displayed.
//Used for now on profiles, but might be useful for other uses in the future.
static void drawSelectableRowStatus(int16_t x, int16_t y, int16_t w, int16_t h,
                                    const char* label,
                                    const char* status, uint16_t statusFg,
                                    bool isSelected,
                                    const lgfx::IFont* font = &fonts::DejaVu9) {
    drawButtonHighlight(x, y, w, h, isSelected);
    uint16_t bg = isSelected ? NAV_SELECTED_BG : EDIT_BG_COLOR;
    uint16_t labelFg = isSelected ? NAV_SELECTED_FG : EDIT_LABEL_COLOR;

    lcd.setFont(font);
    lcd.setTextColor(labelFg, bg);
    lcd.setCursor(x + 4, y + 4);
    lcd.print(label);

    // Status: when selected, force white-on-highlight for legibility;
    // when not selected, use the semantic status color.
    lcd.setTextColor(isSelected ? NAV_SELECTED_FG : statusFg, bg);
    lcd.print(status);
}

// Draw a selectable row: highlight bar + label on the left, optional status marker on the right.
// Used for things like language or Hold mode.
// If isActive is true, the row uses the "active value" foreground color and shows "*".
static void drawSelectableRow(int16_t x, int16_t y, int16_t w, int16_t h,
                              const char* label,
                              bool isSelected, bool isActive,
                              const lgfx::IFont* font = &fonts::DejaVu18) {
    drawButtonHighlight(x, y, w, h, isSelected);

    uint16_t bg = isSelected ? NAV_SELECTED_BG : EDIT_BG_COLOR;
    uint16_t fg;
    if (isSelected)      fg = NAV_SELECTED_FG;
    else if (isActive)   fg = EDIT_VALUE_COLOR;   // cyan — "this is the current value"
    else                 fg = NAV_NORMAL_FG;      // dim — "other option"

    lcd.setFont(font);
    lcd.setTextColor(fg, bg);
    lcd.setCursor(x + 4, y + 3);
    lcd.print(label);

    if (isActive) {
        lcd.setCursor(x + w - 16, y + 3);
        lcd.print("*");
    }
}



// Standard 2x2 increment icon block: top row is up2/up1, bottom row is dn1/dn2.
// Scroll positions are assigned in increment-magnitude order:
//   baseScrollPos+0 = +big, +1 = +small, +2 = -small, +3 = -big
// which matches handleAdjustment()'s bi 4/5/6/7 (and 8/9/10/11) ordering.
static const IconPair EDIT_ICONS_4[] = {
    { &icon_edit_up2_normal,   &icon_edit_up2_selected   },  // +big   (top-left)
    { &icon_edit_up1_normal,   &icon_edit_up1_selected   },  // +small (top-right)
    { &icon_edit_down1_normal, &icon_edit_down1_selected },  // -small (bottom-left)
    { &icon_edit_down2_normal, &icon_edit_down2_selected },  // -big   (bottom-right)
};

// Single-increment 1x2 set (just +1/-1 stacked).
static const IconPair EDIT_ICONS_2[] = {
    { &icon_edit_up1_normal,   &icon_edit_up1_selected   },
    { &icon_edit_down1_normal, &icon_edit_down1_selected },
};

// Draw a 2x2 block of edit icons. (x, y) is the top-left corner of the block.
// Block size: 32w x 40h (no gaps) or 33w x 41h (gap=1).
static void drawEditIconBlock2x2(int16_t x, int16_t y,
                                 const IconPair icons[4],
                                 uint8_t baseScrollPos, uint8_t currentScrollPos,
                                 int16_t gap = 0) {
    const int16_t ICON_W = 16, ICON_H = 20;
    for (uint8_t i = 0; i < 4; i++) {
        int16_t col = i & 1;        // 0 or 1
        int16_t row = i >> 1;       // 0 or 1
        int16_t ix = x + col * (ICON_W + gap);
        int16_t iy = y + row * (ICON_H + gap);
        bool sel = (currentScrollPos == baseScrollPos + i);
        const AssetDesc* a = sel ? icons[i].selected : icons[i].normal;
        lcd.pushImage(ix, iy, ICON_W, ICON_H, (const lgfx::rgb565_t*)a->data);
    }
}

// Draw a vertical 1x2 column of edit icons (just up/down, single increment).
// Block size: 16w x 40h.
static void drawEditIconCol1x2(int16_t x, int16_t y,
                               const IconPair icons[2],
                               uint8_t baseScrollPos, uint8_t currentScrollPos,
                               int16_t gap = 0) {
    const int16_t ICON_W = 16, ICON_H = 20;
    for (uint8_t i = 0; i < 2; i++) {
        int16_t iy = y + i * (ICON_H + gap);
        bool sel = (currentScrollPos == baseScrollPos + i);
        const AssetDesc* a = sel ? icons[i].selected : icons[i].normal;
        lcd.pushImage(x, iy, ICON_W, ICON_H, (const lgfx::rgb565_t*)a->data);
    }
}

// ============================================================================
//  displayInit()
// ============================================================================
void displayInit() {
    Serial.println("[SSD1351] Starting...");
    bool initOK = lcd.init();
    Serial.print("displayInit returned: ");
    Serial.println(initOK ? "SUCCESS" : "FAILED");
    lcd.setRotation(4);
    lcd.setBrightness(255);
    lcd.setColorDepth(16);
    lcd.invertDisplay(false);
    lcd.fillScreen(LIVE_BG_COLOR);

    lcd.setFont(&fonts::DejaVu12);

    delay(200);

}

// ============================================================================
//  displayBootScreen()
// ============================================================================
void displayBootScreen(uint8_t language) {
    activeLang = language;
    lcd.fillScreen(LIVE_BG_COLOR);

    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(LIVE_FG_COLOR, LIVE_BG_COLOR);
    lcd.setCursor(8, 24);
	lcd.print(str(STR_BOOT_TITLE));

    lcd.setFont(&fonts::DejaVu12);
    lcd.setTextColor(LIVE_VALUE_COLOR, LIVE_BG_COLOR);
    lcd.setCursor(8, 52);
	lcd.print(str(STR_BOOT_PROJECT));

    lcd.setTextColor(LIVE_LABEL_COLOR, LIVE_BG_COLOR);
    lcd.setCursor(8, 66);
	lcd.print(str(STR_BOOT_VERSION));
    lcd.setCursor(8, 80);
	lcd.print(str(STR_BOOT_AUTHOR));
    lcd.setTextColor(LIVE_OK_COLOR, LIVE_BG_COLOR);
    lcd.setCursor(8, 100);
	lcd.print(str(STR_BOOT_STATUS));

    delay(2000);
}

// ============================================================================
//  LIVE_FULL_DATA
// ============================================================================
void displaySetupLiveFull(const DeviceConfig& cfg) {
    activeLang = cfg.language;
    resetDisplayHysteresis();
    lcd.fillScreen(LIVE_BG_COLOR);
    drawCurveName(12, 2, cfg.curveIndex, LIVE_FG_COLOR, LIVE_BG_COLOR);
    lastDispCurveIndex = cfg.curveIndex;
    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(LIVE_LABEL_COLOR, LIVE_BG_COLOR);
    lcd.setCursor(90, 26); lcd.print(str(STR_UNIT));
    lcd.setCursor(90, 48); lcd.print(str(STR_VOLTS));
    lcd.setCursor(90, 72); lcd.print(str(STR_PERCENT));
    lcd.drawRect(4, 116, 122, 12, LIVE_FG_COLOR);
}

void displayUpdateLiveFull(const LiveData& data, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    uint8_t errState = getErrorState(data);

    // Curve name (updates on change)
    updateCurveIfChanged(12, 1, cfg.curveIndex, 120);

    // Unit, Psi/Kgf/etc.
    uint16_t deciUnit = (uint16_t)(data.centiUnit / 10u);
    if (deciUnit != lastDispdeciUnit || errState != lastDispErrorState) {
        lcd.fillRect(12, 26, 70, 16, LIVE_BG_COLOR);
        if (errState) printSensorStatus(12, 26, data, LIVE_BG_COLOR, &fonts::DejaVu18);
        else printDeciValue(12, 26, deciUnit, LIVE_OK_COLOR, LIVE_BG_COLOR, &fonts::DejaVu18);
        lastDispdeciUnit = deciUnit;
    }

    // Voltage
    uint16_t deciVolts = (uint16_t)(data.centiVolts / 10u);
    if (deciVolts != lastDispDeciVolts) {
        lcd.fillRect(12, 48, 70, 16, LIVE_BG_COLOR);
        printDeciValue(12, 48, deciVolts, LIVE_ERROR_COLOR, LIVE_BG_COLOR, &fonts::DejaVu18);
        lastDispDeciVolts = deciVolts;
    }

    // Percentage
    uint16_t deciPercent = (uint16_t)(data.centiPercent / 10u);
    if (deciPercent != lastDispDeciPercent || errState != lastDispErrorState) {
        lcd.fillRect(12, 72, 70, 16, LIVE_BG_COLOR);
        if (!errState) printDeciValue(12, 72, deciPercent, LIVE_VALUE_COLOR, LIVE_BG_COLOR, &fonts::DejaVu18);
        lastDispDeciPercent = deciPercent;
        int barWidth = (int)((uint32_t)data.centiPercent * 120UL / 10000UL);
        lcd.fillRect(5, 117, barWidth, 10, LIVE_VALUE_COLOR);
        lcd.fillRect(5 + barWidth, 117, 120 - barWidth, 10, LIVE_BG_COLOR);
    }

    lastDispErrorState = errState;
    updateHoldIfChanged(36, 92, data.holdActive);
}

// ============================================================================
//  LIVE_CLEAN
// ============================================================================
void displaySetupLiveClean(const DeviceConfig& cfg) {
    activeLang = cfg.language;
    resetDisplayHysteresis();
    lcd.fillScreen(LIVE_BG_COLOR);
    // Curve name at top
    drawCurveName(12, 2, cfg.curveIndex, LIVE_FG_COLOR, LIVE_BG_COLOR);
    lastDispCurveIndex = cfg.curveIndex;
}

void displayUpdateLiveClean(const LiveData& data, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    uint8_t errState = getErrorState(data);
    uint16_t deciPercent = (uint16_t)(data.centiPercent / 10u);

    // Curve name
    updateCurveIfChanged(12, 2, cfg.curveIndex, 120);

    if (deciPercent != lastDispDeciPercent || errState != lastDispErrorState) {
        // Large centered percentage
        lcd.fillRect(0, 52, SCREEN_WIDTH, 40, LIVE_BG_COLOR);
        if (errState) {
            printSensorStatus(4, 52, data, LIVE_BG_COLOR, &fonts::DejaVu40);
        } else {

            lcd.setFont(&fonts::DejaVu40);
            lcd.setTextColor(LIVE_VALUE_COLOR, LIVE_BG_COLOR);
            int16_t txtwidth;
            // "XX.X%" — estimate width: ~5 chars × 18px = 90px
            if (deciPercent<= 99) {
                txtwidth = 2 * 22 +28;
            } else if (deciPercent<= 999) {
                txtwidth = 3 * 22 +28;
            } else {
                txtwidth = 4 * 22 +28;
            }
            int16_t tx = (SCREEN_WIDTH - txtwidth) / 2;
            if (tx < 4) tx = 2;
           
            lcd.setCursor(tx, 52);
            lcd.print((deciPercent) / 10u);
            lcd.print(str(STR_PERCENT));
        }
        lastDispDeciPercent = deciPercent;
        lastDispErrorState = errState;
    }

    updateHoldIfChanged(36, 104, data.holdActive);
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
        lcd.fillTriangle(ix1, iy1, ox1, oy1, ox2, oy2, color);
        lcd.fillTriangle(ix1, iy1, ix2, iy2, ox2, oy2, color);
    }
}

// Draw arc-shaped borders along the inner and outer edges of the gauge,
// plus end cap lines connecting inner to outer at the start and end angles.
static void drawArcBorders() {
    uint16_t bc = NAV_NORMAL_FG;
    //This two lines take the average of the two colors in int math. They act as a pseudoalias.
    uint16_t s = NAV_NORMAL_FG ^ LIVE_VALUE_COLOR;
    uint16_t smoothbc = ((s & 0xF7DEU) >> 1) + (NAV_NORMAL_FG & LIVE_VALUE_COLOR) + (s & 0x0821U);

    // Inner and outer border arcs — thin (1-step) arc segments
    const float step = 3.0f;
    for (float angle = ARC_START; angle < ARC_END; angle += step) {
        float a1 = angle * DEG_TO_RAD;
        float a2 = ((angle + step > ARC_END) ? ARC_END : angle + step) * DEG_TO_RAD;

        // Outer border line
        int16_t ox1 = ARC_CX + (int16_t)((ARC_OUTER_R + 1) * cos(a1));
        int16_t oy1 = ARC_CY + (int16_t)((ARC_OUTER_R + 1) * sin(a1));
        int16_t ox2 = ARC_CX + (int16_t)((ARC_OUTER_R + 1) * cos(a2));
        int16_t oy2 = ARC_CY + (int16_t)((ARC_OUTER_R + 1) * sin(a2));
        lcd.drawLine(ox1, oy1, ox2, oy2, bc);

        // Outer border inline fill
        ox1 = ARC_CX + (int16_t)((ARC_OUTER_R + 0.5) * cos(a1));
        oy1 = ARC_CY + (int16_t)((ARC_OUTER_R + 0.5) * sin(a1));
        ox2 = ARC_CX + (int16_t)((ARC_OUTER_R + 0.5) * cos(a2));
        oy2 = ARC_CY + (int16_t)((ARC_OUTER_R + 0.5) * sin(a2));
        lcd.drawLine(ox1, oy1, ox2, oy2, smoothbc);

        // Inner border line
        int16_t ix1 = ARC_CX + (int16_t)((ARC_INNER_R - 1) * cos(a1));
        int16_t iy1 = ARC_CY + (int16_t)((ARC_INNER_R - 1) * sin(a1));
        int16_t ix2 = ARC_CX + (int16_t)((ARC_INNER_R - 1) * cos(a2));
        int16_t iy2 = ARC_CY + (int16_t)((ARC_INNER_R - 1) * sin(a2));
        lcd.drawLine(ix1, iy1, ix2, iy2, bc);

        // Inner border line
        ix1 = ARC_CX + (int16_t)((ARC_INNER_R - 0.5) * cos(a1));
        iy1 = ARC_CY + (int16_t)((ARC_INNER_R - 0.5) * sin(a1));
        ix2 = ARC_CX + (int16_t)((ARC_INNER_R - 0.5) * cos(a2));
        iy2 = ARC_CY + (int16_t)((ARC_INNER_R - 0.5) * sin(a2));
        lcd.drawLine(ix1, iy1, ix2, iy2, smoothbc);
    }

    // End caps — lines from inner to outer at start and end angles
    float sa = ARC_START * DEG_TO_RAD;
    float ea = ARC_END * DEG_TO_RAD;

    int16_t si_x = ARC_CX + (int16_t)((ARC_INNER_R - 1) * cos(sa));
    int16_t si_y = ARC_CY + (int16_t)((ARC_INNER_R - 1) * sin(sa));
    int16_t so_x = ARC_CX + (int16_t)((ARC_OUTER_R + 1) * cos(sa));
    int16_t so_y = ARC_CY + (int16_t)((ARC_OUTER_R + 1) * sin(sa));
    lcd.drawLine(si_x, si_y, so_x, so_y, bc);

    int16_t ei_x = ARC_CX + (int16_t)((ARC_INNER_R - 1) * cos(ea));
    int16_t ei_y = ARC_CY + (int16_t)((ARC_INNER_R - 1) * sin(ea));
    int16_t eo_x = ARC_CX + (int16_t)((ARC_OUTER_R + 1) * cos(ea));
    int16_t eo_y = ARC_CY + (int16_t)((ARC_OUTER_R + 1) * sin(ea));
    lcd.drawLine(ei_x, ei_y, eo_x, eo_y, bc);
}

void displaySetupLiveBar() {
    resetDisplayHysteresis();
    lcd.fillScreen(LIVE_BG_COLOR);
    prevArcPercent = 0xFFFF;

    // Empty gauge track
    drawArcSegment(ARC_START, ARC_END,
                   ARC_INNER_R, ARC_OUTER_R, ARC_CX, ARC_CY, NAV_NORMAL_BG);
    drawArcBorders();
}

void displayUpdateLiveBar(const LiveData& data, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    uint16_t pct = data.centiPercent;

    // Curve name at top
    updateCurveIfChanged(16, 2, cfg.curveIndex, 120);

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
        drawArcBorders();
        prevArcPercent = pct;
    }

    // Center percentage text
    lcd.fillRect(ARC_CX - 30, ARC_CY - 8, 60, 20, LIVE_BG_COLOR);
    uint8_t errState = getErrorState(data);
    if (errState) {
        printSensorStatus(ARC_CX - 16, ARC_CY - 4, data, LIVE_BG_COLOR);
    } else {
        lcd.setFont(&fonts::DejaVu18);
        lcd.setTextColor(LIVE_FG_COLOR, LIVE_BG_COLOR);
        uint16_t intPct = pct / 100;
        int16_t tx = (intPct < 10) ? ARC_CX - 10 :
                     (intPct < 100) ? ARC_CX - 16 : ARC_CX - 24;
        lcd.setCursor(tx, ARC_CY - 8);
        lcd.print(intPct);
        lcd.print(str(STR_PERCENT));
    }

    // Hold at bottom, below the arc
    updateHoldIfChanged(36, 104, data.holdActive);
}

// ============================================================================
//  LIVE_DARK
// ============================================================================
void displaySetupLiveDark() {
    resetDisplayHysteresis();
    lcd.fillScreen(LIVE_BG_COLOR);
}

void displayShowCurveOverlay(uint8_t curveIndex) {
    // Clear overlay area and draw curve name — centered vertically
    lcd.fillRect(0, 44, SCREEN_WIDTH, 40, LIVE_BG_COLOR);
    drawCurveName(16, 52, curveIndex, LIVE_FG_COLOR, LIVE_BG_COLOR);
}

void displayUpdateLiveDark(const LiveData& data, uint8_t language) {
    activeLang = language;
    uint8_t errState = getErrorState(data);

    // Error overlay in upper area (won't conflict with curve overlay zone)
    if (errState != lastDispErrorState) {
        lcd.fillRect(0, 24, SCREEN_WIDTH, 24, LIVE_BG_COLOR);
        if (errState) printSensorStatus(24, 24, data, LIVE_BG_COLOR, &fonts::DejaVu24);
        lastDispErrorState = errState;
    }

// Hold indicator — only show when active, blank when not
    if (data.holdActive) {
        updateHoldIfChanged(36, 96, true);
    } else {
        if (lastDispHoldInit && lastDispHold) {
            // Was showing HOLD, now clear it
            lcd.fillRect(36, 96, 60, 20, LIVE_BG_COLOR);
        }
        lastDispHold = false;
        lastDispHoldInit = true;
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
    int16_t x = NAV_BOX_X[index], y = NAV_BOX_Y[index];
    const AssetDesc* icon = NULL;

    // Special case for edit mode S/X (keep the old text for now)
    if (ui.state == DISPLAY_EDIT_VALUE && (index == 2 || index == 3)) {
        switch (index) {
            case 2: icon = isSelected ? &icon_nav_x_selected  : &icon_nav_x_normal;  break;
            case 3: icon = isSelected ? &icon_nav_s_selected  : &icon_nav_s_normal;  break;
        }
        lcd.pushImage(x, y, 32, 20, (const lgfx::rgb565_t*)icon->data);
        return;
    }

    // Normal arrow icons
    switch (index) {
        case 0: icon = isSelected ? &icon_nav_left_selected  : &icon_nav_left_normal;  break;
        case 1: icon = isSelected ? &icon_nav_right_selected : &icon_nav_right_normal; break;
        case 2: icon = isSelected ? &icon_nav_up_selected    : &icon_nav_up_normal;    break;
        case 3: icon = isSelected ? &icon_nav_down_selected  : &icon_nav_down_normal;  break;
    }

    if (icon) {
        lcd.pushImage(x, y, 32, 20, (const lgfx::rgb565_t*)icon->data);
    }

}

void displayDrawNavBar(const UIState& ui) {
    if (ui.state == DISPLAY_LIVE) return;
    lcd.fillRect(0, NAV_BOX_Y[0], SCREEN_WIDTH, NAV_BOX_H, NAV_NORMAL_BG);
    lcd.fillRect(0, NAV_BOX_H, SCREEN_WIDTH, NAV_SEPARATOR_PX, EDIT_BG_COLOR);
    for (uint8_t i = 0; i < NAV_BOX_COUNT; i++)
        if (isNavBoxVisible(ui, i)) drawNavBox(ui, i, ui.menuScrollPos == i);
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
    lcd.fillRect(0, CONTENT_Y, SCREEN_WIDTH, CONTENT_H, EDIT_BG_COLOR);
}

void displayDrawEditTitle(const char* title) {
    lcd.setFont(&fonts::DejaVu9);
    lcd.setTextColor(EDIT_TITLE_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(4, CONTENT_Y + 4); lcd.print(title);
}

// ============================================================================
//  Edit Screens
// ============================================================================
void displayDrawHoldMode(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_HOLD_MODE));
    bool isFW = (cfg.holdMode == HOLD_FIRMWARE);
    int16_t baseY = CONTENT_Y + 24;

    drawSelectableRow(4, baseY,      120, 22, str(STR_GAME),
                    ui.menuScrollPos == 4, !isFW);
    drawSelectableRow(4, baseY + 30, 120, 22, str(STR_FIRMWARE),
                    ui.menuScrollPos == 5,  isFW);
}

void displayDrawDeadzones(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_DEADZONES));

    // 2x2 icon block: 32w x 40h. Place right-aligned with 2px margin.
    const int16_t ICONS_X = SCREEN_WIDTH - 32 - 16;   // x = 94

    // ---- LOW row ----
    int16_t baseY = CONTENT_Y + 20;
    lcd.setFont(&fonts::DejaVu9);
    lcd.setTextColor(EDIT_LABEL_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(8, baseY);
    lcd.print(str(STR_LABEL_LOW));

    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(EDIT_VALUE_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(8, baseY + 16);
    lcd.print(cfg.deadzoneLow / 10u); lcd.print(".");
    lcd.print(cfg.deadzoneLow % 10u); lcd.print(str(STR_PERCENT));

    // Icons aligned with the value text vertically (block is 40px tall,
    // text+label is ~30px, so center the block on the row visually)
    drawEditIconBlock2x2(ICONS_X, baseY-4, EDIT_ICONS_4, 4, ui.menuScrollPos);

    // ---- HIGH row ----
    int16_t hiY = baseY + 44;
    lcd.setFont(&fonts::DejaVu9);
    lcd.setTextColor(EDIT_LABEL_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(8, hiY);
    lcd.print(str(STR_LABEL_HIGH));

    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(EDIT_VALUE_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(8, hiY + 16);
    lcd.print(cfg.deadzoneHigh / 10u); lcd.print(".");
    lcd.print(cfg.deadzoneHigh % 10u); lcd.print(str(STR_PERCENT));

    drawEditIconBlock2x2(ICONS_X, hiY-4, EDIT_ICONS_4, 8, ui.menuScrollPos);
}

void displayDrawDefaultCurve(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_DEFAULT_CURVE));
    int16_t baseY = CONTENT_Y + 30;
    lcd.setFont(&fonts::DejaVu18); lcd.setTextColor(EDIT_VALUE_COLOR, EDIT_BG_COLOR); lcd.setCursor(8, baseY+10);
    if (cfg.curveIndex < NUM_CURVES) lcd.print(CURVE_NAMES[cfg.curveIndex]);
    drawEditIconCol1x2(SCREEN_WIDTH - 16 - 2, baseY, EDIT_ICONS_2, 4, ui.menuScrollPos);
}

void displayDrawSnapThreshold(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_SNAP_THRESH));
    int16_t baseY = CONTENT_Y + 24;
    lcd.setFont(&fonts::DejaVu24); lcd.setTextColor(EDIT_VALUE_COLOR, EDIT_BG_COLOR); lcd.setCursor(8, baseY+10);
    lcd.print(cfg.snapThreshold/10u); lcd.print("."); lcd.print(cfg.snapThreshold%10u); lcd.print(str(STR_PERCENT));
    drawEditIconBlock2x2(SCREEN_WIDTH - 32 - 2, baseY, EDIT_ICONS_4, 4, ui.menuScrollPos);
}

void displayDrawButtonDebounce(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_DEBOUNCE));
    int16_t baseY = CONTENT_Y + 24;
    lcd.setFont(&fonts::DejaVu24); lcd.setTextColor(EDIT_VALUE_COLOR, EDIT_BG_COLOR); lcd.setCursor(8, baseY+10);
    lcd.print(cfg.debounceMs); lcd.print(" "); lcd.print(str(STR_LABEL_MS));
    drawEditIconCol1x2(SCREEN_WIDTH - 16 - 8, baseY, EDIT_ICONS_2, 4, ui.menuScrollPos);
}

void displayDrawRefreshRates(const UIState& ui, const DeviceConfig& cfg) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_REFRESH));
    int16_t baseY = CONTENT_Y + 20, ICONS_X = 96; //ax = 100
    lcd.setFont(&fonts::DejaVu12); lcd.setTextColor(EDIT_LABEL_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(8, baseY); lcd.print(str(STR_LABEL_USB_ADC));
    lcd.setFont(&fonts::DejaVu18); lcd.setTextColor(EDIT_VALUE_COLOR, EDIT_BG_COLOR); lcd.setCursor(8, baseY+16);
    lcd.print(cfg.sampleRateHz); lcd.print(str(STR_LABEL_HZ));
    drawEditIconCol1x2(ICONS_X, baseY -4, EDIT_ICONS_2, 4, ui.menuScrollPos);  // USB/ADC
    int16_t dY = baseY + 44;
    lcd.setFont(&fonts::DejaVu12); lcd.setTextColor(EDIT_LABEL_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(8, dY); lcd.print(str(STR_LABEL_DISPLAY));
    lcd.setFont(&fonts::DejaVu18); lcd.setTextColor(EDIT_VALUE_COLOR, EDIT_BG_COLOR); lcd.setCursor(8, dY+16);
    lcd.print(cfg.displayRateHz); lcd.print(str(STR_LABEL_HZ));
    drawEditIconCol1x2(ICONS_X, dY -4,    EDIT_ICONS_2, 6, ui.menuScrollPos);  // Display
}

void displayDrawCalibrate(const UIState& ui, const CalibData& calib, uint8_t language) {
    activeLang = language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_CALIBRATE));
    int16_t baseY = CONTENT_Y + 16;
    switch (calib.state) {
        case CALIB_IDLE: case CALIB_PROMPT_ZERO: {
            uint16_t col = (calib.state==CALIB_IDLE) ? EDIT_LABEL_COLOR : LIVE_WARN_COLOR;
            lcd.setFont(&fonts::DejaVu9); lcd.setTextColor(col, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+10); lcd.print(str(STR_CAL_PUSH_DOWN));
            lcd.setCursor(8, baseY+26); lcd.print(str(STR_CAL_HOLD_STEADY));
            break; }
        case CALIB_SETTLING_ZERO: case CALIB_SETTLING_MAX:
            lcd.setFont(&fonts::DejaVu9); lcd.setTextColor(LIVE_VALUE_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+10); lcd.print(str(STR_CAL_SETTLING));
            lcd.setCursor(8, baseY+26); lcd.print(str(STR_CAL_HOLD_STEADY));
            lcd.setTextColor(EDIT_LABEL_COLOR, EDIT_BG_COLOR); lcd.setCursor(8, baseY+48);
            lcd.print(calib.settleStableCount); lcd.print("/"); lcd.print(CALIB_STABILITY_COUNT);
            break;
        case CALIB_SAMPLING_ZERO: case CALIB_SAMPLING_MAX: {
            lcd.setFont(&fonts::DejaVu9); lcd.setTextColor(LIVE_VALUE_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+10); lcd.print(str(STR_CAL_SAMPLING));
            int16_t barW = (int16_t)((uint32_t)calib.sampleCount * 100UL / CALIB_SAMPLE_COUNT);
            lcd.fillRect(8, baseY+30, barW, 10, LIVE_VALUE_COLOR);
            lcd.fillRect(8+barW, baseY+30, 100-barW, 10, NAV_NORMAL_BG);
            lcd.setTextColor(EDIT_LABEL_COLOR, EDIT_BG_COLOR); lcd.setCursor(8, baseY+48);
            lcd.print(calib.sampleCount); lcd.print("/"); lcd.print(CALIB_SAMPLE_COUNT);
            break; }
        case CALIB_RESULT_ZERO:
            lcd.setFont(&fonts::DejaVu9); lcd.setTextColor(LIVE_OK_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+10); lcd.print(str(STR_CAL_ZERO_OK));
            printDeciValue(8, baseY+24, calib.resultCentiVolts/10, EDIT_VALUE_COLOR, EDIT_BG_COLOR, &fonts::DejaVu18);
            lcd.setFont(&fonts::DejaVu9); lcd.setCursor(80, baseY+28); lcd.print(str(STR_VOLTS));
//            lcd.setTextColor(EDIT_LABEL_COLOR); lcd.setCursor(8, baseY+50); lcd.print(str(STR_CAL_RETRY));
            break;
        case CALIB_PROMPT_MAX:
            lcd.setFont(&fonts::DejaVu9); lcd.setTextColor(LIVE_WARN_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+10); lcd.print(str(STR_CAL_PULL_UP));
            lcd.setCursor(8, baseY+26); lcd.print(str(STR_CAL_HOLD_STEADY));
            break;
        case CALIB_RESULT_MAX:
            lcd.setFont(&fonts::DejaVu9); lcd.setTextColor(LIVE_OK_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+10); lcd.print(str(STR_CAL_MAX_OK));
            printDeciValue(8, baseY+24, calib.resultCentiVolts/10, EDIT_VALUE_COLOR, EDIT_BG_COLOR, &fonts::DejaVu18);
            lcd.setFont(&fonts::DejaVu9); lcd.setCursor(80, baseY+28); lcd.print(str(STR_VOLTS));
            break;
        case CALIB_DONE:
            lcd.setFont(&fonts::DejaVu9); lcd.setTextColor(LIVE_OK_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+10); lcd.print(str(STR_CAL_DONE));
            lcd.setTextColor(EDIT_LABEL_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+28); lcd.print(str(STR_CAL_ZERO_OK)); lcd.print(" ADC "); lcd.print(calib.resultAdcZero);
            lcd.setCursor(8, baseY+42); lcd.print(str(STR_CAL_MAX_OK)); lcd.print(" ADC "); lcd.print(calib.resultAdcMax);
            break;
        case CALIB_ERROR:
            lcd.setFont(&fonts::DejaVu18); lcd.setTextColor(LIVE_ERROR_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+10); lcd.print(str(STR_CAL_ERROR));
            lcd.setFont(&fonts::DejaVu9); lcd.setTextColor(LIVE_WARN_COLOR, EDIT_BG_COLOR);
            lcd.setCursor(8, baseY+34); if (calib.errorMsg) lcd.print(calib.errorMsg);
            break;
    }
    // Draw Redo and Next buttons at the bottom of the content area
    // Only shown when in EDIT_VALUE state (not preview from MENU_LIST)
    if (ui.state == DISPLAY_EDIT_VALUE) {
        int16_t btnY = SCREEN_HEIGHT - 20;
        // Redo button
        drawButtonHighlight(4, btnY, 56, 16, ui.menuScrollPos == 4);
        lcd.setFont(&fonts::DejaVu12);
        lcd.setTextColor((ui.menuScrollPos == 4) ? NAV_SELECTED_FG : EDIT_LABEL_COLOR, (ui.menuScrollPos == 4) ? NAV_SELECTED_BG : EDIT_BG_COLOR);
        lcd.setCursor(12, btnY + 2); lcd.print(str(STR_CAL_REDO));
        // Next button
        drawButtonHighlight(66, btnY, 56, 16, ui.menuScrollPos == 5);
        lcd.setTextColor((ui.menuScrollPos == 5) ? NAV_SELECTED_FG : EDIT_LABEL_COLOR, (ui.menuScrollPos == 5) ? NAV_SELECTED_BG : EDIT_BG_COLOR);
        lcd.setCursor(76, btnY + 2); lcd.print(str(STR_CAL_NEXT));
    }
}

void displayDrawLanguage(const UIState& ui, uint8_t currentLanguage) {
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_LANGUAGE));
    int16_t baseY = CONTENT_Y + 20;

    for (uint8_t i = 0; i < NUM_LANGUAGES; i++) {
    drawSelectableRow(4, baseY + i * 22, 120, 22, LANG_NAMES[i],
                      ui.menuScrollPos == (4 + i),
                      i == currentLanguage,
                      &fonts::DejaVu18);
    }
}

void displayDrawQuickSave(const UIState& ui, const DeviceConfig& cfg,
                          uint8_t profileSlot, bool justSaved) {
    activeLang = cfg.language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_QUICK_SAVE));

    int16_t baseY = CONTENT_Y + 14;

    // Hint line
    lcd.setFont(&fonts::DejaVu9);
    lcd.setTextColor(EDIT_LABEL_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(8, baseY);
    lcd.print(str(STR_QUICK_SAVE_HINT));

    // Target profile line: "Prof 3"
    lcd.setFont(&fonts::DejaVu18);
    lcd.setTextColor(EDIT_VALUE_COLOR, EDIT_BG_COLOR);
    lcd.setCursor(8, baseY + 16);
    lcd.print(str(STR_PROFILE)); lcd.print(" "); lcd.print(profileSlot + 1);

    // Big action button (only meaningful in EDIT_VALUE state)
    if (ui.state == DISPLAY_EDIT_VALUE) {
        bool isSel = (ui.menuScrollPos == 4);
        int16_t btnY = baseY + 46;
        drawButtonHighlight(4, btnY, 120, 26, isSel);

        lcd.setFont(&fonts::DejaVu18);
        uint16_t fg = isSel ? NAV_SELECTED_FG : EDIT_VALUE_COLOR;
        uint16_t bg = isSel ? NAV_SELECTED_BG : EDIT_BG_COLOR;
        lcd.setTextColor(fg, bg);
        lcd.setCursor(28, btnY + 4);
        lcd.print(str(STR_SAVE));
    }

    // Confirmation flash — drawn over/under the button area
    if (justSaved) {
        lcd.setFont(&fonts::DejaVu12);
        lcd.setTextColor(LIVE_OK_COLOR, EDIT_BG_COLOR);
        int16_t w = lcd.textWidth(str(STR_QUICK_SAVE_DONE));
        lcd.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT - 14);
        lcd.print(str(STR_QUICK_SAVE_DONE));
    }
}

void displayDrawSaveLoad(const UIState& ui, uint8_t selectedSlot,
                         const bool slotExists[NUM_NVS_PROFILES], uint8_t language) {
    activeLang = language;
    displayClearContentArea();
    displayDrawEditTitle(str(STR_TITLE_SAVE_LOAD));
    int16_t baseY = CONTENT_Y + 16;

    for (uint8_t i = 0; i < NUM_NVS_PROFILES; i++) {
        char label[16];
        snprintf(label, sizeof(label), "%s %u: ", str(STR_PROFILE), i + 1);

        drawSelectableRowStatus(
            4, baseY + i * 14, 120, 16, label,
            slotExists[i] ? str(STR_SAVED) : str(STR_EMPTY),
            slotExists[i] ? LIVE_OK_COLOR  : NAV_NORMAL_FG,
            ui.menuScrollPos == (4 + i)
        );
    }
    int16_t actY = baseY + NUM_NVS_PROFILES*14 + 4;
    uint8_t sb = 4+NUM_NVS_PROFILES, lb = sb+1;
    drawButtonHighlight(4, actY, 56, 16, ui.menuScrollPos==sb);
    lcd.setFont(&fonts::DejaVu9); lcd.setTextColor((ui.menuScrollPos==sb)?NAV_SELECTED_FG:EDIT_LABEL_COLOR, (ui.menuScrollPos==sb)?NAV_SELECTED_BG:EDIT_BG_COLOR);
    lcd.setCursor(12, actY+4); lcd.print(str(STR_SAVE));
    drawButtonHighlight(66, actY, 56, 16, ui.menuScrollPos==lb);
    lcd.setTextColor((ui.menuScrollPos==lb)?NAV_SELECTED_FG:EDIT_LABEL_COLOR, (ui.menuScrollPos==lb)?NAV_SELECTED_BG:EDIT_BG_COLOR);
    lcd.setCursor(76, actY+4); lcd.print(str(STR_LOAD));
}

void displayDrawEditScreen(const UIState& ui, const DeviceConfig& cfg, const LiveData& data) {
    activeLang = cfg.language;
    switch (ui.editScreenIndex) {
        case 0: displayDrawHoldMode(ui, cfg); break;
        case 1: displayDrawDeadzones(ui, cfg); break;
        case 2: displayDrawDefaultCurve(ui, cfg); break;
        case 3: displayDrawSnapThreshold(ui, cfg); break;
        case 4: displayDrawButtonDebounce(ui, cfg); break;
        case 5: displayDrawRefreshRates(ui, cfg); break;
        case 6: { CalibData ec; ec.state=CALIB_IDLE; ec.sampleCount=0; ec.settleStableCount=0; ec.errorMsg=NULL;
                   displayDrawCalibrate(ui, ec, cfg.language); break; }
        case 7: displayDrawLanguage(ui, cfg.language); break;
        case 8: displayDrawQuickSave(ui, cfg, storageGetLastProfile(), false); break;
        case 9: { bool es[NUM_NVS_PROFILES]; for(uint8_t i=0;i<NUM_NVS_PROFILES;i++) es[i]=storageProfileExists(i);
           displayDrawSaveLoad(ui, 0, es, cfg.language); break; }
        default: break;
    }
}
