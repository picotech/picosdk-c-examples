/****************************************************************************
 *
 * Filename:    PicoScaling.h
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 * This header defines file writing functions for PicoScope data.
 *
 ****************************************************************************/
#ifndef __PICOFILEFUNCTIONS_H__
#define __PICOFILEFUNCTIONS_H__

#include "PicoConnectProbes.h"
#include "./PicoUnit.h"
#include "./PicoScaling.h"
#include "./PicoBuffers.h"

 /* Headers for Windows */
#ifdef _WIN32

#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif


#endif

// enums
typedef enum enCaptureMode
{
	BLOCK = 0,
	RAPID_BLOCK = 1,
	STREAMING = 2
}CAPTURE_MODE;
 
// Structures
typedef struct tcaptures_range
{
	uint64_t from;
	uint64_t to;
}CAPTURES_RANGE;

// Function prototypes
// plotChannelMask selects which channels to plot: bit N = channel N (0=A, 1=B, ...).
// Pass 0 to plot all enabled channels.
void WriteArrayToImageGeneric(struct tGenericUnit* unit,
	int16_t*** minBuffers,
	int16_t*** maxBuffers,
	struct tmultiBufferSizes multiBufferSizes,
	struct tPicoProbeScaling* enabledChannelsScaling,
	char startOfFileName[],
	uint64_t Triggersample,
	int16_t* overflow,
	uint32_t plotChannelMask,
	struct tcaptures_range* captures_range);

void WriteArrayToFilesGeneric(struct tGenericUnit* unit,
	int16_t*** minBuffers,
	int16_t*** maxBuffers,
	struct tmultiBufferSizes multiBufferSizes,
	struct tPicoProbeScaling* enabledChannelsScaling,
	char startOfFileName[],
	uint64_t Triggersample,
	int16_t* overflow,
	struct tcaptures_range* captures_range);

void WriteMetaDataToFile(struct tGenericUnit* unit,
	struct tmultiBufferSizes multiBufferSizes,
	struct tPicoProbeScaling* enabledChannelsScaling,
	char startOfFileName[],
	uint64_t Triggersample,
	struct tcaptures_range* captures_range);

void WriteArrayToFilesBinary(struct tGenericUnit* unit,
	int16_t*** minBuffers,
	int16_t*** maxBuffers,
	struct tmultiBufferSizes multiBufferSizes,
	struct tPicoProbeScaling* enabledChannelsScaling,
	char startOfFileName[],
	uint64_t Triggersample,
	int16_t* overflow,
	struct tcaptures_range* captures_range);

void WriteArrayToStdoutGeneric(struct tGenericUnit* unit,
	int16_t*** minBuffers,
	int16_t*** maxBuffers,
	struct tmultiBufferSizes multiBufferSizes,
	struct tPicoProbeScaling* enabledChannelsScaling,
	enum enCaptureMode CaptureMode,
	int16_t numberOfBuffers,
	uint64_t numberOfSamples,
	uint64_t Triggersample,
	int16_t* overflow);

#endif
