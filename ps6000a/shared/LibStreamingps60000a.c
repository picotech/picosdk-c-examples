/*******************************************************************************
 *
 * Filename: Libps6000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 6000 Series (ps6000a) devices,
 *   for Streaming captures.
 *
 * Copyright (C) 2013-2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include "math.h"
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

#define stream_DEBUG = FALSE;

int8_t StreamFile[20] = "streamSegN.txt";
char startOfFileName[] = "StreamingCaptureNoS_";
FILE* fp = NULL;

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase; //extern uint32_t	timebase = 8;
extern const uint64_t constBufferSize;
/***************************************************************************/

/****************************************************************************
* streamDataHandler
* - Used by all streaming data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all to stream.txt
* Input :
* - unit : the unit to use.
****************************************************************************/ 
void streamDataHandler(GENERICUNIT* unit, uint64_t noOfPreTriggerSamples)
{
	int16_t retry = 0;
	int16_t autostop = 0;
	int32_t index = 0;
	uint32_t triggeredAt = 0;
	int16_t channel;
	uint64_t capture;
	int16_t NoEnabledchannels = 0;
	PICO_STATUS status;

	//Set the number buffers needed (2 or greater) for this code.
	#define STREAMINGBUFFERS 3
	const uint64_t nCaptures = STREAMINGBUFFERS;

	//Define acquisition Settings
	uint64_t nSamples = constBufferSize;	//Set the number of samples per capture
	double idealTimeInterval = 1;
	uint32_t sampleIntervalTimeUnits = PICO_US;
	PICO_RATIO_MODE ratioMode = PICO_RATIO_MODE_RAW;//used for RunStreaming()
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);//bitwise OR flags for first buffer that is set
	uint64_t downSampleRatio = 1;

	//Buffers settings (Set DownSampling mode and ratio)
	//Use scope acquisition settings for first data download
	struct tbuffer_settings bufferSettings;
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = ratioMode;
	bufferSettings.downSampleRatio = downSampleRatio;
	bufferSettings.nSamples = nSamples;

	//Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes;// to store buffer sizes
	int16_t*** minBuffers;
	int16_t*** maxBuffers;
	pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffers, &maxBuffers, &multiBufferSizes);

	// Pass first set of channel Buffers to the API
	printf("Calling SetDataBuffers() for BufferSet #0 Channel(s) - ");
	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			NoEnabledchannels++;

			status = ps6000aSetDataBuffers(unit->handle,
				(PICO_CHANNEL)channel,
				maxBuffers[0][channel],
				minBuffers[0][channel],
				(int32_t)nSamples,
				PICO_INT16_T, //PICO_DATA_TYPE
				0,
				PICO_RATIO_MODE_RAW,
				action_flag);

			action_flag = PICO_ADD;//all subsequent calls use ADD!

			printf("%c,", 'A' + channel);
			if (status != PICO_OK)
			{
				printf("\nError from function SetDataBuffers with status: ------ 0x%08lx", status);
				break;
			}
		}
	}

	// Start continuous streaming
	printf("\nStarting Data Capture...");
	
	printf("\nNumber of PreTriggerSamples: %lld", noOfPreTriggerSamples);
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

	//Get scaling Info for each channel
	PICO_PROBE_SCALING enabledChannelsScaling[PS6000A_MAX_CHANNELS] = {0};
	PICO_PROBE_SCALING channelRangeInfoTemp;
	for (uint64_t i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			getRangeScaling(unit->channelSettings[PICO_CHANNEL_A + 0].range, &channelRangeInfoTemp);
			enabledChannelsScaling[i] = channelRangeInfoTemp;
		}
	}

	//Save and print Sample Internal set (in seconds)
	unit->timeInterval = ( idealTimeInterval * (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15) );
	printf("\nRunStreaming sample Internal: %g seconds", unit->timeInterval);
	printf("\nTotal number of samples: %lld", nSamples);
	printf("\nAutostop: %d", autostop);
	printf("\nPress a key to Abort\n");

	//Create Arrays of Structs for GetStreamingLatestValues for each memory segment
	PICO_STREAMING_DATA_TRIGGER_INFO streamingDataTriggerInfoArray[STREAMINGBUFFERS] = {0};
	PICO_STREAMING_DATA_TRIGGER_INFO streamingDataTriggerInfoTemp;
	PICO_STREAMING_DATA_INFO** streamingDataInfoArray;

	// Allocate memory
	streamingDataInfoArray = (PICO_STREAMING_DATA_INFO**)calloc(unit->channelCount, sizeof(PICO_STREAMING_DATA_INFO*));
	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			streamingDataInfoArray[channel] = (PICO_STREAMING_DATA_INFO*)calloc(nCaptures, sizeof(PICO_STREAMING_DATA_INFO));
		}
	}
	PICO_STREAMING_DATA_TRIGGER_INFO StreamingDataTriggerInfo0 = { 0, 0, 0 }; //( triggerAt, triggered, autoStop )
	streamingDataTriggerInfoTemp = StreamingDataTriggerInfo0;
	int j = 0;

	//Fill both Arrays with default struct vaules
	for (j = 0; j < nCaptures; j++)
	{
		streamingDataTriggerInfoArray[j] = StreamingDataTriggerInfo0;
	}
	PICO_STREAMING_DATA_INFO* dataStreamInfo;
	dataStreamInfo = (PICO_STREAMING_DATA_INFO*)calloc(NoEnabledchannels, sizeof(PICO_STREAMING_DATA_INFO));
	//assert(dataStreamInfo == NULL);
	int16_t FileOverflow = 0; //For file writing
	
	if (dataStreamInfo != NULL && streamingDataInfoArray != NULL) //Check for dereferencing null pointers
		//if (dataStreamInfo != NULL) //Check for dereferencing null pointers
	{
		int numEnableCh = 0;
		for (j = 0; j < nCaptures; j++)
		{
			numEnableCh = 0;
			for (short channel = 0; channel < unit->channelCount; channel++)
			{
				if (unit->channelSettings[channel].enabled)
				{//Set default vaules for each struct and set correct channel value
					//dataStreamInfos
					dataStreamInfo[numEnableCh].channel_ = (PICO_CHANNEL)channel;
					dataStreamInfo[numEnableCh].mode_ = ratioMode; // PICO_RATIO_MODE_RAW; // ratioMode;
					dataStreamInfo[numEnableCh].type_ = PICO_INT16_T;//
					numEnableCh++;
				}
			}
		}

		//delay millseconds for driver to fill channel buffer(s)
		//(timeInternal x SI units x samples x 1000) x 0.3 delay in ms to fill buffer 30% (Recommended delay is 30-50%)
		double timedelay_ms = (double)((idealTimeInterval * (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15)) * nSamples * 0.3 * 1000);

		if (status == PICO_OK)
		{
			bool SetDataBufferFlag = false;
			uint64_t i = 0;

			while (i < nCaptures) //loop for each buffer Set created
			{	
				if (SetDataBufferFlag)
				{	// Pass next set of channel Buffers to the API	
					printf("\nCalling SetDataBuffer() for BufferSet #%d Channel(s) - ", (int)i);
					for (short channel = 0; channel < unit->channelCount; channel++)
					{
						if (unit->channelSettings[channel].enabled)
						{
							status = ps6000aSetDataBuffers(unit->handle,
								(PICO_CHANNEL)channel,
								maxBuffers[i][channel],
								minBuffers[i][channel],
								(int32_t)nSamples,
								PICO_INT16_T,
								0,
								PICO_RATIO_MODE_RAW,
								action_flag);
							printf("%c,", 'A' + channel);
							if (status != PICO_OK)
							{
								printf(" - Error from function SetDataBuffers with status: ------ 0x%08lx", status);
								break;
							}
						}
					}
				}
				SetDataBufferFlag = false;

				Sleep((int)timedelay_ms);

				//Call GetStreamingLatestValues() - passing buffer status data in and out
				status = ps6000aGetStreamingLatestValues(unit->handle,
					dataStreamInfo,					//pointer to dataStreamInfo,
					(uint64_t)NoEnabledchannels,	//sizeof(dataStreamInfo)
					&streamingDataTriggerInfoTemp); //pointer to streamingDataTriggerInfoTemp

				///Copy returned Array and sturture to Arrays for each segement
				j = 0;
				for (short channel = 0; channel < unit->channelCount; channel++)
				{
					if (unit->channelSettings[channel].enabled)
					{
						streamingDataInfoArray[channel][i] = dataStreamInfo[j];
						FileOverflow |= dataStreamInfo[j].overflow_; //logic OR all channel overflow flags into variable for file writing
						j++;
					}
				}
				streamingDataTriggerInfoArray[i] = streamingDataTriggerInfoTemp;

				//printf("\nPolling Delay is: %6.3le ms", timedelay);
				if(dataStreamInfo[0].noOfSamples_ != 0)
				{
					printf("\nPolling GetStreamingLatestValues status = 0x%08lx - noOfSamples: %08ld StartIndex: %08ld",
						status, streamingDataInfoArray[0][i].noOfSamples_, streamingDataInfoArray[0][i].startIndex_);
				}

				// If buffers full move to next bufferSet
				if (status == PICO_WAITING_FOR_DATA_BUFFERS)
				{
					//OFFLOAD DATA HERE FOR PROCESSING - "maxBuffers[i] and minBuffers[i]"
					//WRITING TO TEXT FOR DEMO ONLY!, FOR HIGH SPEED SAMPLING WRITE TO BINARY FILE OR COPY TO ANOTHER BUFFER

					//Write one segment to a file as captured
					printf("\nWriting Buffer Set %lld of channels to a file.\n", i);

					//Create file name string
					char buf[58 + (3 * sizeof(int))];
					size_t buf_size = sizeof(buf) / sizeof(buf[0]);
					snprintf(buf, buf_size, "%s%d.txt", startOfFileName, (int)i);


					WriteArrayToFileGeneric(
						unit,
						minBuffers[i],
						maxBuffers[i],
						multiBufferSizes,
						enabledChannelsScaling,
						buf,
						streamingDataTriggerInfoArray[i].triggerAt_, // Triggersample
						&FileOverflow);
					printf(" ");

					if(streamingDataTriggerInfoTemp.autoStop_ == 1)
						break;	//exit loop on Autostop

					i++;		//index next bufferSet and set flag
					SetDataBufferFlag = true;
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
		}
	}

	printf("Stopping Streaming...\n");
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

	// Free memory
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

	free(streamingDataInfoArray);
	free(dataStreamInfo);

}

/****************************************************************************
*  collectStreamingTriggered
*  This function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
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

	streamDataHandler(unit, 0);
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

	printf("Collect streaming ...\n");
	printf("Data is written to disk file (stream.txt)\n");
	printf("Press a key to start\n");
	_getch();

	streamDataHandler(unit, 0);
}