/****************************************************************************
 *
 * Filename:    PicoScaling.h
 * Copyright:   Pico Technology Limited 2023 - 2025
 * Description:
 *
 * This header defines functions for creating buffers to store PicoScope data
 *
 ****************************************************************************/
#ifndef __PICOBUFFERS_H__
#define __PICOBUFFERS_H__

#include "PicoDeviceStructs.h"
#include "./PicoUnit.h"

 /* Headers for Windows */
#ifdef _WIN32

#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif


#endif

typedef struct tbuffer_settings
{
	uint64_t		startIndex;
	uint64_t		nSamples;
	PICO_RATIO_MODE	downSampleRatioMode;
	uint64_t		downSampleRatio;
}BUFFER_SETTINGS;

typedef struct tmultiBufferSizes
{
	uint64_t numberOfBuffers;
	uint64_t maxBufferSize;
	uint64_t minBufferSize;
}MULTIBUFFERSIZES;

// Function prototypes
void data_buffer_sizes(PICO_RATIO_MODE downSampleRatioMode, uint64_t downSampleRatio, uint64_t noOfSamples, uint64_t* maxBufferSize, uint64_t* minBufferSize);

int16_t pico_create_multibuffers(GENERICUNIT* unit, struct tbuffer_settings bufferSettings, uint64_t numberOfBuffers, int16_t**** minBuffers, int16_t**** maxBuffers, struct tmultiBufferSizes* multiBufferSizes);
void pico_release_multibuffers(GENERICUNIT* unit, int16_t**** minBuffers, int16_t**** maxBuffers, struct tmultiBufferSizes* multiBufferSizes);

#endif
