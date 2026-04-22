// ============================================================================
// strings.cpp — Localized String Table Implementation
// ============================================================================
// Project:  BalduHandbrake — Open Source Hydraulic Simracing Handbrake
// License:  Apache 2.0
// ============================================================================

#include "strtable.h"

// ============================================================================
//  Localization String Table
// ============================================================================
static const char* const STRING_TABLE[NUM_LANGUAGES][NUM_STRINGS] = {
    // ---- LANG_EN ----
    {
		"E-BRAKE",				//STR_BOOT_TITLE = 0,
		"v1.3",				    //STR_BOOT_VERSION,
		"A.Belluscio",			//STR_BOOT_AUTHOR,
		"BalduHandbrake",		//STR_BOOT_PROJECT,
		"Initializing...",		//STR_BOOT_STATUS,
		"HOLD",					//STR_HOLD,
		"LIVE",					//STR_LIVE,
		"PSI",					//STR_PSI,
		"kgf",					//STR_FORCE,
		"V",					//STR_VOLTS,
		"%",					//STR_PERCENT,
		"FAIL",					//STR_FAIL,
		"LOW",					//STR_LOW,
		"OVER",					//STR_OVER,
		"Game",					//STR_GAME,
		"Firmware",				//STR_FIRMWARE,
		"Save",					//STR_SAVE,
		"Load",					//STR_LOAD,
		"Empty",				//STR_EMPTY,
		"Saved",				//STR_SAVED,
		"Prof",					//STR_PROFILE,
		"Push handle down",		//STR_CAL_PUSH_DOWN,
		"Pull handle up",		//STR_CAL_PULL_UP,
		"Settling...",			//STR_CAL_SETTLING,
		"Hold steady...",		//STR_CAL_HOLD_STEADY,
		"Sampling...",			//STR_CAL_SAMPLING,
		"Calibration OK",		//STR_CAL_DONE,
		"Zero:",				//STR_CAL_ZERO_OK,
		"Max:",					//STR_CAL_MAX_OK,
		"ERROR",				//STR_CAL_ERROR,
		"X=retry S=keep",		//STR_CAL_RETRY,
		"Purge fluid!",			//STR_CAL_PURGE,
		"Overpressure!",		//STR_CAL_OVERPRESS,
		"Too close to zero",	//STR_CAL_TOO_LOW,
		"Hold Mode",			//STR_TITLE_HOLD_MODE,
		"Deadzones",			//STR_TITLE_DEADZONES,
		"Default Curve",		//STR_TITLE_DEFAULT_CURVE,
		"Snap Threshold",		//STR_TITLE_SNAP_THRESH,
		"Btn Debounce",			//STR_TITLE_DEBOUNCE,
		"Refresh Rates",		//STR_TITLE_REFRESH,
		"Recalibrate",			//STR_TITLE_CALIBRATE,
		"Language",				//STR_TITLE_LANGUAGE,
		"Quick Save",			//STR_TITLE_QUICK_SAVE,
		"Save & Load",			//STR_TITLE_SAVE_LOAD,
		"Save settings",		//STR_QUICK_SAVE_HINT,
		"Saved!",				//STR_QUICK_SAVE_DONE,
		"Low:",					//STR_LABEL_LOW,
		"High:",				//STR_LABEL_HIGH,
		"USB/ADC:",				//STR_LABEL_USB_ADC,
		"Display:",				//STR_LABEL_DISPLAY,
		"ms",					//STR_LABEL_MS,
		"Hz",					//STR_LABEL_HZ,
		"Redo",					//STR_CAL_REDO,
		"Next",					//STR_CAL_NEXT,
		//NUM_STRINGS
    },
	// ---- LANG_ES ----
    {
        "E-BRAKE",              //STR_BOOT_TITLE = 0,
        "v1.3",                 //STR_BOOT_VERSION,
        "A.Belluscio",          //STR_BOOT_AUTHOR,
        "BalduHandbrake",       //STR_BOOT_PROJECT,
        "Iniciando...",         //STR_BOOT_STATUS,
        "SOSTEN",               //STR_HOLD,
        "VIVO",                 //STR_LIVE,
        "PSI",                  //STR_PSI,
        "kgf",                  //STR_FORCE,
        "V",                    //STR_VOLTS,
        "%",                    //STR_PERCENT,
        "FALLO",                //STR_FAIL,
        "BAJA",                 //STR_LOW,
        "SOBRE",                //STR_OVER,
        "Juego",                //STR_GAME,
        "Firmware",             //STR_FIRMWARE,
        "Guardar",              //STR_SAVE,
        "Cargar",               //STR_LOAD,
        "Vacio",                //STR_EMPTY,
        "Guardado",             //STR_SAVED,
        "Perf",                 //STR_PROFILE,
        "Baje palanca",         //STR_CAL_PUSH_DOWN,
        "Tire palanca",         //STR_CAL_PULL_UP,
        "Estabilizando...",     //STR_CAL_SETTLING,
        "Mantenga firme...",    //STR_CAL_HOLD_STEADY,
        "Muestreando...",       //STR_CAL_SAMPLING,
        "Calibracion OK",      //STR_CAL_DONE,
        "Cero:",                //STR_CAL_ZERO_OK,
        "Max:",                 //STR_CAL_MAX_OK,
        "ERROR",                //STR_CAL_ERROR,
        "X=rein S=guard",       //STR_CAL_RETRY,
        "Purgar fluido!",       //STR_CAL_PURGE,
        "Sobrepresion!",        //STR_CAL_OVERPRESS,
        "Muy cerca de cero",    //STR_CAL_TOO_LOW,
        "Modo Espera",          //STR_TITLE_HOLD_MODE,
        "Zonas Muertas",        //STR_TITLE_DEADZONES,
        "Curva Default",        //STR_TITLE_DEFAULT_CURVE,
        "Umbral Snap",          //STR_TITLE_SNAP_THRESH,
        "Antirrebote",          //STR_TITLE_DEBOUNCE,
        "Tasas Refresco",       //STR_TITLE_REFRESH,
        "Recalibrar",           //STR_TITLE_CALIBRATE,
        "Idioma",               //STR_TITLE_LANGUAGE,
        "Guardo Rapido",        //STR_TITLE_QUICK_SAVE,
        "Guardar & Cargar",     //STR_TITLE_SAVE_LOAD,
        "Guardar ajustes",      //STR_QUICK_SAVE_HINT,
        "Guardado!",            //STR_QUICK_SAVE_DONE,
        "Bajo:",                //STR_LABEL_LOW,
        "Alto:",                //STR_LABEL_HIGH,
        "USB/ADC:",             //STR_LABEL_USB_ADC,
        "Pantalla:",            //STR_LABEL_DISPLAY,
        "ms",                   //STR_LABEL_MS,
        "Hz",                   //STR_LABEL_HZ,
        "Rehacer",              //STR_CAL_REDO,
        "Seguir",               //STR_CAL_NEXT,
        //NUM_STRINGS
    },
};

const char* const LANG_NAMES[NUM_LANGUAGES] = { "English", "Espanol" };

// ============================================================================
//  strGet()
// ============================================================================
const char* strGet(StringID id, uint8_t language) {
    if (language >= NUM_LANGUAGES) language = LANG_EN;
    if (id >= NUM_STRINGS) return "";
    return STRING_TABLE[language][id];
}