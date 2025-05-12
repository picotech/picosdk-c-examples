/*******************************************************************************
 *
 * Filename: Libps6000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 6000 Series (ps6000a) devices,
 *   for RapidBlock captures.
 *
 * Copyright (C) 2013-2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include "../../shared/PicoScaling.h"
#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"

#include "./Libps60000a.h"

/* Headers for Windows */
#ifdef _WIN32
#include "ps6000aApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <libps6000a/ps6000aApi.h>
#ifndef PICO_STATUS
#include <libps6000a/PicoStatus.h>
#endif
#include "../../shared/PicoScaling.h"
#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"

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

int16_t   		g_ready = FALSE;

int8_t RapidBlockFile[20] = "rapidblock.txt";
FILE* fp = NULL;

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase;
extern const uint64_t constBufferSize;
/***************************************************************************/

/****************************************************************************
* Block Callback
* used by ps6000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
//void PREF4 CallBackBlock( int16_t handle, PICO_STATUS status, void * pParameter)
static void PREF4 CallBackBlock(int16_t handle, PICO_STATUS status, void* pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
		///*((BOOL*)pParameter) = TRUE;
	}
}

/****************************************************************************
* CollectRapidBlock
*  This function demonstrates how to collect a set of captures using
*  rapid block mode.
*
****************************************************************************/
void rapidblockDataHandler(GENERICUNIT* unit, int8_t* text, int32_t offset)
{
	PICO_STATUS status = 0; 
	int16_t i;
	int16_t channel;
	uint64_t capture;

	int64_t nMaxSamples = 0;
	double timeIndisposed = 0;

	int16_t*** minBuffers;
	int16_t*** maxBuffers;

	uint64_t nCaptures;
	uint64_t nCompletedCaptures;
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);//bitwise OR flags for first buffer that is set

	//Capture settings
	uint64_t nSamples = constBufferSize;	//Set the number of samples per capture
	nCaptures = 3;				//Set the number of captures

	//Buffers settings (Set DownSampling mode and ratio)
	struct tbuffer_settings bufferSettings;
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = PICO_RATIO_MODE_AGGREGATE; //PICO_RATIO_MODE_RAW; // 
	bufferSettings.downSampleRatio = 16;
	bufferSettings.nSamples = constBufferSize;

	//printf(scaleVoltages ? "Volts\n" : "ADC Counts\n");
	printf("Press any key to abort\n");

	setDefaults(unit);

	//Segment the memory
	status = ps6000aMemorySegments(unit->handle, nCaptures, &nMaxSamples);

	//Set the number of captures
	status = ps6000aSetNoOfCaptures(unit->handle, nCaptures);

	//Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes;;// to store buffer sizes
	pico_create_multibuffers(unit, bufferSettings, (int32_t)nCaptures, &minBuffers, &maxBuffers, &multiBufferSizes);

	// Create Overflow Array Buffers
	int16_t* overflowArray;
	overflowArray = (int16_t*)calloc(nCaptures, sizeof(int16_t));

	printf("\nTimebase: %lu  SampleInterval: %le seconds\n", timebase, unit->timeInterval);
	printf("%llu Captures each with %llu Samples\n", nCaptures, nSamples);
	if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_RAW)
		printf("DownSampling Mode is set to: None\n");
	if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_AGGREGATE)
		printf("DownSampling Mode is set to: Aggregate (Min. and Max. values)\n");
	if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_DECIMATE)
		printf("DownSampling Mode is set to: Decimate\n");
	if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_AVERAGE)
		printf("DownSampling Mode is set to: Average\n");
	if (bufferSettings.downSampleRatioMode != PICO_RATIO_MODE_RAW)
		printf("DownSampling Ratio is set to: %llu\n", bufferSettings.downSampleRatio);

	//Start acquisition
	status = ps6000aRunBlock(unit->handle,
		0,
		nSamples,
		timebase,
		&timeIndisposed,
		0,
		CallBackBlock,
		NULL);

	if (status != PICO_OK)
	{
		printf("BlockDataHandler:ps6000aRunBlock ------ 0x%08x \n", status);
	}

	//Wait until data ready
	g_ready = 0;

	while (!g_ready && !_kbhit())
	{
		Sleep(1);
	}

	if (!g_ready)
	{
		_getch();

		status = ps6000aStop(unit->handle);
		status = ps6000aGetNoOfCaptures(unit->handle, &nCompletedCaptures);

		printf("Rapid capture aborted. %llu complete blocks were captured\n", nCompletedCaptures);
		printf("\nPress any key...\n\n");
		_getch();

		if (nCompletedCaptures == 0)
		{
			return;
		}

		// Only display the blocks that were captured
		nCaptures = nCompletedCaptures;
	}
	
	// SetDataBuffers with API
	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++)
			{
				status = ps6000aSetDataBuffers(unit->handle,
					(PICO_CHANNEL)channel,
					maxBuffers[capture][channel],
					minBuffers[capture][channel],
					(int32_t)nSamples,
					PICO_INT16_T, //PICO_DATA_TYPE
					capture,
					bufferSettings.downSampleRatioMode,
					action_flag);
				action_flag = PICO_ADD;//all subsequent calls use ADD!

				if (status != PICO_OK)
				{
					printf("RapidBlockDataHandler:ps6000aSetDataBuffers ------ 0x%08x, for channel %d \n", status, channel);
				}
			}
		}
	}

	// Get data from device
	status = ps6000aGetValuesBulk(unit->handle,
		0,						//Start Index for each segment
		&nSamples,				//Number of samples for each segment
		0,						//From Segment
		nCaptures - 1,			//To Segment
		bufferSettings.downSampleRatio,						//Down Sample Ratio
		bufferSettings.downSampleRatioMode,				//Down Sample Ratio mode
		overflowArray);				//Array of Channel overrage flags

	if (status == PICO_OK)
	{
		// Print first 10 samples from the first 2 captures
		for (capture = 0; capture < 2; capture++) //nCaptures
		{
			printf("\nCapture %llu:- (Max. Values)\n\n", capture + 1);

			for (channel = 0; channel < unit->channelCount; channel++)
			{
				printf("Channel %c:\t", 'A' + channel);
				
			}

			printf("\n");

			for (i = 0; i < 10; i++)
			{
				for (channel = 0; channel < unit->channelCount; channel++)
				{
					if (unit->channelSettings[channel].enabled)
					{
						if (maxBuffers[capture][channel])//Check buffer is not NULL
						{//3.3e //6d
							printf("%3.3e\t", scaleVoltages ?
								adc_to_mv(maxBuffers[capture][channel][i],
									unit->channelSettings[PICO_CHANNEL_A + channel].range,
									unit->maxADCValue)														// If scaleVoltages, print mV value
								: maxBuffers[capture][channel][i]);
						}// else print ADC Count
					}
					else
					{
						printf("   ---  \t"); 
					}
				}
				printf("\n");
			}
		}

		//Get scaling Info for each channel
		PICO_PROBE_SCALING enabledChannelsScaling[PS6000A_MAX_CHANNELS] = {0}; //[unit->channelCount]; //Move to global/golobal struture
		PICO_PROBE_SCALING channelRangeInfoTemp;
		for (i = 0; i < unit->channelCount; i++)
		{
			if (unit->channelSettings[i].enabled)
			{
				getRangeScaling(unit->channelSettings[PICO_CHANNEL_A + 0].range, &channelRangeInfoTemp);
				if (channelRangeInfoTemp.ProbeEnum > PICO_X10_PROBE_RANGES) // Print nonstandard ranges info
				{
					printf("Channel %c:\tEnum range:%d text range:%s MinS:%f MaxS:%f UnitText:%s\n", 'A' + i,
						channelRangeInfoTemp.ProbeEnum,
						channelRangeInfoTemp.Probe_Range_text,
						channelRangeInfoTemp.MinScale,
						channelRangeInfoTemp.MaxScale,
						channelRangeInfoTemp.Unit_text);
				}
			enabledChannelsScaling[i] = channelRangeInfoTemp;
			}
		}

		// Print each segment capture to a file
		printf("\nWriting each of: %lld channel buffer sets to a file.\n", multiBufferSizes.numberOfBuffers);
		WriteArrayToFilesGeneric(
			unit,
			minBuffers,
			maxBuffers,
			multiBufferSizes,
			enabledChannelsScaling,
			"RapidBlockCaptureNo_",
			0,						// Triggersample
			overflowArray);	
	}

	// Stop device
	status = ps6000aStop(unit->handle);

	// Free memory
	clearDataBuffers(unit);
	free(overflowArray);

	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++)
			{
				free(maxBuffers[capture][channel]);
				free(minBuffers[capture][channel]);
			}
		}
	}

	for (capture = 0; capture < nCaptures; capture++)
	{
		free(maxBuffers[capture]);
		free(minBuffers[capture]);
	}
	free(maxBuffers);
	free(minBuffers);
}

/****************************************************************************
* collectRapidBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
void collectRapidBlockImmediate(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;

	printf("Collect RapidBlock immediate...\n");
	printf("Press a key to start\n");
	_getch();

	setDefaults(unit);

	/* Trigger disabled	*/
	status = ps6000aSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

	rapidblockDataHandler(unit, (int8_t*)"First 10 readings\n", 0);
}

/****************************************************************************
* collectRapidBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void collectRapidBlockTriggered(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;

	//Set triggerLevelADC to +50% of set channel voltage range
	int16_t triggerLevelADC = mv_to_adc((double)inputRanges[unit->channelSettings[PICO_CHANNEL_A].range] / 2,
		unit->channelSettings[PICO_CHANNEL_A].range,
		unit->maxADCValue);

	struct tPicoTriggerChannelProperties sourceDetails = {
											triggerLevelADC,	//thresholdUpper
											256 * 10,			//thresholdUpperHysteresis
											triggerLevelADC,	//thresholdLower
											256 * 10,			//thresholdLowerHysteresis
											PICO_CHANNEL_A,		//channel - PICO_CHANNEL
	};

	struct tPicoCondition conditions = { sourceDetails.channel,	//PICO_CHANNEL
											PICO_CONDITION_TRUE	//PICO_TRIGGER_STATE - true/false/Don't care
	};

	struct tPicoDirection directions = {
		directions.channel = conditions.source,
		directions.direction = PICO_RISING,
		directions.thresholdMode = PICO_LEVEL };

	//Create Pulse Width Qualifier structure with settings
	struct tPwq pulseWidth;
	memset(&pulseWidth, 0, sizeof(struct tPwq));//zero out pulseWidth

	printf("Collect RapidBlock triggered...\n");
	printf("Trigger Channel is %c\n", 'A' + sourceDetails.channel);
	printf("Collects when value rises past %d", scaleVoltages ?
		(int16_t)adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[sourceDetails.channel].range, unit->maxADCValue)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	
	printf(scaleVoltages ? " mV\n" : " ADC Counts\n");

	printf("Press a key to start...\n");
	_getch();

	setDefaults(unit);

	status = SetTrigger(unit,
		&sourceDetails, 1,	//channelProperties //nChannelProperties
		1,					//auxOutputEnable
		&conditions, 1,
		&directions, 1,
		&pulseWidth,		//PWQ
		0, 0);				//TrigDelay //AutoTrigger_us

	rapidblockDataHandler(unit, (int8_t*)"First 10 readings after trigger\n", 0);
}

