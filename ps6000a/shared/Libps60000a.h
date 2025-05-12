/****************************************************************************
 *
 * Filename:    Libps6000a.h
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 * This header defines shared functions and structures for
 * all ps6000a example code.
 *
 ****************************************************************************/

#ifndef __LIBPS60000A_H__
#define __LIBPS60000A_H__

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "ps6000aApi.h"
#include "../../shared/PicoUnit.h"
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

#define OCTA_SCOPE		8
#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

#define MAX_PICO_DEVICES 64
#define TIMED_LOOP_STEP 500

//Max channels for this API/series of models
#define PS6000A_MAX_CHANNELS 8 //analog chs only
#define MSO_MAX_CHANNELS 2 //digital chs only

//Default Enabled Channel defines-
#define ENABLED_CHS_LIMIT 8 //Set to limit the max number channels to enable (for example if set to 2 then ChA and CnB will be turned on)
#define TURN_ON_EVERY_N_CH 2 //Set this either 2 or 4 (2 = Every odd Ch is enabled, 4 = Every 4th Ch enabled) Or set to 1 to disable.

typedef struct tPwq
{
	PICO_CONDITION* conditions;
	int16_t nConditions;
	PICO_DIRECTION* directions;
	int16_t nDirections;
	uint32_t  lower;
	uint32_t upper;
	PICO_PULSE_WIDTH_TYPE type;
}PWQ;

typedef enum
{
	SIGGEN_NONE = 0,
	SIGGEN_FUNCTGEN = 1,
	SIGGEN_AWG = 2
}SIGGEN_TYPE;

// Struct to store intelligent probe information
typedef struct tUserProbeInfo
{
	PICO_STATUS status;
	PICO_USER_PROBE_INTERACTIONS userProbeInteractions[PS6000A_MAX_CHANNELS];
	uint32_t numberOfProbes;

}USER_PROBE_INFO;

// Function prototypes
void setDefaults(GENERICUNIT* unit);
void set_info(GENERICUNIT* unit);
void displaySettings(GENERICUNIT* unit);

PICO_STATUS openDevice(GENERICUNIT* unit, int8_t* serial);
void closeDevice(GENERICUNIT* unit);
PICO_STATUS handleDevice(GENERICUNIT* unit);

void setVoltages(GENERICUNIT* unit);
void setTimebase(GENERICUNIT* unit);

void setResolution(GENERICUNIT* unit);
void printResolution(PICO_DEVICE_RESOLUTION* resolution);

PICO_STATUS clearDataBuffers(GENERICUNIT* unit);

PICO_STATUS SetTrigger(GENERICUNIT* unit,
	PICO_TRIGGER_CHANNEL_PROPERTIES* channelProperties,
	int16_t nChannelProperties,
	int16_t auxOutputEnable,
	PICO_CONDITION* triggerConditions,
	int16_t nTriggerConditions,
	PICO_DIRECTION* directions,
	int16_t nDirections,
	struct tPwq* pwq,
	uint32_t delay,
	int32_t autoTrigger_us);

#endif