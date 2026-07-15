/*******************************************************************************
 *
 * Filename: Libpsospa.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 3XXXE Series (psospa) devices,
 *   for Streaming captures.
 *
 * Copyright (C) 2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>
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

#include <psospaApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif

#endif

char startOfFileName[] = "StreamingCaptureNoS_";

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase;
extern int16_t   g_ready;
/***************************************************************************/

/****************************************************************************
* streamDataHandler
* - Used by all streaming data routines
* - Uses multiple buffers (ping-pong) to capture and save data in streaming mode.
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all data to a file.
* - If file type is set to FILE_BIN, a metadata file is also created with
*   the capture settings and Channel settings.
* Input :
* - unit : the unit to use.
* - noOfPreTriggerSamples : number of samples to capture before trigger.
* - noOfPostTriggerSamples : number of samples to capture after trigger.
* - idealTimeInterval : ideal time interval between samples (in seconds).
* - sampleIntervalTimeUnits : time units for idealTimeInterval (0 = fs, 1 = ps, 2 = ns, 3 = us, 4 = ms, 5 = s).
* - nSamples : Set the number of samples per capture - Used by SetDataBuffers()
* - ratioMode : Set the downsampling mode - Used by SetDataBuffers()
* - downSampleRatio : Set the downsampling ratio - Used by SetDataBuffers()
* - autostop : 1 to stop when trigger condition is met, 0 to continue until user stops.
* - filetype : Set the file type to save data, FILE_BIN or FILE_CSV or FILE_NONE to not save.
****************************************************************************/ 
void streamDataHandler(GENERICUNIT* unit,
	uint64_t noOfPreTriggerSamples,		// Used by RunStreaming()
	uint64_t noOfPostTriggerSamples,	// Used by RunStreaming()
	double idealTimeInterval,			// Used by RunStreaming()
	uint32_t sampleIntervalTimeUnits,	// Used by RunStreaming()
	uint64_t nSamples,					// Set the number of samples per capture - Used by SetDataBuffers()
	PICO_RATIO_MODE ratioMode,			// Used by SetDataBuffers()
	uint64_t downSampleRatio,			// Used by SetDataBuffers()
	int16_t autostop,
    FILE_TYPE filetype,
    BOOL imagefile)
{
	uint16_t Triggered = 0;
	uint64_t triggeredAt = 0;
	int32_t TriggeredBufNo = 0; // to store the buffer number where the trigger occured

	int16_t channel = 0;
	uint64_t capture = 0;
	int16_t NoEnabledchannels = 0;
	PICO_STATUS status = PICO_OK;
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);	// bitwise OR flags for first buffer that is set
	uint64_t counter = 0; // counter for number of waveform captures
	unit->CapturesComplete = 0; // clear number of captures done

	//Set the number buffers needed (2 or greater) for this code.
	const uint64_t nCaptures = 2; // Set the number of buffer sets to create

	//Define acquisition Settings

	//Buffers settings (Set DownSampling mode and ratio)
	//Use scope acquisition settings for first data download
	struct tbuffer_settings bufferSettings = { 0 };
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = ratioMode;
	bufferSettings.downSampleRatio = downSampleRatio;
	bufferSettings.nSamples = nSamples;

	//Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes;// to store buffer sizes
	int16_t*** minBuffers;
	int16_t*** maxBuffers;
	if(pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffers, &maxBuffers, &multiBufferSizes))
		printf("\nCreated Buffers");

	printf("\nNumber of PreTriggerSamples: %" PRIu64 "", noOfPreTriggerSamples);

	//Get scaling Info for each channel
	struct tPicoProbeScaling enabledChannelsScaling[PSOSPA_MAX_CHANNELS] = {0};
	struct tPicoProbeScaling channelRangeInfoTemp;
	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			getRangeScaling(unit->channelSettings[PICO_CHANNEL_A + 0].range, &channelRangeInfoTemp);
			enabledChannelsScaling[channel] = channelRangeInfoTemp;
			NoEnabledchannels++;
		}
	}
	for (channel = 0; channel < unit->digitalPortCount; channel++)
	{
		if (unit->digitalChannelSettings[channel].enabled)
		{
			NoEnabledchannels++;
		}
	}
	// Write Metadata to file
	if (filetype == FILE_BIN)
	{
		WriteMetaDataToFile(
			unit,
			multiBufferSizes,
			enabledChannelsScaling,
			"PicoMetaData_Streaming",
			0, // Triggersample
			NULL); // captures_range set to NULL to write full range
	}

	//Save and print Sample Internal set (in seconds)
	unit->timeInterval = ( idealTimeInterval * (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15) );
	printf("\nRunStreaming sample Internal: %g seconds\n", unit->timeInterval);
	//print number of Samples
	printf("%" PRIu64 " Captures each with %" PRIu64 " ADC Samples\n", nCaptures, nSamples);
	if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_RAW)
		printf("DownSampling Mode is set to: None\n");
	if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_AGGREGATE)
		printf("DownSampling Mode is set to: Aggregate (Min. and Max. values)\n");
	if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_DECIMATE)
		printf("DownSampling Mode is set to: Decimate\n");
	if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_AVERAGE)
		printf("DownSampling Mode is set to: Average\n");
	if (bufferSettings.downSampleRatioMode != PICO_RATIO_MODE_RAW)
		printf("DownSampling Ratio is set to: %" PRIu64 "\n", bufferSettings.downSampleRatio);

	printf("\nAutostop: %d", autostop);
	printf("\nPress a key to Abort\n");

	// Create Arrays of Structs for GetStreamingLatestValues for each capture buffer Set, and allocate memory if needed
	// streamingDataTriggerInfoArray and streamingDataInfoArray are not required to run and can be removed if not needed.
	struct tPicoStreamingDataTriggerInfo streamingDataTriggerInfoTemp = { 0, 0, 0 };
	struct tPicoStreamingDataTriggerInfo *streamingDataTriggerInfoArray;
	streamingDataTriggerInfoArray = (struct tPicoStreamingDataTriggerInfo *)calloc(nCaptures,
		sizeof(struct tPicoStreamingDataTriggerInfo));

	struct tPicoStreamingDataInfo* dataStreamInfo;
	dataStreamInfo = (struct tPicoStreamingDataInfo *)calloc(NoEnabledchannels, sizeof(struct tPicoStreamingDataInfo));

	struct tPicoStreamingDataInfo** streamingDataInfoArray;
	streamingDataInfoArray = (struct tPicoStreamingDataInfo**)calloc(unit->channelCount + unit->digitalPortCount,
		sizeof(struct tPicoStreamingDataInfo*));
	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			streamingDataInfoArray[channel] = (struct tPicoStreamingDataInfo*)calloc(nCaptures,
				sizeof(struct tPicoStreamingDataInfo));
		}
	}
	for (channel = 0; channel < unit->digitalPortCount; channel++)
	{
		if (unit->digitalChannelSettings[channel].enabled)
		{
			streamingDataInfoArray[channel + unit->channelCount] = (struct tPicoStreamingDataInfo*)calloc(nCaptures,
				sizeof(struct tPicoStreamingDataInfo));
		}
	}
	// Create Overflow Array Buffers
	int16_t* FileOverflow;
	FileOverflow = (int16_t*)calloc(nCaptures, sizeof(int16_t));
	
	if (dataStreamInfo != NULL && streamingDataInfoArray != NULL) //Check for dereferencing null pointers
	{
		int16_t numEnableCh = 0;
		for (channel = 0; channel < unit->channelCount; channel++)
		{
			if (unit->channelSettings[channel].enabled)
			{//Set default vaules for each struct and set correct channel value
				//dataStreamInfos
				dataStreamInfo[numEnableCh].channel_ = (PICO_CHANNEL)channel;
				dataStreamInfo[numEnableCh].type_ = PICO_INT16_T;
				dataStreamInfo[numEnableCh].mode_ = ratioMode;
				numEnableCh++;
			}
		}
		for (channel = 0; channel < unit->digitalPortCount; channel++)
		{
			if (unit->digitalChannelSettings[channel].enabled)
			{//Set default vaules for each struct and set correct channel value
				//dataStreamInfos
				dataStreamInfo[numEnableCh].channel_ = (PICO_CHANNEL)(PICO_PORT0 + channel);
				dataStreamInfo[numEnableCh].type_ = PICO_INT16_T;
				dataStreamInfo[numEnableCh].mode_ = ratioMode;
				numEnableCh++;
			}
		}

		// delay millseconds for driver to fill channel buffer(s)
		// (timeInternal x SI units x samples x 1000) x 0.3 delay in ms to fill buffer 30% (Recommended delay is 30-50%)
		double timedelay_ms = (double)((idealTimeInterval * (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15)) * nSamples * 0.333 * 1000);
		
		bool RunStreamingFlag = TRUE; // Flag to start streaming only once
		bool SetDataBufferFlag = TRUE; // Flag set to TRUE to set data buffers on first loop
		capture = 0;
		uint64_t printTriggerSample = 0;

		// Filename - we will append the buffer number as each buffer set is written to file in the streaming loop below
		char buf[58 + 20] = { '\0' }; // 20 chars is enough for the largest uint64_t buffer-set number
		size_t buf_size = sizeof(buf) / sizeof(buf[0]);

		while(!_kbhit()) // loop for each buffer set created, exit if a key is pressed
		{	
			if (SetDataBufferFlag)
			{	// Pass next set of channel Buffers to the API	
				// SET ONLY ONE BUFFER SET AT A TIME, and clear the action flag after first call
				SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers, &multiBufferSizes, capture, (CAPTURE_MODE)STREAMING, (int16_t)counter);
				SetDataBufferFlag = FALSE;
			}

			if (RunStreamingFlag)
			{
				// Start continuous streaming
				printf("\nStarting Data Capture...");
				status = psospaRunStreaming(unit->handle,
					&idealTimeInterval,
					sampleIntervalTimeUnits,
					noOfPreTriggerSamples,
					noOfPostTriggerSamples,
					autostop,
					downSampleRatio,
					ratioMode);

				if (status != PICO_OK)
				{
					printf("\nError from function RunStreaming with status: ------ 0x%08x", status);
					return;
				}
				RunStreamingFlag = FALSE;
			}

			if (timedelay_ms > 20)
				Sleep((int)timedelay_ms);

			//Call GetStreamingLatestValues() - passing buffer status data in and out
			status = psospaGetStreamingLatestValues(unit->handle,
				dataStreamInfo,					//pointer to dataStreamInfo,
				(uint64_t)numEnableCh,			// Number of elements in dataStreamInfo
				&streamingDataTriggerInfoTemp); //pointer to streamingDataTriggerInfoTemp

			//Copy returned Array and sturture to Arrays for each segement
			int16_t tempNumofChs = 0;
			for (channel = 0; channel < unit->channelCount; channel++)
			{
				if (unit->channelSettings[channel].enabled)
				{
					if(streamingDataInfoArray[channel])
						streamingDataInfoArray[channel][capture] = dataStreamInfo[tempNumofChs];
					if (FileOverflow != NULL)
						FileOverflow[capture] |= dataStreamInfo[tempNumofChs].overflow_; //logic OR all channel overflow flags into variable for file writing
					tempNumofChs++;
				}
			}
			for (channel = 0; channel < unit->digitalPortCount; channel++)
			{
				if (unit->digitalChannelSettings[channel].enabled)
				{
					if (streamingDataInfoArray[channel + unit->channelCount])
						streamingDataInfoArray[channel + unit->channelCount][capture] = dataStreamInfo[tempNumofChs];
					if (FileOverflow != NULL)
						//logic OR all channel overflow flags into variable for file writing
						*(FileOverflow + capture) |= (dataStreamInfo + tempNumofChs)->overflow_;
					tempNumofChs++;
				}
			}
			if(streamingDataTriggerInfoArray != NULL)
				streamingDataTriggerInfoArray[capture] = streamingDataTriggerInfoTemp;

			// DEBUG CODE
			//if(dataStreamInfo[0].noOfSamples_ != 0)
			//{
				//printf("\nPolling GetStreamingLatestValues status = 0x%08x - noOfSamples: %08ld StartIndex: %08ld",
				//	status, dataStreamInfo[0].noOfSamples_, dataStreamInfo[0].startIndex_);
			//}

			if (streamingDataTriggerInfoTemp.triggered_ == 1) // Latch Triggered flag and sample
			{
				Triggered = 1;
				triggeredAt = streamingDataTriggerInfoTemp.triggerAt_;
				TriggeredBufNo = counter; // Store buffer number where trigger occured
			}
			// If buffers full move to next bufferSet, or continue if autoStop triggered
			if ((status == PICO_WAITING_FOR_DATA_BUFFERS) | (streamingDataTriggerInfoTemp.autoStop_ == 1))
			{
				//OFFLOAD DATA HERE FOR PROCESSING - "maxBuffers[i] and minBuffers[i]"
				if (FileOverflow) // Check for dereferencing null pointer
				{
					printf(".");
					// Setup filename for streaming capture -
					snprintf(buf, buf_size, "%s%" PRIu64 "_SubSet", startOfFileName, counter);			
					struct tcaptures_range captures_range = { capture, capture };// Set range to current capture only

					if (((unit->timeInterval) > 0.9e-06) && (imagefile == TRUE))
					{
						printf("\nSaved plot to %s", buf);
						WriteArrayToImage(
							unit,
							minBuffers,
							maxBuffers,
							multiBufferSizes,
							enabledChannelsScaling,
							buf,
							streamingDataTriggerInfoTemp.triggerAt_,
							(int16_t*)(FileOverflow),
							0,      // plotChannelMask: 0 = all enabled channels
							&captures_range);
					}

					if (filetype != FILE_NONE)
					{
						// Only write to binary file if sample interval is < 0.9us (1.1MS/s) and is requested
						if (((unit->timeInterval) < 0.9e-06) && (filetype == FILE_BIN))
						{
							WriteArrayToFilesBinary(
								unit,
								minBuffers,
								maxBuffers,
								multiBufferSizes,
								enabledChannelsScaling,
								buf,
								streamingDataTriggerInfoTemp.triggerAt_,
								(int16_t*)(FileOverflow),
								&captures_range);
						}
						else // For slower sampling rates write to text file (csv), if file writing is requested
						{
							printf("\nWriting capture %" PRIu64 " (Buffer Set %" PRIu64 ") of channels to a file.\n", counter, capture);
							WriteArrayToFilesGeneric(
								unit,
								minBuffers,
								maxBuffers,
								multiBufferSizes,
								enabledChannelsScaling,
								buf,
								streamingDataTriggerInfoTemp.triggerAt_,
								(int16_t*)(FileOverflow),
								&captures_range);
						}
					}
				}
				if(streamingDataTriggerInfoTemp.autoStop_ == 1)
				{
					printTriggerSample = streamingDataTriggerInfoTemp.triggerAt_;
					printf("\nAutoStop Triggered!\n");
					break;	//exit loop on Autostop	
				}

				capture++;	// index next bufferSet and set flag
				counter++;	// counter for buffers used and file name (loop irritations)
				if (capture == nCaptures) // Create circular buffer
				{
					capture = 0;
				}
				SetDataBufferFlag = TRUE; // Set flag to move to next bufferSet
			}
			else
			{
				if (status != PICO_OK)
				{
					printf("\nError from function GetStreamingLatestValues with status: ------ 0x%08x", status);
					break;
				}
			}
		}
		if (Triggered)
			printf("\nTriggered in Buffer No: %d, At Sample: %" PRIu64 "", TriggeredBufNo, triggeredAt);

		//OR WAIT UNTIL ALL BUFFER SEGMENTS ARE CAPTURED AND PROCESS DATA IN - "maxBuffers and minBuffers"
		// Write to console
		printf("\n");
		WriteArrayToStdoutGeneric(
			unit,
			minBuffers,
			maxBuffers,
			multiBufferSizes,
			enabledChannelsScaling,
			(enum enCaptureMode)STREAMING,
			3,						// Number of buffers to write
			10,						// Number of samples to write
			printTriggerSample,		// passes Triggersample regardless of which buffer triggered,(0 if no trigger)
			FileOverflow);
	}

	printf("Stopping Streaming... ");
	// Stop
	status = psospaStop(unit->handle);
	if (status != PICO_OK)
	{
		printf("\nError from function Stop with status: ------ 0x%08x", status);
	}
	else
		printf("Stopped capture\n");

	unit->CapturesComplete = 1; // set to 1 to indicate complete
	// Release Buffer memory from API
	clearDataBuffers(unit);

	// Free buffers
	pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);

	//Free Streaming data info arrays and dataStreamInfo
	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			free(streamingDataInfoArray[channel]);
		}
	}
	for (channel = 0; channel < unit->digitalPortCount; channel++)
	{
		if (unit->digitalChannelSettings[channel].enabled)
		{
			free(streamingDataInfoArray[channel + unit->channelCount]);
		}
	}
	free(streamingDataInfoArray);
	free(dataStreamInfo);
}
