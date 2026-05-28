/****************************************************************************
 *
 * Filename:    LibStreamingps4000a.h
 * Copyright:   Pico Technology Limited 2026
 * Description:
 *
 * This header defines...
 *
 ****************************************************************************/

#ifndef __LIBSTREAMINGPS4000A_H__
#define __LIBSTREAMINGPS4000A_H__

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"

#include <conio.h>
#include "ps4000aApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <ps4000aApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif

#endif

// Function prototypes
void streamDataHandler(GENERICUNIT* unit,
	uint64_t noOfPreTriggerSamples,		// Used by RunStreaming()
	uint64_t noOfPostTriggerSamples,	// Used by RunStreaming()
	uint32_t idealTimeInterval,			// Used by RunStreaming()
	uint32_t sampleIntervalTimeUnits,	// Used by RunStreaming()
	uint64_t nSamples,					// Set the number of samples per capture - Used by SetDataBuffers()
	PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
	uint64_t downSampleRatio,			// Used by SetDataBuffers()
	int16_t autostop,
	FILE_TYPE filetype);
#endif



