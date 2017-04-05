/**************************************************************************
 *
 * Filename: ps6000con.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use the
 *   PicoScope 6000 Series API functions to perform operations using 
 *	 a PicoScope 6000 Series Oscilloscope.
 * 
 *	Supported PicoScope models:
 *
 *		PicoScope 6402 & 6402A/B/C/D
 *		PicoScope 6403 & 6403A/B/C/D
 *		PicoScope 6404 & 6404A/B/C/D
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *	  Collect data using Equivalent Time Sampling
 *	  Collect data using rapid block mode (with trigger)
 *    Collect a stream of data immediately
 *    Collect a stream of data when a trigger event occurs
 *    Set Signal Generator, using standard or custom signals
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (Win32/x64)
 *			Ensure that the 32-/64-bit ps6000.lib can be located
 *			Ensure that the ps6000Api.h file can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps6000.lib to the project (Microsoft C only)
 *			 Add ps6000Api.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps6000 driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the ps5000acon application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2009 - 2017 Pico Technology Ltd. See LICENSE file for terms.
 *
 **************************************************************************/

#include <stdio.h>

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "ps6000Api.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <libps6000-1.4/ps6000Api.h>
#ifndef PICO_STATUS
#include <libps6000-1.4/PicoStatus.h>
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

int32_t fopen_s(FILE ** a, const int8_t * b, const int8_t * c)
{
FILE * fp = fopen(b,c);
*a = fp;
return (fp>0)?0:-1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif


#define VERSION		1
#define ISSUE		3

int32_t cycles = 0;

#define BUFFER_SIZE 	10000 // Used for block and streaming mode examples

// AWG Parameters
#define	AWG_DAC_FREQUENCY		200e6
#define	AWG_PHASE_ACCUMULATOR	4294967296.0

typedef enum {
	MODEL_NONE = 0,
	MODEL_PS6402  = 0x6402, //Bandwidth: 350MHz, Memory: 32MS, AWG
	MODEL_PS6402A = 0xA402, //Bandwidth: 250MHz, Memory: 128MS, FG
	MODEL_PS6402B = 0xB402, //Bandwidth: 250MHz, Memory: 256MS, AWG
	MODEL_PS6402C = 0xC402, //Bandwidth: 350MHz, Memory: 256MS, AWG
	MODEL_PS6402D = 0xD402, //Bandwidth: 350MHz, Memory: 512MS, AWG
	MODEL_PS6403  = 0x6403, //Bandwidth: 350MHz, Memory: 1GS, AWG
	MODEL_PS6403A = 0xA403, //Bandwidth: 350MHz, Memory: 256MS, FG
	MODEL_PS6403B = 0xB403, //Bandwidth: 350MHz, Memory: 512MS, AWG
	MODEL_PS6403C = 0xC403, //Bandwidth: 350MHz, Memory: 512MS, AWG
	MODEL_PS6403D = 0xD403, //Bandwidth: 350MHz, Memory: 1GS, AWG
	MODEL_PS6404  = 0x6404, //Bandwidth: 500MHz, Memory: 1GS, AWG
	MODEL_PS6404A = 0xA404, //Bandwidth: 500MHz, Memory: 512MS, FG
	MODEL_PS6404B = 0xB404, //Bandwidth: 500MHz, Memory: 1GS, AWG
	MODEL_PS6404C = 0xC404, //Bandwidth: 350MHz, Memory: 1GS, AWG
	MODEL_PS6404D = 0xD404, //Bandwidth: 350MHz, Memory: 2GS, AWG
	MODEL_PS6407  = 0x6407, //Bandwidth: 1GHz,	 Memory: 2GS, AWG

} MODEL_TYPE;

typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
}CHANNEL_SETTINGS;

typedef struct tTriggerDirections
{
	enum enPS6000ThresholdDirection channelA;
	enum enPS6000ThresholdDirection channelB;
	enum enPS6000ThresholdDirection channelC;
	enum enPS6000ThresholdDirection channelD;
	enum enPS6000ThresholdDirection ext;
	enum enPS6000ThresholdDirection aux;
}TRIGGER_DIRECTIONS;

typedef struct tPwq
{
	struct tPS6000PwqConditions * conditions;
	int16_t nConditions;
	enum enPS6000ThresholdDirection direction;
	uint32_t lower;
	uint32_t upper;
	PS6000_PULSE_WIDTH_TYPE type;
}PWQ;

typedef struct
{
	int16_t handle;
	MODEL_TYPE				model;
	int8_t					modelString[8];
	int8_t					serial[10];
	int16_t					complete;
	int16_t					openStatus;
	int16_t					openProgress;
	PS6000_RANGE			firstRange;
	PS6000_RANGE			lastRange;
	int16_t					channelCount;
	BOOL					AWG;
	CHANNEL_SETTINGS		channelSettings [PS6000_MAX_CHANNELS];
	int32_t					awgBufferSize;
}UNIT;

uint32_t	timebase = 8;
int16_t		oversample = 1;
int32_t      scaleVoltages = TRUE;

uint16_t inputRanges [PS6000_MAX_RANGES] = {	10,
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
BOOL        g_ready = FALSE;
int64_t		g_times [PS6000_MAX_CHANNELS];
int16_t     g_timeUnit;
uint32_t    g_sampleCount;
uint32_t	g_startIndex;
int16_t     g_autoStopped;
int16_t     g_trig = 0;
uint32_t	g_trigAt = 0;
int16_t		g_overflow;
int8_t      BlockFile[20]  = "block.txt";
int8_t      ETSBlockFile[20]  = "ETS_block.txt";
int8_t      StreamFile[20] = "stream.txt";

typedef struct tBufferInfo
{
	UNIT * unit;
	int16_t **driverBuffers;
	int16_t **appBuffers;

} BUFFER_INFO;

/****************************************************************************
* Callback
* Used by PS6000 data streaming collection calls, on receipt of data.
* Used to set global flags etc checked by user routines
*
* In this example, a BUFFER_INFO structure holding pointers to the 
* driver and application buffers for a device is used to allow the data 
* from the driver buffers to be copied into the application buffers.
*
****************************************************************************/
void PREF4 CallBackStreaming(	int16_t handle,
								uint32_t noOfSamples,
								uint32_t startIndex,
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
		for (channel = 0; channel < bufferInfo->unit->channelCount; channel++)
		{
			if (bufferInfo->unit->channelSettings[channel].enabled)
			{
				if (bufferInfo->appBuffers && bufferInfo->driverBuffers)
				{
					// Copy data...

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
* Callback
* used by PS6000 data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 CallBackBlock(	int16_t handle,
							PICO_STATUS status,
							void * pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
	}
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

	printf("\nReadings will be scaled in %s\n\n", (scaleVoltages)? ("millivolts") : ("ADC counts"));

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
}

/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void SetDefaults(UNIT * unit)
{
	PICO_STATUS status;
	int32_t i;

	status = ps6000SetEts(unit->handle, PS6000_ETS_OFF, 0, 0, NULL); // Turn off ETS

	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		status = ps6000SetChannel(unit->handle, (PS6000_CHANNEL) (PS6000_CHANNEL_A + i),
			unit->channelSettings[PS6000_CHANNEL_A + i].enabled,
			(PS6000_COUPLING)unit->channelSettings[PS6000_CHANNEL_A + i].DCcoupled,
			(PS6000_RANGE)unit->channelSettings[PS6000_CHANNEL_A + i].range, 0, PS6000_BW_FULL);
	}

}

/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t ch)
{
	return (raw * inputRanges[ch]) / PS6000_MAX_VALUE;
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
	return (mv * PS6000_MAX_VALUE) / inputRanges[ch];
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
void BlockDataHandler(UNIT * unit, int8_t * text, int32_t offset, int16_t etsModeSet)
{
	int16_t * buffers[PS6000_MAX_CHANNEL_BUFFERS];

	int32_t i, j;
	int32_t timeIndisposed;

	uint32_t sampleCount = BUFFER_SIZE;
	uint32_t maxSamples;
	uint32_t segmentIndex = 0;

	float timeInterval = 0.00f;

	int64_t * etsTimes; // Buffer for ETS time data

	FILE * fp = NULL;
	PICO_STATUS status;

	for (i = 0; i < unit->channelCount; i++) 
	{
		if(unit->channelSettings[i].enabled)
		{
			buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

			status = ps6000SetDataBuffers(unit->handle, (PS6000_CHANNEL) i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, PS6000_RATIO_MODE_NONE);

			printf("BlockDataHandler:ps6000SetDataBuffers(channel %d) ------ %d \n", i, status);
		}
	}

	// Set up ETS time buffers if ETS Block mode data is being captured
	if(etsModeSet)
	{
		etsTimes = (int64_t *) calloc(sampleCount, sizeof (int64_t));   
		status = ps6000SetEtsTimeBuffer(unit->handle, etsTimes, sampleCount);
	}	

	/*  Find the maximum number of samples, the time interval (in timeUnits),
	*		 the most suitable time units, and the maximum oversample at the current timebase*/
	while ((status = ps6000GetTimebase2(unit->handle, timebase, sampleCount, &timeInterval, oversample, &maxSamples, segmentIndex)) != PICO_OK )
	{
		timebase++;
	}

	if(!etsModeSet)
	{
		printf("\nTimebase: %lu  SampleInterval: %.2f ns\n", timebase, timeInterval);
	}

	/* Start the device collecting, then wait for completion. */
	g_ready = FALSE;

	status = ps6000RunBlock(unit->handle, 0, sampleCount, timebase, oversample,	&timeIndisposed, segmentIndex, CallBackBlock, NULL);

	if(status != PICO_OK)
	{
		printf("BlockDataHandler:ps6000RunBlock ------ 0x%08lx \n", status);
		status = ps6000Stop(unit->handle);
		return;
	}

	printf("Waiting for trigger...Press a key to abort\n");

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if(g_ready) 
	{
		status = ps6000GetValues(unit->handle, 0, (uint32_t*) &sampleCount, 1, PS6000_RATIO_MODE_NONE, 0, NULL);

		if(status != PICO_OK)
		{
			printf("BlockDataHandler:ps6000GetValues ------ %i \n", status);
		}

		/* Print out the first 10 readings, converting the readings to mV if required */
		printf("\n");
		printf(text);
		printf("\nValues are in %s\n\n", ( scaleVoltages ) ? ("millivolts") : ("ADC Counts"));

		for (j = 0; j < unit->channelCount; j++) 
		{
			if (unit->channelSettings[j].enabled) 
			{
				printf("Channel%c:\t", 'A' + j);
			}
		}
		printf("\n\n");
		
		for (i = offset; i < offset+10; i++) 
		{
			for (j = 0; j < unit->channelCount; j++) 
			{
				if (unit->channelSettings[j].enabled) 
				{
					printf("  %6d        ", scaleVoltages ? 
					adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS6000_CHANNEL_A + j].range)		// If scaleVoltages, print mV value
					:buffers[j * 2][i]);																// else print ADC Count
				}
			}
			printf("\n");
		}

		sampleCount = min(sampleCount, BUFFER_SIZE);

		if(etsModeSet)
		{
			fopen_s(&fp, ETSBlockFile, "w");
		}
		else
		{
			fopen_s(&fp, BlockFile, "w");
		}

		if (fp != NULL)
		{
			if(etsModeSet)
			{
				fprintf(fp, "ETS Block Data log\n\n");
			}
			else
			{
				fprintf(fp, "Block Data log\n\n");
			}

			fprintf(fp,"Results shown for each of the enabled Channels are......\n");
			fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

			if(etsModeSet)
			{
				fprintf(fp, "Time (fs)\t");
			}
			else
			{
				fprintf(fp, "Time (ns)\t");
			}

			for (i = 0; i < unit->channelCount; i++) 
			{
				if (unit->channelSettings[i].enabled) 
				{
					fprintf(fp," Ch   Max ADC  Max mV   Min ADC  Min mV  ");
				}
			}
			fprintf(fp, "\n");

			for (i = 0; (uint32_t)i < sampleCount; i++) 
			{
				if(etsModeSet)
				{
					fprintf(fp, "%d ", etsTimes[i]);
				}
				else
				{
					fprintf(fp, "%u ", g_times[0] + (uint32_t)(i * timeInterval));
				}
				
				for (j = 0; j < unit->channelCount; j++) 
				{
					if (unit->channelSettings[j].enabled) 
					{
						if(etsModeSet)
						{
							fprintf(fp,
								"Ch%C  %d = %dmV   ",
								'A' + j,
								buffers[j * 2][i],
								adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS6000_CHANNEL_A + j].range));
						}
						else
						{
							fprintf(fp,
								"Ch%C  %d = %dmV, %5d = %dmV   ",
								'A' + j,
								buffers[j * 2][i],
								adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS6000_CHANNEL_A + j].range),
								buffers[j * 2 + 1][i],
								adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PS6000_CHANNEL_A + j].range));
						}
					}
				}
				fprintf(fp, "\n");
			}
		}
		else
			printf(	"Cannot open the file %s for writing.\n"
							"Please ensure that you have permission to access.\n", BlockFile);
	} 
	else 
	{
		printf("Data collection aborted\n");
		_getch();
	}

	// Stop device after retrieving data values
	status = ps6000Stop(unit->handle);

	if(status != PICO_OK)
	{
		printf("BlockDataHandler:ps6000Stop ------ %i \n", status);
	}

	if (fp != NULL)
	{
		fclose(fp);
	}

	for (i = 0; i < unit->channelCount; i++) 
	{
		if(unit->channelSettings[i].enabled)
		{
			free(buffers[i * 2]);
			free(buffers[i * 2 + 1]);
		}
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
void StreamDataHandler(UNIT * unit, uint32_t preTrigger)
{
	uint32_t i, j;
	uint32_t sampleCount= BUFFER_SIZE; /*  Make sure buffer large enough to collect data on each iteration */
	FILE * fp;
	int16_t * buffers[PS6000_MAX_CHANNEL_BUFFERS];
	int16_t * appBuffers[PS6000_MAX_CHANNEL_BUFFERS]; // Application buffers to copy data into
	PICO_STATUS status;
	uint32_t sampleInterval = 1;
	int32_t index = 0;
	uint32_t totalSamples;
	uint32_t previousTotal = 0;
	int16_t autoStop = TRUE;
	uint32_t postTrigger = 1000000;
	uint32_t downsampleRatio = 5;
	uint32_t triggeredAt = 0;
	
	BUFFER_INFO bufferInfo;

	for (i = PS6000_CHANNEL_A; (int32_t)i < unit->channelCount; i++) // create data buffers
	{
		if(unit->channelSettings[i].enabled)
		{
			buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			
			status = ps6000SetDataBuffers(	unit->handle, (PS6000_CHANNEL)i,  buffers[i * 2], buffers[i * 2 + 1], 
												sampleCount, PS6000_RATIO_MODE_AGGREGATE);

			appBuffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			appBuffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
		}
	}

	// Set information in structure
	bufferInfo.unit = unit;
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;
	
	if (autoStop)
	{
		printf("\nStreaming Data for %lu samples", postTrigger / downsampleRatio);

		// we pass 0 for preTrigger if we're not setting up a trigger
		printf(preTrigger?" after the trigger occurs\nNote: %lu Pre Trigger samples before Trigger arms\n\n":"\n\n", preTrigger / downsampleRatio);
	}
	else
	{
		printf("\nStreaming Data continually...\n\n");
	}

	g_autoStopped = FALSE;

	status = ps6000RunStreaming(unit->handle, &sampleInterval, PS6000_US, preTrigger, postTrigger - preTrigger, 
									autoStop, downsampleRatio, PS6000_RATIO_MODE_AGGREGATE, sampleCount);

	printf(status?"\nps6000RunStreaming status = 0x%x\n":"", status);

	printf("Streaming data...Press a key to abort\n");

	fopen_s(&fp, StreamFile, "w");

	if (fp != NULL)
	{
		fprintf(fp,"For each of the enabled Channels, results shown are....\n");
		fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

		for (i = 0; (int32_t)i < unit->channelCount; i++) 
		{
			fprintf(fp,"   Max ADC   Max mV   Min ADC   Min mV");
		}

		fprintf(fp, "\n");
	}


	totalSamples = 0;

	while (!_kbhit() && !g_autoStopped)
	{
		/* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
		Sleep(1);
		g_ready = FALSE;

		status = ps6000GetStreamingLatestValues(unit->handle, CallBackStreaming, &bufferInfo);

		if (status != PICO_OK && status != PICO_BUSY)
		{
			printf("Streaming status return 0x%x\n",status);
			break;
		}

		index ++;

		if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
		{
			if (g_trig)
			{
				triggeredAt = totalSamples += g_trigAt;		// calculate where the trigger occurred in the total samples collected
			}

			previousTotal = totalSamples;
			totalSamples += g_sampleCount;
			
			printf("\nCollected %3li samples, index = %5lu, Total: %6d samples ", g_sampleCount, g_startIndex, totalSamples);

			if (g_trig)
			{
				printf("Trig. at index %lu Total at trigger: %lu", triggeredAt, previousTotal + (triggeredAt - g_startIndex + 1) );	// show where trigger occurred
			}
			
			if (fp != NULL)
			{
				for (i = g_startIndex; i < (g_startIndex + g_sampleCount); i++)
				{
					for (j = PS6000_CHANNEL_A; (int32_t)j < unit->channelCount; j++) 
					{
						if (unit->channelSettings[j].enabled) 
						{
							fprintf(	fp,
									"Ch%C %5d = %+5dmV, %5d = %+5dmV  ",
									'A' + j,
									appBuffers[j * 2][i],
									adc_to_mv(appBuffers[j * 2][i], unit->channelSettings[PS6000_CHANNEL_A + j].range),
									appBuffers[j * 2 + 1][i],
									adc_to_mv(appBuffers[j * 2 + 1][i], unit->channelSettings[PS6000_CHANNEL_A + j].range));
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
	
	printf("\n\n");

	if(fp != NULL) 
	{
		fclose(fp);	
	}

	ps6000Stop(unit->handle);

	if (!g_autoStopped) 
	{
		printf("data collection aborted\n");
		_getch();
	}

	for (i = PS6000_CHANNEL_A; (int32_t)i < unit->channelCount; i++) 
	{
		if(unit->channelSettings[i].enabled)
		{
			free(buffers[i * 2]);
			free(buffers[i * 2 + 1]);
		}
	}
}


/****************************************************************************
* SetTrigger
* - Used to set all trigger conditions
*****************************************************************************/
PICO_STATUS SetTrigger(	int16_t handle,
						struct tPS6000TriggerChannelProperties * channelProperties,
						int16_t nChannelProperties,
						struct tPS6000TriggerConditions * triggerConditions,
						int16_t nTriggerConditions,
						TRIGGER_DIRECTIONS * directions,
						struct tPwq * pwq,
						uint32_t delay,
						int16_t auxOutputEnabled,
						int32_t autoTriggerMs)
{
	PICO_STATUS status;

	if ((status = ps6000SetTriggerChannelProperties(handle,
													channelProperties,
													nChannelProperties,
													auxOutputEnabled,
													autoTriggerMs)) != PICO_OK) 
	{
		printf("SetTrigger:ps6000SetTriggerChannelProperties ------ %d \n", status);
		return status;
	}

	if ((status = ps6000SetTriggerChannelConditions(handle,	triggerConditions, nTriggerConditions)) != PICO_OK) 
	{
		printf("SetTrigger:ps6000SetTriggerChannelConditions ------ %d \n", status);
		return status;
	}

	if ((status = ps6000SetTriggerChannelDirections(handle,
													directions->channelA,
													directions->channelB,
													directions->channelC,
													directions->channelD,
													directions->ext,
													directions->aux)) != PICO_OK) 
	{
		printf("SetTrigger:ps6000SetTriggerChannelDirections ------ %d \n", status);
		return status;
	}


	if ((status = ps6000SetTriggerDelay(handle, delay)) != PICO_OK) 
	{
		printf("SetTrigger:ps6000SetTriggerDelay ------ %d \n", status);
		return status;
	}

	if((status = ps6000SetPulseWidthQualifier(handle, 
												pwq->conditions,
												pwq->nConditions, 
												pwq->direction,
												pwq->lower, 
												pwq->upper, 
												pwq->type)) != PICO_OK)
	{
		printf("SetTrigger:ps6000SetPulseWidthQualifier ------ %d \n", status);
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
	struct tTriggerDirections directions;

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block immediate...\n");
	printf("Press a key to start\n");
	_getch();

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	BlockDataHandler(unit, "First 10 readings\n", 0, FALSE);
}

/****************************************************************************
* CollectBlockEts
*  this function demonstrates how to collect a block of
*  data using equivalent time sampling (ETS).
****************************************************************************/
void CollectBlockEts(UNIT * unit)
{
	PICO_STATUS status;
	int32_t ets_sampletime;
	int16_t	triggerVoltage = mv_to_adc(100,	unit->channelSettings[PS6000_CHANNEL_A].range);
	uint32_t delay = 0;
	int16_t etsModeSet = FALSE;

	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	struct tPS6000TriggerChannelProperties sourceDetails = {	triggerVoltage,
																256,
																triggerVoltage,
																256,
																PS6000_CHANNEL_A,
																PS6000_LEVEL };

	struct tPS6000TriggerConditions conditions = {	PS6000_CONDITION_TRUE,
													PS6000_CONDITION_DONT_CARE,
													PS6000_CONDITION_DONT_CARE,
													PS6000_CONDITION_DONT_CARE,
													PS6000_CONDITION_DONT_CARE,
													PS6000_CONDITION_DONT_CARE,
													PS6000_CONDITION_DONT_CARE };



	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));
	directions.channelA = PS6000_RISING;

	printf("Collect ETS block...\n");
	printf("Collects when value rises past %dmV\n",	adc_to_mv(sourceDetails.thresholdUpper,	unit->channelSettings[PS6000_CHANNEL_A].range));
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	//Trigger enabled
	//Rising edge
	//Threshold = 1500mV
	status = SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, delay, 0, 0);

	status = ps6000SetEts(unit->handle, PS6000_ETS_FAST, 20, 4, &ets_sampletime);

	if(status == PICO_OK)
	{
		etsModeSet = TRUE;
	}

	printf("ETS Sample Time is: %ld picoseconds\n", ets_sampletime);

	BlockDataHandler(unit, "Ten readings after trigger\n", BUFFER_SIZE / 10 - 5, etsModeSet); // 10% of data is pre-trigger

	etsModeSet = FALSE;
}

/****************************************************************************
* CollectBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void CollectBlockTriggered(UNIT * unit)
{
	int16_t triggerVoltage		= 1000; // mV
	int32_t triggerChannel		= (int32_t) PS6000_CHANNEL_A;
	int16_t voltageRange		= inputRanges[unit->channelSettings[triggerChannel].range];
	int16_t triggerThreshold	= 0;

	struct tPS6000TriggerChannelProperties sourceDetails;
	struct tPS6000TriggerConditions conditions;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	// If the trigger voltage level is greater than the range selected, set the threshold to half
	// of the range selected e.g. for +/- 200mV, set the threshold to 100mV
	if(triggerVoltage > voltageRange)
	{
		triggerVoltage = (voltageRange / 2);
	}

	triggerThreshold = mv_to_adc(triggerVoltage, unit->channelSettings[triggerChannel].range);

	// Set trigger channel properties
	sourceDetails.thresholdUpper	= triggerThreshold;
	sourceDetails.hysteresisUpper	= 256 * 2;
	sourceDetails.thresholdLower	= triggerThreshold;
	sourceDetails.hysteresisLower	= 256 * 2;
	sourceDetails.channel			= (PS6000_CHANNEL)(triggerChannel);
	sourceDetails.thresholdMode		= PS6000_LEVEL;

	// Set trigger conditions
	conditions.channelA				= PS6000_CONDITION_TRUE;
	conditions.channelB				= PS6000_CONDITION_DONT_CARE;
	conditions.channelC				= PS6000_CONDITION_DONT_CARE;
	conditions.channelD				= PS6000_CONDITION_DONT_CARE;
	conditions.external				= PS6000_CONDITION_DONT_CARE; // Not used
	conditions.aux					= PS6000_CONDITION_DONT_CARE;
	conditions.pulseWidthQualifier	= PS6000_CONDITION_DONT_CARE;

	// Set trigger directions
	directions.channelA = PS6000_RISING;
	directions.channelB = PS6000_NONE;
	directions.channelC = PS6000_NONE;
	directions.channelD = PS6000_NONE;
	directions.ext		= PS6000_NONE;
	directions.aux		= PS6000_NONE;
	 
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block triggered...\n");
	printf("Collects when value rises past %dmV\n", adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[triggerChannel].range));

	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */
	SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0);

	BlockDataHandler(unit, "Ten readings after trigger\n", 0, FALSE);
}

/****************************************************************************
* CollectRapidBlock
*  this function demonstrates how to collect a set of captures using 
*  rapid block mode.
****************************************************************************/
void CollectRapidBlock(UNIT * unit)
{
	uint32_t nCaptures;
	uint32_t nMaxSamples, nSamples = 1000;
	uint32_t nSegments;
	int32_t timeIndisposed;
	uint32_t capture; 
	int16_t channel;
	int16_t ***rapidBuffers;
	int16_t *overflow;
	PICO_STATUS status;
	uint32_t i;
	uint32_t nCompletedCaptures;
	uint32_t segmentIndex = 0;

	float timeInterval = 0.00f;
	uint32_t maxSamples;

	int16_t triggerVoltage		= 500; // mV
	int32_t triggerChannel		= (int32_t) PS6000_CHANNEL_A;
	int16_t voltageRange		= inputRanges[unit->channelSettings[triggerChannel].range];
	int16_t triggerThreshold	= 0;

	struct tPS6000TriggerChannelProperties sourceDetails;
	struct tPS6000TriggerConditions conditions;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	// If the trigger voltage level is greater than the range selected, set the threshold to half
	// of the range selected e.g. for +/- 200mV, set the threshold to 100mV
	if(triggerVoltage > voltageRange)
	{
		triggerVoltage = (voltageRange / 2);
	}

	triggerThreshold = mv_to_adc(triggerVoltage, unit->channelSettings[triggerChannel].range);

	// Set trigger channel properties
	sourceDetails.thresholdUpper	= triggerThreshold;
	sourceDetails.hysteresisUpper	= 256 * 2;
	sourceDetails.thresholdLower	= triggerThreshold;
	sourceDetails.hysteresisLower	= 256 * 2;
	sourceDetails.channel			= (PS6000_CHANNEL)(triggerChannel);
	sourceDetails.thresholdMode		= PS6000_LEVEL;

	// Set trigger conditions
	conditions.channelA				= PS6000_CONDITION_TRUE;
	conditions.channelB				= PS6000_CONDITION_DONT_CARE;
	conditions.channelC				= PS6000_CONDITION_DONT_CARE;
	conditions.channelD				= PS6000_CONDITION_DONT_CARE;
	conditions.external				= PS6000_CONDITION_DONT_CARE; // Not used
	conditions.aux					= PS6000_CONDITION_DONT_CARE;
	conditions.pulseWidthQualifier	= PS6000_CONDITION_DONT_CARE;

	// Set trigger directions
	directions.channelA = PS6000_RISING;
	directions.channelB = PS6000_NONE;
	directions.channelC = PS6000_NONE;
	directions.channelD = PS6000_NONE;
	directions.ext		= PS6000_NONE;
	directions.aux		= PS6000_NONE;


	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect rapid block triggered...\n");
	printf("Collects when value rises past %dmV\n",	
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[triggerChannel].range));
	printf("Press any key to abort\n");

	SetDefaults(unit);

	// Trigger enabled
	SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0);
	
	// Set the number of segments
	nSegments = 16;

	//Set the number of captures
	nCaptures = 10;

	//Segment the memory
	status = ps6000MemorySegments(unit->handle, nSegments, &nMaxSamples);

	//Set the number of captures
	status = ps6000SetNoOfCaptures(unit->handle, nCaptures);

	//Run
	
	while (ps6000GetTimebase2(unit->handle, timebase, nSamples, &timeInterval, oversample, &maxSamples, segmentIndex))
	{
		timebase++;
	}

	printf("Timebase: %d Sample interval: %.2f ns\n\n", timebase, timeInterval);

	status = ps6000RunBlock(unit->handle, 0, nSamples, timebase, 1, &timeIndisposed, segmentIndex, CallBackBlock, NULL);

	//Wait until data ready
	g_ready = 0;

	while(!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if(!g_ready)
	{
		_getch();
		status = ps6000Stop(unit->handle);
		status = ps6000GetNoOfCaptures(unit->handle, &nCompletedCaptures);

		printf("Rapid capture aborted. %d complete blocks were captured\n", nCompletedCaptures);
		printf("\nPress any key...\n\n");
		_getch();

		if(nCompletedCaptures == 0)
		{
			return;
		}

		//Only display the blocks that were captured
		nCaptures = (uint16_t)nCompletedCaptures;
	}

	//Allocate memory
	rapidBuffers = (int16_t***) calloc(unit->channelCount, sizeof(int16_t*));
	overflow = (int16_t*) calloc(unit->channelCount * nCaptures, sizeof(int16_t));

	// Memory for segments
	for (channel = (int16_t) PS6000_CHANNEL_A; channel < unit->channelCount; channel++) 
	{
		if(unit->channelSettings[channel].enabled)
		{
			rapidBuffers[channel] = (int16_t**) calloc(nCaptures, sizeof(int16_t*));
		}
	}

	// Memory for buffers for channel - segment combination
	for (channel = (int16_t) PS6000_CHANNEL_A; channel < unit->channelCount; channel++) 
	{	
		if(unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++) 
			{
				rapidBuffers[channel][capture] = (int16_t*) calloc(nSamples, sizeof(int16_t));
			}
		}
	}

	for (channel = (int16_t) PS6000_CHANNEL_A; channel < unit->channelCount; channel++) 
	{
		if(unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++) 
			{
				status = ps6000SetDataBufferBulk(unit->handle, (PS6000_CHANNEL) channel, rapidBuffers[channel][capture], nSamples, capture, PS6000_RATIO_MODE_NONE);
			}
		}
	}

	// Get data
	status = ps6000GetValuesBulk(unit->handle, &nSamples, 0, nCaptures - 1, 1, PS6000_RATIO_MODE_NONE, overflow);

	// Stop
	status = ps6000Stop(unit->handle);

	// Print first 10 samples from each capture
	for (capture = 0; capture < nCaptures; capture++)
	{
		printf("Capture %d\n", capture + 1);
		printf("----------\n");

		for(channel = (int16_t) PS6000_CHANNEL_A; channel < unit->channelCount; channel++)
		{
			if(unit->channelSettings[channel].enabled)
			{
				printf("Channel %C\t", 'A' + channel);
			}
		}

		printf("\n\n");

		
		for(i = 0; i < 10; i++)
		{
			for(channel = (int16_t) PS6000_CHANNEL_A; channel < unit->channelCount; channel++)
			{
				if(unit->channelSettings[channel].enabled)
				{
					printf("%d\t\t", rapidBuffers[channel][capture][i]);
				}
			}

			printf("\n");
		}

		printf("\n");
	}

	// Free memory
	free(overflow);

	for (channel = (int16_t) PS6000_CHANNEL_A; channel < unit->channelCount; channel++) 
	{	
		if(unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++) 
			{
				free(rapidBuffers[channel][capture]);
			}
		}
	}

	for (channel = (int16_t) PS6000_CHANNEL_A; channel < unit->channelCount; channel++) 
	{
		if(unit->channelSettings[channel].enabled)
		{
			free(rapidBuffers[channel]);
		}
	}

	free(rapidBuffers);

	// Set number of segments and captures back to 1
	status = ps6000MemorySegments(unit->handle, 1, &nMaxSamples);
	status = ps6000SetNoOfCaptures(unit->handle, 1);
}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void set_info(UNIT * unit)
{
	int16_t i = 0;
	int16_t r = 20;
	int8_t line [20];
	int32_t variant;

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

	if (unit->handle) 
	{
		for (i = 0; i < 11; i++) 
		{
			ps6000GetUnitInfo(unit->handle, line, sizeof (line), &r, i);
		
			if (i == 3) 
			{
				// info = 3 - PICO_VARIANT_INFO
		
				variant = atoi(line);
				memcpy(&(unit->modelString),line,sizeof(unit->modelString)==7?7:sizeof(unit->modelString));
		
				//To identify A or B model variants.....
				if (strlen(line) == 4)							// standard, not A, B, C or D, convert model number into hex i.e 6402 -> 0x6402
				{
					variant += 0x4B00;
				}
				else
				{
					if (strlen(line) == 5)						// A, B, C or D variant unit 
					{
						line[4] = toupper(line[4]);

						switch(line[4])
						{
							case 65: // i.e 6402A -> 0xA402
								variant += 0x8B00;
								break;
							case 66: // i.e 6402B -> 0xB402
								variant += 0x9B00;
								break;
							case 67: // i.e 6402C -> 0xC402
								variant += 0xAB00;
								break;
							case 68: // i.e 6402D -> 0xD402
								variant += 0xBB00;
								break;
							default:
								break;
						}
					}
				}
			}

			if (i == 4) 
			{
				// info = 4 - PICO_BATCH_AND_SERIAL
				ps6000GetUnitInfo(unit->handle, unit->serial, sizeof (unit->serial), &r, PICO_BATCH_AND_SERIAL);
			}

			printf("%s: %s\n", description[i], line);
		}

		switch (variant)
		{
			case MODEL_PS6402:
				unit->model		= MODEL_PS6402;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++) 
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6402A:
				unit->model		= MODEL_PS6402A;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = FALSE;
				unit->awgBufferSize = 0;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++) 
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6402B:
				unit->model		= MODEL_PS6402B;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6402C:
				unit->model		= MODEL_PS6402C;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = FALSE;
				unit->awgBufferSize = 0;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6402D:
				unit->model		= MODEL_PS6402D;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = PS640X_C_D_MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6403:
				unit->model		= MODEL_PS6403;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++) 
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6403A:
				unit->model		= MODEL_PS6403;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = FALSE;
				unit->awgBufferSize = 0;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++) 
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6403B:
				unit->model		= MODEL_PS6403B;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->AWG = MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6403C:
				unit->model		= MODEL_PS6403C;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = FALSE;
				unit->awgBufferSize = 0;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6403D:
				unit->model		= MODEL_PS6403D;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = PS640X_C_D_MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6404:
				unit->model		= MODEL_PS6404;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++) 
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6404A:
				unit->model		= MODEL_PS6404;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = FALSE;
				unit->awgBufferSize = 0;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++) 
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6404B:
				unit->model		= MODEL_PS6404B;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6404C:
				unit->model		= MODEL_PS6404C;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = 0;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6404D:
				unit->model		= MODEL_PS6404D;
				unit->firstRange = PS6000_50MV;
				unit->lastRange = PS6000_20V;
				unit->channelCount = 4;
				unit->AWG = TRUE;
				unit->awgBufferSize = PS640X_C_D_MAX_SIG_GEN_BUFFER_SIZE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++)
				{
					unit->channelSettings[i].range = PS6000_5V;
					unit->channelSettings[i].DCcoupled = PS6000_DC_1M;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			case MODEL_PS6407:
				unit->model		= MODEL_PS6407;
				unit->firstRange = PS6000_100MV;
				unit->lastRange = PS6000_100MV;
				unit->channelCount = 4;
				unit->AWG = TRUE;

				for (i = 0; i < PS6000_MAX_CHANNELS; i++) 
				{
					unit->channelSettings[i].range = PS6000_100MV;
					unit->channelSettings[i].DCcoupled = PS6000_DC_50R;
					unit->channelSettings[i].enabled = TRUE;
				}
				break;

			default:
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
	int16_t loop;

	/* See what ranges are available... */
	for (i = unit->firstRange; i <= unit->lastRange; i++) 
	{
		printf("%d -> %d mV\n", i, inputRanges[i]);
	}

	do
	{
		/* Ask the user to select a range */
		printf("Specify voltage range (%d..%d)\n", unit->firstRange, unit->lastRange);

		// In this example, keep channel A turned on, so we can trigger from it.
		printf("99 - switches channel off (ChB ChC & ChD)\n");
		for (ch = 0; ch < unit->channelCount; ch++) 
		{
			printf("\n");
			do 
			{
				printf("Channel %c: ", 'A' + ch);
				fflush(stdin);
				scanf_s("%hd", &unit->channelSettings[ch].range);
				
				if(ch == 0)		// In this example, keep channel A turned on, so we can trigger from it.
				{
					loop = (unit->channelSettings[ch].range < unit->firstRange || unit->channelSettings[ch].range > unit->lastRange);
				}
				else
				{
					loop = unit->channelSettings[ch].range != 99 && (unit->channelSettings[ch].range < unit->firstRange || unit->channelSettings[ch].range > unit->lastRange);
				}

			} while (loop);
			//while (unit->channelSettings[ch].range != 99 && (unit->channelSettings[ch].range < unit->firstRange || unit->channelSettings[ch].range > unit->lastRange));

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
* Select timebase, set oversample to on and time units as nano seconds
*
****************************************************************************/
void SetTimebase(UNIT * unit)
{
	float timeInterval = 0.00f;
	uint32_t maxSamples;
	PICO_STATUS status;

	do
	{
		printf("Specify timebase: ");
		fflush(stdin);
		scanf_s("%lud", &timebase);

		status = ps6000GetTimebase2(unit->handle, timebase, BUFFER_SIZE, &timeInterval, 1, &maxSamples, 0);

		if(status == PICO_INVALID_TIMEBASE)
		{
			printf("Invalid timebase\n\n");
			break;
		}

	} while (status == PICO_INVALID_TIMEBASE);

	printf("Timebase %lu - %.2f ns\n", timebase, timeInterval);
	oversample = TRUE;
}

/****************************************************************************
* Sets the signal generator
* - allows user to set frequency and waveform
* - allows for custom waveform (values 0..4095) of up to 16384 samples int32_t
***************************************************************************/
void SetSignalGenerator(UNIT * unit)
{
	PICO_STATUS status;
	int16_t waveform;
	int32_t frequency = 0;
	int8_t fileName [128];
	FILE * fp;
	int16_t * arbitraryWaveform;
	int32_t waveformSize = 0;
	uint32_t pkpk = 1000000;	// +/- 500mV if 0 offset
	int32_t offset = 0;
	PS6000_EXTRA_OPERATIONS operation;
	int8_t ch;
	uint32_t delta;

	if(unit->AWG == TRUE)
	{
		arbitraryWaveform = (int16_t *) calloc(unit->awgBufferSize, sizeof(int16_t));
	}

	while (_kbhit())			// use up keypress
		_getch();

	do
	{
		printf("\nSignal Generator\n================\n");
		printf("0:\tSINE      \t6:\tGAUSSIAN\n");
		printf("1:\tSQUARE    \t7:\tHALF SINE\n");
		printf("2:\tTRIANGLE  \t8:\tDC VOLTAGE\n");
		printf("3:\tRAMP UP   \t9:\tWHITE NOISE\n");
		printf("4:\tRAMP DOWN\n");
		printf("5:\tSINC\n");
		printf(unit->AWG?"A:\tAWG WAVEFORM\t":"");
		printf("X:\tSigGen Off\n\n");

		ch = _getch();

		if (ch >= '0' && ch <='9')
		{
			waveform = ch -'0';
		}
		else
		{
			ch = toupper(ch);
		}
	}
	while(ch != 'A' && ch != 'X' && (ch < '0' || ch > '9'));


	if(ch == 'X')				// If we're going to turn off siggen
	{
		printf("Signal generator Off\n");
		waveform = (int16_t) PS6000_DC_VOLTAGE;	// DC Voltage
		pkpk = 0;				// 0V
		waveformSize = 0;
		operation = PS6000_ES_OFF;
	}
	else
	{
		if (ch == 'A' && unit->AWG)		// Set the AWG
		{
			waveformSize = 0;

			printf("Select a waveform file to load: ");
			scanf_s("%s", fileName, 128);

			if (fopen_s(&fp, fileName, "r") == 0) 
			{ 
				// Having opened file, read in data - one number per line (at most 16384 or 65536 lines)
				// Values should be in range (0 to 4095)
				while (EOF != fscanf_s(fp, "%hi", (arbitraryWaveform + waveformSize))&& waveformSize++ < unit->awgBufferSize - 1);
				fclose(fp);

				printf("Waveform size: %lu\n", waveformSize);

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
			switch (waveform)
			{
				case PS6000_DC_VOLTAGE:
					do 
					{
						printf("\nEnter offset in uV: (0 to 2500000)\n"); // Ask user to enter DC offset level;
						scanf_s("%lu", &offset);
					} while (offset < 0 || offset > 10000000);
					operation = PS6000_ES_OFF;
					break;

				case 9:
					operation = PS6000_WHITENOISE;
					break;

				default:
					operation = PS6000_ES_OFF;
					offset = 0;
					break;
			}
		}
	}

	if(waveform < PS6000_DC_VOLTAGE || (ch == 'A' && unit->AWG)) // Find out frequency if required
	{
		do 
		{
			printf("\nEnter frequency in Hz: (0.03 to 20000000)\n"); // Ask user to enter signal frequency
			scanf_s("%d", &frequency);
		} while (frequency <= 0.00 || frequency > 20000000.00);
	}

	if (waveformSize > 0)		
	{
		//delta = ((1.0 * frequency * waveformSize) / unit->awgBufferSize) * AWG_PHASE_ACCUMULATOR * (1 / AWG_DAC_FREQUENCY);

		// Convert frequency to phase
		status = ps6000SigGenFrequencyToPhase(unit->handle, frequency, PS6000_SINGLE, waveformSize, &delta);
		
		status = ps6000SetSigGenArbitrary(	unit->handle,
											0, 
											1000000, 
											delta,
											delta,
											0, 
											0, 
											arbitraryWaveform, 
											waveformSize, 
											(PS6000_SWEEP_TYPE) 0,
											PS6000_ES_OFF, 
											PS6000_SINGLE, 
											0, 
											0, 
											PS6000_SIGGEN_RISING,
											PS6000_SIGGEN_NONE, 
											0);

		printf(status?"\nps6000SetSigGenArbitrary: Status Error 0x%x \n":"", (uint32_t)status);		// If status != 0, show the error
	} 
	else 
	{
		status = ps6000SetSigGenBuiltInV2(unit->handle, offset, pkpk, waveform, frequency, frequency, 0, 0, 
			(PS6000_SWEEP_TYPE) 0, operation, 0, 0, (PS6000_SIGGEN_TRIG_TYPE) 0, (PS6000_SIGGEN_TRIG_SOURCE) 0, 0);
		
		printf(status?"\nps6000SetSigGenBuiltIn: Status Error 0x%x \n":"", (uint32_t)status);		// If status != 0, show the error
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
	printf("Press a key to start\n");
	_getch();

	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	StreamDataHandler(unit, 0);
}

/****************************************************************************
* CollectStreamingTriggered
*  this function demonstrates how to collect a stream of data
*  from the unit (start collecting on trigger)
***************************************************************************/
void CollectStreamingTriggered(UNIT * unit)
{
	int16_t triggerVoltage		= 500; // mV
	int32_t triggerChannel		= (int32_t) PS6000_CHANNEL_A;
	int16_t voltageRange		= inputRanges[unit->channelSettings[triggerChannel].range];
	int16_t triggerThreshold	= 0;

	struct tPS6000TriggerChannelProperties sourceDetails;
	struct tPS6000TriggerConditions conditions;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	// If the trigger voltage level is greater than the range selected, set the threshold to half
	// of the range selected e.g. for +/- 200mV, set the threshold to 100mV
	if(triggerVoltage > voltageRange)
	{
		triggerVoltage = (voltageRange / 2);
	}

	triggerThreshold = mv_to_adc(triggerVoltage, unit->channelSettings[triggerChannel].range);

	// Set trigger channel properties
	sourceDetails.thresholdUpper	= triggerThreshold;
	sourceDetails.hysteresisUpper	= 256 * 2;
	sourceDetails.thresholdLower	= triggerThreshold;
	sourceDetails.hysteresisLower	= 256 * 2;
	sourceDetails.channel			= (PS6000_CHANNEL)(triggerChannel);
	sourceDetails.thresholdMode		= PS6000_LEVEL;

	// Set trigger conditions
	conditions.channelA				= PS6000_CONDITION_TRUE;
	conditions.channelB				= PS6000_CONDITION_DONT_CARE;
	conditions.channelC				= PS6000_CONDITION_DONT_CARE;
	conditions.channelD				= PS6000_CONDITION_DONT_CARE;
	conditions.external				= PS6000_CONDITION_DONT_CARE; // Not used
	conditions.aux					= PS6000_CONDITION_DONT_CARE;
	conditions.pulseWidthQualifier	= PS6000_CONDITION_DONT_CARE;

	// Set trigger directions
	directions.channelA = PS6000_RISING;
	directions.channelB = PS6000_NONE;
	directions.channelC = PS6000_NONE;
	directions.channelD = PS6000_NONE;
	directions.ext		= PS6000_NONE;
	directions.aux		= PS6000_NONE;

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect streaming triggered...\n");
	printf("Trigger will occur when value rises past %dmV\n", adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[triggerChannel].range));
	printf("Data is written to disk file (%s)\n", StreamFile);
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
* OpenDevice
* Parameters
* - unit        pointer to the UNIT structure, where the handle will be stored
* - serial		pointer to the int8_t array containing serial number
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS OpenDevice(UNIT *unit, int8_t *serial)
{
	PICO_STATUS status;

	if (serial == NULL)
	{
		status = ps6000OpenUnit(&unit->handle, NULL);
	}
	else
	{
		status = ps6000OpenUnit(&unit->handle, serial);
	}

	unit->openStatus = status;
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

	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	printf("Handle: %d\n", unit->handle);
	if (unit->openStatus != PICO_OK)
	{
		printf("Unable to open device\n");
		printf("Error code : 0x%08x\n", (uint32_t)unit->openStatus);
		while(!_kbhit());
		exit(99); // exit program
	}

	printf("Device opened successfully, cycle %d\n\n", ++cycles);
	// setup device info - unless it's set already
	if (unit->model == MODEL_NONE)
	{
		set_info(unit);
	}
	timebase = 1;

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	SetDefaults(unit);

	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0);

	return unit->openStatus;
}

void CloseDevice(UNIT *unit)
{
	ps6000CloseUnit(unit->handle);
}

/****************************************************************************
* MainMenu
* Controls default functions of the selected unit
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
		printf("B - Immediate block                           V - Set voltages\n");
		printf("T - Triggered block                           I - Set timebase\n");
		printf("E - Collect a block of data using ETS         A - ADC counts/mV\n");
		printf("R - Collect set of rapid captures\n");
		printf("S - Immediate streaming\n");
		printf("W - Triggered streaming\n");
		printf("G - Signal generator\n");
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
			CollectBlockEts(unit);
			break;

		case 'G':
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
			
			if (scaleVoltages)
			{
				printf("Readings will be scaled in millivolts\n");
			}
			else
			{
				printf("Readings will be scaled in ADC counts\n");
			}
			break;

		case 'X':
			break;

		default:
			printf("Invalid operation\n");
			break;
		}
	}
}

int main(void)
{
#define MAX_PICO_DEVICES 64
#define TIMED_LOOP_STEP 500

	int8_t ch;
	uint16_t devCount = 0, listIter = 0,	openIter = 0;
	// Device indexer -  64 chars - 64 is maximum number of picoscope devices handled by driver
	int8_t devChars[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#";
	PICO_STATUS status = PICO_OK;
	UNIT allUnits[MAX_PICO_DEVICES];

	printf("PicoScope 6000 Series Driver Example Program\n");
	printf("\nEnumerating Units...\n");

	do
	{
		memset(&(allUnits[devCount]),0,sizeof (UNIT));
		status = OpenDevice(&(allUnits[devCount]),NULL);
		if(status == PICO_OK || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			allUnits[devCount++].openStatus = status;
		}

	} while(status != PICO_NOT_FOUND);

	if (devCount == 0)
	{
		printf("Picoscope devices not found\n");
		return 1;
	}
	// If there is only one device, open and handle it here
	if (devCount == 1)
	{
		printf("Found one device, opening...\n\n");
		status = allUnits[0].openStatus;

		if (status == PICO_OK || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			status = HandleDevice(&allUnits[0]);
		}

		if (status != PICO_OK)
		{
			printf("Picoscope devices open failed, error code 0x%x\n",(uint32_t)status);
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
			if ((allUnits[listIter].openStatus == PICO_OK ||
					allUnits[listIter].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT))
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
		return 1;
	}
	// Just one - handle it here
	if (openIter == 1)
	{
		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (!(allUnits[listIter].openStatus == PICO_OK ||
					allUnits[listIter].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT))
				break;
		}
		printf("One device openned successfuly\n");
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
	
	// If escape
	while (ch != 27)
	{
		ch = _getch();
		// If escape
		if (ch == 27)
			continue;
		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (ch == devChars[listIter])
			{
				printf("Option %c) selected, opening Picoscope %7s S/N: %s\n",
						devChars[listIter], allUnits[listIter].modelString,
						allUnits[listIter].serial);

				if ((allUnits[listIter].openStatus == PICO_OK
						|| allUnits[listIter].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED
						|| allUnits[listIter].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT))
				{
					status = HandleDevice(&allUnits[listIter]);
				}

				if (status != PICO_OK)
				{
					printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status);
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
