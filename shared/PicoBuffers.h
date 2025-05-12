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
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <libps6000a/ps6000aApi.h>
#ifndef PICO_STATUS
#include <libps6000a/PicoStatus.h>
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

void pico_create_multibuffers(GENERICUNIT* unit, BUFFER_SETTINGS bufferSettings, uint64_t numberOfBuffers, int16_t**** minBuffers, int16_t**** maxBuffers, MULTIBUFFERSIZES* multiBufferSizes);

#endif