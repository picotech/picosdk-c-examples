/****************************************************************************
 *
 * Filename:    LibAWGps4000a.h
 * Copyright:   Pico Technology Limited 2026
 * Description:
 *
 * This header file to use with the
 * PicoScope 4XXX Series (ps4000a) devices,
 * for Signal Generator (AWG) functionality.
 *
 ****************************************************************************/

#ifndef __LIBAWGPS4000A_H__
#define __LIBAWGPS4000A_H__

#include "../../shared/PicoUnit.h"

#include <stdio.h>
#include <stdbool.h>

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
//#include "math.h"
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

#include <libps4000a/ps4000aApi.h>
#ifndef PICO_STATUS
#include <libps4000a/PicoStatus.h>
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
void SigGenAWG(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SineWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SquareWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void TriangleWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void dc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWG(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
int8_t AWGLoadFile(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetPeaktoPVoltage(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetOffsetVoltage(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetFrequency(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetFrequencyStop(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetFrequencyInc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void AWGSetSweepTimeInc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SweepOnOff(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SigGenTriggerOnOff(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SigGenTriggerNow(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void SigGenTriggerExt(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);
void printsigGenSettings(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings);

#endif



