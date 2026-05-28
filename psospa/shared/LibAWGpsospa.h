/****************************************************************************
 *
 * Filename:    LibBlockpsospa.h
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 * This header file to use with the
 * PicoScope 3XXXE Series (psospa) devices,
 * for Block captures.
 *
 ****************************************************************************/

#ifndef __LIBAWGPSOSPA_H__
#define __LIBAWGPSOSPA_H__

#include "../../shared/PicoUnit.h"

#include <stdio.h>
#include <stdbool.h>

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
//#include "math.h"
#include <conio.h>

#include "psospaApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <psospaApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif


#endif

// Function prototypes
void SigGenAWG(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SineWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SquareWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void TriangleWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void dc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWG(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
int8_t AWGLoadFile(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetPeaktoPVoltage(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetOffsetVoltage(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetFrequency(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetFrequencyStop(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetFrequencyInc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetSweepTimeInc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SweepOnOff(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SigGenTriggerOnOff(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SigGenTriggerNow(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SigGenTriggerExt(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void printsigGenSettings(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);

#endif
