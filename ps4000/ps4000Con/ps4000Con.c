/*******************************************************************************
 *
 * Filename: ps4000Con.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to perform 
 *	 operations using a PicoScope 4000 Series device using the 
 *	 PicoScope 4000 Series (ps4000) driver API functions.
 *
 *  Supported PicoScope models:
 *
 *		PicoScope 4223, 4224 & 4224 IEPE
 *		PicoScope 4423 & 4424
 *		PicoScope 4226 & 4227
 *		PicoScope 4262
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *	  Collect a block of samples using Equivalent Time Sampling (ETS)
 *    Collect samples using a rapid block capture with trigger
 *	  Collect samples using a rapid block capture without a trigger
 *    Collect a stream of data immediately
 *    Collect a stream of data when a trigger event occurs
 *    Set Signal Generator (where available), using built in or custom signals
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (Win32/x64)
 *			Ensure that the 32-/64-bit ps2000a.lib can be located
 *			Ensure that the ps4000Api.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			Set up a project for a 32-/64-bit console mode application
 *			Add this file to the project
 *			Add ps4000.lib to the project (Microsoft C only)
 *			Add ps4000Api.h and PicoStatus.h to the project
 *			Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps4000 driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the ps4000Con application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2009-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#ifdef WIN32
/* Headers for Windows */
#include "windows.h"
#include <conio.h>
#include <stdio.h>

/* Definitions of ps4000 driver routines on Windows*/
#include "ps4000Api.h"

#define PREF4 __stdcall

#else
/* Headers for Linux*/
#include <stdlib.h>
#include <sys/types.h> 	
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* For _kbhit / _getch */
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>

/* Definition of ps4000 driver routines on Linux */
#include <libps4000-1.2/ps4000Api.h>
#ifndef PICO_STATUS
#include <libps4000-1.2/PicoStatus.h>
#endif

typedef enum enBOOL
{
	FALSE, TRUE
} BOOL;

#define PREF4
#define __min(a,b) (((a) < (b)) ? (a) : (b))
#define __max(a,b) (((a) > (b)) ? (a) : (b))
#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

/* This example was originally intended for Windows. Simulate the Win32 _kbhit
* and _getch console IO functions */
int32_t _kbhit()
{
	struct termios oldt, newt;
	int32_t bytesWaiting;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return bytesWaiting;
}

int32_t _getch()
{
	struct termios oldt, newt;
	int32_t ch;
	int32_t bytesWaiting;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
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

int32_t fopen_s(FILE ** a, const char * b, const char * c)
{
FILE * fp = fopen(b,c);
*a = fp;
return (fp>0)?0:-1;
}

#endif

int32_t cycles = 0;

#define BUFFER_SIZE 	1024
#define MAX_CHANNELS	4
#define DUAL_SCOPE		2
#define TRIPLE_SCOPE	3
#define QUAD_SCOPE		4
#define SEGMEM			10

// Signal generator
#define	AWG_DAC_FREQUENCY_4000	20e6f			// 20 MS/s update rate
#define	AWG_DAC_FREQUENCY_4262	192000.0f		// 192 kS/s update rate
#define	AWG_PHASE_ACCUMULATOR	4294967296.0f


typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
}CHANNEL_SETTINGS;

typedef enum
{
	MODEL_NONE = 0,
	MODEL_PS4223 = 4223,
	MODEL_PS4224 = 4224,
	MODEL_PS4423 = 4423,
	MODEL_PS4424 = 4424,
	MODEL_PS4226 = 4226, 
	MODEL_PS4227 = 4227,
	MODEL_PS4262 = 4262
} MODEL_TYPE;

typedef struct tTriggerDirections
{
	enum enThresholdDirection channelA;
	enum enThresholdDirection channelB;
	enum enThresholdDirection channelC;
	enum enThresholdDirection channelD;
	enum enThresholdDirection ext;
	enum enThresholdDirection aux;
}TRIGGER_DIRECTIONS;

typedef struct tPwq
{
	struct tPwqConditions * conditions;
	int16_t nConditions;
	enum enThresholdDirection direction;
	uint32_t lower;
	uint32_t upper;
	enum enPulseWidthType type;
}PWQ;

typedef struct
{
	int16_t					handle;
	MODEL_TYPE				model;
	PS4000_RANGE			firstRange;
	PS4000_RANGE			lastRange;
	uint16_t				signalGenerator;
	uint16_t 				ETS;
	int16_t					channelCount;
	CHANNEL_SETTINGS		channelSettings[MAX_CHANNELS];
	PS4000_RANGE			triggerRange;
}UNIT_MODEL;

uint32_t	timebase = 8;
int16_t		oversample = 1;
BOOL		scaleVoltages = TRUE;

uint16_t inputRanges [PS4000_MAX_RANGES] = { 
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
												20000,
												50000};

int16_t     g_ready = FALSE;
int64_t 	g_times [PS4000_MAX_CHANNELS];
int16_t     g_timeUnit;
int32_t     g_sampleCount;
uint32_t	g_startIndex;
int16_t		g_autoStop;

int16_t		g_trig = 0;
uint32_t	g_trigAt = 0;

typedef struct tBufferInfo
{
	UNIT_MODEL * unit;
	int16_t **driverBuffers;
	int16_t **appBuffers;

} BUFFER_INFO;

/****************************************************************************
* Callback
* used by ps4000 data streaimng collection calls, on receipt of data.
* used to set global flags etc checked by user routines
* This callback copies data into application buffers that are the same size
* as the driver buffers. An alternative would be to specify application 
* buffers large enough to store all the data and copy in the data from the 
* driver into the correct location.
****************************************************************************/
void PREF4 CallBackStreaming
(
 int16_t handle,
 int32_t noOfSamples,
 uint32_t startIndex,
 int16_t overflow,
 uint32_t triggerAt,
 int16_t triggered,
 int16_t autoStop,
 void * pParameter
 )
{
	int32_t channel;
	BUFFER_INFO * bufferInfo = NULL;

	if (pParameter != NULL)
	{
		bufferInfo = (BUFFER_INFO *) pParameter;
	}

	// Copy data in callback
	if (bufferInfo != NULL && noOfSamples)
	{
		
		for (channel = 0; channel < bufferInfo->unit->channelCount; channel++)
		{
			if (bufferInfo->unit->channelSettings[channel].enabled)
			{
				if (bufferInfo->appBuffers && bufferInfo->driverBuffers)
				{
					// Max buffers
					if (bufferInfo->appBuffers[channel * 2]  && bufferInfo->driverBuffers[channel * 2])
					{
						memcpy_s (&bufferInfo->appBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
					}

					// Min buffers
					if (bufferInfo->appBuffers[channel * 2 + 1] && bufferInfo->driverBuffers[channel * 2 + 1])
					{
						memcpy_s (&bufferInfo->appBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
					}
				}
			}
		}
	}

	// Used for streaming
	g_sampleCount = noOfSamples;
	g_startIndex = startIndex;
	g_autoStop = autoStop;

	// Flags to show if & where a trigger has occurred
	g_trig = triggered;
	g_trigAt = triggerAt;

	// Flag to say done reading data
	g_ready = TRUE;

}

/****************************************************************************
* Callback
* used by ps4000 data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 CallBackBlock
(
 int16_t handle,
 PICO_STATUS status,
 void * pParameter
 )
{
	// flag to say done reading data
	g_ready = TRUE;
}

/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void SetDefaults(UNIT_MODEL * unit)
{
	PICO_STATUS status;
	int32_t i;

	if(unit->ETS)
	{
		status = ps4000SetEts(unit->handle, PS4000_ETS_OFF, 0, 0, NULL); // Turn off ETS
		printf(status?"SetDefaults: ps4000SetEts ------ %d \n":"", status);
	}

	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		status = ps4000SetChannel(unit->handle, (PS4000_CHANNEL) PS4000_CHANNEL_A + i,
												unit->channelSettings[PS4000_CHANNEL_A + i].enabled,
												unit->channelSettings[PS4000_CHANNEL_A + i].DCcoupled,
												(PS4000_RANGE) unit->channelSettings[PS4000_CHANNEL_A + i].range);

		printf(status?"SetDefaults: ps4000SetChannel(channel: %d)------ %d \n":"", i, status);
	}
}

/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/

int32_t adc_to_mv(int32_t raw, int32_t ch)
{
	return (raw * inputRanges[ch]) / PS4000_MAX_VALUE;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t ch)
{
	return ((mv * PS4000_MAX_VALUE ) / inputRanges[ch]);
}

/****************************************************************************
* RapidBlockDataHandler
* - Used by Rapid block data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all to data.txt
* Input :
* - unit : the unit to use.
* - text : the text to display before the display of data slice
* - offset : the offset into the data buffer to start the display's slice.
****************************************************************************/
int32_t RapidBlockDataHandler(UNIT_MODEL * unit, char * text, int32_t offset)
{
	int32_t i, j;
	int32_t timeInterval;
	uint32_t sampleCount = 50000;
	FILE * fp = NULL;
	int32_t maxSamples;
	int16_t * buffers[PS4000_MAX_CHANNEL_BUFFERS * 2];
	int32_t timeIndisposed;
	int32_t nMaxSamples;
	uint32_t segmentIndex;
	uint32_t noOfSamples;
	PICO_STATUS status;

	/* 
	* Find the maximum number of samples, and the time interval (in nanoseconds), at the current timebase if it is valid.
	* If the timebase index is not valid, increment by 1 and try again.
	*/
	while (ps4000GetTimebase(unit->handle, timebase, sampleCount, &timeInterval, oversample, &maxSamples, 0))
	{
		timebase++;
	}
	printf("Rapid Block mode with aggregation:- timebase: %lu\toversample:%hd\n", timebase, oversample);

	// Set the memory segments (must be equal or more than no of waveforms)
	ps4000MemorySegments(unit->handle, 100, &nMaxSamples);

	// sampleCount must be < nMaxSamples
	sampleCount = 20000;
	printf("Rapid Block Mode with aggregation: memory Max smaples = %ld \n", nMaxSamples);

	// Set the number of waveforms to 100
	ps4000SetNoOfCaptures(unit->handle, 100);

	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;

	status = ps4000RunBlock(unit->handle, 0, sampleCount, timebase, oversample,
													&timeIndisposed, 0, CallBackBlock, NULL);

	printf("RapidBlockDataHandler::Run Block : %i\n", status);
	printf("Waiting for trigger...Press a key to abort\n");

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	for (i = 0; i < unit->channelCount; i++) 
	{
		buffers[i * 2] = (int16_t*)malloc(sampleCount * sizeof(int16_t));
		buffers[i * 2 + 1] = (int16_t*)malloc(sampleCount * sizeof(int16_t));
		ps4000SetDataBuffers(unit->handle, (PS4000_CHANNEL)i, buffers[i * 2], buffers[i* 2 + 1], sampleCount);
	}

	ps4000Stop(unit->handle);

	if (g_ready) 
	{
		// Retrieve data of 10 Segments 
		// One segment is a bulk of data of one waveform
		// and print out the first 10 readings
		fopen_s(&fp, "data.txt", "w");

		noOfSamples = sampleCount;

		for (segmentIndex = 80; segmentIndex < 90; segmentIndex ++)
		{
			printf("\nRapid Block Mode with aggregation: Reading Segement:-- %lu \n", segmentIndex);
			// Get values of 
			sampleCount = noOfSamples;
			status = ps4000GetValues(unit->handle, 0, (uint32_t*) &sampleCount, 1, RATIO_MODE_NONE, segmentIndex, NULL);
			printf("\nRapid Block Mode with aggregation: Reading Segement:-- ps4000GetValues: %i \n", status);
			/* Print out the first 10 readings, converting the readings to mV if required */
			printf(text);
			printf("Value (%s)\n", ( scaleVoltages ) ? ("mV") : ("ADC Counts"));

			for (j = 0; j < unit->channelCount; j++) 
			{
				if (unit->channelSettings[j].enabled) 
				{
					printf("  Ch%c:      ", 'A' + j);
				}
			}
			printf("\n");

			for (i = offset; i < offset+10; i++) 
			{
				for (j = 0; j < unit->channelCount; j++) 
				{
					if (unit->channelSettings[j].enabled) 
					{
						printf("%6d      ",scaleVoltages?
								adc_to_mv(buffers[j * 2][i],unit->channelSettings[PS4000_CHANNEL_A + j].range)
								: buffers[j * 2][i]);
					}
				}
				printf("\n");
			}

			sampleCount = __min(sampleCount, BUFFER_SIZE);

			if (fp != NULL)
			{
				fprintf(fp, "Rapid Block mode with aggregation Data log\n\n");
				fprintf(fp,"Results shown for each of the %d Channels are......\n",unit->channelCount);
				fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

				fprintf(fp, "Time  ");

				for (i = 0; i < unit->channelCount; i++)
				{
					if (unit->channelSettings[i].enabled) 
					{
						fprintf(fp,"Ch%C   Max ADC   Max mV   Min ADC   Min mV   ", 'A' + i);
					}
				}
				fprintf(fp, "\n");

				for (i = 0; (uint32_t)i < sampleCount; i++) 
				{
					fprintf(fp, "%lld ", g_times[0] + (int64_t)(i * timeInterval));

					for (j = 0; j < unit->channelCount; j++) 
					{
						if (unit->channelSettings[j].enabled) 
						{
							fprintf(	fp,
							"Ch%C  %d = %+dmV, %d = %+dmV   ",
							'A' + j,
							buffers[j * 2][i],
							adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS4000_CHANNEL_A + j].range),
							buffers[j * 2 + 1][i],
							adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PS4000_CHANNEL_A + j].range));
						}
					}
					fprintf(fp, "\n");
				}
			}
			else
			{
				printf("Cannot open the file data.txt for writing. \nPlease ensure that you have permission to access. \n");
			}
		}
	} 
	else 
	{
		printf("data collection aborted\n");
		_getch();
	}

	if (fp != NULL)
	{
		fclose(fp);
	}

	for (i = 0; i < unit->channelCount * 2; i++) 
	{
		free(buffers[i]);
	}
	return 1;
}

/****************************************************************************
* No_Agg_RapidBlockDataHandler
* - Used by Rapid block data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all to data.txt
* Input :
* - unit : the unit to use.
* - text : the text to display before the display of data slice
* - offset : the offset into the data buffer to start the display's slice.
****************************************************************************/
int32_t No_Agg_RapidBlockDataHandler(UNIT_MODEL * unit, char * text, int32_t offset)
{
	int32_t i, j;
	int32_t timeInterval;
	int32_t sampleCount = 50000;
	FILE * fp = NULL;
	int32_t maxSamples;
	int16_t * buffers[PS4000_MAX_CHANNEL_BUFFERS][SEGMEM];
	int16_t overflow[SEGMEM] = {1,1,1,1,1,1,1,1,1,1};
	int32_t timeIndisposed;
	int32_t nMaxSamples;
	uint32_t segmentIndex;
	uint32_t noOfSamples;
	PICO_STATUS status;


	/*
	* Find the maximum number of samples, and the time interval (in nanoseconds), at the current timebase if it is valid.
	* If the timebase index is not valid, increment by 1 and try again.
	*/
	while (ps4000GetTimebase(unit->handle, timebase, sampleCount,	&timeInterval, oversample, &maxSamples, 0))
	{
		timebase++;
	}
	printf("Rapid Block mode without aggregation:- timebase: %lu\toversample:%hd\n", timebase, oversample);


	// Set the memory segments (must be equal or more than no of waveforms)
	ps4000MemorySegments(unit->handle, 100, &nMaxSamples);

	// smapleCount must be < nMaxSamples
	sampleCount = 50000;
	printf("Rapid Block Mode without aggregation: memory Max samples = %ld \n", nMaxSamples);

	// Set the number of waveforms to 100
	ps4000SetNoOfCaptures(unit->handle, 100);

	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;

	status = ps4000RunBlock(unit->handle, 0, sampleCount, timebase, oversample, &timeIndisposed, 0, CallBackBlock, NULL);
	
	if (status != PICO_OK)
	{
		printf("No_Agg_RapidBlockDataHandler:ps4000RunBlock : %i\n", status);
	}
	
	printf("Waiting for trigger...Press a key to abort\n");

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	// Set the data buffer bulk
	// (SEGMEM) 10 segments for each channel
	for (i = 0; i < unit->channelCount; i++) 
	{
		for (j = 0; j < SEGMEM; j ++)
		{
			buffers[i][j] = (int16_t*)malloc(sampleCount * sizeof(int16_t));
			ps4000SetDataBufferBulk(unit->handle, (PS4000_CHANNEL)i, buffers[i][j], sampleCount, j);
		}
	}

	ps4000Stop(unit->handle);

	if (g_ready) 
	{
		// Retrieve data of 10 Segments 
		// One segment is a bulk of data of one waveform
		// and print out the first 10 readings
		fopen_s(&fp, "data.txt", "w");

		noOfSamples = sampleCount;

		ps4000GetValuesBulk(unit->handle, (uint32_t *) &sampleCount, 0 // start segment index
							, 9 // end segment index (Please refer to Programmer's Guide for more details)
							, overflow);

		for (segmentIndex = 0; segmentIndex < 9; segmentIndex++)
		{
			printf("\nRapid Block Mode without aggregation: Reading Segement:-- %lu \n", segmentIndex);
			// Get values
			sampleCount = noOfSamples;
			ps4000GetValues(unit->handle, 0, (uint32_t*)&sampleCount, 1, RATIO_MODE_NONE, segmentIndex, NULL);

			// Print out the first 10 readings, converting the readings to mV if required 
			printf(text);
			printf("Value (%s)\n", (scaleVoltages) ? ("mV") : ("ADC Counts"));

			for (j = 0; j < unit->channelCount; j++)
			{
				if (unit->channelSettings[j].enabled)
				{
					printf("  Ch%c:      ", 'A' + j);
				}
			}
			printf("\n");

			for (i = offset; i < offset + 10; i++)
			{
				for (j = 0; j < unit->channelCount; j++)
				{
					if (unit->channelSettings[j].enabled)
					{
						printf("%6d      ", scaleVoltages ?
							adc_to_mv(buffers[j][segmentIndex][i], unit->channelSettings[PS4000_CHANNEL_A + j].range)
							: buffers[j][segmentIndex][i]);
					}
				}
				printf("\n");
			}

			sampleCount = __min(sampleCount, BUFFER_SIZE);

			if (fp != NULL)
			{
				fprintf(fp, "Rapid Block mode without aggregation Data log\n\n");
				fprintf(fp, "Results shown for each of the %d Channels are......\n", unit->channelCount);
				fprintf(fp, "ADC Count & mV\n\n");

				fprintf(fp, "Time  ");

				for (i = 0; i < unit->channelCount; i++)
				{
					if (unit->channelSettings[i].enabled)
						fprintf(fp, "Ch%C     ADC         mV   ", 'A' + i);
				}
				fprintf(fp, "\n");

				for (i = 0; i < sampleCount; i++)
				{
					fprintf(fp, "%lld ", g_times[0] + (int64_t)(i * timeInterval));

					for (j = 0; j < unit->channelCount; j++)
					{

						if (unit->channelSettings[j].enabled)
						{
							fprintf(fp,
								"Ch%C  %d = %+dmV   ",
								'A' + j,
								buffers[j][segmentIndex][i],
								adc_to_mv(buffers[j][segmentIndex][i],
									unit->channelSettings[PS4000_CHANNEL_A + j].range));
						}
					}
					fprintf(fp, "\n");
				}
			}
			else
			{
			
				printf("Cannot open the file data.txt for writing. \nPlease ensure that you have permission to access. \n");
			}
		}
	} 
	else 
	{
		printf("data collection aborted\n");
		_getch();
	}

	if (fp != NULL)
	{
		fclose(fp);
	}

	for (i = 0; i < unit->channelCount; i++) 
	{
		for (j = 0; j < SEGMEM; j ++)
		{
			free(buffers[i][j]);		
		}
	}
	return 1;
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
void BlockDataHandler(UNIT_MODEL * unit, char * text, int32_t offset)
{
	int32_t i, j;
	int32_t timeInterval;
	int32_t sampleCount= BUFFER_SIZE;
	FILE * fp = NULL;
	int32_t maxSamples;
	int16_t * buffers[PS4000_MAX_CHANNEL_BUFFERS * 2];
	int32_t timeIndisposed;
	PICO_STATUS status;

	for (i = 0; i < unit->channelCount; i++) 
	{
		buffers[i * 2] = (int16_t*)malloc(sampleCount * sizeof(int16_t));
		buffers[i * 2 + 1] = (int16_t*)malloc(sampleCount * sizeof(int16_t));
		status = ps4000SetDataBuffers(unit->handle, (PS4000_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount);

		printf("BlockDataHandler:ps4000SetDataBuffers(channel %d) ------ %d \n", i, status);
	}

	/*
	* Find the maximum number of samples, and the time interval (in nanoseconds), at the current timebase if it is valid.
	* If the timebase index is not valid, increment by 1 and try again.
	*/
	while (ps4000GetTimebase(unit->handle, timebase, sampleCount, &timeInterval, oversample, &maxSamples, 0))
	{
		timebase++;
	}

	printf("timebase: %ld\toversample:%hd\n", timebase, oversample);

	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;
	status = ps4000RunBlock(unit->handle, 0, sampleCount, timebase, oversample,	&timeIndisposed, 0, CallBackBlock, NULL);

	printf("BlockDataHandler:ps4000RunBlock ------ %i \n", status);
	printf("Waiting for trigger...Press a key to abort\n");

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (g_ready) 
	{
		status = ps4000GetValues(unit->handle, 0, (uint32_t*) &sampleCount, 1, RATIO_MODE_NONE, 0, NULL);
		printf("BlockDataHandler:ps4000GetValues ------ %i \n", status);

		/* Print out the first 10 readings, converting the readings to mV if required */
		printf(text);
		printf("Value (%s)\n", ( scaleVoltages ) ? ("mV") : ("ADC Counts"));

		for (j = 0; j < unit->channelCount; j++) 
		{
			if (unit->channelSettings[j].enabled) 
			{
				printf("Channel%c:    ", 'A' + j);
			}
		}
		printf("\n");

		for (i = offset; i < offset+10; i++) 
		{
			for (j = 0; j < unit->channelCount; j++) 
			{
				if (unit->channelSettings[j].enabled) 
				{
					printf("  %6d     ", scaleVoltages ? 
							adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS4000_CHANNEL_A + j].range)	// If scaleVoltages, print mV value
							: buffers[j * 2][i]);															// else print ADC Count
				}
			}
			printf("\n");
		}

		sampleCount = __min(sampleCount, BUFFER_SIZE);

		fopen_s(&fp, "block.txt", "w");

		if (fp != NULL)
		{
			fprintf(fp, "Block Data log\n\n");
			fprintf(fp,"Results shown for each of the %d Channels are......\n", unit->channelCount);
			fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

			fprintf(fp, "Time  ");

			for (i = 0; i < unit->channelCount; i++)
			{
				if (unit->channelSettings[i].enabled) 
				{
					fprintf(fp,"Ch%C   Max ADC   Max mV   Min ADC   Min mV   ", 'A' + i);
				}
			}

			fprintf(fp, "\n");

			for (i = 0; i < sampleCount; i++) 
			{
				fprintf(fp, "%lld ", g_times[0] + (int64_t)(i * timeInterval));

				for (j = 0; j < unit->channelCount; j++) 
				{
					if (unit->channelSettings[j].enabled) 
					{
						fprintf(	fp,
							"Ch%C  %d = %+dmV, %d = %+dmV   ",
							'A' + j,
							buffers[j * 2][i],
							adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS4000_CHANNEL_A + j].range),
							buffers[j * 2 + 1][i],
							adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PS4000_CHANNEL_A + j].range));
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
	else 
	{
		printf("data collection aborted\n");
		_getch();
	}

	if ((status = ps4000Stop(unit->handle)) != PICO_OK)
	{
		printf("BlockDataHandler:ps4000Stop ------ 0x%08lx \n", status);
	}

	if (fp != NULL)
	{
		fclose(fp);
	}

	for (i = 0; i < unit->channelCount * 2; i++) 
	{
		free(buffers[i]);
	}
}


/****************************************************************************
* Stream Data Handler
* - Used by the two stream data examples - untriggered and triggered
* Inputs:
* - unit - the unit to sample on
* - preTrigger - the number of samples in the pre-trigger phase 
*					(0 if no trigger has been set)
***************************************************************************/
void StreamDataHandler(UNIT_MODEL * unit, uint32_t preTrigger)
{
	int32_t i, j;
	uint32_t sampleCount = 50000; /* Make sure buffer large enough */
	FILE * fp = NULL;
	int16_t * buffers[PS4000_MAX_CHANNEL_BUFFERS];
	int16_t * appBuffers[PS4000_MAX_CHANNEL_BUFFERS];
	PICO_STATUS status;
	uint32_t sampleInterval = 1;
	int32_t index = 0;
	int32_t totalSamples;
	uint32_t triggeredAt = 0;
	int16_t retry = 0;

	BUFFER_INFO bufferInfo;

	for (i = 0; i < unit->channelCount; i++) // create data buffers
	{
		buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
		buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

		status = ps4000SetDataBuffers(	unit->handle, 
										(PS4000_CHANNEL)i, 
										buffers[i * 2],
										buffers[i * 2 + 1], 
										sampleCount);

		// Application buffers to copy data into
		appBuffers[i * 2] = (int16_t*)malloc(sampleCount * sizeof(int16_t));
		appBuffers[i * 2 + 1] = (int16_t*)malloc(sampleCount * sizeof(int16_t));
	}

	bufferInfo.unit = unit;	
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;

	printf("Waiting for trigger...Press a key to abort\n");
	g_autoStop = FALSE;

	status = ps4000RunStreaming(unit->handle, 
									&sampleInterval, 
									PS4000_US,
									preTrigger, 
									1000000 - preTrigger, 
									TRUE,	//FALSE,
									100,
									sampleCount);

	printf("Streaming data...Press a key to abort\n");

	fopen_s(&fp, "stream.txt", "w");

	if (fp != NULL)
	{
		fprintf(fp,"For each of the %d Channels, results shown are....\n",unit->channelCount);
		fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

		for (i = 0; i < unit->channelCount; i++) 
		{
			if (unit->channelSettings[i].enabled) 
			{
				fprintf(fp,"Ch  Max ADC    Max mV  Min ADC    Min mV   ");
			}
		}
		fprintf(fp, "\n");
	}

	totalSamples = 0;

	while (!_kbhit() && !g_autoStop)
	{
		/* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
		//Sleep(100);
		g_ready = FALSE;

		status = ps4000GetStreamingLatestValues(unit->handle, CallBackStreaming, &bufferInfo);
		
		index ++;

		if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
		{
			if (g_trig)
			{
				triggeredAt = totalSamples + g_trigAt;
			}

			totalSamples += g_sampleCount;
			printf("\nCollected %li samples, index = %lu, Total: %d samples  ", g_sampleCount, g_startIndex, totalSamples);

			if (g_trig)
			{
				printf("Trig. at index %lu", triggeredAt);
			}

			if (fp != NULL)
			{
				for (i = g_startIndex; i < (int32_t)(g_startIndex + g_sampleCount); i++) 
				{
					for (j = 0; j < unit->channelCount; j++) 
					{
						if (unit->channelSettings[j].enabled) 
						{
							fprintf(	fp,
								"Ch%C %d = %+dmV, %d = %+dmV   ",
								(char)('A' + j),
								appBuffers[j * 2][i],
								adc_to_mv(appBuffers[j * 2][i], unit->channelSettings[PS4000_CHANNEL_A + j].range),
								appBuffers[j * 2 + 1][i],
								adc_to_mv(appBuffers[j * 2 + 1][i], unit->channelSettings[PS4000_CHANNEL_A + j].range));
						}
					}
					fprintf(fp, "\n");
				}
			}
			else
			{
				printf("Cannot open the file stream.txt for writing.\n");
			}
		}
	}

	if (fp != NULL) 
	{
		fclose(fp);	
	}

	ps4000Stop(unit->handle);

	if (!g_autoStop) 
	{
		printf("\ndata collection aborted\n");
		_getch();
	}

	for (i = 0; i < unit->channelCount * 2; i++) 
	{
		free(buffers[i]);
		free(appBuffers[i]);
	}
}

/****************************************************************************
* SetTrigger
*  this function calls the API trigger functions 
****************************************************************************/
PICO_STATUS SetTrigger(	int16_t handle,
						struct tTriggerChannelProperties * channelProperties,
						int16_t nChannelProperties,
						struct tTriggerConditions * triggerConditions,
						int16_t nTriggerConditions,
						TRIGGER_DIRECTIONS * directions,
						struct tPwq * pwq,
						uint32_t delay,
						int16_t auxOutputEnabled,
						int32_t autoTriggerMs)
{
	PICO_STATUS status;

	if ((status = ps4000SetTriggerChannelProperties(handle,
													channelProperties,
													nChannelProperties,
													auxOutputEnabled,
													autoTriggerMs)) != PICO_OK)
	{

			printf("SetTrigger:ps4000SetTriggerChannelProperties ------ %d \n", status);
			return status;
	}

	if ((status = ps4000SetTriggerChannelConditions(handle,
													triggerConditions,
													nTriggerConditions)) != PICO_OK) 
	{
		printf("SetTrigger:ps4000SetTriggerChannelConditions ------ %d \n", status);
		return status;
	}

	if ((status = ps4000SetTriggerChannelDirections(handle,
													directions->channelA,
													directions->channelB,
													directions->channelC,
													directions->channelD,
													directions->ext,
													directions->aux)) != PICO_OK) 
	{
		printf("SetTrigger:ps4000SetTriggerChannelDirections ------ %d \n", status);
		return status;
	}

	if ((status = ps4000SetTriggerDelay(handle, delay)) != PICO_OK) 
	{
		printf("SetTrigger:ps4000SetTriggerDelay ------ %d \n", status);
		return status;
	}

	if((status = ps4000SetPulseWidthQualifier(handle, pwq->conditions, pwq->nConditions, 
												pwq->direction, pwq->lower, pwq->upper, pwq->type)) != PICO_OK)
	{
		printf("SetTrigger:ps4000SetPulseWidthQualifier ------ %d \n", status);
		return status;
	}

	return status;
}

/****************************************************************************
* CollectBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
void CollectBlockImmediate(UNIT_MODEL * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block immediate...\n");
	printf("Press a key to start\n");
	_getch();

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	BlockDataHandler(unit, "First 10 readings\n", 0);
}

/****************************************************************************
* CollectRapidBlockImmediate
*  this function demonstrates how to rapidly collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
int32_t CollectRapidBlockImmediate(UNIT_MODEL * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect Rapid block immediate with aggregation...\n");
	printf("Press a key to start\n");
	_getch();

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);


	return RapidBlockDataHandler(unit, "First 10 readings\n", 0);
}

/****************************************************************************
* CollectRapidBlock_No_Agg
*  this function demonstrates how to use rapid block mode without
*  aggregation
****************************************************************************/
int32_t CollectRapidBlock_No_Agg(UNIT_MODEL * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect Rapid block immediate without aggregation...\n");
	printf("Press a key to start\n");
	_getch();

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	return No_Agg_RapidBlockDataHandler(unit, "First 10 readings\n", 0);
}


/****************************************************************************
* CollectBlockEts
*  this function demonstrates how to collect a block of
*  data using equivalent time sampling (ETS).
****************************************************************************/
void CollectBlockEts(UNIT_MODEL * unit)
{
	PICO_STATUS status;
	int32_t ets_sampletime;
	int16_t triggerVoltage = mv_to_adc(	100, unit->channelSettings[PS4000_CHANNEL_A].range); // ChannelInfo stores ADC counts
	
	struct tTriggerChannelProperties sourceDetails = {	triggerVoltage,
														10,
														triggerVoltage,
														10,
														PS4000_CHANNEL_A,
														LEVEL };

	struct tTriggerConditions conditions = {	CONDITION_TRUE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE };
	
	uint32_t delay = 0;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));
	directions.channelA = RISING;

	printf("Collect ETS block...\n");
	printf("Collects when value rises past %d", scaleVoltages? 
		adc_to_mv(sourceDetails.thresholdUpper,	unit->channelSettings[PS4000_CHANNEL_A].range)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	printf(scaleVoltages? "mV\n" : "ADC Counts\n");
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 1500mV
	* 10% pre-trigger  (negative is pre-, positive is post-) */

	status = SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1,
		&directions, &pulseWidth, delay, 0, 0);

	/* Enable ETS in fast mode */
	status = ps4000SetEts(unit->handle, PS4000_ETS_FAST, 20, 4, &ets_sampletime);

	printf("ETS Sample Time is: %ld\n", ets_sampletime);

	BlockDataHandler(unit, "Ten readings after trigger\n", BUFFER_SIZE / 10 - 5); // 10% of data is pre-trigger
}

/****************************************************************************
* CollectBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void CollectBlockTriggered(UNIT_MODEL * unit)
{
	int16_t	triggerVoltage = mv_to_adc(1000,	unit->channelSettings[PS4000_CHANNEL_A].range); // ChannelInfo stores ADC counts

	struct tTriggerChannelProperties sourceDetails = {	triggerVoltage,
														256,
														triggerVoltage,
														256,
														PS4000_CHANNEL_A,
														LEVEL };

	struct tTriggerConditions conditions = {	CONDITION_TRUE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE };

	struct tPwq pulseWidth;
	
	struct tTriggerDirections directions = {	RISING,
												NONE,
												NONE,
												NONE,
												NONE,
												NONE };
	
	memset(&pulseWidth, 0, sizeof(struct tPwq));

		printf("Collect block triggered...\n");
	printf("Collects when value rises past %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS4000_CHANNEL_A].range)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");

	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 100mV */
	SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0);

	BlockDataHandler(unit, "Ten readings after trigger\n", 0);
}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void get_info(UNIT_MODEL * unit)
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
	int32_t variant;
	PICO_STATUS status = PICO_OK;

	if (unit->handle) 
	{
		for (i = 0; i < 11; i++) 
		{
			status = ps4000GetUnitInfo(unit->handle, line, sizeof(line), &r, i);

			if (i == PICO_VARIANT_INFO) 
			{
				variant = atoi(line);
			}
			else if (i == PICO_ANALOGUE_HARDWARE_VERSION || PICO_DIGITAL_HARDWARE_VERSION)
			{
				// Ignore Analogue and Digital H/W
			}
			else
			{
				// Do nothing
			}

			printf("%s: %s\n", description[i], line);
		}

		

	switch (variant) 
	{
	case MODEL_PS4223:
		unit->model           = MODEL_PS4223;
		unit->signalGenerator = FALSE;
		unit->ETS             = FALSE;
		unit->firstRange      = PS4000_50MV;
		unit->lastRange       = PS4000_50V;
		unit->channelCount    = DUAL_SCOPE;
		break;

	case MODEL_PS4224:
		unit->model           = MODEL_PS4224;
		unit->signalGenerator = FALSE;
		unit->ETS             = FALSE;
		unit->firstRange      = PS4000_50MV;
		unit->lastRange       = PS4000_20V;
		unit->channelCount    = DUAL_SCOPE;
		break;

	case MODEL_PS4423:
		unit->model           = MODEL_PS4423;
		unit->signalGenerator = FALSE;
		unit->ETS             = FALSE;
		unit->firstRange      = PS4000_50MV;
		unit->lastRange       = PS4000_50V;
		unit->channelCount    = QUAD_SCOPE;
		break;

	case MODEL_PS4424:
		unit->model           = MODEL_PS4424;
		unit->signalGenerator = FALSE;
		unit->ETS             = FALSE;
		unit->firstRange      = PS4000_50MV;
		unit->lastRange       = PS4000_20V;
		unit->channelCount    = QUAD_SCOPE;
		break;

	case MODEL_PS4226:
		unit->model           = MODEL_PS4226;
		unit->signalGenerator = TRUE;
		unit->ETS             = TRUE;
		unit->firstRange      = PS4000_50MV;
		unit->lastRange       = PS4000_20V;
		unit->channelCount    = DUAL_SCOPE;
		break;

	case MODEL_PS4227:
		unit->model           = MODEL_PS4227;
		unit->signalGenerator = TRUE;
		unit->ETS             = TRUE;
		unit->firstRange      = PS4000_50MV;
		unit->lastRange       = PS4000_20V;
		unit->channelCount    = DUAL_SCOPE;
		break;

	case MODEL_PS4262:
		unit->model           = MODEL_PS4262;
		unit->signalGenerator = TRUE;
		unit->ETS             = FALSE;
		unit->firstRange      = PS4000_10MV;
		unit->lastRange       = PS4000_20V;
		unit->channelCount    = DUAL_SCOPE;
		break;

	default:
		break;
		}
	}
}

/****************************************************************************
* Select input voltage ranges for channels A and B
****************************************************************************/
void set_voltages(UNIT_MODEL * unit)
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
			}
		}
		printf(count == 0? "\n** At least 1 channel must be enabled **\n\n":"");
	}
	while(count == 0);	// must have at least one channel enabled

	SetDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
*
* Select timebase, set oversample to one
*
****************************************************************************/
void SetTimebase(UNIT_MODEL unit)
{
	int32_t timeInterval;
	int32_t maxSamples;

	printf("Specify desired timebase: ");
	fflush(stdin);
	scanf_s("%lud", &timebase);

	while (ps4000GetTimebase(unit.handle, timebase, BUFFER_SIZE, &timeInterval, 1, &maxSamples, 0))
	{
		timebase++;  // Increase timebase if the one specified can't be used. 
	}

	printf("Timebase used %lu = %ldns Sample Interval\n", timebase, timeInterval);
	oversample = TRUE;
}


/****************************************************************************
* Sets the signal generator
* - allows user to set frequency and waveform
* - allows for custom waveforms...
* - PicoScope 4226 & 4227 - values 0..4095, up to 8192 samples
* - PicoScope 4262 - values -32768..32767, 4096 samples
******************************************************************************/
void SetSignalGenerator(UNIT_MODEL unit)
{
	PICO_STATUS status;
	int16_t waveform;
	int32_t frequency = 1;
	int8_t fileName [128];
	FILE * fp = NULL;
	int16_t *arbitraryWaveform;
	int32_t waveformSize = 0;
	uint32_t pkpk = 1000000;
	int32_t offset = 0;
	int8_t ch;
	int16_t choice;
	double delta;
	int16_t AWGFileSize;
	float UCVal;
	int32_t maxFreq;


	while (_kbhit())			// use up keypress
		_getch();

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


	if (unit.model == MODEL_PS4262)									
	{
		AWGFileSize = MAX_SIG_GEN_BUFFER_SIZE>>1;	// PicoScope 4262 has 4 kS AWG buffer
		UCVal = 1 / AWG_DAC_FREQUENCY_4262;			// 1 / 192KHz = 5.2083e-6
		maxFreq = 20000;							// 20 MHz maximum frequency							
	}
	else																						
	{
		AWGFileSize = MAX_SIG_GEN_BUFFER_SIZE;		// PicoScope 4226 / 4227 have 8 kS AWG buffer
			
		UCVal = 1 / AWG_DAC_FREQUENCY_4000;			// 1/20 MHz = 5e-8
		maxFreq = 100000;							// 100 MHz maximum frequency		
	}


	if (ch == 'F')			// If we're going to turn off siggen
	{
		printf("Signal generator Off\n");
		waveform = 8;		// DC Voltage
		pkpk = 0;			// 0V
		waveformSize = 0;
	}
	else if (ch == 'A')		// Set the AWG
	{
		arbitraryWaveform = (int16_t*) malloc( AWGFileSize * sizeof(int16_t));
		memset(arbitraryWaveform, 0, AWGFileSize * sizeof(int16_t));

		waveformSize = 0;

		printf("Select a waveform file to load: ");
		scanf_s("%s", fileName, 128);

		if (fopen_s(&fp, fileName, "r") == 0) 
		{ 
			// Having opened file, read in data - one number per line 
			while (EOF != fscanf_s(fp, "%hi", (arbitraryWaveform + waveformSize))&& waveformSize++ < AWGFileSize - 1);
			fclose(fp);
			printf("File successfully loaded\n");
		} 
		else 
		{
			printf("Invalid filename\n");
			return;
		}
	}
	else	// Set one of the built in waveforms
	{
		switch (choice)
		{
			case 0:
				waveform = PS4000_SINE;
				break;

			case 1:
				waveform = PS4000_SQUARE;
				break;

			case 2:
				waveform = PS4000_TRIANGLE;
				break;

			case 3:
				waveform = PS4000_DC_VOLTAGE;
				do 
				{
					printf("\nEnter offset in uV: (0 to 2500000)\n"); // Ask user to enter DC offset level;
					scanf_s("%lu", &offset);
				} while (offset < 0 || offset > 10000000);
				break;

			case 4:
				waveform = PS4000_RAMP_UP;
				break;

			case 5:
				waveform = PS4000_RAMP_DOWN;
				break;

			case 6:
				waveform = PS4000_SINC;
				break;

			case 7:
				waveform = PS4000_GAUSSIAN;
				break;

			case 8:
				waveform = PS4000_HALF_SINE;
				break;

			default:
				waveform = PS4000_SINE;
				break;
		}
	}

	if(waveform < 8 || ch == 'A' )				// Find out frequency if required
	{
		do 
		{
			printf("\nEnter frequency in Hz: (1 to %d)\n", maxFreq); // Ask user to enter signal frequency;
			scanf_s("%lu", &frequency);
		} while (frequency <= 0 || frequency > maxFreq);
	}

	if (waveformSize > 0)		
	{
		delta = ((1.0 * frequency * waveformSize) / AWGFileSize) * (AWG_PHASE_ACCUMULATOR * UCVal);		
	
		status = ps4000SetSigGenArbitrary(	unit.handle, 
												0,						// offset voltage
												pkpk,					// PkToPk in microvolts. Max = 4000000uV  (± 2V)
												(uint32_t)delta,		// start delta
												(uint32_t)delta,		// stop delta
												0, 
												0, 
												arbitraryWaveform, 
												waveformSize, 
												(SWEEP_TYPE)0, 
												0, 
												SINGLE, 
												0, 
												0, 
												SIGGEN_RISING,
												SIGGEN_NONE, 
												0);

		// If status != PICO_OK, show the error
		printf(status?"\nps4000SetSigGenArbitrary: Status Error 0x%x \n":"", (uint32_t) status); 
	} 
	else 
	{
		status = ps4000SetSigGenBuiltIn( unit.handle, 
											offset, 
											pkpk, 
											waveform, 
											(float)frequency, 
											(float)frequency, 
											0, 
											0, 
											(SWEEP_TYPE)0, 
											0, 
											0, 
											0, 
											(SIGGEN_TRIG_TYPE)0, 
											(SIGGEN_TRIG_SOURCE)0, 
											0);

		// If status != PICO_OK, show the error
		printf(status?"\nps4000SetSigGenBuiltIn: Status Error 0x%x \n":"", (uint32_t) status);
	}
}



/****************************************************************************
* CollectStreamingImmediate
*  This function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
***************************************************************************/
void CollectStreamingImmediate(UNIT_MODEL * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));

	SetDefaults(unit);

	printf("Collect streaming...\n");
	printf("Data is written to disk file (stream.txt)\n");
	printf("Press a key to start\n");
	_getch();


	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	StreamDataHandler(unit, 0);
}

/****************************************************************************
* CollectStreamingTriggered
*  This function demonstrates how to collect a stream of data
*  from the unit (start collecting on trigger)
***************************************************************************/
void CollectStreamingTriggered(UNIT_MODEL * unit)
{
	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS4000_CHANNEL_A].range); // ChannelInfo stores ADC counts

	struct tTriggerChannelProperties sourceDetails = {	triggerVoltage,
														256 * 10,
														triggerVoltage,
														256 * 10,
														PS4000_CHANNEL_A,
														LEVEL };

	struct tTriggerConditions conditions = {	CONDITION_TRUE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE,
												CONDITION_DONT_CARE };

	struct tPwq pulseWidth;
	
	struct tTriggerDirections directions = {	RISING,
												NONE,
												NONE,
												NONE,
												NONE,
												NONE };			
	
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect streaming triggered...\n");
	printf("Data is written to disk file (stream.txt)\n");
	printf("Press a key to start\n");
	_getch();
	SetDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 100mV */

	SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0);
	
	StreamDataHandler(unit, 100000);
}


/****************************************************************************
* DisplaySettings 
* Displays information about the user configurable settings in this example
* Parameters 
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void DisplaySettings(UNIT_MODEL *unit)
{
	int32_t ch;
	int32_t voltage;

	printf("\n\nReadings will be scaled in %s\n", (scaleVoltages)? ("mV") : ("ADC counts"));

	for (ch = 0; ch < unit->channelCount; ch++)
	{
		voltage = inputRanges[unit->channelSettings[ch].range];
		printf("Channel %c Voltage Range = ", 'A' + ch);

		if (voltage == 0)
			printf("Off\n");
		else
		{
			if (voltage < 1000)
				printf("%dmV\n", voltage);
			else
				printf("%dV\n", voltage / 1000);
		}
	}
	printf("\n");
}

int32_t main(void)
{
	int8_t ch;
	int32_t i;
	PICO_STATUS status;
	UNIT_MODEL unit;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	printf("PicoScope 4000 Series (ps4000) Driver Example Program\n");
	printf("\n\nOpening the device...\n");

	status = ps4000OpenUnit(&(unit.handle));
	printf("Handle: %d\n", unit.handle);
	
	if (status != PICO_OK && status != PICO_EEPROM_CORRUPT) 
	{
		printf("Unable to open device\n");
		printf("Error code : %d\n", (int32_t) status);
		while (!_kbhit());
		exit( 99); // exit program - nothing after this executes
	}

	printf("Device opened successfully, cycle %d\n\n", ++cycles);

	// setup devices
	get_info(&unit);
	timebase = 1;

	for (i = 0; i < MAX_CHANNELS; i++) 
	{
		unit.channelSettings[i].enabled = TRUE;
		unit.channelSettings[i].DCcoupled = TRUE;
		unit.channelSettings[i].range = PS4000_5V;
	}

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	SetDefaults(&unit);

	/* Trigger disabled	*/
	SetTrigger(unit.handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	ch = ' ';

	while (ch != 'X')
	{
		DisplaySettings(&unit);

		printf("\n");
		printf("B - Immediate block                             V - Set voltages\n");
		printf("T - Triggered block                             I - Set timebase\n");
		printf("R - Immediate rapid block with aggregation      A - ADC counts/mV\n");
		printf("N - Rapid block without aggregation\n");
		printf("Q - Collect a block using ETS\n");
		printf("S - Immediate streaming\n");
		printf("W - Triggered streaming\n");
		printf("G - Signal generator\n");
		printf("                                                X - Exit\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");

		switch (ch) 
		{
			case 'B':
				CollectBlockImmediate(&unit);
				break;

			case 'R':
				CollectRapidBlockImmediate(&unit);
				break;

			case 'N':
				CollectRapidBlock_No_Agg(&unit);
				break;

			case 'T':
				CollectBlockTriggered(&unit);
				break;

			case 'S':
				CollectStreamingImmediate(&unit);
				break;

			case 'W':
				CollectStreamingTriggered(&unit);
				break;

			case 'Q':

				if (unit.ETS == FALSE)
				{
					printf("This model does not have ETS\n\n");
					break;
				}

				CollectBlockEts(&unit);
				break;

			case 'G':
				
				if (unit.signalGenerator == FALSE)
				{
					printf("This model does not have a signal generator\n\n");
					break;
				}

				SetSignalGenerator(unit);
				break;

			case 'V':
				set_voltages(&unit);
				break;

			case 'I':
				SetTimebase(unit);
				break;

			case 'A':
				scaleVoltages = !scaleVoltages;
				break;

			case 'X':
				break;

			default:
				printf("Invalid operation\n");
				break;
		}
	}
	
	ps4000CloseUnit(unit.handle);
	return 1;
}
