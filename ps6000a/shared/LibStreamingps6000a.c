/*******************************************************************************
 *
 * Filename: Libps6000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 6XXXE Series (ps6000a) devices,
 *   for Streaming captures.
 *
 * Copyright (C) 2013-2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "../../shared/PicoScaling.h"
#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"

#include "./Libps6000a.h"

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

char startOfFileName[] = "StreamingCaptureNoS_";
FILE* fp = NULL;

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase;
extern const uint64_t constBufferSize;
/***************************************************************************/

/****************************************************************************
* streamDataHandler
* - Used by all streaming data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all data to a file.
* Input :
* - unit : the unit to use.
* - noOfPreTriggerSamples : number of samples to capture before trigger.
* - autostop : 1 to stop when trigger condition is met, 0 to continue until user stops.
****************************************************************************/ 
void streamDataHandler(GENERICUNIT* unit, uint64_t noOfPreTriggerSamples, int16_t autostop)
{
	int32_t index = 0;
	uint32_t triggeredAt = 0;
	int16_t channel = 0;
	uint64_t capture = 0;
	int16_t NoEnabledchannels = 0;
	PICO_STATUS status = PICO_OK;

	//--------------------------------------------------------------------------//
	//Set the number buffers needed (2 or greater) for this code.
	const uint64_t nCaptures = 3;
	int16_t numOfAnalogChs = 0;

	//Define acquisition Settings
	double idealTimeInterval = 1;
	uint32_t sampleIntervalTimeUnits = PICO_US;
	uint64_t nSamples = constBufferSize; //Set the number of samples per capture - Used by RunStreaming()
	PICO_RATIO_MODE ratioMode = PICO_RATIO_MODE_RAW;		//used by RunStreaming()
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);	// bitwise OR flags for first buffer that is set
	uint64_t downSampleRatio = 1;							//used by RunStreaming()

	//Buffers settings (Set DownSampling mode and ratio)
	//Use scope acquisition settings for first data download
	struct tbuffer_settings bufferSettings = {0};
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = ratioMode;
	bufferSettings.downSampleRatio = downSampleRatio;
	bufferSettings.nSamples = nSamples;
	//--------------------------------------------------------------------------//

	//Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes;// to store buffer sizes
	int16_t*** minBuffers;
	int16_t*** maxBuffers;
	if(pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffers, &maxBuffers, &multiBufferSizes))
	printf("\nCreated Buffers");

	printf("\nNumber of PreTriggerSamples: %lld", noOfPreTriggerSamples);

	//Get scaling Info for each channel
	struct tPicoProbeScaling enabledChannelsScaling[PS6000A_MAX_CHANNELS] = {0};
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

	//Save and print Sample Internal set (in seconds)
	unit->timeInterval = ( idealTimeInterval * (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15) );
	printf("\nRunStreaming sample Internal: %g seconds\n", unit->timeInterval);
	//print number of Samples
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

		while (capture < nCaptures) //loop for each buffer Set created
		{	
			if (SetDataBufferFlag)
			{	// Pass next set of channel Buffers to the API	
				printf("\nCalling SetDataBuffer() for BufferSet #%d Channel(s) - ", (int)capture);
				for (channel = 0; channel < unit->channelCount; channel++)
				{
					if (unit->channelSettings[channel].enabled)
					{
						status = ps6000aSetDataBuffers(unit->handle,
							(PICO_CHANNEL)channel,
							maxBuffers[capture][channel],
							minBuffers[capture][channel],
							multiBufferSizes.maxBufferSize,
							PICO_INT16_T,
							0,
							bufferSettings.downSampleRatioMode,
							action_flag);
						action_flag = PICO_ADD;//all subsequent calls use ADD!
						printf("%c,", 'A' + channel);
						if (status != PICO_OK)
						{
							printf(" - Error from function SetDataBuffers with status: ------ 0x%08lx", status);
							break;
						}
					}
				}
				//digital channels
				for (channel = 0; channel < unit->digitalPortCount; channel++)
				{
					if (unit->digitalChannelSettings[channel].enabled)
					{
						status = ps6000aSetDataBuffers(unit->handle,
							PICO_PORT0 + (PICO_CHANNEL)channel,
							maxBuffers[capture][channel + unit->channelCount], // 1 waveform buffer only
							minBuffers[capture][channel + unit->channelCount], // 1 waveform buffer only
							multiBufferSizes.maxBufferSize,
							PICO_INT16_T,
							0,			//waveform number
							bufferSettings.downSampleRatioMode,
							action_flag);
						action_flag = PICO_ADD;//all subsequent calls use ADD!
						printf("PORT%d,", channel);
						if (status != PICO_OK)
						{
							printf(status ? "blockDataHandler:ps6000aSetDataBuffers(channel %d) ------ 0x%08lx \n" : "", PICO_PORT0 + channel, status);
						}
					}
				}
				SetDataBufferFlag = FALSE;
			}

			if (RunStreamingFlag)
			{
				// Start continuous streaming
				printf("\nStarting Data Capture...");
				status = ps6000aRunStreaming(unit->handle,
					&idealTimeInterval,
					sampleIntervalTimeUnits,
					noOfPreTriggerSamples,
					nSamples - noOfPreTriggerSamples,
					autostop,
					downSampleRatio,
					ratioMode);

				if (status != PICO_OK)
				{
					printf("\nError from function RunStreaming with status: ------ 0x%08lx", status);
					return;
				}
				RunStreamingFlag = FALSE;
			}

 			Sleep((int)timedelay_ms);

			//Call GetStreamingLatestValues() - passing buffer status data in and out
			status = ps6000aGetStreamingLatestValues(unit->handle,
				dataStreamInfo,					//pointer to dataStreamInfo,
				(uint64_t)numEnableCh,			// Number of elements in dataStreamInfo
				&streamingDataTriggerInfoTemp); //pointer to streamingDataTriggerInfoTemp

			///Copy returned Array and sturture to Arrays for each segement
			int16_t tempNumofChs = 0;
			for (channel = 0; channel < unit->channelCount; channel++)
			{
				if (unit->channelSettings[channel].enabled)
				{
					if(streamingDataInfoArray[channel])
						streamingDataInfoArray[channel][capture] = dataStreamInfo[tempNumofChs];
					if ((FileOverflow + capture) != NULL)
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
					if ((FileOverflow + capture) != NULL)
						//logic OR all channel overflow flags into variable for file writing
						*(FileOverflow + capture) |= (dataStreamInfo + tempNumofChs)->overflow_;
					tempNumofChs++;
				}
			}
			if(streamingDataTriggerInfoArray != NULL)
				streamingDataTriggerInfoArray[capture] = streamingDataTriggerInfoTemp;

			if(dataStreamInfo[0].noOfSamples_ != 0)
			{
				printf("\nPolling GetStreamingLatestValues status = 0x%08lx - noOfSamples: %08ld StartIndex: %08ld",
					status, dataStreamInfo[0].noOfSamples_, dataStreamInfo[0].startIndex_);
			}

			// If buffers full move to next bufferSet, or continue if autoStop triggered
			if ((status == PICO_WAITING_FOR_DATA_BUFFERS) | (streamingDataTriggerInfoTemp.autoStop_ == 1))
			{
				//OFFLOAD DATA HERE FOR PROCESSING - "maxBuffers[i] and minBuffers[i]"
				//WRITING TO TEXT FOR DEMO ONLY!, FOR HIGH SPEED SAMPLING WRITE TO BINARY FILE OR COPY TO ANOTHER BUFFER

				//Write one segment to a file as captured
				printf("\nWriting Buffer Set %lld of channels to a file.\n", capture);

				//Create file name string
				char buf[58 + (3 * sizeof(int))];
				size_t buf_size = sizeof(buf) / sizeof(buf[0]);
				//snprintf(buf, buf_size, "%s%d.txt", startOfFileName, (int)capture);
				snprintf(buf, buf_size, "%s", startOfFileName);

				if (streamingDataTriggerInfoArray && FileOverflow) // Check for dereferencing null pointers
				{
					struct tcaptures_range captures_range = { capture, capture };// Set range to current capture only
					WriteArrayToFilesGeneric(
						unit,
						minBuffers,
						maxBuffers,
						multiBufferSizes,
						enabledChannelsScaling,
						buf,
						streamingDataTriggerInfoTemp.triggerAt_, // Triggersample
						(int16_t*)(FileOverflow),
						&captures_range);
				}

				
				if(streamingDataTriggerInfoTemp.autoStop_ == 1)
				{
					printTriggerSample = streamingDataTriggerInfoTemp.triggerAt_;
					printf("\nAutoStop Triggered!\n");
					break;	//exit loop on Autostop	
				}

				capture++;	//index next bufferSet and set flag
				SetDataBufferFlag = TRUE;
			}
			else
			{
				if (status != PICO_OK)
				{
					printf("\nError from function GetStreamingLatestValues with status: ------ 0x%08lx", status);
					break;
				}
			}
		}
 		printf("\n");
		//OR WAIT UNTIL ALL BUFFER SEGMENTS ARE CAPTURED AND PROCESS DATA IN - "maxBuffers and minBuffers"
		//Write to console
		WriteArrayToStdoutGeneric(
			unit,
			minBuffers,
			maxBuffers,
			multiBufferSizes,
			enabledChannelsScaling,
			(CAPTURE_MODE)STREAMING,
			3,						// Number of buffers to write
			10,						// Number of samples to write
			printTriggerSample,		// passes Triggersample regardless of which buffer triggered,(0 if no trigger)
			FileOverflow);

	}

	printf("Stopping Streaming... ");
	// Stop
	status = ps6000aStop(unit->handle);
	if (status != PICO_OK)
	{
		printf("\nError from function Stop with status: ------ 0x%08lx", status);
	}
	else
		printf("Stopped capture\n");

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

/****************************************************************************
*  collectStreamingTriggered
*  This function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
*  If autoStop is set to 1, the function will stop when the trigger condition is met.
***************************************************************************/
void collectStreamingTriggered(GENERICUNIT* unit)
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

	printf("Collect streaming...\n");
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

	streamDataHandler(unit, 0, 1); // unit, 0: PreTriggerSamples, 1: Autostop(On)
}
/****************************************************************************
*  collectStreamingImmediate
*  This function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
***************************************************************************/
void collectStreamingImmediate(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;

 	setDefaults(unit);

	/* Trigger disabled	*/
	status = ps6000aSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

	printf("Collect streaming ...\n");
	printf("Press a key to start\n");
	_getch();

	streamDataHandler(unit, 0, 0); // unit, 0: PreTriggerSamples, 0: Autostop(Off)
}
