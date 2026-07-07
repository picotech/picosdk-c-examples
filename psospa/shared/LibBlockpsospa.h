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

#ifndef __LIBBLOCKPSOSPA_H__
#define __LIBBLOCKPSOSPA_H__

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
void blockDataHandler(GENERICUNIT* unit,
							uint64_t noOfPreTriggerSamples,		// Used by RunBlock()
							uint64_t noOfPostTriggerSamples,	// Used by RunBlock()
							double idealTimeInterval,			// Used by RunBlock()
							uint64_t nSamples,					// Used by SetDataBuffers()
							PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
							uint64_t downSampleRatio,			// Used by SetDataBuffers()
							FILE_TYPE filetype,
							BOOL imagefile
);

void blockOverlappedDataHandler(GENERICUNIT* unit,
	uint64_t noOfPreTriggerSamples,		// Used by RunBlock()
	uint64_t noOfPostTriggerSamples,	// Used by RunBlock()
	double idealTimeInterval,			// Used by RunBlock()
	uint64_t nSamples,					// Used by SetDataBuffers()
	PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
	uint64_t downSampleRatio			// Used by SetDataBuffers()
);

#endif
