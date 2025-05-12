/*******************************************************************************
 *
 * Filename: Libps6000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 6000 Series (ps6000a) devices,
 *   for Block captures.
 *
 * Copyright (C) 2013-2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
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

int8_t BlockFile[20] = "block.txt";

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
* BlockDataHandler
* - Used by all block data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all to block.txt
* Input :
* - unit : the unit to use.
* - text : the text to display before the display of data slice
* - offset : the offset into the data buffer to start the display's slice.
****************************************************************************/
void blockDataHandler(GENERICUNIT* unit, int8_t* text, int32_t offset)
{
	int16_t retry;
	int16_t triggerEnabled = 0;
	int16_t pwqEnabled = 0;

	int32_t i;
	double timeInterval;
	uint64_t maxSamples;
	double timeIndisposed;

	PICO_STATUS status;

	uint64_t nSamples = constBufferSize;	//Set the number of samples per capture
	PICO_RATIO_MODE ratioMode = PICO_RATIO_MODE_RAW;//used for RunBlock()
	
	PICO_RATIO_MODE downSample = PICO_RATIO_MODE_RAW;
	uint64_t downSampleRatio = 1;//used for GetValues()

	//Buffers settings (Set DownSampling mode and ratio)
	//Use scope acquisition settings for first data download
	struct tbuffer_settings bufferSettings;
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = ratioMode;
	bufferSettings.downSampleRatio = downSampleRatio;
	bufferSettings.nSamples = constBufferSize;

	//Create Buffers - Min and Max (3D buffer - 1 Segment, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes;// to store buffer sizes
	int16_t*** minBuffers;
	int16_t*** maxBuffers;
	pico_create_multibuffers(unit, bufferSettings, 1, &minBuffers, &maxBuffers, &multiBufferSizes);

	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);//bitwise OR flags for first buffer that is set

	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			status = ps6000aSetDataBuffers(unit->handle,
				(PICO_CHANNEL)i,
				maxBuffers[0][i], // 1 waveform buffer only
				minBuffers[0][i], // 1 waveform buffer only
				(int32_t)bufferSettings.nSamples,
				PICO_INT16_T,
				0,			//waveform number
				bufferSettings.downSampleRatioMode,
				action_flag);

			action_flag = PICO_ADD;//all subsequent calls use ADD!
			if (status != PICO_OK)
			{
				printf(status ? "blockDataHandler:ps6000aSetDataBuffers(channel %d) ------ 0x%08lx \n" : "", i, status);
			}
		}
	}

	/*  Find the maximum number of samples and the time interval (in nanoseconds).
	 *	If the function returns PICO_OK, the timebase will be used.
	 */
	do
	{
		status = ps6000aGetTimebase(unit->handle, timebase, nSamples, &timeInterval, &maxSamples, 0);
		if (status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION ||
			status == PICO_CHANNEL_COMBINATION_NOT_VALID_IN_THIS_RESOLUTION)
		{
			printf("BlockDataHandler: Error - Invalid number of channels for resolution. Or incorrect set of channels enabled.\n");
			return;
		}
		else if (status == PICO_OK)
		{
			// Do nothing
		}
		else
		{
			timebase++;
		}
	} while (status != PICO_OK);

	printf("\nTimebase: %lu  SampleInterval: %le seconds\n", timebase, timeInterval * 1e-9);
	printf("Number of Capture Samples: %llu\n", nSamples);
	if(ratioMode == PICO_RATIO_MODE_RAW)
		printf("DownSampling Mode is set to: None\n");
	if (ratioMode == PICO_RATIO_MODE_AGGREGATE)
		printf("DownSampling Mode is set to: Aggregate (Min. and Max. values)\n");
	if (ratioMode == PICO_RATIO_MODE_DECIMATE)
		printf("DownSampling Mode is set to: Decimate\n");
	if (ratioMode == PICO_RATIO_MODE_AVERAGE)
		printf("DownSampling Mode is set to: Average\n");
	if (ratioMode != PICO_RATIO_MODE_RAW)
		printf("\nDownSampling Ratio is set to: %llu\n", downSampleRatio);

	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;

	do
	{
		retry = 0;

		status = ps6000aRunBlock(unit->handle, 0, nSamples, timebase, &timeIndisposed, 0, CallBackBlock, NULL);

		if (status != PICO_OK)
		{
			printf("BlockDataHandler:ps5000aRunBlock ------ 0x%08lx \n", status);
			return;
		}
	} while (retry);

	//status = ps5000aIsTriggerOrPulseWidthQualifierEnabled(unit->handle, &triggerEnabled, &pwqEnabled);

	if (triggerEnabled || pwqEnabled)
	{
		printf("Waiting for trigger... Press any key to abort\n");
	}
	else
	{
		printf("Press any key to abort\n");
	}

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (g_ready)
	{

		// Can retrieve data using different ratios and ratio modes from driver
		int16_t overflow = 0;

		status = ps6000aGetValues(unit->handle, 0, (uint64_t*)&nSamples, downSampleRatio, ratioMode, 0, &overflow);

		if (status != PICO_OK)
		{
			printf("blockDataHandler:ps6000aGetValues ------ 0x%08lx \n", status);
		}
		else
		{
			printf("blockDataHandler:ps6000aGetValues Channel Over Range flags (Ch. order- HGFEDCBA bit0) ------ 0x%08lx \n", overflow);
			/* Print out the first 10 readings, converting the readings to mV if required */
			printf("%s ", text);

			// Print first 10 samples from the first 2 captures
			printf("(Max. Values)\n\n");
			int16_t channel;
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
						if (maxBuffers[0][channel])//Check buffer is not NULL
						{//3.3e //6d
							printf("%+3.3e\t", scaleVoltages ?
								adc_to_mv(maxBuffers[0][channel][i],
									unit->channelSettings[PICO_CHANNEL_A + channel].range,
									unit->maxADCValue)			// If scaleVoltages, print mV value
								: maxBuffers[0][channel][i]);
						}// else print ADC Count
					}
					else
					{
						printf("   ---  \t");
					}
				}
				printf("\n");
			}
			
			nSamples = min(nSamples, constBufferSize);

			//Get scaling Info for each channel
			PICO_PROBE_SCALING enabledChannelsScaling[PS6000A_MAX_CHANNELS] = {0}; //[unit->channelCount]; //Move to global/golobal struture
			PICO_PROBE_SCALING channelRangeInfoTemp;
			for (i = 0; i < unit->channelCount; i++)
			{
				if (unit->channelSettings[i].enabled)
				{
					getRangeScaling(unit->channelSettings[PICO_CHANNEL_A + 0].range, &channelRangeInfoTemp);
					enabledChannelsScaling[i] = channelRangeInfoTemp;
				}
			}

			//Write one segment to a file as captured
			printf("\nWriting Capture of enabled channels to file.\n");
			WriteArrayToFileGeneric(
				unit,
				minBuffers[0],
				maxBuffers[0],
				multiBufferSizes,
				enabledChannelsScaling,
				BlockFile,
				0,						// Triggersample
				&overflow);
		}
	}
	else
	{
		printf("Data collection aborted\n");
		_getch();
	}

	if ((status = ps6000aStop(unit->handle)) != PICO_OK)
	{
		printf("blockDataHandler:ps6000aStop ------ 0x%08lx \n", status);
	}

	clearDataBuffers(unit);
}

/****************************************************************************
* collectBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
void collectBlockImmediate(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;

	printf("Collect block immediate...\n");
	printf("Press a key to start\n");
	_getch();

	setDefaults(unit);

	/* Trigger disabled	*/
	status = ps6000aSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

	blockDataHandler(unit, (int8_t*)"First 10 readings\n", 0);
}

/****************************************************************************
* collectBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void collectBlockTriggered(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;

	//Set triggerLevelADC to +50% of set channel voltage range
	int16_t triggerLevelADC = mv_to_adc( (double)inputRanges[unit->channelSettings[PICO_CHANNEL_A].range] / 2,
		unit->channelSettings[PICO_CHANNEL_A].range,
		unit->maxADCValue);

	struct tPicoTriggerChannelProperties sourceDetails = {
											triggerLevelADC,	//thresholdUpper
											256 * 16,			//thresholdUpperHysteresis
											triggerLevelADC,	//thresholdLower
											256 * 16,			//thresholdLowerHysteresis
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

	printf("Collect block triggered...\n");
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

	blockDataHandler(unit, (int8_t*)"First 10 readings after trigger\n", 0);
}
