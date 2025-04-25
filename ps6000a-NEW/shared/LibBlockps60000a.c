/*******************************************************************************
 *
 * Filename: Libps6000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 6000 Series (ps6000a) devices.
 *
 * Copyright (C) 2013-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include "../../shared/PicoUnit.h"
#include "../../shared/PicoScaling.h"
#include "./Libps60000a.h"

#include "./LibBlockps60000a.h"

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
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

#include <libps6000a-1.0/ps6000aApi.h>
#ifndef PICO_STATUS
#include <libps6000a-1.0/PicoStatus.h>
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

//int32_t cycles = 0;

//c = FALSE;
//uint64_t ptest = FALSE;

//int16_t			g_autoStop;
//int16_t			g_autoStopped;

int16_t   		g_ready = FALSE;

//BOOL			c = FALSE;
//uint64_t 		g_times[PS6000A_MAX_CHANNELS];
//int16_t     	g_timeUnit;
//int32_t      	g_sampleCount;
//uint32_t		g_startIndex;
//int16_t			g_trig = 0;
//uint32_t		g_trigAt = 0;


int8_t BlockFile[20] = "block.txt";

//uint32_t	timebase = 8;

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase; //extern uint32_t	timebase = 8;
/***************************************************************************/

/****************************************************************************
* Block Callback
* used by ps6000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
//void PREF4 CallBackBlock( int16_t handle, PICO_STATUS status, void * pParameter)
void PREF4 CallBackBlock(int16_t handle, PICO_STATUS status, void* pParameter)
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

	int16_t* buffers[2 * PS6000A_MAX_CHANNELS];

	int32_t i, j;
	double timeInterval;
	uint64_t sampleCount = BUFFER_SIZE;
	uint64_t maxSamples;
	double timeIndisposed;

	PICO_STATUS status;

	PICO_RATIO_MODE ratioMode = PICO_RATIO_MODE_RAW;//used for RunBlock()
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);//bitwise OR flags for first buffer that is set

	PICO_RATIO_MODE downSample = PICO_RATIO_MODE_RAW;//used for GetValues()
	uint64_t downSampleRatio = 1;

	FILE* fp = NULL;

	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			buffers[i * 2] = (int16_t*)calloc(sampleCount, sizeof(int16_t));
			buffers[i * 2 + 1] = (int16_t*)calloc(sampleCount, sizeof(int16_t));

			status = ps6000aSetDataBuffers(unit->handle, (PICO_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1],
				(int32_t)sampleCount, PICO_INT16_T, 0, ratioMode, action_flag);
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
		status = ps6000aGetTimebase(unit->handle, timebase, sampleCount, &timeInterval, &maxSamples, 0);
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


	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;

	do
	{
		retry = 0;

		status = ps6000aRunBlock(unit->handle, 0, sampleCount, timebase, &timeIndisposed, 0, CallBackBlock, NULL);

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

		status = ps6000aGetValues(unit->handle, 0, (uint64_t*)&sampleCount, downSampleRatio, ratioMode, 0, &overflow);

		if (status != PICO_OK)
		{
			printf("blockDataHandler:ps6000aGetValues ------ 0x%08lx \n", status);
		}
		else
		{
			printf("blockDataHandler:ps6000aGetValues Channel Over Range flags (Ch. order- HGFEDCBA bit0) ------ 0x%08lx \n", overflow);
			/* Print out the first 10 readings, converting the readings to mV if required */
			printf("%s\n", text);

			printf("Channels are in (%s):-\n\n", (scaleVoltages) ? ("mV") : ("ADC Counts"));

			for (j = 0; j < unit->channelCount; j++)
			{
				if (unit->channelSettings[j].enabled)
				{
					printf("Channel %c:    ", 'A' + j);
				}
			}

			printf("\n\n");

			for (i = offset; i < offset + 10; i++)
			{
				for (j = 0; j < unit->channelCount; j++)
				{
					if (unit->channelSettings[j].enabled)
					{ //3.3e //6d
						printf("  %6d    ", scaleVoltages ?
							(int16_t)adc_to_mv(buffers[j * 2][i], // If scaleVoltages, print mV value
								unit->channelSettings[PICO_CHANNEL_A + j].range,
								unit->maxADCValue)	
							: buffers[j * 2][i]);	// else print ADC Count
					}
				}

				printf("\n");
			}

			sampleCount = min(sampleCount, BUFFER_SIZE);

			fopen_s(&fp, BlockFile, "w");

			if (fp != NULL)
			{
				fprintf(fp, "Block Data log\n\n");

				//fprintf(fp, "Results shown for each of the %d Channels are......\n", unit->channelCount);
				fprintf(fp, "Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

				fprintf(fp, "Time(s) ");

				for (i = 0; i < unit->channelCount; i++)
				{
					if (unit->channelSettings[i].enabled)
					{
						fprintf(fp, "Ch%C_Max-ADC Max_mV Min_ADC Min_mV ", 'A' + i);
						//fprintf(fp, " Ch    Max ADC   Max mV  Min ADC   Min mV   ");
					}
				}
				fprintf(fp, "\n");

				for (i = 0; i < sampleCount; i++)
				{
					//fprintf(fp, "%I64u ", g_times[0] + (uint64_t)(i * timeInterval));
					fprintf(fp, "%3.3e ", (double)(i * timeInterval * 1e-9));

					for (j = 0; j < unit->channelCount; j++)
					{
						if (unit->channelSettings[j].enabled)
						{
							fprintf(fp,
								"%+5d %+3.3e %+5d %+3.3e   ",
								buffers[j * 2][i],
								(double)adc_to_mv(buffers[j * 2][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue),
								buffers[j * 2 + 1][i],
								(double)adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue));
						}
					}

					fprintf(fp, "\n");
				}

			}
			else
			{
				printf("Cannot open the file %s for writing.\n"
					"Please ensure that you have permission to access the file.\n",BlockFile);
			}
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
											256 * 10,			//thresholdUpperHysteresis
											triggerLevelADC,	//thresholdLower
											256 * 10,			//thresholdLowerHysteresis
											PICO_CHANNEL_A,		//channel - PICO_CHANNEL
											};

	struct tPicoCondition conditions = { sourceDetails.channel,	//PICO_CHANNEL
											PICO_CONDITION_TRUE	//PICO_TRIGGER_STATE - true/false/Don't care
										};

	struct tPicoDirection directions;
	directions.channel = conditions.source;
	directions.direction = PICO_RISING;
	directions.thresholdMode = PICO_LEVEL;

	//Create Pulse Width Qualifier structure with settings
	struct tPwq pulseWidth;
	memset(&pulseWidth, 0, sizeof(struct tPwq));//zero out pulseWidth

	printf("Collect block triggered...\n");
	printf("Collects when value rises past %d", scaleVoltages ?
		(int16_t)adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[sourceDetails.channel].range, unit->maxADCValue)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	printf("Trigger Channel is %c\n", 'A' + sourceDetails.channel);
	printf(scaleVoltages ? "mV\n" : "ADC Counts\n");

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
