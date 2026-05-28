/****************************************************************************
 *
 * Filename:    Libps6000a.h
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 *   This header file to use with the
 *   PicoScope 3XXXE Series (psospa) devices,
 *   for Streaming captures.
 *
 ****************************************************************************/

#ifndef __LIBSTREAMINGPSOSPA_H__
#define __LIBSTREAMINGPSOSPA_H__



 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"

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
void streamDataHandler(GENERICUNIT* unit,
	uint64_t noOfPreTriggerSamples,		// Used by RunStreaming()
	uint64_t noOfPostTriggerSamples,	// Used by RunStreaming()
	double idealTimeInterval,			// Used by RunStreaming()
	uint32_t sampleIntervalTimeUnits,	// Used by RunStreaming()
	uint64_t nSamples,					// Set the number of samples per capture - Used by SetDataBuffers()
	PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
	uint64_t downSampleRatio,			// Used by SetDataBuffers()
	int16_t autostop,
	FILE_TYPE filetype
	);

#endif
