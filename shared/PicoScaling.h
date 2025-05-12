/****************************************************************************
 *
 * Filename:    PicoScaling.h
 * Copyright:   Pico Technology Limited 2023 - 2025
 * Description:
 *
 * This header defines scaling related to all channel and probe ranges
 * with corresponding units.
 * For example - voltage/current/resistance/pressure/temperature etc.
 *
 ****************************************************************************/
#ifndef __PICOSCALING_H__
#define __PICOSCALING_H__

#include <stdbool.h>
#include "PicoConnectProbes.h"

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
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

typedef struct tPicoProbeScaling   //tPicoTriggerChannelProperties
{
	PICO_CONNECT_PROBE_RANGE	ProbeEnum;
	int8_t						Probe_Range_text[256];
	//char						Probe_Range_text[256]; //const char	Probe_Range_text[256];
	double						MinScale;
	double						MaxScale;
	int8_t						Unit_text[8];
	//char						Unit_text[8]; //const char	Unit_text[8];
} PICO_PROBE_SCALING;

static const uint16_t inputRanges[PICO_X1_PROBE_RANGES] = {
												10,
												20,
												50,
												100,
												200,
												500,
												1000,
												2000,
												5000,
												10000,
												20000 };

static const uint16_t inputRangesx10[PICO_X10_PROBE_RANGES] = {
												100,
												200,
												500,
												1000,
												2000,
												5000,
												10000,
												20000,
												50000 };

static const PICO_PROBE_SCALING PicoProbeScaling[] = {
// x1
{PICO_X1_PROBE_10MV,		"10mV",									 -0.01, 0.01, "V"},
{PICO_X1_PROBE_20MV,		"20mV",									 -0.02, 0.02, "V"},
{PICO_X1_PROBE_50MV,		"50mV",									 -0.05, 0.05, "V"},
{PICO_X1_PROBE_100MV,		"100mV",								 -0.1,	0.1,  "V"},
{PICO_X1_PROBE_200MV,		"200mV",								 -0.2,  0.2,  "V"},
{PICO_X1_PROBE_500MV,		"500mV",								 -0.5,  0.5,  "V"},
{PICO_X1_PROBE_1V,			"1V",									 -1,    1,    "V"},
{PICO_X1_PROBE_2V,			"2V",									 -2,    2,    "V"},
{PICO_X1_PROBE_5V,			"5V",									 -5,    5,    "V"},
{PICO_X1_PROBE_10V,			"10V",									 -10,   10,   "V"},
{PICO_X1_PROBE_50V,			"50V",									 -50,   50,   "V"},
// x10
{PICO_X10_PROBE_100MV,		"x10_v100mV",								 -0.1,	0.1,  "V"},
{PICO_X10_PROBE_200MV,		"x10_200mV",								 -0.2,  0.2,  "V"},
{PICO_X10_PROBE_500MV,		"x10_500mV",								 -0.5,  0.5,  "V"},
{PICO_X10_PROBE_1V,			"x10_1V",									 -1,    1,    "V"},
{PICO_X10_PROBE_2V,			"x10_2V",									 -2,    2,    "V"},
{PICO_X10_PROBE_5V,			"x10_5V",									 -5,    5,    "V"},
{PICO_X10_PROBE_10V,		"x10_10V",									 -10,   10,   "V"},
{PICO_X10_PROBE_50V,		"x10_50V",									 -50,   50,   "V"},
{PICO_X10_PROBE_100V,		"x10_100V",									 -100,   100,   "V"},
{PICO_X10_PROBE_200V,		"x10_200V",									 -200,   200,   "V"},
{PICO_X10_PROBE_500V,		"x10_500V",									 -500,   500,   "V"},
// D9_BNC
{PICO_D9_BNC_10MV,		"D9_BNC_10mV",									-0.01,	0.01,  "V"},
{PICO_D9_BNC_20MV,		"D9_BNC_20mV",									-0.02,  0.02,  "V"},
{PICO_D9_BNC_50MV,		"D9_BNC_50mV",									-0.05,  0.05,  "V"},
{PICO_D9_BNC_100MV,		"D9_BNC_100mV",									 -0.1,    0.1,    "V"},
{PICO_D9_BNC_200MV,		"D9_BNC_200mV",									 -0.2,    0.2,    "V"},
{PICO_D9_BNC_500MV,		"D9_BNC_500mV",									 -0.5,    0.5,    "V"},
{PICO_D9_BNC_1V,			"D9_BNC_1V",									-1,   1,   "V"},
{PICO_D9_BNC_2V,			"D9_BNC_2V",									 -2,   2,   "V"},
{PICO_D9_BNC_5V,			"D9_BNC_5V",									 -5,   5,   "V"},
{PICO_D9_BNC_10V,		"D9_BNC_10V",									 -10,   10,   "V"},
{PICO_D9_BNC_20V,		"D9_BNC_20V",									 -20,   20,   "V"},
{PICO_D9_BNC_50V,		"D9_BNC_50V",									 -50,   50,   "V"},
// D9_2X_BNC
{PICO_D9_2X_BNC_10MV,		"D9_2X_BNC_10mV",								-0.01,	0.01,  "V"},
{PICO_D9_2X_BNC_20MV,		"D9_2X_BNC_20mV",								-0.02,  0.02,  "V"},
{PICO_D9_2X_BNC_50MV,		"D9_2X_BNC_50mV",								-0.05,  0.05,  "V"},
{PICO_D9_2X_BNC_100MV,		"D9_2X_BNC_100mV",								 -0.1,  0.1,    "V"},
{PICO_D9_2X_BNC_200MV,		"D9_2X_BNC_200mV",								 -0.2,  0.2,    "V"},
{PICO_D9_2X_BNC_500MV,		"D9_2X_BNC_500mV",								 -0.5,  0.5,    "V"},
{PICO_D9_2X_BNC_1V,			"D9_2X_BNC_1V",									 -1,	1,   "V"},
{PICO_D9_2X_BNC_2V,			"D9_2X_BNC_2V",									 -2,	2,   "V"},
{PICO_D9_2X_BNC_5V,			"D9_2X_BNC_5V",									 -5,	5,   "V"},
{PICO_D9_2X_BNC_10V,		"D9_2X_BNC_10V",								 -10,   10,   "V"},
{PICO_D9_2X_BNC_20V,		"D9_2X_BNC_20V",								 -20,   20,   "V"},
{PICO_D9_2X_BNC_50V,		"D9_2X_BNC_50V",								 -50,   50,   "V"},
// DIFFERENTIAL
{PICO_DIFFERENTIAL_10MV,		"DIFFERENTIAL_10mV",									-0.01,	0.01,  "V"},
{PICO_DIFFERENTIAL_20MV,		"DIFFERENTIAL_20mV",									-0.02,	0.02,  "V"},
{PICO_DIFFERENTIAL_50MV,		"DIFFERENTIAL_50mV",									-0.05,	0.05,  "V"},
{PICO_DIFFERENTIAL_100MV,		"DIFFERENTIAL_100mV",									-0.1,	0.1,  "V"},
{PICO_DIFFERENTIAL_200MV,		"DIFFERENTIAL_200mV",									-0.2,	0.2,  "V"},
{PICO_DIFFERENTIAL_500MV,		"DIFFERENTIAL_500mV",									-0.5,	0.5,  "V"},
{PICO_DIFFERENTIAL_1V,			"DIFFERENTIAL_1V",										-1,		1,  "V"},
{PICO_DIFFERENTIAL_2V,			"DIFFERENTIAL_2V",										-2,		2,  "V"},
{PICO_DIFFERENTIAL_5V,			"DIFFERENTIAL_5V",										-5,		5,  "V"},
{PICO_DIFFERENTIAL_10V,			"DIFFERENTIAL_10V",										-10,	10,  "V"},
{PICO_DIFFERENTIAL_20V,			"DIFFERENTIAL_20V",										-20,	20,  "V"},
// Resistance Probe
// 
// PICO_CURRENT_CLAMP_200A
{PICO_CURRENT_CLAMP_200A_2kA_1A,		"PICO_CURRENT_CLAMP_200A_2kA_1A",		 -1,		1,		"A"},
{PICO_CURRENT_CLAMP_200A_2kA_2A,		"PICO_CURRENT_CLAMP_200A_2kA_2A",		 -2,		2,		"A"},
{PICO_CURRENT_CLAMP_200A_2kA_5A,		"PICO_CURRENT_CLAMP_200A_2kA_5A",		 -5,		5,		"A"},
{PICO_CURRENT_CLAMP_200A_2kA_10A,		"PICO_CURRENT_CLAMP_200A_2kA_10A",		 -10,		10,		"A"},
{PICO_CURRENT_CLAMP_200A_2kA_20A,		"PICO_CURRENT_CLAMP_200A_2kA_20A",		 -20,		20,		"A"},
{PICO_CURRENT_CLAMP_200A_2kA_50A,		"PICO_CURRENT_CLAMP_200A_2kA_50A",		 -50,		50,		"A"},
{PICO_CURRENT_CLAMP_200A_2kA_100A,		"PICO_CURRENT_CLAMP_200A_2kA_100A",		 -100,		100,	"A"},
{PICO_CURRENT_CLAMP_200A_2kA_200A,		"PICO_CURRENT_CLAMP_200A_2kA_200A",		 -200,		200,	"A"},
{PICO_CURRENT_CLAMP_200A_2kA_500A,		"PICO_CURRENT_CLAMP_200A_2kA_500A",		 -500,		500,	"A"},
{PICO_CURRENT_CLAMP_200A_2kA_1000A,		"PICO_CURRENT_CLAMP_200A_2kA_1000A",	 -1000,		1000,   "A"},
{PICO_CURRENT_CLAMP_200A_2kA_2000A,		"PICO_CURRENT_CLAMP_200A_2kA_2000A",	 -2000,		2000,   "A"},
// CURRENT_CLAMP_40A
{PICO_CURRENT_CLAMP_40A_100mA,         "PICO_CURRENT_CLAMP_40A_100mA",			 -0.1, 0.1,  "A"},
{PICO_CURRENT_CLAMP_40A_200mA,         "PICO_CURRENT_CLAMP_40A_200mA",			 -0.2, 0.2,  "A"},
{PICO_CURRENT_CLAMP_40A_500mA,         "PICO_CURRENT_CLAMP_40A_500mA",			 -0.5, 0.5,  "A"},
{PICO_CURRENT_CLAMP_40A_1A,		       "PICO_CURRENT_CLAMP_40A_1A",				 -1,   1,    "A"},
{PICO_CURRENT_CLAMP_40A_2A,		       "PICO_CURRENT_CLAMP_40A_2A",				 -2,   2,    "A"},
{PICO_CURRENT_CLAMP_40A_5A,		       "PICO_CURRENT_CLAMP_40A_5A",				 -5,   5,    "A"},
{PICO_CURRENT_CLAMP_40A_10A,		       "PICO_CURRENT_CLAMP_40A_10A",		 -10,  10,   "A"},
{PICO_CURRENT_CLAMP_40A_20A,		       "PICO_CURRENT_CLAMP_40A_20A",		 -20,  20,   "A"},
{PICO_CURRENT_CLAMP_40A_40A,		       "PICO_CURRENT_CLAMP_40A_40A",		 -40,  40,   "A"},
// 1kV CAT III probe
{PICO_1KV_2_5V,				"1KV_2.5V",								-2.5,   2.5,	"V"},
{PICO_1KV_5V,				"1KV_5V",								-5,		 5,		"V"},
{PICO_1KV_12_5V,			"1KV_12.5V",							-12.5,   12.5,	"V"},
{PICO_1KV_25V,				"1KV_25V",								-25,	  25,	"V"},
{PICO_1KV_50V,				"1KV_50V",								-50,	  50,	"V"},
{PICO_1KV_125V,				"1KV_125V",								-125,	 125,	"V"},
{PICO_1KV_500V,				"1KV_500V",								-500,	 500,	"V"},
{PICO_1KV_1000V,			"1KV_1000V",							-1000,   1000,	 "V"},
// CURRENT_CLAMP_2000ARMS
{PICO_CURRENT_CLAMP_2000ARMS_10A,			"CURRENT_CLAMP_2000ARMS_10A",		-10,	10,   "A"},
{PICO_CURRENT_CLAMP_2000ARMS_20A,			"CURRENT_CLAMP_2000ARMS_20A",		-20,	20,   "A"},
{PICO_CURRENT_CLAMP_2000ARMS_50A,			"CURRENT_CLAMP_2000ARMS_50A",		-50,	50,   "A"},
{PICO_CURRENT_CLAMP_2000ARMS_100A,			"CURRENT_CLAMP_2000ARMS_100A",		-100,	100,   "A"},
{PICO_CURRENT_CLAMP_2000ARMS_200A,			"CURRENT_CLAMP_2000ARMS_200A",		-200,	200,   "A"},
{ PICO_CURRENT_CLAMP_2000ARMS_500A,			"CURRENT_CLAMP_2000ARMS_500A",		-500,	500,   "A" },
{ PICO_CURRENT_CLAMP_2000ARMS_1000A,		"CURRENT_CLAMP_2000ARMS_1000A",		-1000,  1000,   "A" },
{ PICO_CURRENT_CLAMP_2000ARMS_2000A,		"CURRENT_CLAMP_2000ARMS_2000A",		-2000,  2000,   "A" },
{ PICO_CURRENT_CLAMP_2000ARMS_5000A,		"CURRENT_CLAMP_2000ARMS_5000A",		-5000,  5000,   "A" },
// CURRENT_CLAMP_100A
{ PICO_CURRENT_CLAMP_100A_2_5A,			"CURRENT_CLAMP_100A_2_5A",		-2.5,	2.5,   "A" },
{ PICO_CURRENT_CLAMP_100A_5A,			"CURRENT_CLAMP_100A_5A",		-5,		5,   "A" },
{ PICO_CURRENT_CLAMP_100A_10A,			"CURRENT_CLAMP_100A_10A",		-10,	10,   "A" },
{ PICO_CURRENT_CLAMP_100A_25A,			"CURRENT_CLAMP_100A_25A",		-25,	25,   "A" },
{ PICO_CURRENT_CLAMP_100A_50A,			"CURRENT_CLAMP_100A_50A",		-50,	50,   "A" },
{ PICO_CURRENT_CLAMP_100A_100A,			"CURRENT_CLAMP_100A_100A",		-100,	100,   "A" },
// CURRENT_CLAMP_60A
{ PICO_CURRENT_CLAMP_60A_2A,			"CURRENT_CLAMP_60A_2A",			-2,		2,		"A" },
{ PICO_CURRENT_CLAMP_60A_5A,			"CURRENT_CLAMP_60A_5A",			-5,		5,		"A" },
{ PICO_CURRENT_CLAMP_60A_10A,			"CURRENT_CLAMP_60A_10A",		-10,	10,		"A" },
{ PICO_CURRENT_CLAMP_60A_20A,			"CURRENT_CLAMP_60A_20A",		-20,	20,		"A" },
{ PICO_CURRENT_CLAMP_60A_50A,			"CURRENT_CLAMP_60A_50A",		-50,	50,		"A" },
{ PICO_CURRENT_CLAMP_60A_60A,			"CURRENT_CLAMP_60A_60A",		-60,	60,		"A" },
// CURRENT_CLAMP_60A_V2
{ PICO_CURRENT_CLAMP_60A_V2_0_5A,		"CURRENT_CLAMP_60A_V2_0_5A",	-0.5,	0.5,	"A" },
{ PICO_CURRENT_CLAMP_60A_V2_1A,			"CURRENT_CLAMP_60A_V2_2A",		-1,		1,		"A" },
{ PICO_CURRENT_CLAMP_60A_V2_2A,			"CURRENT_CLAMP_60A_V2_5A",		-2,		2,		"A" },
{ PICO_CURRENT_CLAMP_60A_V2_5A,			"CURRENT_CLAMP_60A_V2_10A",		-5,		5,		"A" },
{ PICO_CURRENT_CLAMP_60A_V2_10A,		"CURRENT_CLAMP_60A_V2_20A",		-10,	10,		"A" },
{ PICO_CURRENT_CLAMP_60A_V2_20A,		"CURRENT_CLAMP_60A_V2_50A",		-20,	20,		"A" },
{ PICO_CURRENT_CLAMP_60A_V2_50A,		"CURRENT_CLAMP_60A_V2_100A",	-50,	50,		"A" },
{ PICO_CURRENT_CLAMP_60A_V2_60A,		"CURRENT_CLAMP_60A_V2_200A",	-60,	60,		"A" },
// X10_ACTIVE_PROBE
{PICO_X10_ACTIVE_PROBE_100MV,	"X10_ACTIVE_PROBE_100MV",						 -0.1,	0.1,  "V"},
{PICO_X10_ACTIVE_PROBE_200MV,	"X10_ACTIVE_PROBE_200MV",						 -0.2,  0.2,  "V"},
{PICO_X10_ACTIVE_PROBE_500MV,	"X10_ACTIVE_PROBE_500MV",						 -0.5,  0.5,  "V"},
{PICO_X10_ACTIVE_PROBE_1V,		"X10_ACTIVE_PROBE_1V",							 -1,    1,    "V"},
{PICO_X10_ACTIVE_PROBE_2V,		"X10_ACTIVE_PROBE_2V",							 -2,    2,    "V"},
{PICO_X10_ACTIVE_PROBE_5V,		"X10_ACTIVE_PROBE_5V",							 -5,    5,    "V"},
//Probe Off
{PICO_CONNECT_PROBE_OFF,			"PicoConnect: Probe Disabled",				-1,   1,   "NA"}
 };

// Function prototypes
BOOL getRangeScaling(PICO_CONNECT_PROBE_RANGE ChannelRange, PICO_PROBE_SCALING* ChannelRangeInfo);

double adc_to_mv(int16_t raw, PICO_CONNECT_PROBE_RANGE ChannelRange, int16_t maxADCValue);
int16_t mv_to_adc(double scaled, PICO_CONNECT_PROBE_RANGE ChannelRange, int16_t maxADCValue);

double adc_to_scaled_value(int16_t raw, PICO_PROBE_SCALING ChannelRangeInfo, int16_t maxADCValue);

#endif