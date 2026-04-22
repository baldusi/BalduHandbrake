/*  BalduHandrake
	Open Source Hydraulic Simracing Handbrake
	Copyright (c) 2026 Alejandro Belluscio
	Additional copyright holders listed inline below.
	This file is licensed under the Apache 2.0 license
	Full licence text: see LICENSE in this repository. 
*/
// ============================================================================
// ui.cpp — User Interface Implementation
// ============================================================================

#include "ui.h"
#include "display.h"
#include "sensor.h"
#include "storage.h"
#include "strtable.h"
#include <RotaryEncoder.h>

// ============================================================================
//  Module-local state
// ============================================================================
static RotaryEncoder encoder(ROTARY_CHANNEL_A_PIN, ROTARY_CHANNEL_B_PIN,
                              RotaryEncoder::LatchMode::FOUR3);
static int16_t lastEncoderPos = 0;

static unsigned long lastEncBtnDebounceTime = 0;
static int           lastEncBtnReading      = HIGH;
static bool          encBtnStable           = false;
static bool          encBtnPrevStable       = false;
static bool          encBtnHeld             = false;
static bool          btnUsedAsModifier      = false;

static UIState       uiState;
static UIState       prevUiState;
static bool          forceFullRedraw        = true;

static DeviceConfig  editConfig;

static const uint8_t TOTAL_EDIT_SCREENS = 10;
static const uint8_t EDIT_SCREEN_BUTTONS[] = { 2, 8, 2, 4, 2, 4, 2, 2, 1, 7 };

static unsigned long lastDisplayUpdate = 0;
static unsigned long curveOverlayStartMs = 0;
static bool          curveOverlayActive  = false;

static CalibData     calibData;
static unsigned long calibSampleInterval = 0;
static unsigned long lastCalibSampleMs   = 0;

static bool          quickSaveJustDone   = false;
static unsigned long quickSaveDoneAtMs   = 0;
static const unsigned long QUICK_SAVE_FLASH_MS = 1200;

static uint8_t       selectedProfileSlot = 0;
static bool          profileSlotExists[NUM_NVS_PROFILES];

// ============================================================================
//  uiInit()
// ============================================================================
void uiInit() {
    encoder.setPosition(0);
    lastEncoderPos = 0;
    pinMode(ROTARY_BUTTON_PIN, INPUT_PULLUP);
    uiState.state = DISPLAY_LIVE;
    uiState.menuScrollPos = 0;
    uiState.editScreenIndex = 0;
    uiState.liveScreen = LIVE_FULL_DATA;
    prevUiState = uiState;
    forceFullRedraw = true;
    calibData.state = CALIB_IDLE;
    calibData.sampleCount = 0;
    calibData.errorMsg = NULL;
    calibData.settleRingIdx = 0;
    calibData.settleStableCount = 0;
    calibData.settleRunningSum = 0;
    calibData.settleBufferFull = false;
}

// ============================================================================
//  Encoder button
// ============================================================================
static void updateEncoderButton(unsigned long nowMs, uint8_t debounceMs) {
    int reading = digitalRead(ROTARY_BUTTON_PIN);
    if (reading != lastEncBtnReading) lastEncBtnDebounceTime = nowMs;
    lastEncBtnReading = reading;
    encBtnPrevStable = encBtnStable;
    if ((nowMs - lastEncBtnDebounceTime) >= debounceMs)
        encBtnStable = (reading == LOW);
    encBtnHeld = encBtnStable;
}

static bool encBtnJustReleased() {
    return !encBtnStable && encBtnPrevStable;
}

// ============================================================================
//  Parameter editing
// ============================================================================
static void backupEditParams(const DeviceConfig& cfg) { editConfig = cfg; }

static void commitEditParams(DeviceConfig& pendingConfig, volatile bool& configDirty) {
    pendingConfig = editConfig;
    configDirty = true;
}

static uint16_t clampU16(int32_t val, uint16_t lo, uint16_t hi) {
    if (val < (int32_t)lo) return lo;
    if (val > (int32_t)hi) return hi;
    return (uint16_t)val;
}

static uint8_t clampU8(int16_t val, uint8_t lo, uint8_t hi) {
    if (val < (int16_t)lo) return lo;
    if (val > (int16_t)hi) return hi;
    return (uint8_t)val;
}

static bool handleAdjustment(uint8_t si, uint8_t bi) {
    switch (si) {
        case 0:
            if (bi==4) { editConfig.holdMode=HOLD_GAME; return true; }
            if (bi==5) { editConfig.holdMode=HOLD_FIRMWARE; return true; }
            break;
        case 1:
            if (bi==4)  { editConfig.deadzoneLow =clampU16(editConfig.deadzoneLow +10,0,200); return true; }
            if (bi==5)  { editConfig.deadzoneLow =clampU16(editConfig.deadzoneLow + 1,0,200); return true; }
            if (bi==6)  { editConfig.deadzoneLow =clampU16((int32_t)editConfig.deadzoneLow - 1,0,200); return true; }
            if (bi==7)  { editConfig.deadzoneLow =clampU16((int32_t)editConfig.deadzoneLow -10,0,200); return true; }
            if (bi==8)  { editConfig.deadzoneHigh=clampU16(editConfig.deadzoneHigh+10,0,200); return true; }
            if (bi==9)  { editConfig.deadzoneHigh=clampU16(editConfig.deadzoneHigh+ 1,0,200); return true; }
            if (bi==10) { editConfig.deadzoneHigh=clampU16((int32_t)editConfig.deadzoneHigh- 1,0,200); return true; }
            if (bi==11) { editConfig.deadzoneHigh=clampU16((int32_t)editConfig.deadzoneHigh-10,0,200); return true; }
            break;
        case 2:
            if (bi==4) { editConfig.curveIndex=(editConfig.curveIndex+NUM_CURVES-1)%NUM_CURVES; return true; }
            if (bi==5) { editConfig.curveIndex=(editConfig.curveIndex+1)%NUM_CURVES; return true; }
            break;
        case 3:
            if (bi==4) { editConfig.snapThreshold=clampU16(editConfig.snapThreshold+10,0,1000); return true; }
            if (bi==5) { editConfig.snapThreshold=clampU16(editConfig.snapThreshold+ 1,0,1000); return true; }
            if (bi==6) { editConfig.snapThreshold=clampU16((int32_t)editConfig.snapThreshold- 1,0,1000); return true; }
            if (bi==7) { editConfig.snapThreshold=clampU16((int32_t)editConfig.snapThreshold-10,0,1000); return true; }
            break;
        case 4:
            if (bi==4) { editConfig.debounceMs=clampU8(editConfig.debounceMs+1,5,200); return true; }
            if (bi==5) { editConfig.debounceMs=clampU8(editConfig.debounceMs-1,5,200); return true; }
            break;
        case 5: {
            //static const uint16_t sr[]={250,475,860,1000}; static const uint8_t nsr=4;
            const uint16_t* sr = SENSOR_RATE_OPTIONS; const uint8_t nsr = SENSOR_RATE_COUNT;
            static const uint16_t dr[]={10,15,20,30}; static const uint8_t ndr=4;
            if (bi==4||bi==5) {
                uint8_t idx=0; for(uint8_t i=0;i<nsr;i++) if(sr[i]==editConfig.sampleRateHz){idx=i;break;}
                idx=(bi==4)?(idx+1)%nsr:(idx+nsr-1)%nsr;
                editConfig.sampleRateHz=sr[idx]; return true;
            }
            if (bi==6||bi==7) {
                uint8_t idx=0; for(uint8_t i=0;i<ndr;i++) if(dr[i]==editConfig.displayRateHz){idx=i;break;}
                idx=(bi==6)?(idx+1)%ndr:(idx+ndr-1)%ndr;
                editConfig.displayRateHz=dr[idx]; return true;
            }
            break; }
        case 7:
            if (bi>=4&&bi<4+NUM_LANGUAGES) { editConfig.language=bi-4; return true; }
            break;
        case 8:
            // Quick Save has no adjustable parameters; action handled in handleEncoderClick.
            break;
        case 9:
            if (bi>=4&&bi<4+NUM_NVS_PROFILES) { selectedProfileSlot=bi-4; return true; }
            break;
    }
    return false;
}

// ============================================================================
//  Calibration
// ============================================================================
static void calibResetSettling() {
    calibData.settleRingIdx=0; calibData.settleStableCount=0;
    calibData.settleRunningSum=0; calibData.settleBufferFull=false;
}

static void calibReset() {
    calibData.state=CALIB_IDLE; calibData.sampleCount=0;
    calibData.errorMsg=NULL; calibResetSettling();
}

static bool calibSettleFeed(uint16_t adcVal, bool isZero, const DeviceConfig& cfg) {
    uint16_t span=cfg.calAdcMax-cfg.calAdcZero;
    if(span==0) span=1;
    bool inZone;
    if(isZero) {
        uint16_t ceiling=cfg.calAdcZero+(uint16_t)((uint32_t)span*CALIB_ZERO_CEILING_PCT/100);
        inZone=(adcVal<ceiling);
    } else {
        uint16_t floor=cfg.calAdcZero+(uint16_t)((uint32_t)span*CALIB_MAX_FLOOR_PCT/100);
        inZone=(adcVal>floor);
    }
    if(!inZone) { calibData.settleStableCount=0; return false; }

    uint8_t idx=calibData.settleRingIdx;
    if(calibData.settleBufferFull) calibData.settleRunningSum-=calibData.settleRingBuf[idx];
    calibData.settleRingBuf[idx]=adcVal;
    calibData.settleRunningSum+=adcVal;
    calibData.settleRingIdx=(idx+1)%CALIB_STABILITY_COUNT;
    if(!calibData.settleBufferFull && calibData.settleRingIdx==0) calibData.settleBufferFull=true;
    if(!calibData.settleBufferFull) return false;

    uint16_t avg=(uint16_t)(calibData.settleRunningSum/CALIB_STABILITY_COUNT);
    uint16_t band=(uint16_t)((uint32_t)span*CALIB_STABILITY_BAND_PCT/100);
    bool allStable=true;
    for(uint8_t i=0;i<CALIB_STABILITY_COUNT;i++) {
        uint16_t val=calibData.settleRingBuf[i];
        if((int32_t)val > (int32_t)avg + band || (int32_t)val < (int32_t)avg - band) { allStable=false; break; }
    }
    if(allStable) calibData.settleStableCount++; else calibData.settleStableCount=0;
    return (calibData.settleStableCount>=1);
}

static void sortU16(uint16_t* arr, uint16_t count) {
    for(uint16_t i=1;i<count;i++) {
        uint16_t key=arr[i]; int16_t j=i-1;
        while(j>=0&&arr[j]>key) { arr[j+1]=arr[j]; j--; }
        arr[j+1]=key;
    }
}

static uint16_t processCalibSamples(uint16_t* samples, uint16_t count, float rp) {
    sortU16(samples,count);
    uint16_t rej=(uint16_t)(count*rp);
    uint16_t s=rej, e=count-rej;
    if(e<=s){s=0;e=count;}
    uint32_t sum=0; for(uint16_t i=s;i<e;i++) sum+=samples[i];
    return (uint16_t)(sum/(e-s));
}

static void calibStartSettling(bool isZero) {
    calibResetSettling();
    calibData.state=isZero?CALIB_SETTLING_ZERO:CALIB_SETTLING_MAX;
    forceFullRedraw=true;
}

static void calibStartSampling(bool isZero, unsigned long nowMs) {
    calibData.sampleCount=0; calibData.samplingStartMs=nowMs;
    calibSampleInterval=CALIB_SAMPLE_DURATION_MS/CALIB_SAMPLE_COUNT;
    lastCalibSampleMs=nowMs;
    calibData.state=isZero?CALIB_SAMPLING_ZERO:CALIB_SAMPLING_MAX;
    forceFullRedraw=true;
}

static void calibUpdate(const LiveData& liveData, const DeviceConfig& ac, unsigned long nowMs) {
    switch(calibData.state) {
        case CALIB_SETTLING_ZERO: case CALIB_SETTLING_MAX: {
            bool isZero=(calibData.state==CALIB_SETTLING_ZERO);
            if(calibSettleFeed(liveData.rawAdc,isZero,editConfig))
                calibStartSampling(isZero,nowMs);
            static unsigned long lastSR=0;
            if((nowMs-lastSR)>=200) { lastSR=nowMs; forceFullRedraw=true; }
            break; }
        case CALIB_SAMPLING_ZERO: case CALIB_SAMPLING_MAX: {
            unsigned long elapsed=nowMs-calibData.samplingStartMs;
            bool isZero=(calibData.state==CALIB_SAMPLING_ZERO);
            if(elapsed>=CALIB_SAMPLE_DURATION_MS) {
                uint16_t result=processCalibSamples(calibData.samples,calibData.sampleCount,CALIB_OUTLIER_REJECT_PCT);
                calibData.resultCentiVolts=(uint16_t)(((uint32_t)result*1875UL+50000UL)/100000UL);
                if(isZero) {
                    if(result<CALIB_REJECT_ZERO_LOW) { calibData.state=CALIB_ERROR; calibData.errorMsg=strGet(STR_CAL_PURGE,editConfig.language); }   //In load cell mode this error messaage makes no sense
                    else { calibData.resultAdcZero=result; calibData.state=CALIB_RESULT_ZERO; }
                } else {
                    if(result>CALIB_REJECT_MAX_HIGH) { calibData.state=CALIB_ERROR; calibData.errorMsg=strGet(STR_CAL_OVERPRESS,editConfig.language); }   //In load cell mode this error messaage makes no sense
                    else if(result<=calibData.resultAdcZero+500) { calibData.state=CALIB_ERROR; calibData.errorMsg=strGet(STR_CAL_TOO_LOW,editConfig.language); }
                    else { calibData.resultAdcMax=result; calibData.state=CALIB_RESULT_MAX; }
                }
                forceFullRedraw=true;
            } else if(calibData.sampleCount<CALIB_SAMPLE_COUNT) {
                if((nowMs-lastCalibSampleMs)>=calibSampleInterval) {
                    calibData.samples[calibData.sampleCount]=liveData.rawAdc;
                    calibData.sampleCount++; lastCalibSampleMs=nowMs;
                }
            }
            static unsigned long lastSamplingRedraw=0;
            if((nowMs-lastSamplingRedraw)>=200) { lastSamplingRedraw=nowMs; forceFullRedraw=true; }
            break; }
        default: break;
    }
}

// ============================================================================
//  Encoder click
// ============================================================================
static void handleEncoderClick(const DeviceConfig& ac, DeviceConfig& pc,
                                volatile bool& cd, unsigned long nowMs) {
    switch(uiState.state) {
        case DISPLAY_LIVE:
            uiState.state=DISPLAY_MENU_LIST; uiState.menuScrollPos=3;
            forceFullRedraw=true; break;
        case DISPLAY_MENU_LIST:
            if(uiState.menuScrollPos==0) {
                uiState.editScreenIndex=(uiState.editScreenIndex+TOTAL_EDIT_SCREENS-1)%TOTAL_EDIT_SCREENS;
                forceFullRedraw=true;
            } else if(uiState.menuScrollPos==1) {
                uiState.editScreenIndex=(uiState.editScreenIndex+1)%TOTAL_EDIT_SCREENS;
                forceFullRedraw=true;
            } else if(uiState.menuScrollPos==2) {
                uiState.state=DISPLAY_LIVE; forceFullRedraw=true;
            } else if(uiState.menuScrollPos==3) {
                uiState.state=DISPLAY_EDIT_VALUE; uiState.menuScrollPos=4;
                backupEditParams(ac);
                quickSaveJustDone = false; //Clear Quick Save's flash message.
                if(uiState.editScreenIndex==6) calibReset();
                else if(uiState.editScreenIndex==9) {
                    selectedProfileSlot=0;
                    for(uint8_t i=0;i<NUM_NVS_PROFILES;i++) profileSlotExists[i]=storageProfileExists(i);
                }
                forceFullRedraw=true;
            }
            break;
        case DISPLAY_EDIT_VALUE:
            if(uiState.menuScrollPos==2) {
                if(uiState.editScreenIndex==6) calibReset();
                editConfig = ac; uiState.state=DISPLAY_MENU_LIST; uiState.menuScrollPos=3; forceFullRedraw=true;
            } else if(uiState.menuScrollPos==3) {
                if(uiState.editScreenIndex==6&&calibData.state==CALIB_DONE) {
                    editConfig.calAdcZero=calibData.resultAdcZero;
                    editConfig.calAdcMax=calibData.resultAdcMax;
                }
                commitEditParams(pc,cd);
                uiState.state=DISPLAY_MENU_LIST; uiState.menuScrollPos=3; forceFullRedraw=true;
             } else {
                uint8_t bi=uiState.menuScrollPos;
                if(uiState.editScreenIndex==6) {
                    if(bi==4) {
                        // Redo button
                        switch(calibData.state) {
                            case CALIB_RESULT_ZERO: calibStartSettling(true); break;
                            case CALIB_RESULT_MAX:  calibStartSettling(false); break;
                            case CALIB_DONE:        calibReset(); calibStartSettling(true); break;
                            case CALIB_ERROR:       calibReset(); calibStartSettling(true); break;
                            default: break;  // Ignored during settling/sampling/idle
                        }
                        forceFullRedraw=true;
                    } else if(bi==5) {
                        // Next button
                        switch(calibData.state) {
                            case CALIB_IDLE: case CALIB_PROMPT_ZERO: calibStartSettling(true); break;
                            case CALIB_RESULT_ZERO: calibData.state=CALIB_PROMPT_MAX; forceFullRedraw=true; break;
                            case CALIB_PROMPT_MAX:  calibStartSettling(false); break;
                            case CALIB_RESULT_MAX:  calibData.state=CALIB_DONE; forceFullRedraw=true; break;
                            default: break;  // Ignored during settling/sampling/done
                        }
                    }
                } else if(uiState.editScreenIndex==8) {
                    // Quick Save — single action button at bi=4
                    if(bi==4) {
                        // Capture the user's current LIVE view into the config before saving
                        editConfig = ac;
                        editConfig.liveScreen = uiState.liveScreen;
                        uint8_t slot = storageGetLastProfile();
                        storageSaveProfile(slot, editConfig);
                        // Also push to active config so subsequent loads stay consistent
                        commitEditParams(pc, cd);
                        quickSaveJustDone = true;
                        quickSaveDoneAtMs = nowMs;
                        forceFullRedraw = true;
                    }
                } else if(uiState.editScreenIndex==9) {
                    if(bi==4+NUM_NVS_PROFILES) {
                        // Save: capture current liveScreen before persisting
                        editConfig.liveScreen = uiState.liveScreen;
                        storageSaveProfile(selectedProfileSlot,editConfig);
                        profileSlotExists[selectedProfileSlot]=true;
                        forceFullRedraw=true; }
                    else if(bi==4+NUM_NVS_PROFILES+1) {
                        if(storageLoadProfile(selectedProfileSlot,editConfig)) forceFullRedraw=true; }
                    else { handleAdjustment(uiState.editScreenIndex,bi); forceFullRedraw=true; }
                } else {
                    if(handleAdjustment(uiState.editScreenIndex,bi)) forceFullRedraw=true;
                }
            }
            break;
    }
}

// ============================================================================
//  Encoder rotation
// ============================================================================
static void handleEncoderRotation(int16_t delta, const DeviceConfig& ac,
                                   DeviceConfig& pc, volatile bool& cd, unsigned long nowMs) {
    switch(uiState.state) {
        case DISPLAY_LIVE:
            if(encBtnHeld) {
                int16_t ns=(int16_t)uiState.liveScreen+delta;
                uiState.liveScreen=(uint8_t)((ns+NUM_LIVE_SCREENS)%NUM_LIVE_SCREENS);
                editConfig=ac; editConfig.liveScreen=uiState.liveScreen;
                pc=editConfig; cd=true; btnUsedAsModifier=true; forceFullRedraw=true;
            } else {
                int16_t nc=(int16_t)ac.curveIndex+delta;
                uint8_t next=(uint8_t)((nc+NUM_CURVES)%NUM_CURVES);
                editConfig=ac; editConfig.curveIndex=next; pc=editConfig; cd=true;
                if(uiState.liveScreen==LIVE_DARK) {
                    displayShowCurveOverlay(next);
                    curveOverlayActive=true; curveOverlayStartMs=nowMs;
                } else forceFullRedraw=true;
            }
            break;
        case DISPLAY_MENU_LIST:
            uiState.menuScrollPos=(uint8_t)(((int16_t)uiState.menuScrollPos+delta+NAV_BOX_COUNT)%NAV_BOX_COUNT);
            break;
        case DISPLAY_EDIT_VALUE: {
            uint8_t nb=EDIT_SCREEN_BUTTONS[uiState.editScreenIndex];
            uint8_t total=nb+2;
            int16_t pos=(int16_t)uiState.menuScrollPos-2;
            pos=((pos+delta)%total+total)%total;
            uiState.menuScrollPos=(uint8_t)(pos+2);
            forceFullRedraw=true;
            break; }
    }
}

// ============================================================================
//  Display update
// ============================================================================
static void updateDisplay(const LiveData& ld, const DeviceConfig& ac, unsigned long nowMs) {

    if(curveOverlayActive && uiState.liveScreen==LIVE_DARK) {
        if((nowMs-curveOverlayStartMs)>=ac.liveDarkTimeoutMs) {
            displaySetupLiveDark(); curveOverlayActive=false;
        }
    }

    if(forceFullRedraw) {
        forceFullRedraw=false;
        switch(uiState.state) {
            case DISPLAY_LIVE:
                switch(uiState.liveScreen) {
                    case LIVE_FULL_DATA: displaySetupLiveFull(ac); break;
                    case LIVE_CLEAN: displaySetupLiveClean(ac); break;
                    case LIVE_BAR_ONLY: displaySetupLiveBar(); break;
                    case LIVE_DARK: displaySetupLiveDark(); break;
                }
                break;
            case DISPLAY_MENU_LIST:
                displayDrawNavBar(uiState);
                displayDrawEditScreen(uiState, ac, ld);
                break;
            case DISPLAY_EDIT_VALUE:
                displayDrawNavBar(uiState);
                if (uiState.editScreenIndex==6) {
                    displayDrawCalibrate(uiState,calibData,editConfig.language);
                } else if (uiState.editScreenIndex==7) {
                    displayDrawLanguage(uiState,editConfig.language);
                } else if(uiState.editScreenIndex==8) {
                    // Auto-clear the "Saved!" flash after the timeout
                    if(quickSaveJustDone && (nowMs - quickSaveDoneAtMs) >= QUICK_SAVE_FLASH_MS) {
                        quickSaveJustDone = false;
                    }
                    displayDrawQuickSave(uiState, editConfig, storageGetLastProfile(), quickSaveJustDone);
                } else if(uiState.editScreenIndex==9) {
                    displayDrawSaveLoad(uiState,selectedProfileSlot,profileSlotExists,editConfig.language); 
                } else {
                    displayDrawEditScreen(uiState,editConfig,ld);
                }
                break;
        }
        return;
    }

    if(uiState.state==DISPLAY_MENU_LIST && uiState.menuScrollPos!=prevUiState.menuScrollPos)
        displayUpdateNavCursor(uiState,prevUiState.menuScrollPos);

    if(uiState.state==DISPLAY_LIVE) {
        uint16_t interval=1000/ac.displayRateHz;
        if((nowMs-lastDisplayUpdate)>=interval) {
            lastDisplayUpdate=nowMs;
            switch(uiState.liveScreen) {
                case LIVE_FULL_DATA: displayUpdateLiveFull(ld,ac); break;
                case LIVE_CLEAN: displayUpdateLiveClean(ld,ac); break;
                case LIVE_BAR_ONLY: displayUpdateLiveBar(ld,ac); break;
                case LIVE_DARK: displayUpdateLiveDark(ld,ac.language); break;
            }
        }
    }
}

// ============================================================================
//  uiUpdate()
// ============================================================================
void uiUpdate(const LiveData& ld, const DeviceConfig& ac,
              DeviceConfig& pc, volatile bool& cd, unsigned long nowMs) {
    
    prevUiState=uiState;
    if(uiState.state==DISPLAY_LIVE && uiState.liveScreen!=ac.liveScreen) {
        uiState.liveScreen=ac.liveScreen; forceFullRedraw=true;
    }
    // Quick Save flash timeout — trigger a redraw when it expires
    if (quickSaveJustDone && (nowMs - quickSaveDoneAtMs) >= QUICK_SAVE_FLASH_MS) {
        forceFullRedraw = true;
        // Note: the flag itself is cleared in updateDisplay above.
    }
    encoder.tick();
    int16_t cp=encoder.getPosition(); int16_t delta=cp-lastEncoderPos;
    if(delta!=0) { lastEncoderPos=cp; handleEncoderRotation(delta,ac,pc,cd, nowMs); }
    updateEncoderButton(nowMs,ac.debounceMs);
    if(encBtnJustReleased()) {
        if(btnUsedAsModifier) btnUsedAsModifier=false;
        else handleEncoderClick(ac,pc,cd,nowMs);
    }
    if(uiState.editScreenIndex==6&&uiState.state==DISPLAY_EDIT_VALUE) calibUpdate(ld,ac,nowMs);
    updateDisplay(ld,ac,nowMs);
}
