/*******************************************************************************
 *
 * Filename: Libpsospa.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 3XXXXE Series (psospa) devices,
 *   for RapidBlock captures.
 *
 * Copyright (C) 2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include "../../shared/PicoScaling.h"
#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"

#include "./Libpsospa.h"

/* Headers for Windows */
#ifdef _WIN32
#include "psospaApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <libpsospa/psospaApi.h>
#ifndef PICO_STATUS
#include <libpsospa/PicoStatus.h>
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
* used by psospa data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
//void PREF4 CallBackBlock( int16_t handle, PICO_STATUS status, void * pParameter)
static void PREF4 CallBackBlock(int16_t handle, PICO_STATUS status, void* pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
		//*((BOOL*)pParameter) = TRUE;
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
	//--------------------------------------------------------------------------//
	//Capture settings
	uint64_t nSamples = constBufferSize;	//Set the number of samples per capture
	uint64_t noOfPreTriggerSamples = 0;
	nCaptures = 3;				//Set the number of captures

	//Buffers settings (Set DownSampling mode and ratio)
	struct tbuffer_settings bufferSettings = {0};
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = PICO_RATIO_MODE_AGGREGATE;
	bufferSettings.downSampleRatio = 4;
	bufferSettings.nSamples = constBufferSize;
	//--------------------------------------------------------------------------//
	//printf(scaleVoltages ? "Volts\n" : "ADC Counts\n");
	printf("Press any key to abort\n");

	setDefaults(unit);

	//Segment the memory
	status = psospaMemorySegments(unit->handle, nCaptures, &nMaxSamples);

	//Set the number of captures
	status = psospaSetNoOfCaptures(unit->handle, nCaptures);

	//Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
	pico_create_multibuffers(unit, bufferSettings, (int32_t)nCaptures, &minBuffers, &maxBuffers, &multiBufferSizes);

	// Create Overflow Array Buffers
	int16_t* overflowArray;
	overflowArray = (int16_t*)calloc(nCaptures, sizeof(int16_t));

	printf("\nTimebase: %lu  SampleInterval: %le seconds\n", timebase, unit->timeInterval);
	printf("%llu Captures each with %llu ADC Samples\n", nCaptures, nSamples);
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
	status = psospaRunBlock(unit->handle,
		noOfPreTriggerSamples,
		nSamples - noOfPreTriggerSamples,
		timebase,
		&timeIndisposed,
		0,
		CallBackBlock,
		NULL);

	if (status != PICO_OK)
	{
		printf("BlockDataHandler:psospaRunBlock ------ 0x%08x \n", status);
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

		status = psospaStop(unit->handle);
		status = psospaGetNoOfCaptures(unit->handle, &nCompletedCaptures);

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
				status = psospaSetDataBuffers(unit->handle,
					(PICO_CHANNEL)channel,
					maxBuffers[capture][channel],
					minBuffers[capture][channel],
					multiBufferSizes.maxBufferSize,
					PICO_INT16_T, //PICO_DATA_TYPE
					capture,
					bufferSettings.downSampleRatioMode,
					action_flag);
				action_flag = PICO_ADD;//all subsequent calls use ADD!

				if (status != PICO_OK)
				{
					printf("RapidBlockDataHandler:psospaSetDataBuffers ------ 0x%08x, for channel %d \n", status, channel);
				}
			}
		}
	}
	//digital channels
	for (channel = 0; channel < unit->digitalPortCount; channel++)
	{
		if (unit->digitalChannelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++)
			{
				status = psospaSetDataBuffers(unit->handle,
					PICO_PORT0 + (PICO_CHANNEL)channel,
					maxBuffers[capture][channel + unit->channelCount],
					minBuffers[capture][channel + unit->channelCount],
					multiBufferSizes.maxBufferSize,
					PICO_INT16_T,
					capture,			//waveform number
					bufferSettings.downSampleRatioMode,
					action_flag);
				action_flag = PICO_ADD;//all subsequent calls use ADD!
				if (status != PICO_OK)
				{
					printf(status ? "blockDataHandler:psospaSetDataBuffers(channel %d) ------ 0x%08lx \n" : "", PICO_PORT0 + channel, status);
				}
			}
		}
	}

	// Get data from device
	status = psospaGetValuesBulk(unit->handle,
		0,						//Start Index for each segment
		&nSamples,				//Number of samples for each segment
		0,						//From Segment
		nCaptures - 1,			//To Segment
		bufferSettings.downSampleRatio,						//Down Sample Ratio
		bufferSettings.downSampleRatioMode,				//Down Sample Ratio mode
		overflowArray);				//Array of Channel overrage flags

	if (status == PICO_OK)
	{
		//Get scaling Info for each channel
		struct tPicoProbeScaling enabledChannelsScaling[PSOSPA_MAX_CHANNELS] = { 0 };
		struct tPicoProbeScaling channelRangeInfoTemp;
		for (i = 0; i < unit->channelCount; i++)
		{
			if (unit->channelSettings[i].enabled)
			{
				getRangeScaling(unit->channelSettings[PICO_CHANNEL_A + 0].range, &channelRangeInfoTemp);
				enabledChannelsScaling[i] = channelRangeInfoTemp;
			}
		}
		//Write to console
		WriteArrayToStdoutGeneric(
			unit,
			minBuffers,
			maxBuffers,
			multiBufferSizes,
			enabledChannelsScaling,
			(CAPTURE_MODE)RAPID_BLOCK,
			3,						// Number of buffers to write
			10,						// Number of samples to write
			noOfPreTriggerSamples,	// Triggersample
			overflowArray);
		// Print each segment capture to a file
		printf("\nWriting each of: %lld channel buffer sets to a file.\n", multiBufferSizes.numberOfBuffers);
		WriteArrayToFilesGeneric(
			unit,
			minBuffers,
			maxBuffers,
			multiBufferSizes,
			enabledChannelsScaling,
			"RapidBlockCaptureNo_",
			noOfPreTriggerSamples,	// Triggersample
			overflowArray,
			NULL);	

		// Get relative segment trigger timestamps (in samples)
		PICO_TRIGGER_INFO* triggerInfo;
		triggerInfo = (PICO_TRIGGER_INFO*)calloc(nCaptures, sizeof(PICO_TRIGGER_INFO));
		status = psospaGetTriggerInfo(unit->handle,
					triggerInfo,	//PICO_TRIGGER_INFO * triggerInfo,
					0,				//firstSegmentIndex,
					nCaptures		//segmentCount (number of segments)			
					);
		if (status != PICO_OK)
		{
			printf("RapidBlockDataHandler:psospaGetTriggerInfo ------ 0x%08x \n", status);
		}
		// Print first 3 trigger timestamps
		uint64_t maxprintCaptures = min(nCaptures, 3);
		for (capture = 0; capture < maxprintCaptures; capture++)
		{
			if (triggerInfo != NULL)
			{
				printf("\nCapture/segment: %llu, Trigger Timestamp: %llu", capture, triggerInfo[capture].timeStampCounter);
				if(triggerInfo[capture].status == PICO_DEVICE_TIME_STAMP_RESET || capture == 0)
				{
					printf(" Delta: NA");
				}
				else //NOTE: PICO_DEVICE_TIME_STAMP_RESET/counter wrap around is NOT accounted for. (counter is a unsigned 2^56 bits)
				{
					printf(" Delta Samples: %llu, ", triggerInfo[capture].timeStampCounter - triggerInfo[capture - 1].timeStampCounter);
					printf("Delta (seconds): %3.3e", (triggerInfo[capture].timeStampCounter - triggerInfo[capture - 1].timeStampCounter) * unit->timeInterval);
				}
			}
		}
		printf("\n");
	}
	// Stop device
	status = psospaStop(unit->handle);

	// Release buffers from API
	clearDataBuffers(unit);
	//free buffers
	pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);
	free(overflowArray);
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
	status = psospaSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

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
	// If scaleVoltages, print mV value
	// else print ADC Count
	printf("Collects when value rises past %d", scaleVoltages ?
		(int16_t)adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[sourceDetails.channel].range, unit->maxADCValue)
		: sourceDetails.thresholdUpper);
	
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

