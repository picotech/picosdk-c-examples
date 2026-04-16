/****************************************************************************
 *
 * Filename:    Libps6000a.h
 * Copyright:   Pico Technology Limited 2024-2025
 * Description:
 *
 * This header file to use with the
 * PicoScope 6XXXXE Series (ps6000a) devices,
 * for RapidBlock captures.
 *
 ****************************************************************************/

#ifndef __LIBRAPIDBLOCKPS6000A_H__
#define __LIBRAPIDBLOCKPS6000A_H__

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include "math.h"
#include <conio.h>
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

#include <libps6000a-1.0/ps6000aApi.h>
#ifndef PICO_STATUS
#include <libps6000a-1.0/PicoStatus.h>
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

// Function prototypes
void rapidblockDataHandler(GENERICUNIT* unit,
									uint64_t noOfPreTriggerSamples,		// Used by RunBlock()
									uint64_t noOfPostTriggerSamples,	// Used by RunBlock()
									double idealTimeInterval,			// Used by RunBlock()
									uint64_t nSamples,					// Used by SetDataBuffers()
									uint64_t nCaptures,
									PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
									uint64_t downSampleRatio,			// Used by SetDataBuffers()
									FILE_TYPE filetype
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