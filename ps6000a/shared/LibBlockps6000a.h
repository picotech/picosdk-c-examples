/****************************************************************************
 *
 * Filename:    Libps6000a.h
 * Copyright:   Pico Technology Limited 2024
 * Description:
 *
 * This header file to use with the
 * PicoScope 6XXXE Series (ps6000a) devices,
 * for Block captures.
 *
 ****************************************************************************/

#ifndef __LIBBLOCKPS6000A_H__
#define __LIBBLOCKPS6000A_H__

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
//#include "math.h"
#include <conio.h>
#include <stdio.h>
#include <stdbool.h>


#include "ps6000aApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <ps6000aApi.h>
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