/******************************************************************************
 *
 * Filename: ps2000aCon.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to perform 
 *	 operations using a PicoScope 2200 Series device using the 
 *	 PicoScope 2000 Series (ps2000a) driver functions.
 *   
 *	Supported PicoScope models:
 *
 *		PicoScope 2205 MSO & 2205A MSO
 *		PicoScope 2405A
 *		PicoScope 2206, 2206A, 2206B, 2206B MSO & 2406B
 *		PicoScope 2207, 2207A, 2207B, 2207B MSO & 2407B
 *		PicoScope 2208, 2208A, 2208B, 2208B MSO & 2408B
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *	  Collect a block of samples using Equivalent Time Sampling (ETS)
 *    Collect samples using a rapid block capture with trigger
 *    Collect a stream of data immediately
 *    Collect a stream of data when a trigger event occurs
 *    Set Signal Generator, using standard or custom signals
 * 
 * Digital Examples (MSO veriants only): 
 *    Collect a block of digital samples immediately
 *    Collect a block of digital samples when a trigger event occurs
 *    Collect a block of analogue & digital samples when analogue AND digital trigger events occurs
 *    Collect a block of analogue & digital samples when analogue OR digital trigger events occurs
 *    Collect a stream of digital data immediately
 *	  Collect a stream of digital data and show aggregated values
 *    
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps2000a.lib can be located
 *			Ensure that the ps2000aApi.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps2000a.lib to the project (Microsoft C only)
 *			 Add ps2000aApi.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps2000a driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the ps2000acon application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2011-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "ps2000aApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <libps2000a-1.1/ps2000aApi.h>
#ifndef PICO_STATUS
#include <libps2000a-1.1/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL
{
	FALSE, TRUE
} BOOL;

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
	do
	{
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

int32_t fopen_s(FILE ** a, const char * b, const char * c)
{
	FILE * fp = fopen(b, c);
	*a = fp;
	return (fp > 0) ? 0 : -1;
}

int32_t strcmpi(const char * a, const char * b)
{
	return strcmp(a, b);
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

#define PREF4 __stdcall

#define		BUFFER_SIZE 	1024
#define		DUAL_SCOPE		2
#define		QUAD_SCOPE		4

#define		AWG_DAC_FREQUENCY      20e6
#define		AWG_DAC_FREQUENCY_MSO  2e6
#define		AWG_PHASE_ACCUMULATOR  4294967296.0

int32_t cycles = 0;

typedef enum
{
	ANALOGUE,
	DIGITAL,
	AGGREGATED,
	MIXED
}MODE;

typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
}CHANNEL_SETTINGS;

typedef struct tTriggerDirections
{
	PS2000A_THRESHOLD_DIRECTION channelA;
	PS2000A_THRESHOLD_DIRECTION channelB;
	PS2000A_THRESHOLD_DIRECTION channelC;
	PS2000A_THRESHOLD_DIRECTION channelD;
	PS2000A_THRESHOLD_DIRECTION ext;
	PS2000A_THRESHOLD_DIRECTION aux;
}TRIGGER_DIRECTIONS;

typedef struct tPwq
{
	PS2000A_PWQ_CONDITIONS * conditions;
	int16_t nConditions;
	PS2000A_THRESHOLD_DIRECTION direction;
	uint32_t lower;
	uint32_t upper;
	PS2000A_PULSE_WIDTH_TYPE type;
}PWQ;

typedef struct
{
	int16_t					handle;
	PS2000A_RANGE			firstRange;
	PS2000A_RANGE			lastRange;
	uint8_t					signalGenerator;
	uint8_t					ETS;
	int16_t                 channelCount;
	int16_t					maxValue;
	CHANNEL_SETTINGS		channelSettings [PS2000A_MAX_CHANNELS];
	int16_t					digitalPorts;
	int16_t					awgBufferSize;
	double					awgDACFrequency;
}UNIT;

// Global Variables
uint32_t	timebase = 8;
int16_t     oversample = 1;
BOOL		scaleVoltages = TRUE;

uint16_t inputRanges [PS2000A_MAX_RANGES] = {	10,
	20,
	50,
	100,
	200,
	500,
	1000,
	2000,
	5000,
	10000,
	20000,
	50000};

BOOL     		g_ready = FALSE;
int32_t 		g_times [PS2000A_MAX_CHANNELS];
int16_t     	g_timeUnit;
int32_t      	g_sampleCount;
uint32_t		g_startIndex;
int16_t			g_autoStopped;
int16_t			g_trig = 0;
uint32_t		g_trigAt = 0;
int16_t			g_overflow = 0;

char BlockFile[20]		= "block.txt";
char DigiBlockFile[20]	= "digiblock.txt";
char StreamFile[20]		= "stream.txt";

// Use this struct to help with streaming data collection
typedef struct tBufferInfo
{
	UNIT * unit;
	MODE mode;
	int16_t **driverBuffers;
	int16_t **appBuffers;
	int16_t **driverDigBuffers;
	int16_t **appDigBuffers;

} BUFFER_INFO;


/****************************************************************************
* Callback
* used by ps2000a data streaming collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 CallBackStreaming(	int16_t handle,
	int32_t noOfSamples,
	uint32_t	startIndex,
	int16_t overflow,
	uint32_t triggerAt,
	int16_t triggered,
	int16_t autoStop,
	void	*pParameter)
{
	int32_t channel;
	int32_t digiPort;
	BUFFER_INFO * bufferInfo = NULL;

	if (pParameter != NULL)
	{
		bufferInfo = (BUFFER_INFO *) pParameter;
	}

	// used for streaming
	g_sampleCount	= noOfSamples;
	g_startIndex	= startIndex;
	g_autoStopped	= autoStop;
	g_overflow		= overflow;

	// flag to say done reading data
	g_ready = TRUE;

	// flags to show if & where a trigger has occurred
	g_trig = triggered;
	g_trigAt = triggerAt;

	if (bufferInfo != NULL && noOfSamples)
	{
		if (bufferInfo->mode == ANALOGUE)
		{
			for (channel = 0; channel < bufferInfo->unit->channelCount; channel++)
			{
				if (bufferInfo->unit->channelSettings[channel].enabled)
				{
					if (bufferInfo->appBuffers && bufferInfo->driverBuffers)
					{
						if (bufferInfo->appBuffers[channel * 2]  && bufferInfo->driverBuffers[channel * 2])
						{
							memcpy_s (&bufferInfo->appBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
								&bufferInfo->driverBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
						}
						if (bufferInfo->appBuffers[channel * 2 + 1] && bufferInfo->driverBuffers[channel * 2 + 1])
						{
							memcpy_s (&bufferInfo->appBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
								&bufferInfo->driverBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
						}
					}
				}
			}
		}
		else if (bufferInfo->mode == AGGREGATED)
		{
			for (channel = 0; channel < bufferInfo->unit->digitalPorts; channel++)
			{
				if (bufferInfo->appDigBuffers && bufferInfo->driverDigBuffers)
				{
					if (bufferInfo->appDigBuffers[channel * 2] && bufferInfo->driverDigBuffers[channel * 2])
					{
						memcpy_s (&bufferInfo->appDigBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverDigBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
					}
					if (bufferInfo->appDigBuffers[channel * 2 + 1] && bufferInfo->driverDigBuffers[channel * 2 + 1])
					{
						memcpy_s (&bufferInfo->appDigBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverDigBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
					}
				}
			}
		}
		else if (bufferInfo->mode == DIGITAL)
		{
			for (digiPort = 0; digiPort < bufferInfo->unit->digitalPorts; digiPort++)
			{
				if (bufferInfo->appDigBuffers && bufferInfo->driverDigBuffers)
				{
					if (bufferInfo->appDigBuffers[digiPort] && bufferInfo->driverDigBuffers[digiPort])
					{
						memcpy_s (&bufferInfo->appDigBuffers[digiPort][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverDigBuffers[digiPort][startIndex], noOfSamples * sizeof(int16_t));
					}
				}
			}
		}
	}
}

/****************************************************************************
* Callback
* used by ps2000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 CallBackBlock(	int16_t handle, PICO_STATUS status, void * pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
	}
}

/****************************************************************************
* CloseDevice 
****************************************************************************/
void CloseDevice(UNIT *unit)
{
	ps2000aCloseUnit(unit->handle);
}


/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void SetDefaults(UNIT * unit)
{
	PICO_STATUS status;
	int32_t i;

	status = ps2000aSetEts(unit->handle, PS2000A_ETS_OFF, 0, 0, NULL); // Turn off ETS

	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		status = ps2000aSetChannel(unit->handle, (PS2000A_CHANNEL) (PS2000A_CHANNEL_A + i),
			unit->channelSettings[PS2000A_CHANNEL_A + i].enabled,
			(PS2000A_COUPLING) unit->channelSettings[PS2000A_CHANNEL_A + i].DCcoupled,
			(PS2000A_RANGE) unit->channelSettings[PS2000A_CHANNEL_A + i].range, 0);
	}
}

/****************************************************************************
* SetDigitals - enable or disable Digital Channels
****************************************************************************/
PICO_STATUS SetDigitals(UNIT *unit, int16_t state)
{
	PICO_STATUS status;

	int16_t logicLevel;
	float logicVoltage = 1.5;
	int16_t maxLogicVoltage = 5;

	int16_t timebase = 1;
	int16_t port;


	// Set logic threshold
	logicLevel =  (int16_t) ((logicVoltage / maxLogicVoltage) * PS2000A_MAX_LOGIC_LEVEL);

	// Enable or Disable Digital ports
	for (port = PS2000A_DIGITAL_PORT0; port <= PS2000A_DIGITAL_PORT1; port++)
	{
		status = ps2000aSetDigitalPort(unit->handle, (PS2000A_DIGITAL_PORT)port, state, logicLevel);
		printf(status?"SetDigitals:ps2000aSetDigitalPort(Port 0x%X) ------ 0x%08lx \n":"", port, status);
	}
	return status;
}

/****************************************************************************
* DisableAnalogue - Disable Analogue Channels
****************************************************************************/
PICO_STATUS DisableAnalogue(UNIT *unit)
{
	PICO_STATUS status;
	int16_t ch;

	// Turn off analogue channels retaining settings
	for (ch = 0; ch < unit->channelCount; ch++)
	{

		status = ps2000aSetChannel(unit->handle, (PS2000A_CHANNEL) ch, 0, (PS2000A_COUPLING) unit->channelSettings[ch].DCcoupled, 
			(PS2000A_RANGE) unit->channelSettings[ch].range, 0);

		if (status != PICO_OK)
		{
			printf("DisableAnalogue:ps2000aSetChannel(channel %d) ------ 0x%08lx \n", ch, status);
		}
	}
	return status;
}

/****************************************************************************
* RestoreAnalogueSettings - Restores Analogue Channel settings
****************************************************************************/
PICO_STATUS RestoreAnalogueSettings(UNIT *unit)
{
	PICO_STATUS status;
	int16_t ch;

	// Turn on analogue channels using previous settings
	for (ch = 0; ch < unit->channelCount; ch++)
	{

		status = ps2000aSetChannel(unit->handle, (PS2000A_CHANNEL) ch, unit->channelSettings[ch].enabled, (PS2000A_COUPLING) unit->channelSettings[ch].DCcoupled, 
			(PS2000A_RANGE) unit->channelSettings[ch].range, 0);

		if (status != PICO_OK)
		{
			printf("RestoreAnalogueSettings:ps2000aSetChannel(channel %d) ------ 0x%08lx \n", ch, status);
		}
	}
	return status;
}

/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t ch, UNIT * unit)
{
	return (raw * inputRanges[ch]) / unit->maxValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t ch, UNIT * unit)
{
	return (mv * unit->maxValue) / inputRanges[ch];
}

/****************************************************************************
* timeUnitsToString
*
* Converts PS2000A_TIME_UNITS enumeration to string (used for streaming mode)
*
****************************************************************************/
int8_t* timeUnitsToString(PS2000A_TIME_UNITS timeUnits)
{
	int8_t* timeUnitsStr = (int8_t*) "ns";

	switch(timeUnits)
	{
		case PS2000A_FS:

			timeUnitsStr = (int8_t*) "fs";
			break;

		case PS2000A_PS:

			timeUnitsStr = (int8_t*) "ps";
			break;

		case PS2000A_NS:

			timeUnitsStr = (int8_t*) "ns";
			break;

		case PS2000A_US:

			timeUnitsStr = (int8_t*) "us";
			break;

		case PS2000A_MS:

			timeUnitsStr = (int8_t*) "ms";
			break;

		case PS2000A_S:

			timeUnitsStr = (int8_t*) "s";
			break;

		default:

			timeUnitsStr = (int8_t*) "ns";
	}

	return timeUnitsStr;

}

/****************************************************************************
* ClearDataBuffers
*
* stops GetData writing values to memory that has been released
****************************************************************************/
PICO_STATUS ClearDataBuffers(UNIT * unit)
{
	int32_t i;
	PICO_STATUS status;

	for (i = 0; i < unit->channelCount; i++) 
	{
		if ((status = ps2000aSetDataBuffers(unit->handle, (int16_t)i, NULL, NULL, 0, 0, PS2000A_RATIO_MODE_NONE)) != PICO_OK)
		{
			printf("ClearDataBuffers:ps2000aSetDataBuffers(channel %d) ------ 0x%08lx \n", i, status);
		}
	}


	for (i= 0; i < unit->digitalPorts; i++) 
	{
		if ((status = ps2000aSetDataBuffer(unit->handle, (PS2000A_CHANNEL) (i + PS2000A_DIGITAL_PORT0), NULL, 0, 0, PS2000A_RATIO_MODE_NONE))!= PICO_OK)
		{
			printf("ClearDataBuffers:ps2000aSetDataBuffer(port 0x%X) ------ 0x%08lx \n", i + PS2000A_DIGITAL_PORT0, status);
		}
	}

	return status;
}

/****************************************************************************
* BlockDataHandler
* - Used by all block data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all to data.txt
* Input :
* - unit : the unit to use.
* - text : the text to display before the display of data slice
* - offset : the offset into the data buffer to start the display's slice.
****************************************************************************/
void BlockDataHandler(UNIT * unit, char * text, int32_t offset, MODE mode, int16_t etsModeSet)
{
	uint16_t bit;
	uint16_t bitValue;
	uint16_t digiValue;
	uint32_t segmentIndex = 0;

	int32_t i, j;
	int32_t timeInterval;
	int32_t sampleCount = BUFFER_SIZE;
	int32_t maxSamples;
	int32_t timeIndisposed;

	int16_t * buffers[PS2000A_MAX_CHANNEL_BUFFERS];
	int16_t * digiBuffer[PS2000A_MAX_DIGITAL_PORTS];

	int64_t * etsTime; // Buffer for ETS time data

	FILE * fp = NULL;
	FILE * digiFp = NULL;
	
	PICO_STATUS status;
	PS2000A_RATIO_MODE ratioMode = PS2000A_RATIO_MODE_NONE;
	
	if (mode == ANALOGUE || mode == MIXED)		// Analogue or (MSO Only) MIXED 
	{
		for (i = 0; i < unit->channelCount; i++) 
		{
			if (unit->channelSettings[i].enabled)
			{
				buffers[i * 2] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
				buffers[i * 2 + 1] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
				
				status = ps2000aSetDataBuffers(unit->handle, (int32_t) i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, segmentIndex, ratioMode);

				printf(status?"BlockDataHandler:ps2000aSetDataBuffers(channel %d) ------ 0x%08lx \n":"", i, status);
			}
		}
	}

	// Set up ETS time buffers if ETS Block mode data is being captured (only when Analogue channels are enabled)
	if (mode == ANALOGUE && etsModeSet == TRUE)
	{
		etsTime = (int64_t *) calloc(sampleCount, sizeof (int64_t));   
		status = ps2000aSetEtsTimeBuffer(unit->handle, etsTime, sampleCount);
	}

	if (mode == DIGITAL || mode == MIXED)		// (MSO Only) Digital or MIXED
	{
		for (i= 0; i < unit->digitalPorts; i++) 
		{
			digiBuffer[i] = (int16_t*) malloc(sampleCount* sizeof(int16_t));
			status = ps2000aSetDataBuffer(unit->handle, (int32_t) (i + PS2000A_DIGITAL_PORT0), digiBuffer[i], sampleCount, 0, ratioMode);
			printf(status?"BlockDataHandler:ps2000aSetDataBuffer(port 0x%X) ------ 0x%08lx \n":"", i + PS2000A_DIGITAL_PORT0, status);
		}
	}

	/*  Validate the current timebase index, and find the maximum number of samples and the time interval (in nanoseconds)*/
	while (ps2000aGetTimebase(unit->handle, timebase, sampleCount, &timeInterval, oversample, &maxSamples, 0) != PICO_OK)
	{
		timebase++;
	}

	if (!etsModeSet)
	{
		printf("\nTimebase: %lu  SampleInterval: %ldnS  oversample: %hd\n", timebase, timeInterval, oversample);
	}

	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;
	status = ps2000aRunBlock(unit->handle, 0, sampleCount, timebase, oversample,	&timeIndisposed, 0, CallBackBlock, NULL);
	printf(status?"BlockDataHandler:ps2000aRunBlock ------ 0x%08lx \n":"", status);

	printf("Waiting for trigger...Press a key to abort\n");

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (g_ready) 
	{
		status = ps2000aGetValues(unit->handle, 0, (uint32_t*) &sampleCount, 10, ratioMode, 0, NULL);
		printf(status?"BlockDataHandler:ps2000aGetValues ------ 0x%08lx \n":"", status);

		/* Print out the first 10 readings, converting the readings to mV if required */
		printf("%s\n",text);

		if (mode == ANALOGUE || mode == MIXED)		// if we're doing analogue or MIXED
		{
			printf("Channels are in (%s)\n\n", ( scaleVoltages ) ? ("mV") : ("ADC Counts"));

			for (j = 0; j < unit->channelCount; j++) 
			{
				if (unit->channelSettings[j].enabled) 
				{
					printf("Channel%c:\t", 'A' + j);
				}
			}

			printf("\n");
		}

		if (mode == DIGITAL || mode == MIXED)	// if we're doing digital or MIXED
		{
			printf("Digital\n");
		}

		printf("\n");

		for (i = offset; i < offset+10; i++) 
		{
			if (mode == ANALOGUE || mode == MIXED)	// if we're doing analogue or MIXED
			{
				for (j = 0; j < unit->channelCount; j++) 
				{
					if (unit->channelSettings[j].enabled) 
					{
						printf("  %6d        ", scaleVoltages ? 
							adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS2000A_CHANNEL_A + j].range, unit)	// If scaleVoltages, print mV value
							: buffers[j * 2][i]);																	// else print ADC Count
					}
				}
			}

			if (mode == DIGITAL || mode == MIXED)	// if we're doing digital or MIXED
			{
				digiValue = 0x00ff & digiBuffer[1][i];
				digiValue <<= 8;
				digiValue |= digiBuffer[0][i];
				printf("0x%04X", digiValue);
			}
			printf("\n");
		}

		if (mode == ANALOGUE || mode == MIXED)		// if we're doing analogue or MIXED
		{
			sampleCount = min(sampleCount, BUFFER_SIZE);

			fopen_s(&fp, BlockFile, "w");
			
			if (fp != NULL)
			{
				if (etsModeSet)
				{
					fprintf(fp, "ETS Block Data log\n\n");
				}
				else
				{
					fprintf(fp, "Block Data log\n\n");
				}

				fprintf(fp, "Results shown for each of the %d Channels are......\n",unit->channelCount);
				fprintf(fp, "Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

				if (etsModeSet)
				{
					fprintf(fp, "Time (fs) ");
				}
				else
				{
					fprintf(fp, "Time (ns)  ");
				}

				for (i = 0; i < unit->channelCount; i++) 
				{
					fprintf(fp," Ch   Max ADC  Max mV   Min ADC  Min mV  ");
				}

				fprintf(fp, "\n");

				for (i = 0; i < sampleCount; i++) 
				{
					if (mode == ANALOGUE && etsModeSet == TRUE)
					{
						fprintf(fp, "%d ", etsTime[i]);
					}
					else
					{
						fprintf(fp, "%7d ", g_times[0] + (int32_t)(i * timeInterval));
					}

					for (j = 0; j < unit->channelCount; j++) 
					{
						if (unit->channelSettings[j].enabled) 
						{
							fprintf(	fp,
								"Ch%C  %5d = %+5dmV, %5d = %+5dmV   ",
								(char)('A' + j),
								buffers[j * 2][i],
								adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS2000A_CHANNEL_A + j].range, unit),
								buffers[j * 2 + 1][i],
								adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PS2000A_CHANNEL_A + j].range, unit));
						}
					}
					fprintf(fp, "\n");
				}
			}
			else
			{
				printf(	"Cannot open the file block.txt for writing.\n"
					"Please ensure that you have permission to access.\n");
			}
		}

		if (mode == DIGITAL || mode == MIXED)
		{
			fopen_s(&digiFp, DigiBlockFile, "w");

			if (digiFp != NULL)
			{
				fprintf(digiFp, "Block Digital Data log.\n");
				fprintf(digiFp,"Results shown for D15 - D8 and D7 to D0.\n\n");

				for(i = 0; i < sampleCount; i++)
				{
					digiValue = 0x00ff & digiBuffer[1][i];	// Mask Port 1 values to get lower 8 bits
					digiValue <<= 8;						// Shift by 8 bits to place in upper 8 bits of 16-bit word
					digiValue |= digiBuffer[0][i];			// Mask Port 0 values to get lower 8 bits

					for (bit = 0; bit < 16; bit++)
					{
						// Shift value (32768 - binary 1000 0000 0000 0000), AND with value to get 1 or 0 for channel
						// Order will be D15 to D8, then D7 to D0

						bitValue = (0x8000 >> bit) & digiValue? 1 : 0;
						fprintf(digiFp, "%u ", bitValue);
					}

					fprintf(digiFp, "\n");
				}
			}
			else
			{
				printf(	"Cannot open the file digiblock.txt for writing.\n"
					"Please ensure that you have permission to access.\n");
			}
		}

	} 
	else 
	{
		printf("data collection aborted\n");
		_getch();
	}

	status = ps2000aStop(unit->handle);
	printf(status?"BlockDataHandler:ps2000aStop ------ 0x%08lx \n":"", status);

	if (fp != NULL)
	{
		fclose(fp);
	}

	if (digiFp != NULL)
	{
		fclose(digiFp);
	}

	if (mode == ANALOGUE || mode == MIXED)		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->channelCount; i++) 
		{
			if (unit->channelSettings[i].enabled)
			{
				free(buffers[i * 2]);
				free(buffers[i * 2 + 1]);
			}
		}
	}

	if (mode == ANALOGUE && etsModeSet == TRUE)	// Only if we allocated this buffers
	{
		free(etsTime);
	}

	if (mode == DIGITAL || mode == MIXED)		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->digitalPorts; i++) 
		{
			free(digiBuffer[i]);
		}
	}

	ClearDataBuffers(unit);
}

/****************************************************************************
* Stream Data Handler
* - Used by the two stream data examples - untriggered and triggered
* Inputs:
* - unit - the unit to sample on
* - preTrigger - the number of samples in the pre-trigger phase 
*					(0 if no trigger has been set)
***************************************************************************/
void StreamDataHandler(UNIT * unit, uint32_t preTrigger, MODE mode)
{
	int8_t * timeUnitsStr;
	
	int16_t  autostop;
	uint16_t portValue, portValueOR, portValueAND;
	uint32_t segmentIndex = 0;

	int16_t * buffers[PS2000A_MAX_CHANNEL_BUFFERS];
	int16_t * appBuffers[PS2000A_MAX_CHANNEL_BUFFERS];
	int16_t * digiBuffers[PS2000A_MAX_DIGITAL_PORTS];
	int16_t * appDigiBuffers[PS2000A_MAX_DIGITAL_PORTS];
	
	int32_t index = 0;
	int32_t totalSamples;
	int32_t bit;
	int32_t i, j;

	int32_t sampleCount = 40000; /*make sure buffer large enough */
	uint32_t postTrigger;
	uint32_t downsampleRatio = 1;
	uint32_t sampleInterval;
	uint32_t triggeredAt = 0;

	BUFFER_INFO bufferInfo;
	FILE * fp = NULL;

	PICO_STATUS status;
	PS2000A_TIME_UNITS timeUnits;
	PS2000A_RATIO_MODE ratioMode;

	if (mode == ANALOGUE)		// Analogue 
	{
		for (i = 0; i < unit->channelCount; i++) 
		{
			if (unit->channelSettings[i].enabled)
			{
				buffers[i * 2] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
				buffers[i * 2 + 1] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
				status = ps2000aSetDataBuffers(unit->handle, (int32_t)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, segmentIndex, PS2000A_RATIO_MODE_AGGREGATE);

				appBuffers[i * 2] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
				appBuffers[i * 2 + 1] = (int16_t*) malloc(sampleCount * sizeof(int16_t));

				printf(status?"StreamDataHandler:ps2000aSetDataBuffers(channel %ld) ------ 0x%08lx \n":"", i, status);
			}
		}

		downsampleRatio = 20;
		timeUnits = PS2000A_US;
		sampleInterval = 1;
		ratioMode = PS2000A_RATIO_MODE_AGGREGATE;
		postTrigger = 1000000;
		autostop = TRUE;
	}

	bufferInfo.unit = unit;
	bufferInfo.mode = mode;	
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;
	bufferInfo.driverDigBuffers = digiBuffers;
	bufferInfo.appDigBuffers = appDigiBuffers;

	if (mode == AGGREGATED)		// (MSO Only) AGGREGATED
	{
		for (i= 0; i < unit->digitalPorts; i++) 
		{

			digiBuffers[i * 2] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
			digiBuffers[i * 2 + 1] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
			status = ps2000aSetDataBuffers(unit->handle, (PS2000A_CHANNEL) (i + PS2000A_DIGITAL_PORT0), digiBuffers[i * 2], digiBuffers[i * 2 + 1], sampleCount, 0, PS2000A_RATIO_MODE_AGGREGATE);

			appDigiBuffers[i * 2] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
			appDigiBuffers[i * 2 + 1] = (int16_t*) malloc(sampleCount * sizeof(int16_t)); 

			printf(status?"StreamDataHandler:ps2000aSetDataBuffer(channel %ld) ------ 0x%08lx \n":"", i, status);
		}

		downsampleRatio = 10;
		timeUnits = PS2000A_MS;
		sampleInterval = 10;
		ratioMode = PS2000A_RATIO_MODE_AGGREGATE;
		postTrigger = 10;
		autostop = FALSE;
	}

	if (mode == DIGITAL)		// (MSO Only) Digital 
	{
		for (i= 0; i < unit->digitalPorts; i++) 
		{
			digiBuffers[i] = (int16_t*) malloc(sampleCount* sizeof(int16_t));
			status = ps2000aSetDataBuffer(unit->handle, (PS2000A_CHANNEL) (i + PS2000A_DIGITAL_PORT0), digiBuffers[i], sampleCount, 0, PS2000A_RATIO_MODE_NONE);

			appDigiBuffers[i] = (int16_t*) malloc(sampleCount * sizeof(int16_t));

			printf(status?"StreamDataHandler:ps2000aSetDataBuffer(channel %ld) ------ 0x%08lx \n":"", i, status);
		}

		downsampleRatio = 1;
		timeUnits = PS2000A_MS;
		sampleInterval = 10;
		ratioMode = PS2000A_RATIO_MODE_NONE;
		postTrigger = 10;
		autostop = FALSE;
	}

	if (autostop)
	{
		printf("\nStreaming Data for %lu samples", postTrigger / downsampleRatio);

		if (preTrigger)	// we pass 0 for preTrigger if we're not setting up a trigger
		{
			printf(" after the trigger occurs\nNote: %lu Pre Trigger samples before Trigger arms\n\n",preTrigger / downsampleRatio);
		}
		else
		{
			printf("\n\n");
		}
	}
	else
	{
		printf("\nStreaming Data continually\n\n");
	}

	g_autoStopped = FALSE;

	status = ps2000aRunStreaming(unit->handle, &sampleInterval, timeUnits, preTrigger, postTrigger - preTrigger, 
				autostop, downsampleRatio, ratioMode, (uint32_t) sampleCount);

	if (status == PICO_OK)
	{	
		timeUnitsStr = timeUnitsToString(timeUnits);
		printf("Streaming data... (interval: %d %s) Press a key to stop\n", sampleInterval, timeUnitsStr);
	}
	else
	{
		printf("StreamDataHandler:ps2000aRunStreaming ------ 0x%08lx \n", status);
	}

	if (mode == ANALOGUE)
	{
		fopen_s(&fp, StreamFile, "w");

		if (fp != NULL)
		{
			fprintf(fp,"For each of the %d Channels, results shown are....\n",unit->channelCount);
			fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

			for (i = 0; i < unit->channelCount; i++) 
			{
				if (unit->channelSettings[i].enabled) 
				{
					fprintf(fp,"Max ADC   Max mV   Min ADC   Min mV");
				}
			}

			fprintf(fp, "\n");
		}
	}

	totalSamples = 0;

	// Capture data unless a key is pressed or the g_autoStopped flag is set in the streaming callback
	while (!_kbhit() && !g_autoStopped)
	{
		/* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
		g_ready = FALSE;

		status = ps2000aGetStreamingLatestValues(unit->handle, CallBackStreaming, &bufferInfo);
		index ++;

		if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
		{
			if (g_trig)
			{
				triggeredAt = totalSamples + g_trigAt;		// calculate where the trigger occurred in the total samples collected
			}

			totalSamples += g_sampleCount;
			printf("\nCollected %3li samples, index = %5lu, Total: %6d samples ", g_sampleCount, g_startIndex, totalSamples);

			if (g_trig)
			{
				printf("Trig. at index %lu", triggeredAt);	// show where trigger occurred
			}

			for (i = g_startIndex; i < (int32_t)(g_startIndex + g_sampleCount); i++) 
			{
				if (mode == ANALOGUE)
				{
					if (fp != NULL)
					{
						for (j = 0; j < unit->channelCount; j++) 
						{
							if (unit->channelSettings[j].enabled) 
							{
								fprintf(	fp,
									"%d, %d, %d, %d, ",
									appBuffers[j * 2][i],
									adc_to_mv(appBuffers[j * 2][i], unit->channelSettings[PS2000A_CHANNEL_A + j].range, unit),
									appBuffers[j * 2 + 1][i],
									adc_to_mv(appBuffers[j * 2 + 1][i], unit->channelSettings[PS2000A_CHANNEL_A + j].range, unit));
							}
						}

						fprintf(fp, "\n");
					}
					else
					{
						printf("Cannot open the file stream.txt for writing.\n");
					}

				}

				if (mode == DIGITAL)
				{
					portValue = 0x00ff & appDigiBuffers[1][i];	// Mask Port 1 values to get lower 8 bits
					portValue <<= 8;							// Shift by 8 bits to place in upper 8 bits of 16-bit word
					portValue |= 0x00ff & appDigiBuffers[0][i];	// Mask Port 0 values to get lower 8 bits

					printf("\nIndex=%04lu: Value = 0x%04X  =  ", i, portValue);

					for (bit = 0; bit < 16; bit++)
					{
						// Shift value (32768 - binary 1000 0000 0000 0000), AND with value to get 1 or 0 for channel
						// Order will be D15 to D8, then D7 to D0
						printf( (0x8000 >> bit) & portValue? "1 " : "0 ");
					}
				}

				if (mode == AGGREGATED)
				{
					portValueOR = 0x00ff & appDigiBuffers[2][i];
					portValueOR <<= 8;
					portValueOR |= 0x00ff & appDigiBuffers[0][i];

					portValueAND = 0x00ff & appDigiBuffers[3][i];
					portValueAND <<= 8;
					portValueAND |= 0x00ff & appDigiBuffers[1][i];

					printf("\nIndex=%04lu: Bitwise  OR of last %ld readings = 0x%04X ",i,  downsampleRatio, portValueOR);
					printf("\nIndex=%04lu: Bitwise AND of last %ld readings = 0x%04X ",i,  downsampleRatio, portValueAND);
				}
			}
		}
	}

	ps2000aStop(unit->handle);

	if (!g_autoStopped) 
	{
		printf("\nData collection aborted.\n");
		_getch();
	}

	if (g_overflow)
	{
		printf("Overflow on voltage range.\n");
	}

	if (fp != NULL) 
	{
		fclose(fp);	
	}

	if (mode == ANALOGUE)		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->channelCount; i++) 
		{
			if (unit->channelSettings[i].enabled)
			{
				free(buffers[i * 2]);
				free(buffers[i * 2 + 1]);

				free(appBuffers[i * 2]);
				free(appBuffers[i * 2 + 1]);
			}
		}
	}

	if (mode == DIGITAL) 		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->digitalPorts; i++) 
		{
			free(digiBuffers[i]);
			free(appDigiBuffers[i]);
		}

	}

	if (mode == AGGREGATED) 		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->digitalPorts * 2; i++) 
		{
			free(digiBuffers[i]);
			free(appDigiBuffers[i]);
		}
	}

	ClearDataBuffers(unit);
}


/****************************************************************************
* SetTrigger
* 
* Parameters
* - *unit               - pointer to the UNIT structure
* - *channelProperties  - pointer to the PS2000A_TRIGGER_CHANNEL_PROPERTIES structure
* - nChannelProperties  - the number of PS2000A_TRIGGER_CHANNEL_PROPERTIES elements in channelProperties
* - *triggerConditions  - pointer to the PS2000A_TRIGGER_CONDITIONS structure
* - nTriggerConditions  - the number of PS2000A_TRIGGER_CONDITIONS elements in triggerConditions
* - *directions         - pointer to the TRIGGER_DIRECTIONS structure
* - *pwq                - pointer to the pwq (Pulse Width Qualifier) structure
* - delay               - Delay time between trigger & first sample
* - auxOutputEnable     - Not used
* - autoTriggerMs       - timeout period if no trigger occurrs
* - *digitalDirections  - pointer to the PS2000A_DIGITAL_CHANNEL_DIRECTIONS structure
* - nDigitalDirections  - the number of PS2000A_DIGITAL_CHANNEL_DIRECTIONS elements in digitalDirections
*
* Returns			    - PICO_STATUS - to show success or if an error occurred
*					
***************************************************************************/
PICO_STATUS SetTrigger(	UNIT * unit,
	PS2000A_TRIGGER_CHANNEL_PROPERTIES * channelProperties,
	int16_t nChannelProperties,
	PS2000A_TRIGGER_CONDITIONS * triggerConditions,
	int16_t nTriggerConditions,
	TRIGGER_DIRECTIONS * directions,
	PWQ * pwq,
	uint32_t delay,
	int16_t auxOutputEnabled,
	int32_t autoTriggerMs,
	PS2000A_DIGITAL_CHANNEL_DIRECTIONS * digitalDirections,
	int16_t nDigitalDirections)
{
	PICO_STATUS status;

	if ((status = ps2000aSetTriggerChannelProperties(unit->handle,
		channelProperties,
		nChannelProperties,
		auxOutputEnabled,
		autoTriggerMs)) != PICO_OK) 
	{
		printf("SetTrigger:ps2000aSetTriggerChannelProperties ------ Ox%8lx \n", status);
		return status;
	}

	if ((status = ps2000aSetTriggerChannelConditions(unit->handle,	triggerConditions, nTriggerConditions)) != PICO_OK) 
	{
		printf("SetTrigger:ps2000aSetTriggerChannelConditions ------ 0x%8lx \n", status);
		return status;
	}

	if ((status = ps2000aSetTriggerChannelDirections(unit->handle,
		directions->channelA,
		directions->channelB,
		directions->channelC,
		directions->channelD,
		directions->ext,
		directions->aux)) != PICO_OK) 
	{
		printf("SetTrigger:ps2000aSetTriggerChannelDirections ------ 0x%08lx \n", status);
		return status;
	}

	if ((status = ps2000aSetTriggerDelay(unit->handle, delay)) != PICO_OK) 
	{
		printf("SetTrigger:ps2000aSetTriggerDelay ------ 0x%08lx \n", status);
		return status;
	}

	if ((status = ps2000aSetPulseWidthQualifier(unit->handle,
		pwq->conditions,
		pwq->nConditions, 
		pwq->direction,
		pwq->lower, 
		pwq->upper, 
		pwq->type)) != PICO_OK)
	{
		printf("SetTrigger:ps2000aSetPulseWidthQualifier ------ 0x%08lx \n", status);
		return status;
	}

	if (unit->digitalPorts)					// ps2000aSetTriggerDigitalPortProperties function only applies to MSO	
	{
		if ((status = ps2000aSetTriggerDigitalPortProperties(unit->handle,
			digitalDirections, 
			nDigitalDirections)) != PICO_OK) 
		{
			printf("SetTrigger:ps2000aSetTriggerDigitalPortProperties ------ 0x%08lx \n", status);
			return status;
		}
	}
	return status;
}

/****************************************************************************
* CollectBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
void CollectBlockImmediate(UNIT * unit)
{
	PWQ pulseWidth;
	TRIGGER_DIRECTIONS directions;

	memset(&directions, 0, sizeof(TRIGGER_DIRECTIONS));
	memset(&pulseWidth, 0, sizeof(PWQ));

	printf("Collect block immediate\n");
	printf("Data is written to disk file (%s)\n", BlockFile);
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	BlockDataHandler(unit, "\nFirst 10 readings:\n", 0, ANALOGUE, FALSE);
}

/****************************************************************************
* CollectBlockEts
*  this function demonstrates how to collect a block of
*  data using equivalent time sampling (ETS).
****************************************************************************/
void CollectBlockEts(UNIT * unit)
{
	PICO_STATUS status;
	int32_t		ets_sampletime;
	int16_t		triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS2000A_CHANNEL_A].range, unit);
	uint32_t	delay = 0;
	int16_t		etsModeSet = FALSE;

	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	PS2000A_TRIGGER_CHANNEL_PROPERTIES sourceDetails = {	triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS2000A_CHANNEL_A,
		PS2000A_LEVEL};

	PS2000A_TRIGGER_CONDITIONS conditions = {	PS2000A_CONDITION_TRUE,
		PS2000A_CONDITION_DONT_CARE,
		PS2000A_CONDITION_DONT_CARE,
		PS2000A_CONDITION_DONT_CARE,
		PS2000A_CONDITION_DONT_CARE,
		PS2000A_CONDITION_DONT_CARE,
		PS2000A_CONDITION_DONT_CARE,
		PS2000A_CONDITION_DONT_CARE};


	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));
	directions.channelA = PS2000A_RISING;

	printf("Collect ETS block...\n");
	printf("Collects when value rises past %d", scaleVoltages? 
		adc_to_mv(sourceDetails.thresholdUpper,	unit->channelSettings[PS2000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	
	printf(scaleVoltages? "mV\n" : "ADC Counts\n");
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	// Trigger enabled
	// Rising edge
	// Threshold = 1000mV
	status = SetTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, delay, 0, 0, 0, 0);

	status = ps2000aSetEts(unit->handle, PS2000A_ETS_FAST, 20, 4, &ets_sampletime);

	if (status == PICO_OK)
	{
		etsModeSet = TRUE;
	}
	else
	{
		printf("CollectBlockEts:ps2000aSetEts ------ 0x%08lx \n", status);
	}

	printf("ETS Sample Time is: %ld picoseconds\n", ets_sampletime);

	BlockDataHandler(unit, "Ten readings after trigger\n", BUFFER_SIZE / 10 - 5, ANALOGUE, etsModeSet); // 10% of data is pre-trigger

	status = ps2000aSetEts(unit->handle, PS2000A_ETS_OFF, 20, 4, &ets_sampletime);

	etsModeSet = FALSE;
}

/****************************************************************************
* CollectBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void CollectBlockTriggered(UNIT * unit)
{
	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS2000A_CHANNEL_A].range, unit);

	PS2000A_TRIGGER_CHANNEL_PROPERTIES sourceDetails = {	triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS2000A_CHANNEL_A,
		PS2000A_LEVEL};

	PS2000A_TRIGGER_CONDITIONS conditions = {	PS2000A_CONDITION_TRUE,				// Channel A
		PS2000A_CONDITION_DONT_CARE,		// Channel B 
		PS2000A_CONDITION_DONT_CARE,		// Channel C
		PS2000A_CONDITION_DONT_CARE,		// Channel D
		PS2000A_CONDITION_DONT_CARE,		// external
		PS2000A_CONDITION_DONT_CARE,		// aux
		PS2000A_CONDITION_DONT_CARE,		// PWQ
		PS2000A_CONDITION_DONT_CARE};		// digital



	TRIGGER_DIRECTIONS directions = {	PS2000A_RISING,			// Channel A
		PS2000A_NONE,			// Channel B
		PS2000A_NONE,			// Channel C
		PS2000A_NONE,			// Channel D
		PS2000A_NONE,			// ext
		PS2000A_NONE };			// aux

	PWQ pulseWidth;
	memset(&pulseWidth, 0, sizeof(PWQ));

	printf("Collect block triggered\n");
	printf("Data is written to disk file (%s)\n", BlockFile);
	printf("Collects when value rises past %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS2000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");

	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */
	SetTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	BlockDataHandler(unit, "Ten readings after trigger\n", 0, ANALOGUE, FALSE);
}

/****************************************************************************
* CollectRapidBlock
*  this function demonstrates how to collect a set of captures using 
*  rapid block mode.
****************************************************************************/
void CollectRapidBlock(UNIT * unit)
{
	int16_t i;
	int16_t channel;
	int16_t ***rapidBuffers;
	int16_t *overflow;
	
	uint32_t nCaptures;
	uint32_t capture;
	uint32_t maxSegments = 0;
	
	int32_t nMaxSamples;
	int32_t timeIndisposed;
	
	uint32_t nSamples = 1000;
	uint32_t nCompletedCaptures;

	PICO_STATUS status;

	// Convert threshold to ADC counts
	int16_t	triggerVoltage = mv_to_adc(100, unit->channelSettings[PS2000A_CHANNEL_A].range, unit);

	struct tPS2000ATriggerChannelProperties sourceDetails = {	triggerVoltage,
																256,
																triggerVoltage,
																256,
																PS2000A_CHANNEL_A,
																PS2000A_LEVEL};

	struct tPS2000ATriggerConditions conditions = {	PS2000A_CONDITION_TRUE,				// Channel A
													PS2000A_CONDITION_DONT_CARE,		// Channel B
													PS2000A_CONDITION_DONT_CARE,		// Channel C
													PS2000A_CONDITION_DONT_CARE,		// Channel D
													PS2000A_CONDITION_DONT_CARE,		// external
													PS2000A_CONDITION_DONT_CARE,		// aux
													PS2000A_CONDITION_DONT_CARE,		// PWQ
													PS2000A_CONDITION_DONT_CARE};		// digital

	struct tPwq pulseWidth;

	struct tTriggerDirections directions = {	PS2000A_RISING,			// Channel A
												PS2000A_NONE,			// Channel B
												PS2000A_NONE,			// Channel C
												PS2000A_NONE,			// Channel D
												PS2000A_NONE,			// ext
												PS2000A_NONE };			// aux

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect rapid block triggered...\n");
	printf("Collects when value rises past %d",	scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS2000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");
	printf("Press any key to abort\n");

	SetDefaults(unit);

	// Trigger enabled
	SetTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	// Set the number of captures
	nCaptures = 10;

	// Find the maximum number of segments for the device
	status = ps2000aGetMaxSegments(unit->handle, &maxSegments);

	if (nCaptures > maxSegments)
	{
		nCaptures = maxSegments;
	}

	// Segment the memory
	status = ps2000aMemorySegments(unit->handle, nCaptures, &nMaxSamples);

	// Set the number of captures
	status = ps2000aSetNoOfCaptures(unit->handle, nCaptures);

	// Run
	timebase = 160;		// Refer to the Programmer's Guide Timebases section
	status = ps2000aRunBlock(unit->handle, 0, nSamples, timebase, 1, &timeIndisposed, 0, CallBackBlock, NULL);

	// Wait until data ready
	g_ready = 0;

	while(!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (!g_ready)
	{
		_getch();
		status = ps2000aStop(unit->handle);
		status = ps2000aGetNoOfCaptures(unit->handle, &nCompletedCaptures);
		
		printf("Rapid capture aborted. %lu complete blocks were captured\n", nCompletedCaptures);
		printf("\nPress any key...\n\n");
		_getch();

		if (nCompletedCaptures == 0)
		{
			return;
		}

		// Only display the blocks that were captured
		nCaptures = (uint16_t) nCompletedCaptures;
	}

	// Allocate memory
	rapidBuffers = (int16_t ***) calloc(unit->channelCount, sizeof(int16_t*));
	overflow = (int16_t *) calloc(unit->channelCount * nCaptures, sizeof(int16_t));

	for (channel = 0; channel < unit->channelCount; channel++) 
	{
		rapidBuffers[channel] = (int16_t**) calloc(nCaptures, sizeof(int16_t*));
	}

	for (channel = 0; channel < unit->channelCount; channel++) 
	{	
		if (unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++) 
			{
				rapidBuffers[channel][capture] = (int16_t *) calloc(nSamples, sizeof(int16_t));
			}
		}
	}

	for (channel = 0; channel < unit->channelCount; channel++) 
	{
		if (unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++) 
			{
				status = ps2000aSetDataBuffer(unit->handle, channel, rapidBuffers[channel][capture], nSamples, capture, PS2000A_RATIO_MODE_NONE);
			}
		}
	}

	// Get data
	status = ps2000aGetValuesBulk(unit->handle, &nSamples, 0, nCaptures - 1, 1, PS2000A_RATIO_MODE_NONE, overflow);

	// Stop
	status = ps2000aStop(unit->handle);

	// Print first 10 samples from each capture
	for (capture = 0; capture < nCaptures; capture++)
	{
		printf("\nCapture %d:\n\n", capture + 1);

		for (channel = 0; channel < unit->channelCount; channel++) 
		{
			printf("Channel %c\t", 'A' + channel);
		}

		printf("\n");

		for(i = 0; i < 10; i++)
		{
			for (channel = 0; channel < unit->channelCount; channel++) 
			{
				if (unit->channelSettings[channel].enabled)
				{
					printf("%d\t\t", rapidBuffers[channel][capture][i]);
				}
			}

			printf("\n");
		}
	}

	// Free memory
	free(overflow);

	for (channel = 0; channel < unit->channelCount; channel++) 
	{	
		if (unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++) 
			{
				free(rapidBuffers[channel][capture]);
			}
		}
	}

	for (channel = 0; channel < unit->channelCount; channel++) 
	{
		free(rapidBuffers[channel]);
	}

	free(rapidBuffers);

	// Set number of segments and captures back to 1
	// status = ps2000aMemorySegments(unit->handle, 1, &nMaxSamples);
	// status = ps2000aSetNoOfCaptures(unit->handle, 1);

}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void get_info(UNIT * unit)
{
	int8_t description [11][25]= { "Driver Version",
									"USB Version",
									"Hardware Version",
									"Variant Info",
									"Serial",
									"Cal Date",
									"Kernel",
									"Digital H/W",
									"Analogue H/W",
									"Firmware 1",
									"Firmware 2"};

	int16_t i, r = 0;
	int8_t line [80];
	PICO_STATUS status = PICO_OK;
	int16_t numChannels = DUAL_SCOPE;
	int8_t channelNum = 0; 
	int8_t character = 'A';

	unit->signalGenerator	= TRUE;
	unit->ETS				= FALSE;
	unit->firstRange		= PS2000A_20MV; // This is for new PicoScope 220X B, B MSO, 2405A and 2205A MSO models, older devices will have a first range of 50 mV
	unit->lastRange			= PS2000A_20V; 
	unit->channelCount		= DUAL_SCOPE;
	unit->digitalPorts      = 0;
	unit->awgBufferSize		= PS2000A_MAX_SIG_GEN_BUFFER_SIZE;

	if (unit->handle) 
	{
		for (i = 0; i < 11; i++) 
		{
			status = ps2000aGetUnitInfo(unit->handle, (int8_t *) line, sizeof (line), &r, i);
			
			if (i == PICO_VARIANT_INFO) 
			{
				// Check if device has four channels

				channelNum = line[1];
				numChannels = atoi(&channelNum);

				if (numChannels == QUAD_SCOPE)
				{
					unit->channelCount = QUAD_SCOPE;
				}

				// Set first range for voltage if device is a 2206/7/8, 2206/7/8A or 2205 MSO
				if (numChannels == DUAL_SCOPE)
				{
					if (strlen(line) == 4 || (strlen(line) == 5 && strcmpi(&line[4], "A") == 0) || (strcmpi(line, "2205MSO")) == 0)
					{
						unit->firstRange = PS2000A_50MV;
					}
				}

				// Check if device is an MSO 
				if (strstr(line, "MSO"))
				{
					unit->digitalPorts = 2;
				}

			}
			printf("%s: %s\n", description[i], line);
		}
	}
}

/****************************************************************************
* Select input voltage ranges for channels
****************************************************************************/
void SetVoltages(UNIT * unit)
{
	int32_t i, ch;
	int32_t count = 0;

	/* See what ranges are available... */
	for (i = unit->firstRange; i <= unit->lastRange; i++) 
	{
		printf("%d -> %d mV\n", i, inputRanges[i]);
	}

	do
	{
		/* Ask the user to select a range */
		printf("Specify voltage range (%d..%d)\n", unit->firstRange, unit->lastRange);
		printf("99 - switches channel off\n");
		for (ch = 0; ch < unit->channelCount; ch++) 
		{
			printf("\n");
			do 
			{
				printf("Channel %c: ", 'A' + ch);
				fflush(stdin);
				scanf_s("%hd", &unit->channelSettings[ch].range);
			} while (unit->channelSettings[ch].range != 99 && (unit->channelSettings[ch].range < unit->firstRange || unit->channelSettings[ch].range > unit->lastRange));

			if (unit->channelSettings[ch].range != 99) 
			{
				printf(" - %d mV\n", inputRanges[unit->channelSettings[ch].range]);
				unit->channelSettings[ch].enabled = TRUE;
				count++;
			} 
			else 
			{
				printf("Channel Switched off\n");
				unit->channelSettings[ch].enabled = FALSE;
				unit->channelSettings[ch].range = PS2000A_MAX_RANGES-1;
			}
		}
		printf(count == 0? "\n** At least 1 channel must be enabled **\n\n":"");
	}
	while(count == 0);	// must have at least one channel enabled

	SetDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
*
* Select timebase, set oversample to on and time units as nano seconds
*
****************************************************************************/
void SetTimebase(UNIT unit)
{
	int32_t timeInterval;
	int32_t maxSamples;

	printf("Specify desired timebase: ");
	fflush(stdin);
	scanf_s("%lud", &timebase);

	while (ps2000aGetTimebase(unit.handle, timebase, BUFFER_SIZE, &timeInterval, 1, &maxSamples, 0))
	{
		timebase++;  // Increase timebase if the one specified can't be used. 
	}

	printf("Timebase %lu used = %ld ns\n", timebase, timeInterval);
	oversample = TRUE;
}



/****************************************************************************
* Sets the signal generator
* - allows user to set frequency and waveform
* - allows for custom waveform (values -32768..32767) of up to 8192 samples int32_t
***************************************************************************/
void SetSignalGenerator(UNIT unit)
{
	PICO_STATUS status;
	int16_t waveform;
	int32_t frequency;
	char fileName [128];
	FILE * fp = NULL;
	int16_t arbitraryWaveform [PS2000A_MAX_SIG_GEN_BUFFER_SIZE];
	int16_t waveformSize = 0;
	uint32_t pkpk = 2000000;
	int32_t offset = 0;
	uint32_t delta =0;
	char ch;
	int16_t choice;

	memset(&arbitraryWaveform, 0, PS2000A_MAX_SIG_GEN_BUFFER_SIZE);

	while (_kbhit())			// use up keypress
	{
		_getch();
	}

	do
	{
		printf("\nSignal Generator\n================\n");
		printf("0 - SINE         1 - SQUARE\n");
		printf("2 - TRIANGLE     3 - DC VOLTAGE\n");
		printf("4 - RAMP UP      5 - RAMP DOWN\n");
		printf("6 - SINC         7 - GAUSSIAN\n");
		printf("8 - HALF SINE    A - AWG WAVEFORM\n");
		printf("F - SigGen Off\n\n");

		ch = _getch();

		if (ch >= '0' && ch <='9')
			choice = ch -'0';
		else
			ch = toupper(ch);
	}
	while(ch != 'A' && ch != 'F' && (ch < '0' || ch > '8')  );




	if (ch == 'F')				// If we're going to turn off siggen
	{
		printf("Signal generator Off\n");
		waveform = 8;		// DC Voltage
		pkpk = 0;				// 0V
		waveformSize = 0;
	}
	else
		if (ch == 'A' )		// Set the AWG
		{
			waveformSize = 0;

			printf("Select a waveform file to load: ");
			scanf_s("%s", fileName, 128);
			if (fopen_s(&fp, fileName, "r") == 0) 
			{ 
				// Having opened file, read in data - one number per line (at most 8192 lines), with values in (-32768 to 32767)
				while (EOF != fscanf_s(fp, "%hi", (arbitraryWaveform + waveformSize))&& waveformSize++ < (PS2000A_MAX_SIG_GEN_BUFFER_SIZE - 1));
				fclose(fp);
				printf("File successfully loaded\n");
			} 
			else 
			{
				printf("Invalid filename\n");
				return;
			}
		}
		else			// Set one of the built in waveforms
		{
			switch (choice)
			{
				case 0:
					waveform = PS2000A_SINE;
					break;

				case 1:
					waveform = PS2000A_SQUARE;
					break;

				case 2:
					waveform = PS2000A_TRIANGLE;
					break;

				case 3:
					waveform = PS2000A_DC_VOLTAGE;
					do 
					{
						printf("\nEnter offset in uV: (0 to 2500000)\n"); // Ask user to enter DC offset level;
						scanf_s("%lu", &offset);
					} while (offset < 0 || offset > 10000000);
					break;

				case 4:
					waveform = PS2000A_RAMP_UP;
					break;

				case 5:
					waveform = PS2000A_RAMP_DOWN;
					break;

				case 6:
					waveform = PS2000A_SINC;
					break;

				case 7:
					waveform = PS2000A_GAUSSIAN;
					break;

				case 8:
					waveform = PS2000A_HALF_SINE;
					break;

				default:
					waveform = PS2000A_SINE;
					break;
			}
		}

		if (waveform < 8 || ch == 'A' )				// Find out frequency if required
		{
			do 
			{
				printf("\nEnter frequency in Hz: (1 to 1000000)\n"); // Ask user to enter signal frequency;
				scanf_s("%lu", &frequency);
			} while (frequency <= 0 || frequency > 1000000);
		}

		if (waveformSize > 0)		
		{
			ps2000aSigGenFrequencyToPhase(unit.handle, frequency, PS2000A_SINGLE, waveformSize, &delta);

			status = ps2000aSetSigGenArbitrary(	unit.handle, 
				0, 
				pkpk, 
				(uint32_t) delta, 
				(uint32_t) delta, 
				0, 
				0, 
				arbitraryWaveform, 
				waveformSize, 
				(PS2000A_SWEEP_TYPE) 0,
				(PS2000A_EXTRA_OPERATIONS) 0, 
				PS2000A_SINGLE, 
				0, 
				0, 
				PS2000A_SIGGEN_RISING,
				PS2000A_SIGGEN_NONE, 
				0);

			printf(status?"\nps2000aSetSigGenArbitrary: Status Error 0x%x \n":"", (uint32_t)status);		// If status != 0, show the error
		} 
		else 
		{
			status = ps2000aSetSigGenBuiltIn(unit.handle, offset, pkpk, waveform, (float)frequency, (float)frequency, 0, 0, 
				(PS2000A_SWEEP_TYPE) 0, (PS2000A_EXTRA_OPERATIONS) 0, 0, 0, (PS2000A_SIGGEN_TRIG_TYPE) 0, (PS2000A_SIGGEN_TRIG_SOURCE) 0, 0);

			printf(status?"\nps2000aSetSigGenBuiltIn: Status Error 0x%x \n":"", (uint32_t)status);		// If status != 0, show the error
		}
}

/****************************************************************************
* CollectStreamingImmediate
*  this function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
***************************************************************************/
void CollectStreamingImmediate(UNIT * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));

	SetDefaults(unit);

	printf("Collect streaming...\n");
	printf("Data is written to disk file (%s)\n", StreamFile);
	printf("Press a key to start...\n");
	_getch();

	/* Trigger disabled	*/
	SetTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	StreamDataHandler(unit, 0, ANALOGUE);
}

/****************************************************************************
* CollectStreamingTriggered
*  this function demonstrates how to collect a stream of data
*  from the unit (start collecting on trigger)
***************************************************************************/
void CollectStreamingTriggered(UNIT * unit)
{
	int16_t triggerVoltage = mv_to_adc(1000,	unit->channelSettings[PS2000A_CHANNEL_A].range, unit); // ChannelInfo stores ADC counts
	struct tPwq pulseWidth;

	struct tPS2000ATriggerChannelProperties sourceDetails = {	triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS2000A_CHANNEL_A,
		PS2000A_LEVEL };

	struct tPS2000ATriggerConditions conditions = {	PS2000A_CONDITION_TRUE,				// Channel A
		PS2000A_CONDITION_DONT_CARE,		// Channel B
		PS2000A_CONDITION_DONT_CARE,		// Channel C
		PS2000A_CONDITION_DONT_CARE,		// Channel D
		PS2000A_CONDITION_DONT_CARE,		// External
		PS2000A_CONDITION_DONT_CARE,		// aux
		PS2000A_CONDITION_DONT_CARE,		// PWQ
		PS2000A_CONDITION_DONT_CARE };		// digital

	struct tTriggerDirections directions = {	PS2000A_RISING,			// Channel A
		PS2000A_NONE,			// Channel B
		PS2000A_NONE,			// Channel C
		PS2000A_NONE,			// Channel D
		PS2000A_NONE,			// External
		PS2000A_NONE };			// Aux

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect streaming triggered...\n");
	printf("Data is written to disk file (%s)\n", StreamFile);
	printf("Indicates when value rises past %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS2000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */
	SetTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	StreamDataHandler(unit, 0, ANALOGUE);
}


/****************************************************************************
* OpenDevice 
* Parameters 
* - unit        pointer to the UNIT structure, where the handle will be stored
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS OpenDevice(UNIT *unit)
{
	int16_t value = 0;
	int32_t i;
	PWQ pulseWidth;
	TRIGGER_DIRECTIONS directions;

	PICO_STATUS status = ps2000aOpenUnit(&(unit->handle), NULL);

	printf("Handle: %d\n", unit->handle);

	if (status != PICO_OK) 
	{
		printf("Unable to open device\n");
		printf("Error code : %d\n", (int32_t)status);
		while(!_kbhit());
		exit(99); // exit program
	}

	printf("Device opened successfully, cycle %d\n\n", ++cycles);

	// setup devices
	get_info(unit);
	timebase = 1;

	ps2000aMaximumValue(unit->handle, &value);
	unit->maxValue = value;

	for ( i = 0; i < unit->channelCount; i++) 
	{
		unit->channelSettings[i].enabled = TRUE;
		unit->channelSettings[i].DCcoupled = TRUE;
		unit->channelSettings[i].range = PS2000A_5V;
	}

	memset(&directions, 0, sizeof(TRIGGER_DIRECTIONS));
	memset(&pulseWidth, 0, sizeof(PWQ));

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	return status;
}


/****************************************************************************
* DisplaySettings 
* Displays information about the user configurable settings in this example
* Parameters 
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void DisplaySettings(UNIT *unit)
{
	int32_t ch;
	int32_t voltage;

	printf("\n\nReadings will be scaled in (%s)\n", (scaleVoltages)? ("mV") : ("ADC counts"));

	for (ch = 0; ch < unit->channelCount; ch++)
	{
		if (!(unit->channelSettings[ch].enabled))
		{
			printf("Channel %c Voltage Range = Off\n", 'A' + ch);
		}
		else
		{
			voltage = inputRanges[unit->channelSettings[ch].range];
			printf("Channel %c Voltage Range = ", 'A' + ch);

			if (voltage < 1000)
			{
				printf("%dmV\n", voltage);
			}
			else
			{
				printf("%dV\n", voltage / 1000);
			}
		}
	}
	printf("\n");

	if (unit->digitalPorts > 0)
	{
		printf("Digital Ports switched off.\n\n");
	}
}


/****************************************************************************
* ANDAnalogueDigital
* This function shows how to collect a block of data from the analogue
* ports and the digital ports at the same time, triggering when the 
* digital conditions AND the analogue conditions are met
*
* Returns       none
***************************************************************************/
void ANDAnalogueDigitalTriggered(UNIT * unit)
{
	int32_t channel = 0;
	PICO_STATUS status = PICO_OK;

	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS2000A_CHANNEL_A].range, unit);


	PS2000A_TRIGGER_CHANNEL_PROPERTIES sourceDetails = {	triggerVoltage,			// thresholdUpper
															256 * 10,				// thresholdUpper Hysteresis
															triggerVoltage,			// thresholdLower
															256 * 10,				// thresholdLower Hysteresis
															PS2000A_CHANNEL_A,		// channel
															PS2000A_LEVEL};			// mode

	PS2000A_TRIGGER_CONDITIONS conditions = {	PS2000A_CONDITION_TRUE,					// Channel A
												PS2000A_CONDITION_DONT_CARE,			// Channel B
												PS2000A_CONDITION_DONT_CARE,			// Channel C
												PS2000A_CONDITION_DONT_CARE,			// Channel D
												PS2000A_CONDITION_DONT_CARE,			// external
												PS2000A_CONDITION_DONT_CARE,			// aux 
												PS2000A_CONDITION_DONT_CARE,			// pwq
												PS2000A_CONDITION_TRUE};				// digital

	TRIGGER_DIRECTIONS directions = {	PS2000A_ABOVE,				// Channel A
										PS2000A_NONE,				// Channel B
										PS2000A_NONE,				// Channel C
										PS2000A_NONE,				// Channel D
										PS2000A_NONE,				// external
										PS2000A_NONE };				// aux

	PS2000A_DIGITAL_CHANNEL_DIRECTIONS digDirections[2];		// Array size can be up to 16, an entry for each digital bit

	PWQ pulseWidth;
	memset(&pulseWidth, 0, sizeof(PWQ));

	// Set the Digital trigger so it will trigger when bit 0 is Rising and bit 4 is HIGH
	// All non-declared bits are taken as PS2000A_DIGITAL_DONT_CARE
	//

	digDirections[0].channel = PS2000A_DIGITAL_CHANNEL_0;
	digDirections[0].direction = PS2000A_DIGITAL_DIRECTION_RISING;

	digDirections[1].channel = PS2000A_DIGITAL_CHANNEL_4;
	digDirections[1].direction = PS2000A_DIGITAL_DIRECTION_HIGH;

	printf("\nCombination Block Triggered\n");
	printf("Collects when value is above %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS2000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");

	printf("AND \n");
	printf("Digital Channel  0   --- Rising\n");
	printf("Digital Channel  4   --- High\n");
	printf("Other Digital Channels - Don't Care\n");

	printf("Press a key to start...\n");
	_getch();

	for (channel = 0; channel < unit->channelCount; channel++)
	{
		unit->channelSettings[channel].enabled = TRUE;
	}

	SetDefaults(unit);	// Enable Analogue channels.

	/* Trigger enabled
	* Rising edge
	* Threshold = 100mV */

	status = SetTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, digDirections, 2);

	if (status == PICO_OK)
	{
		BlockDataHandler(unit, "\nFirst 10 readings:\n", 0, MIXED, FALSE);
	}

	DisableAnalogue(unit);			// Disable Analogue ports when finished;
}


/****************************************************************************
* ORAnalogueDigital
* This function shows how to collect a block of data from the analogue
* ports and the digital ports at the same time, triggering when either the 
* digital conditions OR the analogue conditions are met
*
* Returns       none
***************************************************************************/
void ORAnalogueDigitalTriggered(UNIT * unit)
{
	int32_t channel = 0;

	PICO_STATUS status = PICO_OK;

	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS2000A_CHANNEL_A].range, unit);

	PS2000A_TRIGGER_CHANNEL_PROPERTIES sourceDetails = {	triggerVoltage,		// thresholdUpper
															256 * 10,			// thresholdUpper Hysteresis
															triggerVoltage,		// thresholdLower
															256 * 10,			// thresholdLower Hysteresis
															PS2000A_CHANNEL_A,	// channel
															PS2000A_LEVEL};		// mode


	PS2000A_TRIGGER_CONDITIONS conditions[2];


	TRIGGER_DIRECTIONS directions = {	PS2000A_RISING,			// Channel A
										PS2000A_NONE,			// Channel B
										PS2000A_NONE,			// Channel C
										PS2000A_NONE,			// Channel D
										PS2000A_NONE,			// external
										PS2000A_NONE };			// aux

	PS2000A_DIGITAL_CHANNEL_DIRECTIONS digDirections[2];		// Array size can be up to 16, an entry for each digital bit

	PWQ pulseWidth;


	conditions[0].channelA				= PS2000A_CONDITION_TRUE;					// Channel A
	conditions[0].channelB				= PS2000A_CONDITION_DONT_CARE;				// Channel B
	conditions[0].channelC				= PS2000A_CONDITION_DONT_CARE;				// Channel C
	conditions[0].channelD				= PS2000A_CONDITION_DONT_CARE;				// Channel D
	conditions[0].external				= PS2000A_CONDITION_DONT_CARE;				// external
	conditions[0].aux					= PS2000A_CONDITION_DONT_CARE;				// aux
	conditions[0].pulseWidthQualifier	= PS2000A_CONDITION_DONT_CARE;				// pwq
	conditions[0].digital				= PS2000A_CONDITION_DONT_CARE;				// digital


	conditions[1].channelA				= PS2000A_CONDITION_DONT_CARE;				// Channel A
	conditions[1].channelB				= PS2000A_CONDITION_DONT_CARE;				// Channel B
	conditions[1].channelC				= PS2000A_CONDITION_DONT_CARE;				// Channel C
	conditions[1].channelD				= PS2000A_CONDITION_DONT_CARE;				// Channel D
	conditions[1].external				= PS2000A_CONDITION_DONT_CARE;				// external
	conditions[1].aux					= PS2000A_CONDITION_DONT_CARE;				// aux
	conditions[1].pulseWidthQualifier	= PS2000A_CONDITION_DONT_CARE;				// pwq
	conditions[1].digital				= PS2000A_CONDITION_TRUE;					// digital

	memset(&pulseWidth, 0, sizeof(PWQ));

	// Set the Digital trigger so it will trigger when bit 0 is Rising and bit 4 is HIGH
	// All non-declared bits are taken as PS2000A_DIGITAL_DONT_CARE

	digDirections[0].channel = PS2000A_DIGITAL_CHANNEL_0;
	digDirections[0].direction = PS2000A_DIGITAL_DIRECTION_RISING;

	digDirections[1].channel = PS2000A_DIGITAL_CHANNEL_4;
	digDirections[1].direction = PS2000A_DIGITAL_DIRECTION_HIGH;

	printf("\nCombination Block Triggered\n");
	printf("Collects when value rises past %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS2000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");

	printf("OR \n");
	printf("Digital Channel  0   --- Rising\n");
	printf("Digital Channel 4   --- High\n");
	printf("Other Digital Channels - Don't Care\n");

	printf("Press a key to start...\n");
	_getch();

	for (channel = 0; channel < unit->channelCount; channel++)
	{
		unit->channelSettings[channel].enabled = TRUE;
	}

	SetDefaults(unit);	// Enable analogue ports

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */

	status = SetTrigger(unit, &sourceDetails, 1, conditions, 2, &directions, &pulseWidth, 0, 0, 0, digDirections, 2);

	if (status == PICO_OK)
	{

		BlockDataHandler(unit, "\nFirst 10 readings:\n", 0, MIXED, FALSE);
	}

	DisableAnalogue(unit);	// Disable Analogue ports when finished;
}

/****************************************************************************
* DigitalBlockTriggered
* This function shows how to collect a block of data from the digital ports
* with triggering enabled
*
* Returns       none
***************************************************************************/

void DigitalBlockTriggered(UNIT * unit)
{
	PWQ pulseWidth;
	TRIGGER_DIRECTIONS directions;
	PS2000A_DIGITAL_CHANNEL_DIRECTIONS digDirections[2];		// Array size can be up to 16, an entry for each digital bit

	PS2000A_TRIGGER_CONDITIONS conditions = {	PS2000A_CONDITION_DONT_CARE,		// Channel A
		PS2000A_CONDITION_DONT_CARE,		// Channel B
		PS2000A_CONDITION_DONT_CARE,		// Channel C
		PS2000A_CONDITION_DONT_CARE,		// Channel D
		PS2000A_CONDITION_DONT_CARE,		// external
		PS2000A_CONDITION_DONT_CARE,		// aux
		PS2000A_CONDITION_DONT_CARE,		// pwq
		PS2000A_CONDITION_TRUE};			// digital


	printf("\nDigital Block Triggered\n");

	memset(&directions, 0, sizeof(TRIGGER_DIRECTIONS));
	memset(&pulseWidth, 0, sizeof(PWQ));

	printf("Collect block of data when the trigger occurs...\n");
	printf("Digital Channel  0   --- Rising\n");
	printf("Digital Channel  4   --- High\n");
	printf("Other Digital Channels - Don't Care\n");


	digDirections[0].channel = PS2000A_DIGITAL_CHANNEL_0;
	digDirections[0].direction = PS2000A_DIGITAL_DIRECTION_RISING;

	digDirections[1].channel = PS2000A_DIGITAL_CHANNEL_4;
	digDirections[1].direction = PS2000A_DIGITAL_DIRECTION_HIGH;


	if (SetTrigger(unit, NULL, 0, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, digDirections, 2) == PICO_OK)
	{
		printf("Press a key to start...\n");
		_getch();
		BlockDataHandler(unit, "\nFirst 10 readings:\n", 0, DIGITAL, FALSE);
	}
}


/****************************************************************************
* DigitalBlockImmediate
* This function shows how to collect a block of data from the digital ports
* with triggering disabled
*
* Returns       none
***************************************************************************/
void DigitalBlockImmediate(UNIT *unit)
{
	PWQ pulseWidth;
	TRIGGER_DIRECTIONS directions;
	PS2000A_DIGITAL_CHANNEL_DIRECTIONS digDirections;

	printf("\nDigital Block Immediate\n");
	memset(&directions, 0, sizeof(TRIGGER_DIRECTIONS));
	memset(&pulseWidth, 0, sizeof(PWQ));
	memset(&digDirections, 0, sizeof(PS2000A_DIGITAL_CHANNEL_DIRECTIONS));

	SetTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, &digDirections, 0);

	printf("Press a key to start...\n");
	_getch();

	BlockDataHandler(unit, "\nFirst 10 readings:\n", 0, DIGITAL, FALSE);
}


/****************************************************************************
*  DigitalStreamingAggregated
*  this function demonstrates how to collect a stream of Aggregated data
*  from the unit's Digital inputs (start collecting immediately)
***************************************************************************/
void DigitalStreamingAggregated(UNIT * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));


	printf("Digital streaming with Aggregation...\n");
	printf("Press a key to start...\n");
	_getch();

	/* Trigger disabled	*/
	SetTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	StreamDataHandler(unit, 0, AGGREGATED);
}


/****************************************************************************
*  DigitalStreamingImmediate
*  this function demonstrates how to collect a stream of data
*  from the unit's Digital inputs (start collecting immediately)
***************************************************************************/
void DigitalStreamingImmediate(UNIT * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));

	printf("Digital streaming...\n");
	printf("Press a key to start...\n");
	_getch();

	/* Trigger disabled	*/
	SetTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	StreamDataHandler(unit, 0, DIGITAL);
}


/****************************************************************************
* DigitalMenu 
* Displays digital examples available
* Parameters 
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void DigitalMenu(UNIT *unit)
{
	char ch;
	int16_t enabled = TRUE;
	int16_t disabled = !enabled;

	DisableAnalogue(unit);					// Disable Analogue ports;
	SetDigitals(unit, enabled);				// Enable Digital ports

	ch = ' ';
	while (ch != 'X')
	{
		printf("\n");
		printf("\nDigital Port Menu\n\n");
		printf("B - Digital Block Immediate\n");
		printf("T - Digital Block Triggered\n");
		printf("A - Analogue 'AND' Digital Triggered Block\n");
		printf("O - Analogue 'OR'  Digital Triggered Block\n");
		printf("S - Digital Streaming Mode\n");
		printf("V - Digital Streaming Aggregated\n");
		printf("X - Return to previous menu\n\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");
		switch (ch) 
		{
			case 'B':
				DigitalBlockImmediate(unit);
				break;

			case 'T':
				DigitalBlockTriggered(unit);
				break;

			case 'A':
				ANDAnalogueDigitalTriggered(unit);
				break;

			case 'O':
				ORAnalogueDigitalTriggered(unit);
				break;

			case 'S':
				DigitalStreamingImmediate(unit);
				break;

			case 'V':
				DigitalStreamingAggregated(unit);
				break;

			default:

				printf("Invalid option.\n");
				break;
		}
	}

	SetDigitals(unit, disabled);				// Disable Digital ports when finished
	RestoreAnalogueSettings(unit);
}

/****************************************************************************
* main
* 
***************************************************************************/
int32_t main(void)
{
	int8_t ch;

	PICO_STATUS status;
	UNIT unit;

	printf("PicoScope 2000 Series (A API) Driver Example Program\n");
	printf("Version 2.3\n\n");
	printf("\n\nOpening the device...\n");

	status = OpenDevice(&unit);

	ch = ' ';

	while (ch != 'X')
	{
		DisplaySettings(&unit);

		printf("\n");
		printf("B - Immediate block                           V - Set voltages\n");
		printf("T - Triggered block                           I - Set timebase\n");
		printf("E - Collect a block of data using ETS         A - ADC counts/mV\n");
		printf("R - Collect set of rapid captures             G - Signal generator\n");
		printf("S - Immediate streaming\n");
		printf("W - Triggered streaming\n");
		printf(unit.digitalPorts? "D - Digital Ports menu\n":"");
		printf("                                              X - Exit\n\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");

		switch (ch) 
		{
			case 'B':
				CollectBlockImmediate(&unit);
				break;

			case 'T':
				CollectBlockTriggered(&unit);
				break;

			case 'R':
				CollectRapidBlock(&unit);
				break;

			case 'S':
				CollectStreamingImmediate(&unit);
				break;

			case 'W':
				CollectStreamingTriggered(&unit);
				break;

			case 'E':
				CollectBlockEts(&unit);
				break;

			case 'G':
				SetSignalGenerator(unit);
				break;

			case 'V':
				SetVoltages(&unit);
				break;

			case 'I':
				SetTimebase(unit);
				break;

			case 'A':
				scaleVoltages = !scaleVoltages;
				break;

			case 'D':
				if (unit.digitalPorts)
					DigitalMenu(&unit);
				break;

			case 'X':
				break;

			default:
				printf("Invalid operation.\n");
				break;
		}
	}

	CloseDevice(&unit);

	return 1;
}
