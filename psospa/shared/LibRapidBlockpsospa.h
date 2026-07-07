/****************************************************************************
 *
 * Filename:    Libps6000a.h
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 * This header file to use with the
 * PicoScope 3XXXXE Series (psospa) devices,
 * for RapidBlock captures.
 *
 ****************************************************************************/

#ifndef __LIBRAPIDBLOCKPSOSPA_H__
#define __LIBRAPIDBLOCKPSOSPA_H__



 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include "math.h"
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
void rapidblockDataHandler(GENERICUNIT* unit,
									uint64_t noOfPreTriggerSamples,		// Used by RunBlock()
									uint64_t noOfPostTriggerSamples,	// Used by RunBlock()
									double idealTimeInterval,			// Used by RunBlock()
									uint64_t nSamples,					// Used by SetDataBuffers()
									uint64_t nCaptures,
									PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
									uint64_t downSampleRatio,			// Used by SetDataBuffers()
									FILE_TYPE filetype,
									BOOL imagefile
									);

void rapidblockOverlappedDataHandler(GENERICUNIT* unit,
									uint64_t noOfPreTriggerSamples,		// Used by RunBlock()
									uint64_t noOfPostTriggerSamples,	// Used by RunBlock()
									double idealTimeInterval,			// Used by RunBlock()
									uint64_t nSamples,					// Used by SetDataBuffers()
									uint64_t nCaptures,
									PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
									uint64_t downSampleRatio			// Used by SetDataBuffers()
									);
#endif
