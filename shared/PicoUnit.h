/****************************************************************************
 *
 * Filename:    PicoScaling.h
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 * This header defines shared datatypes/enums/structures for Pico Examples.
 *
 ****************************************************************************/
#ifndef __PICOUNIT_H__
#define __PICOUNIT_H__

#include "PicoConnectProbes.h"

//#include <stdint.h>
 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "PicoDeviceStructs.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <shared/PicoDeviceStructs.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL { FALSE, TRUE } BOOL;

/* A function to detect a keyboard press on Linux */
int32_t _getch()
{
	struct termios oldt, newt;
	int32_t ch;
	int32_t bytesWaiting;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	do {
		ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
		if (bytesWaiting)
			getchar();
	} while (bytesWaiting);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}

int32_t _kbhit()
{
	struct termios oldt, newt;
	int32_t bytesWaiting;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return bytesWaiting;
}

int32_t fopen_s(FILE** a, const char* b, const char* c)
{
	FILE* fp = fopen(b, c);
	*a = fp;
	return (fp > 0) ? 0 : -1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif
typedef enum
{
	MODEL_NONE = 0,//this is used
	//models values not used in the code
}MODEL_TYPE; 

typedef struct
{
	int16_t enabled;
	PICO_COUPLING DCcoupled;
	PICO_CONNECT_PROBE_RANGE range;	// For APIs that use enum channel range

	// 3 Items below for psospa API only
	PICO_PROBE_RANGE_INFO rangeType;	//x1 or x10 scaling
	int64_t			rangeMin;		// In nV
	int64_t			rangeMax;		// In nV
	
	double analogueOffset;
	PICO_BANDWIDTH_LIMITER bandwithLimit;
}CHANNEL_SETTINGS;

typedef struct
{
	int16_t enabled;
	double threshold[8];//voltage threshold per digital channel I/P, only threshold[0] for non 6000a API units
}MSO_CHANNEL_SETTINGS;

typedef struct tGenericUnit
{
	int16_t						handle;
	MODEL_TYPE					model;
	int8_t						modelString[8];
	int8_t						serial[10];
	int16_t						complete;
	int16_t						openStatus;
	int16_t						openProgress;
	PICO_CONNECT_PROBE_RANGE	firstRange;
	PICO_CONNECT_PROBE_RANGE	lastRange;
	int16_t						channelCount;
	int16_t						maxADCValue;
	PICO_WAVE_TYPE				sigGen;
	int16_t						hasHardwareETS;
	uint16_t					awgBufferSize;
	CHANNEL_SETTINGS			channelSettings[8];
	PICO_DEVICE_RESOLUTION		resolution;
	double						timeInterval;
	int16_t						digitalPortCount;
	MSO_CHANNEL_SETTINGS		digitalChannelSettings[2];
}GENERICUNIT;

// Function prototypes


#endif