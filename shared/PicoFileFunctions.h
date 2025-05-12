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

// Function prototypes
void WriteArrayToFilesGeneric(GENERICUNIT* unit,
	int16_t*** minBuffers,
	int16_t*** maxBuffers,
	MULTIBUFFERSIZES multiBufferSizes,
	PICO_PROBE_SCALING* enabledChannelsScaling, ////////////////////////////////////////////////////////////////////////////////////////
	//double actualTimeInterval,// = 1,
	char startOfFileName[],// = "Output",
	int16_t Triggersample, // = 0,//int16_t maxADCValue) // =0
	int16_t* overflow);

void WriteArrayToFileGeneric(GENERICUNIT* unit,
	int16_t** minBuffers,
	int16_t** maxBuffers,
	MULTIBUFFERSIZES multiBufferSizes,
	PICO_PROBE_SCALING* enabledChannelsScaling, ////////////////////////////////////////////////////////////////////////////////////////
	//double actualTimeInterval,
	char startOfFileName[],
	int16_t Triggersample,
	int16_t* overflow);


#endif