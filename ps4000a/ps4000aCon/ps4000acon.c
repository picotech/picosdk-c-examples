/**************************************************************************
 *
 * Filename: ps4000acon.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use the
 *   PicoScope 4000 Series A API driver functions.
 *
 *	Supported PicoScope models:
 *
 *		PicoScope 4225 & 4425
 *		PicoScope 4444
 *		PicoScope 4824
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *	  Collect data in rapid block mode
 *    Collect a stream of data immediately
 *    Collect a stream of data when a trigger event occurs
 *    Set Signal Generator, using standard or custom signals
 *    Change timebase & voltage scales
 *    Display data in mV or ADC counts
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (Win32/x64)
 *			Ensure that the 32-/64-bit ps4000a.lib can be located
 *			Ensure that the ps4000aApi.h file can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps4000a.lib to the project (Microsoft C only)
 *			 Add ps4000aApi.h, PicoConnectProbes.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps4000a driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the ps4000acon application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2017 Pico Technology Ltd. See LICENSE file for terms.
 *
 **************************************************************************/

#include <stdio.h>

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
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

#include <libps4000a-1.0/ps4000aApi.h>
#ifndef PICO_STATUS
#include <libps4000a-1.0/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL{FALSE,TRUE} BOOL;

/* A function to detect a keyboard press on Linux */
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

int32_t fopen_s(FILE ** a, const char * b, const char * c)
{
FILE * fp = fopen(b,c);
*a = fp;
return (fp>0)?0:-1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

int32_t cycles = 0;

#define BUFFER_SIZE 	1024

#define OCTO_SCOPE		8
#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

#define MAX_PICO_DEVICES 64
#define TIMED_LOOP_STEP 500

typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
	float analogueOffset;
}CHANNEL_SETTINGS;


typedef struct tPwq
{
	PS4000A_CONDITION * conditions;
	int16_t nConditions;
	PS4000A_THRESHOLD_DIRECTION direction;
	uint32_t  lower;
	uint32_t upper;
	PS4000A_PULSE_WIDTH_TYPE type;
}PWQ;

typedef enum
{
	MODEL_NONE = 0,
	MODEL_PS4824 = 0x12d8,
	MODEL_PS4225 = 0x1081,
	MODEL_PS4425 = 0x1149,
	MODEL_PS4444 = 0x115C
} MODEL_TYPE;

typedef enum
{
	SIGGEN_NONE = 0,
	SIGGEN_FUNCTGEN = 1,
	SIGGEN_AWG = 2
} SIGGEN_TYPE;

typedef struct
{
	int16_t handle;
	MODEL_TYPE					model;
	int8_t						modelString[8];
	int8_t						serial[11];
	int16_t						complete;
	int16_t						openStatus;
	int16_t						openProgress;
	PS4000A_RANGE				firstRange;
	PS4000A_RANGE				lastRange;
	int16_t						channelCount;
	int16_t						maxADCValue;
	SIGGEN_TYPE					sigGen;
	int16_t						hasETS;
	uint16_t					AWGFileSize;
	CHANNEL_SETTINGS			channelSettings [PS4000A_MAX_CHANNELS];
}UNIT;

uint32_t timebase = 8;
BOOL      scaleVoltages = TRUE;

uint16_t inputRanges [PS4000A_MAX_RANGES] = {
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

int16_t			g_autoStopped;
int16_t   		g_ready = FALSE;
uint64_t 		g_times [PS4000A_MAX_CHANNELS];
int16_t     	g_timeUnit;
int32_t      	g_sampleCount;
uint32_t		g_startIndex;
int16_t			g_trig = 0;
uint32_t		g_trigAt = 0;

int8_t BlockFile[20]  = "block.txt";
int8_t StreamFile[20] = "stream.txt";

typedef struct tBufferInfo
{
	UNIT * unit;
	int16_t **driverBuffers;
	int16_t **appBuffers;

} BUFFER_INFO;


/****************************************************************************
* Streaming Callback
* used by ps4000a data streaming collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 CallBackStreaming(	short handle,
	int32_t noOfSamples,
	uint32_t	startIndex,
	int16_t overflow,
	uint32_t triggerAt,
	int16_t triggered,
	int16_t autoStop,
	void	*pParameter)
{
	int32_t channel;
	BUFFER_INFO * bufferInfo = NULL;

	if (pParameter != NULL)
	{
		bufferInfo = (BUFFER_INFO *) pParameter;
	}

	// used for streaming
	g_sampleCount = noOfSamples;
	g_startIndex  = startIndex;
	g_autoStopped = autoStop;

	// flag to say done reading data
	g_ready = TRUE;

	// flags to show if & where a trigger has occurred
	g_trig = triggered;
	g_trigAt = triggerAt;

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
}

/****************************************************************************
* Block Callback
* used by ps4000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 CallBackBlock( int16_t handle, PICO_STATUS status, void * pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
	}
}




/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void SetDefaults(UNIT * unit)
{
	PICO_STATUS status;
	PICO_STATUS powerStatus;
	int32_t i;

	if (unit->hasETS) 
	{
		status = ps4000aSetEts(unit->handle, PS4000A_ETS_OFF, 0, 0, NULL);					// Turn off ETS
		printf(status?"SetDefaults:ps4000aSetEts------ 0x%08x \n":"", status);
	}

	powerStatus = ps4000aCurrentPowerSource(unit->handle);

	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		status = ps4000aSetChannel(unit->handle, (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + i),
										unit->channelSettings[PS4000A_CHANNEL_A + i].enabled,
										(PS4000A_COUPLING)unit->channelSettings[PS4000A_CHANNEL_A + i].DCcoupled,
										(PS4000A_RANGE)unit->channelSettings[PS4000A_CHANNEL_A + i].range,
										unit->channelSettings[PS4000A_CHANNEL_A + i].analogueOffset);

		printf(status?"SetDefaults:ps4000aSetChannel------ 0x%08x \n":"", status);
	}
}

/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t rangeIndex, UNIT * unit)
{
	return (raw * inputRanges[rangeIndex]) / unit->maxADCValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t rangeIndex, UNIT * unit)
{
	return (mv * unit->maxADCValue) / inputRanges[rangeIndex];
}

/******************************************************************************
* ChangePowerSource -
* 	function to handle switches between USB 3.0 and non-USB 3.0 connections
*******************************************************************************/
PICO_STATUS ChangePowerSource(int16_t handle, PICO_STATUS status)
{
	int8_t ch = 'Y';

	switch (status)
	{
	
		case PICO_POWER_SUPPLY_NOT_CONNECTED:

			do
			{
				printf("\n5 V power supply not connected.");
				printf("\nDo you want to run using USB only Y/N?\n");
				
				ch = toupper(_getch());
				
				if (ch == 'Y')
				{
					printf("\nPower OK\n");
					status = ps4000aChangePowerSource(handle, PICO_POWER_SUPPLY_NOT_CONNECTED);		// Tell the driver that's ok
				}

			} while(ch != 'Y' && ch != 'N');

			printf(ch == 'N'?"Please set correct USB connection setting for this device\n":"");
			break;

		case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:			// User must acknowledge they want to power via USB

			do
			{
				printf("\nUSB 3.0 device on non-USB 3.0 port.\n");
				status = ps4000aChangePowerSource(handle, PICO_USB3_0_DEVICE_NON_USB3_0_PORT);		// Tell the driver that's ok
				
			} while(ch != 'Y' && ch != 'N');

			printf(ch == 'N'?"Please set correct USB connection setting for this device\n":"");
			break;
	}
	return status;
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
		if (unit->channelSettings[i].enabled)
		{
			if ((status = ps4000aSetDataBuffers(unit->handle, (PS4000A_CHANNEL) i, NULL, NULL, 0, 0, PS4000A_RATIO_MODE_NONE)) != PICO_OK)
			{
				printf("ClearDataBuffers:ps4000aSetDataBuffers(channel %d) ------ 0x%08x \n", i, status);
			}
		}
	}
	return status;
}

/****************************************************************************
* BlockDataHandler
* - Used by all block data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all to block.txt
* Input :
* - unit : the unit to use.
* - text : the text to display before the display of data slice
* - offset : the offset into the data buffer to start the display's slice.
****************************************************************************/
void BlockDataHandler(UNIT * unit, int8_t * text, int32_t offset)
{
	int32_t timeIndisposed;
	int32_t maxSamples;
	int32_t i, j;
	float timeInterval;
	int32_t sampleCount = BUFFER_SIZE;
	
	int16_t * buffers[PS4000A_MAX_CHANNEL_BUFFERS];
	
	FILE * fp = NULL;
	PICO_STATUS status;

	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));		
			buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));	// Min data, if aggregation is used

			status = ps4000aSetDataBuffers(unit->handle, (PS4000A_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, 0, PS4000A_RATIO_MODE_NONE);
			
			printf(status?"BlockDataHandler:ps4000aSetDataBuffers(channel %d) ------ 0x%08x \n":"", i, status);
		}
	}


	/*  find the maximum number of samples, the time interval (in timeUnits),
	*		 the most suitable time units */
	while (ps4000aGetTimebase2(unit->handle, timebase, sampleCount, &timeInterval, &maxSamples, 0))
	{
		timebase++;
	}

	printf("\nTimebase: %u  SampleInterval: %.1f ns\n", timebase, timeInterval);

	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;

	status = ps4000aRunBlock(unit->handle, 0, sampleCount, timebase, &timeIndisposed, 0, CallBackBlock, NULL);

	if (status != PICO_OK)
	{
		printf("BlockDataHandler:ps4000aRunBlock ------ 0x%08x \n", status);
		return;	
	}

	printf("Waiting for trigger...Press a key to abort\n");

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (g_ready)
	{
		status = ps4000aGetValues(unit->handle, 0, (uint32_t*) &sampleCount, 1, PS4000A_RATIO_MODE_NONE, 0, NULL);

		if (status != PICO_OK)
		{
			printf("BlockDataHandler:ps4000aGetValues ------ 0x%08x \n", status);
		}
		else
		{
			/* Print out the first 10 readings, converting the readings to mV if required */
			printf("%s\n",text);

			printf("Channel readings are in %s.\n\n", ( scaleVoltages ) ? ("mV") : ("ADC Counts"));

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
							adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS4000A_CHANNEL_A + j].range, unit)	// If scaleVoltages, print mV value
							: buffers[j * 2][i]);																	// else print ADC Count
					}
				}

				printf("\n");
			}

			sampleCount = min(sampleCount, BUFFER_SIZE);

			fopen_s(&fp, BlockFile, "w");

			if (fp != NULL)
			{
				fprintf(fp, "Block Data log\n\n");
				fprintf(fp,"Results shown for each of the %d Channels are......\n",unit->channelCount);
				fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

				fprintf(fp, "Time  ");

				for (i = 0; i < unit->channelCount; i++)
				{
					if (unit->channelSettings[i].enabled) 
					{
						fprintf(fp," Ch   Max ADC   Max mV   Min ADC   Min mV   ");
					}
				}
				fprintf(fp, "\n");

				for (i = 0; i < sampleCount; i++) 
				{
					fprintf(fp, "%I64d ", g_times[0] + (uint64_t)(i * timeInterval));

					for (j = 0; j < unit->channelCount; j++)
					{
						if (unit->channelSettings[j].enabled) 
						{
							fprintf(	fp,
								"Ch%C  %d = %dmV, %d = %dmV   ",
								'A' + j,
								buffers[j * 2][i],
								adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS4000A_CHANNEL_A + j].range, unit),
								buffers[j * 2 + 1][i],
								adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PS4000A_CHANNEL_A + j].range, unit));
						}
					}
					fprintf(fp, "\n");
				}

			}
			else
			{
				printf(	"Cannot open the file %s for writing.\n"
					"Please ensure that you have permission to access the file.\n", BlockFile);
			}
		}
	}
	else 
	{
		printf("data collection aborted\n");
		_getch();
	}

	if ((status = ps4000aStop(unit->handle)) != PICO_OK)
	{
		printf("BlockDataHandler:ps4000aStop ------ 0x%08x \n", status);
	}

	if (fp != NULL)
	{
		fclose(fp);
	}

	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			free(buffers[i * 2]);
			free(buffers[i * 2 + 1]);
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
void StreamDataHandler(UNIT * unit, uint32_t preTrigger)
{
	int16_t retry = 0;
	int16_t autostop = 0;
	int32_t index = 0;
	int32_t totalSamples = 0;

	uint32_t sampleInterval;
	uint32_t postTrigger;
	
	uint32_t downsampleRatio;
	uint32_t triggeredAt = 0;

	int32_t i, j;
	uint32_t sampleCount = 200000; /*  Make sure buffer size is large enough to copy data into on each iteration */
	
	FILE * fp = NULL;
	
	int16_t * buffers[PS4000A_MAX_CHANNEL_BUFFERS];
	int16_t * appBuffers[PS4000A_MAX_CHANNEL_BUFFERS]; // Temporary application buffers to copy data into from driver buffers.
	
	PICO_STATUS status;
	
	PS4000A_TIME_UNITS timeUnits;
	PS4000A_RATIO_MODE ratioMode;

	BUFFER_INFO bufferInfo;

	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			status = ps4000aSetDataBuffers(unit->handle, (PS4000A_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, 0, PS4000A_RATIO_MODE_NONE);

			appBuffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			appBuffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

			printf(status?"StreamDataHandler:ps4000aSetDataBuffers(channel %d) ------ 0x%08x \n":"", i, status);
		}
	}

	downsampleRatio = 1;
	timeUnits = PS4000A_US;
	sampleInterval = 1;
	ratioMode = PS4000A_RATIO_MODE_NONE;
	postTrigger = 1000000;
	autostop = TRUE;

	bufferInfo.unit = unit;
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;

	if (autostop)
	{
		printf("\nStreaming Data for %u samples", postTrigger / downsampleRatio);
		
		if (preTrigger)							// we pass 0 for preTrigger if we're not setting up a trigger
		{
			printf(" after the trigger occurs\nNote: %u Pre Trigger samples before Trigger arms\n\n", preTrigger / downsampleRatio);
		}
		else
		{
			printf("\n\n");
		}
	}
	else
	{
		printf("\nStreaming Data continually...\n\n");
	}

	g_autoStopped = FALSE;

	status = ps4000aRunStreaming(unit->handle, &sampleInterval, timeUnits, preTrigger, postTrigger, autostop, downsampleRatio, ratioMode, sampleCount);

	if (status != PICO_OK)
	{
		printf("StreamDataHandler:ps4000aRunStreaming ------ 0x%08x \n", status);
		return;
	}
	
	printf("Streaming data...Press a key to stop\n");

	fopen_s(&fp, StreamFile, "w");

	if (fp != NULL)
	{
		fprintf(fp,"For each of the %d Channels, results shown are....\n",unit->channelCount);
		fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

		for (i = 0; i < unit->channelCount; i++)
		{
			if (unit->channelSettings[i].enabled) 
			{
				fprintf(fp,"   Max ADC    Max mV  Min ADC  Min mV   ");
			}
		}
		fprintf(fp, "\n");
	}


	totalSamples = 0;

	while (!_kbhit() && !g_autoStopped)
	{
		/* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
		Sleep(1);
		g_ready = FALSE;

		status = ps4000aGetStreamingLatestValues(unit->handle, CallBackStreaming, &bufferInfo);

		if (status != PICO_OK)
		{
			printf("\nStreamDataHandler:ps4000aGetStreamingLatestValues ------ 0x%08x \n", status);
		}

		index ++;

		if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
		{
			if (g_trig)
			{
				triggeredAt = totalSamples += g_trigAt;		// calculate where the trigger occurred in the total samples collected
			}

			totalSamples += g_sampleCount;

			printf("\nCollected %3i samples, index = %6u, Total: %d samples ", g_sampleCount, g_startIndex, totalSamples);

			if (g_trig)
			{
				printf("Trig. at index %u", triggeredAt);	// show where trigger occurred
			}

			for (i = g_startIndex; i < (int32_t)(g_startIndex + g_sampleCount); i++)
			{

				if (fp != NULL)
				{
					for (j = 0; j < unit->channelCount; j++)
					{
						if (unit->channelSettings[j].enabled) 
						{
							fprintf(	fp,
								"Ch%C  %d = %dmV, %d = %dmV   ",
								(int8_t)('A' + j),
								appBuffers[j * 2][i],
								adc_to_mv(appBuffers[j * 2][i], unit->channelSettings[PS4000A_CHANNEL_A + j].range, unit),
								appBuffers[j * 2 + 1][i],
								adc_to_mv(appBuffers[j * 2 + 1][i], unit->channelSettings[PS4000A_CHANNEL_A + j].range, unit));
						}
					}

					fprintf(fp, "\n");
				}
				else
				{
					printf("Cannot open the file %s for writing.\n", StreamFile);
				}

			}
		}
	}

	ps4000aStop(unit->handle);

	if (!g_autoStopped)
	{
		printf("\nData collection aborted.\n");
		_getch();
	}
	else
	{
		printf("\nData collection complete.\n\n");
	}

	if (fp != NULL)
	{
		fclose(fp);
	}

	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			free(buffers[i * 2]);
			free(appBuffers[i * 2]);

			free(buffers[i * 2 + 1]);
			free(appBuffers[i * 2 + 1]);
		}
	}

	ClearDataBuffers(unit);
}

/****************************************************************************
* SetTrigger
*
* - Used to call aall the functions required to set up triggering
*
***************************************************************************/
PICO_STATUS SetTrigger(	UNIT * unit,
	struct tPS4000ATriggerChannelProperties * channelProperties,
	int16_t nChannelProperties,
	PS4000A_CONDITION * triggerConditions,
	int16_t nTriggerConditions,
	PS4000A_DIRECTION * directions,
	int16_t nDirections,
	struct tPwq * pwq,
	uint32_t delay,
	int16_t auxOutputEnabled,
	int32_t autoTriggerMs)
{
	PICO_STATUS status;
	PS4000A_CONDITIONS_INFO info = PS4000A_CLEAR;
	PS4000A_CONDITIONS_INFO pwqInfo = PS4000A_CLEAR;

	if ((status = ps4000aSetTriggerChannelProperties(unit->handle,
		channelProperties,
		nChannelProperties,
		auxOutputEnabled,
		autoTriggerMs)) != PICO_OK)
	{
		printf("SetTrigger:ps4000aSetTriggerChannelProperties ------ Ox%08x \n", status);
		return status;
	}

	if (nTriggerConditions != 0)
	{
		info = (PS4000A_CONDITIONS_INFO)(PS4000A_CLEAR | PS4000A_ADD); // Clear and add trigger condition specified unless no trigger conditions have been specified
	}
	
	if ((status = ps4000aSetTriggerChannelConditions(unit->handle, triggerConditions, nTriggerConditions, info) != PICO_OK))
	{
		printf("SetTrigger:ps4000aSetTriggerChannelConditions ------ 0x%08x \n", status);
		return status;
	}

	if ((status = ps4000aSetTriggerChannelDirections(unit->handle, directions, nDirections)) != PICO_OK)
	{
		printf("SetTrigger:ps4000aSetTriggerChannelDirections ------ 0x%08x \n", status);
		return status;
	}

	if ((status = ps4000aSetTriggerDelay(unit->handle, delay)) != PICO_OK)
	{
		printf("SetTrigger:ps4000aSetTriggerDelay ------ 0x%08x \n", status);
		return status;
	}

	if ((status = ps4000aSetPulseWidthQualifierProperties(unit->handle, pwq->direction, pwq->lower,
		pwq->upper, pwq->type)) != PICO_OK)
	{
		printf("SetTrigger:ps4000aSetPulseWidthQualifierProperties ------ 0x%08x \n", status);
		return status;
	}

	// Clear and add pulse width qualifier condition, clear if no pulse width qualifier has been specified
	if (pwq->nConditions != 0)
	{
		pwqInfo = (PS4000A_CONDITIONS_INFO)(PS4000A_CLEAR | PS4000A_ADD);
	}

	if ((status = ps4000aSetPulseWidthQualifierConditions(unit->handle, pwq->conditions, pwq->nConditions, pwqInfo)) != PICO_OK)
	{
		printf("SetTrigger:ps4000aSetPulseWidthQualifierConditions ------ 0x%08x \n", status);
		return status;
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
	struct tPwq pulseWidth;
	struct tPS4000ADirection directions;

	memset(&directions, 0, sizeof(struct tPS4000ADirection));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block immediate...\n");
	printf("Press a key to start\n");
	_getch();

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit, NULL, 0, NULL, 0, &directions, 1, &pulseWidth, 0, 0, 0);

	BlockDataHandler(unit, "First 10 readings\n", 0);
}

/****************************************************************************
* CollectBlockEts
*  this function demonstrates how to collect a block of
*  data using equivalent time sampling (ETS).
****************************************************************************/
void CollectBlockEts(UNIT * unit)
{
	int32_t ets_sampletime;
	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS4000A_CHANNEL_A].range, unit);
	uint32_t delay = 0;
	
	struct tPwq pulseWidth;
	struct tPS4000ADirection directions;

	struct tPS4000ATriggerChannelProperties sourceDetails = {	triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS4000A_CHANNEL_A,
		PS4000A_LEVEL };

	struct tPS4000ACondition conditions[1] = {{ PS4000A_CHANNEL_A, PS4000A_CONDITION_TRUE }};

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	directions.channel = (conditions[0]).source;
	directions.direction = PS4000A_RISING;

	printf("Collect ETS block...\n");
	
	printf("Collects when value rises past %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper,	unit->channelSettings[PS4000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count

	printf(scaleVoltages? "mV\n" : "ADC Counts\n");
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	//Trigger enabled
	//Rising edge
	//Threshold = 1000mV
	SetTrigger(unit, &sourceDetails, 1, conditions, 1, &directions, 1, &pulseWidth, delay, 0, 0);

	ps4000aSetEts(unit->handle, PS4000A_ETS_FAST, 20, 4, &ets_sampletime);
	printf("ETS Sample Time is: %d\n", ets_sampletime);

	BlockDataHandler(unit, "Ten readings after trigger\n", BUFFER_SIZE / 10 - 5); // 10% of data is pre-trigger

	ps4000aSetEts(unit->handle, PS4000A_ETS_OFF, 0, 0, &ets_sampletime);
}

/****************************************************************************
* CollectBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void CollectBlockTriggered(UNIT * unit)
{
	int16_t triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS4000A_CHANNEL_A].range, unit);

	struct tPS4000ATriggerChannelProperties sourceDetails = {	triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS4000A_CHANNEL_A,
		PS4000A_LEVEL};

	struct tPS4000ACondition conditions = {sourceDetails.channel, PS4000A_CONDITION_TRUE};

	struct tPwq pulseWidth;

	struct tPS4000ADirection directions;
	directions.channel = conditions.source;
	directions.direction = PS4000A_RISING;

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block triggered...\n");
	printf("Collects when value rises past %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[sourceDetails.channel].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");

	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */
	SetTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, 1, &pulseWidth, 0, 0, 0);

	BlockDataHandler(unit, "Ten readings after trigger\n", 0);
}

/****************************************************************************
* CollectRapidBlock
*  This function demonstrates how to collect a set of captures using
*  rapid block mode.
****************************************************************************/
void CollectRapidBlock(UNIT * unit)
{
	int16_t i;
	int16_t channel;
	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS4000A_CHANNEL_A].range, unit);

	uint16_t capture;

	int32_t nMaxSamples;
	int32_t timeIndisposed;

	uint32_t nCaptures;
	uint32_t nSamples = 1000;
	uint32_t nCompletedCaptures;
	
	int16_t ***rapidBuffers;
	int16_t *overflow;
	
	PICO_STATUS status;
	
	struct tPS4000ATriggerChannelProperties sourceDetails = { triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS4000A_CHANNEL_A,
		PS4000A_LEVEL};

	struct tPS4000ACondition conditions = { PS4000A_CHANNEL_A,	PS4000A_CONDITION_TRUE };

	struct tPwq pulseWidth;

	struct tPS4000ADirection directions;
	directions.channel = conditions.source;
	directions.direction = PS4000A_RISING;

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect rapid block triggered...\n");
	
	printf("Collects when value rises past %d",	scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS4000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");
	printf("Press any key to abort\n");

	SetDefaults(unit);

	// Trigger enabled
	SetTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, 1, &pulseWidth, 0, 0, 0);

	//Set the number of captures
	nCaptures = 10;

	//Segment the memory
	status = ps4000aMemorySegments(unit->handle, nCaptures, &nMaxSamples);

	//Set the number of captures
	status = ps4000aSetNoOfCaptures(unit->handle, nCaptures);

	//Run
	timebase = 7; // 10 MS/s

	status = ps4000aRunBlock(unit->handle, 0, nSamples, timebase, &timeIndisposed, 0, CallBackBlock, NULL); 

	if (status != PICO_OK)
	{
		printf("BlockDataHandler:ps4000aRunBlock ------ 0x%08x \n", status);	
	}

	//Wait until data ready
	g_ready = 0;

	while(!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (!g_ready)
	{
		_getch();

		status = ps4000aStop(unit->handle);
		status = ps4000aGetNoOfCaptures(unit->handle, &nCompletedCaptures);
		
		printf("Rapid capture aborted. %u complete blocks were captured\n", nCompletedCaptures);
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
		if (unit->channelSettings[channel].enabled)
		{
			rapidBuffers[channel] = (int16_t **) calloc(nCaptures, sizeof(int16_t*));
		}
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
				status = ps4000aSetDataBuffer(unit->handle, (PS4000A_CHANNEL)channel, rapidBuffers[channel][capture], nSamples, capture, PS4000A_RATIO_MODE_NONE);
			}
		}
	}

	// Get data
	status = ps4000aGetValuesBulk(unit->handle, &nSamples, 0, nCaptures - 1, 1, PS4000A_RATIO_MODE_NONE, overflow);

	if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
		printf("\nPower Source Changed. Data collection aborted.\n");
	}

	if (status == PICO_OK)
	{
		// Print first 10 samples from each capture
		for (capture = 0; capture < nCaptures; capture++)
		{
			printf("\nCapture %d:-\n\n", capture + 1);

			for (channel = 0; channel < unit->channelCount; channel++)
			{
				printf("Channel %c:\t", 'A' + channel);
			}

			printf("\n");

			for(i = 0; i < 10; i++)
			{
				for (channel = 0; channel < unit->channelCount; channel++)
				{
					if (unit->channelSettings[channel].enabled)
					{
						printf("   %6d       ", scaleVoltages ?
							adc_to_mv(rapidBuffers[channel][capture][i], unit->channelSettings[PS4000A_CHANNEL_A +channel].range, unit)	// If scaleVoltages, print mV value
							: rapidBuffers[channel][capture][i]);																	// else print ADC Count
					}
				}
				printf("\n");
			}
		}
	}

	// Stop
	status = ps4000aStop(unit->handle);

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
}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void set_info(UNIT * unit)
{
	int8_t description [11][25]= { "Driver Version",
		"USB Version",
		"Hardware Version",
		"Variant Info",
		"Serial",
		"Cal Date",
		"Kernel Version",
		"Digital H/W",
		"Analogue H/W",
		"Firmware 1",
		"Firmware 2"};

	int16_t i = 0;
	int16_t requiredSize = 0;
	int8_t line [80] = {0};

	int16_t minArbitraryWaveformValue = 0;
	int16_t maxArbitraryWaveformValue = 0;

	int32_t variant;

	uint32_t minArbitraryWaveformBufferSize = 0;
	uint32_t maxArbitraryWaveformBufferSize = 0;

	PICO_STATUS status = PICO_OK;

	if (unit->handle) 
	{
		for (i = 0; i < 11 && status == PICO_OK; i++)
		{
			status = ps4000aGetUnitInfo(unit->handle, (int8_t *)line, sizeof (line), &requiredSize, i);

			// info = 3 - PICO_VARIANT_INFO
			if (i == PICO_VARIANT_INFO)
			{
				variant = atoi(line);
				memcpy(&(unit->modelString), line, sizeof(unit->modelString)==5?5:sizeof(unit->modelString));
			}
			else if (i == PICO_BATCH_AND_SERIAL)	// info = 4 - PICO_BATCH_AND_SERIAL
			{
				memcpy(&(unit->serial), line, requiredSize);
			}

			printf("%s: %s\n", description[i], line);
		}

		printf("\n");

		// Find the maxiumum AWG buffer size
		status = ps4000aSigGenArbitraryMinMaxValues(unit->handle, &minArbitraryWaveformValue, &maxArbitraryWaveformValue, &minArbitraryWaveformBufferSize, &maxArbitraryWaveformBufferSize);

		switch (variant)
		{
			case MODEL_PS4824:
				unit->model			= MODEL_PS4824;
				unit->sigGen		= SIGGEN_AWG;
				unit->firstRange	= PS4000A_10MV;
				unit->lastRange		= PS4000A_50V;
				unit->channelCount	= OCTO_SCOPE;
				unit->hasETS			= FALSE;
				unit->AWGFileSize	= maxArbitraryWaveformBufferSize;
				break;

			case MODEL_PS4225:
				unit->model			= MODEL_PS4225;
				unit->sigGen		= SIGGEN_NONE;
				unit->firstRange	= PS4000A_50MV;
				unit->lastRange		= PS4000A_200V;
				unit->channelCount	= DUAL_SCOPE;
				unit->hasETS			= FALSE;
				unit->AWGFileSize	= 0;
				break;

			case MODEL_PS4425:
				unit->model			= MODEL_PS4425;
				unit->sigGen		= SIGGEN_NONE;
				unit->firstRange	= PS4000A_50MV;
				unit->lastRange		= PS4000A_200V;
				unit->channelCount	= QUAD_SCOPE;
				unit->hasETS			= FALSE;
				unit->AWGFileSize	= 0;
				break;

			case MODEL_PS4444:
				unit->model = MODEL_PS4444;
				unit->sigGen = SIGGEN_NONE;
				unit->firstRange = PS4000A_10MV;
				unit->lastRange = PS4000A_50V;
				unit->channelCount = QUAD_SCOPE;
				unit->hasETS = FALSE;
				unit->AWGFileSize = 0;
				break;

			default:
				unit->model			= MODEL_NONE;
				break;
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
				scanf_s("%hd", &(unit->channelSettings[ch].range));
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
				unit->channelSettings[ch].range = PS4000A_MAX_RANGES-1;
			}
		}
		printf(count == 0? "\n** At least 1 channel must be enabled **\n\n":"");
	}
	while(count == 0);	// must have at least one channel enabled

	SetDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
*
* Select timebase, set time units as nano seconds
*
****************************************************************************/
void SetTimebase(UNIT * unit)
{
	float timeInterval;
	int32_t maxSamples;

	printf("Specify desired timebase: ");
	fflush(stdin);
	scanf_s("%ud", &timebase);

	while (ps4000aGetTimebase2(unit->handle, timebase, BUFFER_SIZE, &timeInterval, &maxSamples, 0))
	{
		timebase++;  // Increase timebase if the one specified can't be used. 
	}

	printf("Timebase used %u = %.1f ns sample interval\n", timebase, timeInterval);
}

/****************************************************************************************************
* Sets the signal generator
* - allows user to set frequency and waveform
* - allows for custom waveform (values -32768..32767) of up to 16384 samples (for the PicoScope 4824)
****************************************************************************************************/
void SetSignalGenerator(UNIT * unit)
{
	PICO_STATUS status;
	int16_t waveform;
	uint32_t frequency = 1;
	int8_t fileName [128];
	FILE * fp = NULL;
	int16_t * arbitraryWaveform;
	int32_t waveformSize = 0;
	uint32_t pkpk = 4000000;	//2V
	int32_t offset = 0;
	int8_t ch;
	int16_t choice;
	uint32_t delta;

	while (_kbhit())			// use up keypress
	{
		_getch();
	}

	do
	{
		printf("\nSignal Generator\n================\n");
		printf("0 - SINE         1 - SQUARE\n");
		printf("2 - TRIANGLE     3 - DC VOLTAGE\n");

		if (unit->sigGen == SIGGEN_AWG)
		{
			printf("4 - RAMP UP      5 - RAMP DOWN\n");
			printf("6 - SINC         7 - GAUSSIAN\n");
			printf("8 - HALF SINE    A - AWG WAVEFORM\n");
		}
		printf("F - SigGen Off\n\n");

		ch = _getch();

		if (ch >= '0' && ch <='9')
		{
			choice = ch -'0';
		}
		else
		{
			ch = toupper(ch);
		}
	}
	while((unit->sigGen == SIGGEN_FUNCTGEN && ch != 'F' &&
		(ch < '0' || ch > '3')) || (unit->sigGen == SIGGEN_AWG && ch != 'A' && ch != 'F' && (ch < '0' || ch > '8')));

	if (ch == 'F')			// If we're going to turn off siggen
	{
		printf("Signal generator Off\n");
		waveform = PS4000A_DC_VOLTAGE;		// DC Voltage
		pkpk = 0;							// 0V
		waveformSize = 0;
	}
	else
	{
		if (ch == 'A' && unit->sigGen == SIGGEN_AWG)		// Set the AWG
		{
			arbitraryWaveform = (int16_t*) calloc( unit->AWGFileSize, sizeof(int16_t));
			memset(arbitraryWaveform, 0, unit->AWGFileSize * sizeof(int16_t));

			waveformSize = 0;

			printf("Select a waveform file to load: ");
			scanf_s("%s", fileName, 128);

			if (fopen_s(&fp, fileName, "r") == 0)
			{ // Having opened file, read in data - one number per line
				while (EOF != fscanf_s(fp, "%hi", (arbitraryWaveform + waveformSize)) && waveformSize++ < unit->AWGFileSize - 1);
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
					waveform = PS4000A_SINE;
					break;

				case 1:
					waveform = PS4000A_SQUARE;
					break;

				case 2:
					waveform = PS4000A_TRIANGLE;
					break;

				case 3:
					waveform = PS4000A_DC_VOLTAGE;
					do
					{
						printf("\nEnter offset in uV: (0 to 2000000)\n"); // Ask user to enter DC offset level;
						scanf_s("%u", &offset);
					} while (offset < 0 || offset > 2000000);
					break;

				case 4:
					waveform = PS4000A_RAMP_UP;
					break;

				case 5:
					waveform = PS4000A_RAMP_DOWN;
					break;

				case 6:
					waveform = PS4000A_SINC;
					break;

				case 7:
					waveform = PS4000A_GAUSSIAN;
					break;

				case 8:
					waveform = PS4000A_HALF_SINE;
					break;

				default:
					waveform = PS4000A_SINE;
					break;
			}
		}

		if (waveform < 8 || (ch == 'A' && unit->sigGen == SIGGEN_AWG))	// Find out frequency if required
		{
			do
			{
				printf("\nEnter frequency in Hz: (1 to 1000000)\n"); // Ask user to enter signal frequency, signal will be output at constant frequency
				scanf_s("%u", &frequency);
			} while (frequency <= 0 || frequency > 1000000);
		}

		if (waveformSize > 0)
		{
			// Find the delta phase corresponding to the frequency
			status = ps4000aSigGenFrequencyToPhase(unit->handle, frequency, PS4000A_SINGLE, waveformSize, &delta);

			status = ps4000aSetSigGenArbitrary(	unit->handle,
				0,				// offset voltage
				pkpk,			// PkToPk in microvolts. Max = 4000000 uV  +2v to -2V
				delta,			// start delta
				delta,			// stop delta
				0,
				0,
				arbitraryWaveform,
				waveformSize,
				(PS4000A_SWEEP_TYPE)0,
				(PS4000A_EXTRA_OPERATIONS)0,
				PS4000A_SINGLE,
				0,
				0,
				PS4000A_SIGGEN_RISING,
				PS4000A_SIGGEN_NONE,
				0);

			printf(status?"\nps4000aSetSigGenArbitrary: Status Error 0x%x \n":"", status);	// If status != 0, show the error
		}
		else
		{
			status = ps4000aSetSigGenBuiltIn(unit->handle,
				offset,
				pkpk,
				(PS4000A_WAVE_TYPE) waveform,
				(double) frequency,
				(double) frequency,
				0,
				0,
				(PS4000A_SWEEP_TYPE) 0,
				(PS4000A_EXTRA_OPERATIONS) 0,
				0,
				0,
				(PS4000A_SIGGEN_TRIG_TYPE) 0,
				(PS4000A_SIGGEN_TRIG_SOURCE) 0,
				0);

			printf(status?"\nps4000aSetSigGenBuiltIn: Status Error 0x%x \n":"", status);		// If status != 0, show the error
		}
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
	struct tPS4000ADirection directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tPS4000ADirection));

	SetDefaults(unit);

	printf("Collect streaming...\n");
	printf("Data is written to disk file (stream.txt)\n");
	printf("Press a key to start\n");
	_getch();

	/* Trigger disabled	*/
	SetTrigger(unit, NULL, 0, NULL, 0, &directions, 1, &pulseWidth, 0, 0, 0);

	StreamDataHandler(unit, 0);
}

/****************************************************************************
* CollectStreamingTriggered
*  this function demonstrates how to collect a stream of data
*  from the unit (start collecting on trigger)
***************************************************************************/
void CollectStreamingTriggered(UNIT * unit)
{
	int16_t triggerVoltage = mv_to_adc(1000,	unit->channelSettings[PS4000A_CHANNEL_A].range, unit); // ChannelInfo stores ADC counts
	struct tPwq pulseWidth;

	struct tPS4000ATriggerChannelProperties sourceDetails = { triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS4000A_CHANNEL_A,
		PS4000A_LEVEL };

	struct tPS4000ACondition conditions [1] = {{PS4000A_CHANNEL_A,	PS4000A_CONDITION_TRUE }};

	struct tPS4000ADirection directions [1];
	directions[0].channel = conditions[0].source;
	directions[0].direction = PS4000A_RISING;

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect streaming triggered...\n");
	printf("Data is written to disk file (stream.txt)\n");
	printf("Press a key to start\n");
	_getch();
	SetDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */
	SetTrigger(unit, &sourceDetails, 1, conditions, 1, directions, 1, &pulseWidth, 0, 0, 0);

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
void DisplaySettings(UNIT *unit)
{
	int32_t ch;
	int32_t voltage;

	printf("\nChannel Voltage Settings:\n\n");

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

	printf("\nReadings will be scaled in (%s)\n\n", (scaleVoltages)? ("mV") : ("ADC counts"));
}

/****************************************************************************
* OpenDevice
* Parameters
* - unit        pointer to the UNIT structure, where the handle will be stored
* - serial		pointer to the char array containing serial number
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS OpenDevice(UNIT *unit, int8_t *serial)
{
	PICO_STATUS status;
	int16_t asyncStatus = 0;
	int16_t progressPercent = 0;
	int16_t complete = 0;
	int16_t count = 0;

	if (serial == NULL)
	{
		status = ps4000aOpenUnit(&unit->handle, NULL);
	}
	else
	{
		status = ps4000aOpenUnit(&unit->handle, (int8_t *) serial);
	}

	unit->openStatus = (int16_t) status;
	unit->complete = 1;

	return status;
}

/****************************************************************************
* HandleDevice
* Parameters
* - unit        pointer to the UNIT structure, where the handle will be stored
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS HandleDevice(UNIT * unit)
{
	int16_t value = 0;
	int32_t i;
	struct tPwq pulseWidth;
	struct tPS4000ADirection directions;
	PICO_STATUS currentPowerStatus;

	if (unit->openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || unit->openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
		unit->openStatus = (int16_t) ChangePowerSource(unit->handle, unit->openStatus);

		currentPowerStatus = ps4000aCurrentPowerSource(unit->handle);

		if (currentPowerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
		{
			printf("USB Powered");
		}
		else
		{
			printf("5 V Power Supply Connected");
		}

		printf("\n");
	}

	printf("Handle: %d\n", unit->handle);
	
	if (unit->openStatus != PICO_OK && unit->openStatus != PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
		printf("Unable to open device\n");
		printf("Error code : 0x%08x\n", (uint32_t) unit->openStatus);
		while(!_kbhit());
		exit(99); // exit program
	}

	printf("Device opened successfully, cycle %d\n", ++cycles);
	// setup device info - unless it's set already
	if (unit->model == MODEL_NONE)
	{
		set_info(unit);
	}

	timebase = 1;

	ps4000aMaximumValue(unit->handle, &value);
	unit->maxADCValue = value;

	ps4000aCurrentPowerSource(unit->handle);

	for (i = 0; i < unit->channelCount; i++)
	{
		unit->channelSettings[i].enabled = TRUE;
		unit->channelSettings[i].DCcoupled = TRUE;
		unit->channelSettings[i].range = PS4000A_5V;
		unit->channelSettings[i].analogueOffset = 0.0f;
	}

	memset(&directions, 0, sizeof(struct tPS4000ADirection));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit, NULL, 0, NULL, 0, &directions, 1, &pulseWidth, 0, 0, 0);

	return unit->openStatus;
}

/****************************************************************************
* CloseDevice
****************************************************************************/
void CloseDevice(UNIT *unit)
{
	ps4000aCloseUnit(unit->handle); 
}

/****************************************************************************
* MainMenu
* Controls default functions of the seelected unit
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void MainMenu(UNIT *unit)
{
	int8_t ch = '.';

	while (ch != 'X')
	{
		DisplaySettings(unit);

		printf("\n");
		printf("Please select an operation:\n\n");

		printf("B - Immediate block                           V - Set voltages\n");
		printf("T - Triggered block                           I - Set timebase\n");

		if (unit->hasETS)
		{
			printf("E - Collect a block of data using ETS         A - ADC counts/mV\n");
		}
		else
		{
			printf("A - ADC counts/mV\n");
		}
		
		printf("R - Collect set of rapid captures\n");
		printf("S - Immediate streaming\n");
		printf("W - Triggered streaming\n");
		
		if (unit->sigGen != SIGGEN_NONE)
		{
			printf("G - Signal generator\n");
		}

		printf("                                              X - Exit\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");

		switch (ch) 
		{
			case 'B':
				CollectBlockImmediate(unit);
				break;

			case 'T':
				CollectBlockTriggered(unit);
				break;

			case 'R':
				CollectRapidBlock(unit);
				break;

			case 'S':
				CollectStreamingImmediate(unit);
				break;

			case 'W':
				CollectStreamingTriggered(unit);
				break;

			case 'E':
				if (unit->hasETS == FALSE)
				{
					printf("This model does not support ETS.\n\n");
					break;
				}

				CollectBlockEts(unit);
				break;

			case 'G':
				if (unit->sigGen == SIGGEN_NONE)
				{
					printf("This model does not have a signal generator.\n\n");
					break;
				}

				SetSignalGenerator(unit);
				break;

			case 'V':
				SetVoltages(unit);
				break;

			case 'I':
				SetTimebase(unit);
				break;

			case 'A':
				scaleVoltages = !scaleVoltages;
				break;

			case 'X':
				printf("Exit main menu.");
				break;

			default:
				printf("Invalid operation\n");
				break;
		}
	}
}


/****************************************************************************
* main
*
***************************************************************************/
int32_t main(void)
{
	int8_t ch;
	uint16_t devCount = 0, listIter = 0,	openIter = 0;
	//device indexer -  64 chars - 64 is maximum number of PicoScope devices handled by driver
	int8_t devChars[] =
			"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#";
	PICO_STATUS status = PICO_OK;
	UNIT allUnits[MAX_PICO_DEVICES];
	int16_t count = 0;
	int8_t serials[100];
	int16_t serialsLength = 100;


	printf("PS4000A driver example program\n");
	printf("\nEnumerating Units...\n");

	status = ps4000aEnumerateUnits(&count, serials, &serialsLength);

	if (status == PICO_OK)
	{
		printf("Found %d devices - serial numbers: %s\n", count, serials);
	}

	do
	{
		status = OpenDevice(&(allUnits[devCount]), NULL);

		if (status == PICO_OK || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_NOT_CONNECTED)
		{
			allUnits[devCount++].openStatus = (int16_t) status;
		}

	} while(status != PICO_NOT_FOUND);

	if (devCount == 0)
	{
		printf("Picoscope devices not found\n");
		_getch();
		return 1;
	}
	
	// If there is only one device, open and handle it here
	if (devCount == 1)
	{
		printf("Found one device, opening...\n\n");
		status = allUnits[0].openStatus;

		if (status == PICO_OK || status == PICO_POWER_SUPPLY_NOT_CONNECTED || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			set_info(&allUnits[0]);
			status = HandleDevice(&allUnits[0]);
		}

		if (status != PICO_OK && status != PICO_POWER_SUPPLY_NOT_CONNECTED)
		{
			printf("Picoscope devices open failed, error code 0x%x\n",(uint32_t)status);
			_getch();
			return 1;
		}

		MainMenu(&allUnits[0]);
		CloseDevice(&allUnits[0]);
		printf("Exit...\n");
		return 0;
	}
	else
	{
		// More than one unit
		printf("Found %d devices, initializing...\n\n", devCount);

		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
			{
				set_info(&allUnits[listIter]);
				openIter++;
			}
		}
	}

	// None
	if (openIter == 0)
	{
		printf("Picoscope devices init failed\n");
		_getch();
		return 1;
	}

	// Just one - handle it here
	if (openIter == 1)
	{
		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (!(allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT))
			{
				break;
			}
		}

		printf("One device opened successfully\n");
		printf("Model\t: %s\nS/N\t: %s\n", allUnits[listIter].modelString, allUnits[listIter].serial);
		status = HandleDevice(&allUnits[listIter]);

		if (status != PICO_OK)
		{
			printf("Picoscope device open failed, error code 0x%x\n", (uint32_t)status);
			return 1;
		}

		MainMenu(&allUnits[listIter]);
		CloseDevice(&allUnits[listIter]);
		printf("Exit...\n");
		return 0;
	}
	printf("Found %d devices, pick one to open from the list:\n", devCount);

	for (listIter = 0; listIter < devCount; listIter++)
	{
		printf("%c) Picoscope %7s S/N: %s\n", devChars[listIter],
				allUnits[listIter].modelString, allUnits[listIter].serial);
	}

	printf("ESC) Cancel\n");

	ch = '.';

	// If Escape
	while (ch != 27)
	{
		ch = _getch();

		// If Escape
		if (ch == 27)
		{
			continue;
		}

		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (ch == devChars[listIter])
			{
				printf("Option %c) selected, opening Picoscope %7s S/N: %s\n",
						devChars[listIter], allUnits[listIter].modelString,
						allUnits[listIter].serial);

				if ((allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED))
				{
					status = HandleDevice(&allUnits[listIter]);
				}

				if (status != PICO_OK)
				{
					printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status);
					_getch();
					return 1;
				}

				MainMenu(&allUnits[listIter]);

				printf("Found %d devices, pick one to open from the list:\n",devCount);

				for (listIter = 0; listIter < devCount; listIter++)
				{
					printf("%c) Picoscope %7s S/N: %s\n", devChars[listIter],
							allUnits[listIter].modelString,
							allUnits[listIter].serial);
				}

				printf("ESC) Cancel\n");
			}
		}
	}

	for (listIter = 0; listIter < devCount; listIter++)
	{
		CloseDevice(&allUnits[listIter]);
	}

	printf("Exit...\n");
	return 0;
}
