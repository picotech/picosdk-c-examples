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

int8_t RapidBlockFile[] = "rapidBlock_Segment";
int8_t RapidBlockOverlappedFile[] = "rapidBlockOverlapped";
FILE* fp = NULL;

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase;
extern int16_t   g_ready;
extern const uint64_t constBufferSize;
/***************************************************************************/

/****************************************************************************
* rapidblockDataHandler
*  This function demonstrates how to collect a set of captures using
*  rapid block mode.
* Inputs :
* - unit : the unit to use.
* - noOfPreTriggerSamples : number of samples to capture before trigger.
* - noOfPostTriggerSamples : number of samples to capture after trigger.
* - idealTimeInterval : the desired time interval (in seconds) between samples.
* - nSamples : Set the number of samples per capture - Used by SetDataBuffers()
* - nCaptures : Set the number of captures - Used by SetNoOfCaptures()
* - ratioMode : Set the downsampling mode - Used by SetDataBuffers()
* - downSampleRatio : Set the downsampling ratio - Used by SetDataBuffers()
* Returns       none
****************************************************************************/
void rapidblockDataHandler(GENERICUNIT* unit,
							uint64_t noOfPreTriggerSamples,		// Used by RunBlock()
							uint64_t noOfPostTriggerSamples,	// Used by RunBlock()
							double idealTimeInterval,			// Used by RunBlock()
							uint64_t nSamples,					// Used by SetDataBuffers()
							uint64_t nCaptures,
							PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
							uint64_t downSampleRatio,			// Used by SetDataBuffers()
							FILE_TYPE filetype
)
{
	PICO_STATUS status = 0; 
	int16_t i;
	uint64_t capture;

	int64_t nMaxSamples = 0;
	double timeIndisposed = 0;

	int16_t*** minBuffers;
	int16_t*** maxBuffers;

	uint64_t nCompletedCaptures = 0;
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);//bitwise OR flags for first buffer that is set

	//Capture settings
	//Buffers settings (Set DownSampling mode and ratio)
	struct tbuffer_settings bufferSettings = {0};
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = ratioMode;
	bufferSettings.downSampleRatio = downSampleRatio;
	bufferSettings.nSamples = nSamples;

	//printf(scaleVoltages ? "Volts\n" : "ADC Counts\n");
	printf("Press any key to abort\n");

	setDefaults(unit);

	//Segment the memory
	status = psospaMemorySegments(unit->handle, nCaptures, &nMaxSamples);

	//Set the number of captures
	status = psospaSetNoOfCaptures(unit->handle, nCaptures);

	//Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
	pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffers, &maxBuffers, &multiBufferSizes);

	// Create Overflow Array Buffers
	int16_t* overflowArray;
	overflowArray = (int16_t*)calloc(nCaptures, sizeof(int16_t));

	// Find nearest timebase
	// Find the analogue channels that are enabled
	PICO_CHANNEL_FLAGS enabledChannelOrPortFlags = (PICO_CHANNEL_FLAGS)0;
	for (int32_t ch = 0; ch < unit->channelCount; ch++)
	{
		if (unit->channelSettings[ch].enabled)
		{
			enabledChannelOrPortFlags = enabledChannelOrPortFlags | (PICO_CHANNEL_FLAGS)(1 << ch);
		}
	}
	if (unit->digitalChannelSettings[0].enabled)
		enabledChannelOrPortFlags = enabledChannelOrPortFlags | PICO_PORT0_FLAGS;
	if (unit->digitalChannelSettings[1].enabled)
		enabledChannelOrPortFlags = enabledChannelOrPortFlags | PICO_PORT1_FLAGS;

		status = psospaNearestSampleIntervalStateless(unit->handle,
														enabledChannelOrPortFlags,
														idealTimeInterval,
														1, // round faster
														unit->resolution,
														&timebase,
														&(unit->timeInterval));
		if (status != PICO_OK)
		{
			printf("RapidBlockDataHandler:psospaNearestSampleIntervalStateless ------ 0x%08x \n", status);
			return;
		}

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
		noOfPostTriggerSamples,
		timebase,
		&timeIndisposed,
		0,
		callBackBlockReady,
		NULL);

	if (status != PICO_OK)
	{
		printf("BlockDataHandler:psospaRunBlock ------ 0x%08x \n", status);
	}

	//Wait until data ready
	g_ready = FALSE;

	while (!g_ready && !_kbhit())
	{
		Sleep(1);
	}

	if (!g_ready) // If user aborted stop the acquisition
	{
		_getch();
		printf("Rapid capture aborted. ");
		status = psospaStop(unit->handle);
	}
	// Get the number of captures that were completed
	status = psospaGetNoOfCaptures(unit->handle, &nCompletedCaptures);
	printf("%llu complete blocks were captured\n", nCompletedCaptures);
	printf("\nPress any key...\n\n");
	_getch();

	if (nCompletedCaptures == 0)
	{
		return; // Exit if no captures were made
	}

	// Only use the blocks that were captured
	nCaptures = nCompletedCaptures;
	unit->CapturesComplete = nCompletedCaptures;
	
	// SetDataBuffers with API
	SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers, &multiBufferSizes, 0, (CAPTURE_MODE)RAPID_BLOCK, 0);

	// Get data from device
	status = psospaGetValuesBulk(unit->handle,
		0,									// Start Index for each segment
		&nSamples,							// Number of samples for each segment
		0,									// From Segment
		nCaptures - 1,						// To Segment
		bufferSettings.downSampleRatio,		// Down Sample Ratio
		bufferSettings.downSampleRatioMode,	// Down Sample Ratio mode
		overflowArray);						// Array of Channel overrage flags

	if (status == PICO_OK)
	{
		// Get scaling Info for each channel
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
			(enum enCaptureMode)RAPID_BLOCK,
			3,						// Number of buffers to write
			10,						// Number of samples to write
			noOfPreTriggerSamples,	// Triggersample
			overflowArray);
		// Print each segment capture to a file
		printf("\nWriting each of: %lld channel buffer sets to a file.\n", multiBufferSizes.numberOfBuffers);

		if (filetype == FILE_TXT)
		{
			WriteArrayToFilesGeneric(
				unit,
				minBuffers,
				maxBuffers,
				multiBufferSizes,
				enabledChannelsScaling,
				RapidBlockFile,
				noOfPreTriggerSamples,	// Triggersample
				overflowArray,
				NULL);
		}
		if (filetype == FILE_BIN)
		{
			WriteArrayToFilesBinary(
				unit,
				minBuffers,
				maxBuffers,
				multiBufferSizes,
				enabledChannelsScaling,
				RapidBlockFile,
				noOfPreTriggerSamples,	// Triggersample
				overflowArray,
				NULL);
		}
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
		PICO_STATUS rapidStatus;
		for (capture = 0; capture < maxprintCaptures; capture++)
		{
			if (triggerInfo != NULL)
			{
				rapidStatus = triggerInfo[capture].status & PICO_DEVICE_TIME_STAMP_RESET;
				printf("\nCapture/segment: %llu, Trigger Timestamp: %llu", capture, triggerInfo[capture].timeStampCounter);
				if(  (rapidStatus * (uint32_t)(capture != 0)) == 0   ) // Ignore Seg #0 PICO_STATUS
				{
					printf(" Delta Samples: %llu, ", triggerInfo[capture].timeStampCounter - triggerInfo[capture - 1].timeStampCounter);
					printf("Delta (seconds): %3.3e", (triggerInfo[capture].timeStampCounter - triggerInfo[capture - 1].timeStampCounter)* unit->timeInterval);
				}
				else
				{
					//NOTE: PICO_DEVICE_TIME_STAMP_RESET/counter wrap around is NOT accounted for. (counter is a unsigned 2^56 bits)
					printf("PICO_DEVICE_TIME_STAMP_RESET--- 0x%08x, Capture %llu", triggerInfo[capture].status, capture);
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
* rapidblockOverlappedDataHandler
*  This function demonstrates how to collect a set of captures using
*  rapid block Overlapped mode in a loop.
* (repeated rapid blocks, used to save calls to the unit
* (deferred requests for data))
* Inputs :
* - unit : the unit to use.
* - noOfPreTriggerSamples : number of samples to capture before trigger.
* - noOfPostTriggerSamples : number of samples to capture after trigger.
* - idealTimeInterval : the desired time interval (in seconds) between samples.
* - nSamples : Set the number of samples per capture - Used by SetDataBuffers()
* - nCaptures : Set the number of captures - Used by SetNoOfCaptures()
* - ratioMode : Set the downsampling mode - Used by SetDataBuffers()
* - downSampleRatio : Set the downsampling ratio - Used by SetDataBuffers()
* Returns       none
****************************************************************************/
void rapidblockOverlappedDataHandler(GENERICUNIT* unit,
	uint64_t noOfPreTriggerSamples,		// Used by RunBlock()
	uint64_t noOfPostTriggerSamples,	// Used by RunBlock()
	double idealTimeInterval,			// Used by RunBlock()
	uint64_t nSamples,					// Used by SetDataBuffers()
	uint64_t nCaptures,
	PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
	uint64_t downSampleRatio			// Used by SetDataBuffers()
)
{ 
	PICO_STATUS status = 0;
	int16_t i;
	uint64_t capture;

	int64_t nMaxSamples = 0;
	double timeIndisposed = 0;

	int16_t*** minBuffers;
	int16_t*** maxBuffers;

	uint64_t nCompletedCaptures = 0;
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);//bitwise OR flags for first buffer that is set

	//Capture settings
	//Buffers settings (Set DownSampling mode and ratio)
	struct tbuffer_settings bufferSettings = { 0 };
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = ratioMode;
	bufferSettings.downSampleRatio = downSampleRatio;
	bufferSettings.nSamples = nSamples;

	printf("Rapidblock Overlapped capture looping...\n");
	printf("Press any key to abort\n");

	setDefaults(unit);

	//Segment the memory
	status = psospaMemorySegments(unit->handle, nCaptures, &nMaxSamples);

	//Set the number of captures
	status = psospaSetNoOfCaptures(unit->handle, nCaptures);

	//Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
	pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffers, &maxBuffers, &multiBufferSizes);

	// SetDataBuffers with API
	SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers, &multiBufferSizes, 0, (CAPTURE_MODE)RAPID_BLOCK, 0);

	// Create Overflow Array Buffers
	int16_t* overflowArray;
	overflowArray = (int16_t*)calloc(nCaptures, sizeof(int16_t));

	// Find nearest timebase
	// Find the analogue channels that are enabled
	PICO_CHANNEL_FLAGS enabledChannelOrPortFlags = (PICO_CHANNEL_FLAGS)0;
	for (int32_t ch = 0; ch < unit->channelCount; ch++)
	{
		if (unit->channelSettings[ch].enabled)
		{
			enabledChannelOrPortFlags = enabledChannelOrPortFlags | (PICO_CHANNEL_FLAGS)(1 << ch);
		}
	}
	if (unit->digitalChannelSettings[0].enabled)
		enabledChannelOrPortFlags = enabledChannelOrPortFlags | PICO_PORT0_FLAGS;
	if (unit->digitalChannelSettings[1].enabled)
		enabledChannelOrPortFlags = enabledChannelOrPortFlags | PICO_PORT1_FLAGS;

	status = psospaNearestSampleIntervalStateless(unit->handle,
		enabledChannelOrPortFlags,
		idealTimeInterval,
		1, // round faster
		unit->resolution,
		&timebase,
		&(unit->timeInterval));
	if (status != PICO_OK)
	{
		printf("RapidBlockDataHandler:psospaNearestSampleIntervalStateless ------ 0x%08x \n", status);
		return;
	}

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
	printf("\n");

	// Setup deferred request for data
	status = psospaGetValuesOverlapped(unit->handle,
		0,									// Start Index for each segment
		(uint64_t*)&nSamples,				// Number of samples for each segment
		bufferSettings.downSampleRatio,		// Down Sample Ratio
		bufferSettings.downSampleRatioMode,	// Down Sample Ratio mode
		0,									// From Segment
		nCaptures - 1,						// To Segment
		overflowArray);

	/////////////////////// Loop for overlapped captures ////////////////////
	uint16_t NumOverlapped = 4;
	for (uint16_t OverlappedtestNo = 0; OverlappedtestNo < NumOverlapped; OverlappedtestNo++)
	{
		g_ready = FALSE; 
		printf("Loop: #%d of %d Rapid Block Overlapped captures\n", OverlappedtestNo +1, NumOverlapped);

		// Start acquisition
		status = psospaRunBlock(unit->handle,
			noOfPreTriggerSamples,
			noOfPostTriggerSamples,
			timebase,
			&timeIndisposed,
			0,
			callBackBlockReady,
			NULL);

		if (status != PICO_OK)
		{
			printf("BlockDataHandler:psospaRunBlock ------ 0x%08x \n", status);
		}

		// Wait for capture to complete or for user to abort
		printf("Press any key to abort\n");
		while (!g_ready && !_kbhit())
		{
			Sleep(1);
		}

		if (!g_ready) // If user aborted stop the acquisition
		{
			_getch();
			printf("Rapid capture aborted.\n");
			status = psospaStop(unit->handle);
		}
		// Get the number of captures that were completed
		status = psospaGetNoOfCaptures(unit->handle, &nCompletedCaptures);
		printf("%llu complete blocks were captured\n", nCompletedCaptures);

		if (nCompletedCaptures == 0)
		{
			return; // Exit if no captures were made
		}

		// Only use the blocks that were captured
		nCaptures = nCompletedCaptures;
		unit->CapturesComplete = nCompletedCaptures;

		if (status == PICO_OK)
		{
			// Get scaling Info for each channel
			struct tPicoProbeScaling enabledChannelsScaling[PSOSPA_MAX_CHANNELS] = { 0 };
			struct tPicoProbeScaling channelRangeInfoTemp;
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
			printf("Writing each of: %lld channel buffer sets to a file.\n", multiBufferSizes.numberOfBuffers);
			//Create file name string
			char buf[58 + (3 * sizeof(int))];
			size_t buf_size = sizeof(buf) / sizeof(buf[0]);
			snprintf(buf, buf_size, "%s%d_Segment", RapidBlockOverlappedFile, OverlappedtestNo);
			printf("\nWriting capture %ld of channels to a file.\n", OverlappedtestNo);
			WriteArrayToFilesGeneric(
				unit,
				minBuffers,
				maxBuffers,
				multiBufferSizes,
				enabledChannelsScaling,
				buf,
				noOfPreTriggerSamples,	// Triggersample
				overflowArray,
				NULL);

			// Get relative segment trigger timestamps (in samples)
			printf("Get relative segment trigger timestamps (in samples)\n");
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

			if (status != PICO_OK)
			{
				printf("RapidBlockDataHandler:psospaGetTriggerInfo ------ 0x%08x \n", status);
			}
			// Print first 3 trigger timestamps
			uint64_t maxprintCaptures = min(nCaptures, 3);
			PICO_STATUS rapidStatus;
			for (capture = 0; capture < maxprintCaptures; capture++)
			{
				if (triggerInfo != NULL)
				{
					rapidStatus = triggerInfo[capture].status & PICO_DEVICE_TIME_STAMP_RESET;
					printf("\nCapture/segment: %llu, Trigger Timestamp: %llu", capture, triggerInfo[capture].timeStampCounter);
					if ((rapidStatus * (uint32_t)(capture != 0)) == 0) // Ignore Seg #0 PICO_STATUS
					{
						printf(" Delta Samples: %llu, ", triggerInfo[capture].timeStampCounter - triggerInfo[capture - 1].timeStampCounter);
						printf("Delta (seconds): %3.3e", (triggerInfo[capture].timeStampCounter - triggerInfo[capture - 1].timeStampCounter) * unit->timeInterval);
					}
					else
					{
						//NOTE: PICO_DEVICE_TIME_STAMP_RESET/counter wrap around is NOT accounted for. (counter is a unsigned 2^56 bits)
						printf("PICO_DEVICE_TIME_STAMP_RESET--- 0x%08x, Capture %llu", triggerInfo[capture].status, capture);
					}
				}
			}
			
			printf("\n");
		}
	}//////////////////////// End Overlapped loop ///////////////////////////////////////
	// Stop device
	status = psospaStop(unit->handle);

	// Release buffers from API
	clearDataBuffers(unit);
	//free buffers
	pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);
	free(overflowArray);
}