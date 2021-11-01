#pragma once

#include "ps2000.h"
#include "ps2000aApi.h"
#include "ps3000aApi.h"
#include "ps4000aApi.h"
#include "ps5000aApi.h"

extern uint16_t inputRanges[PS5000A_MAX_RANGES];

int32_t adc_to_mv(int32_t raw, int32_t rangeIndex, int16_t maxADCValue);

int16_t mv_to_adc(int16_t mv, int16_t rangeIndex, int16_t maxADCValue);

extern bool g_ready;

void PREF4 callBackBlock(int16_t handle, PICO_STATUS status, void* pParameter);

extern uint32_t downSampleRatio;

extern PS2000A_RATIO_MODE PS2000A_ratioMode;
extern PS3000A_RATIO_MODE PS3000A_ratioMode;
extern PS4000A_RATIO_MODE PS4000A_ratioMode;
extern PS5000A_RATIO_MODE PS5000A_ratioMode;
extern int16_t g_overflow;

void getStatusCodeCSV();
void getStatusCode();
void getStatusCode(int);