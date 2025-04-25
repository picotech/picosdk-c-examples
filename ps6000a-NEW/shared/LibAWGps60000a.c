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
#include "../../shared/PicoScaling.h"

#include "./Libps60000a.h"

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

int32_t cycles = 0;

//c = FALSE;
//uint64_t ptest = FALSE;

int16_t			g_autoStop;
int16_t			g_autoStopped;
int16_t   		g_ready = FALSE;
BOOL			c = FALSE;
uint64_t 		g_times[PS6000A_MAX_CHANNELS];
int16_t     	g_timeUnit;
int32_t      	g_sampleCount;
uint32_t		g_startIndex;
int16_t			g_trig = 0;
uint32_t		g_trigAt = 0;
int16_t			g_probeStateChanged = 0;

USER_PROBE_INFO userProbeInfo;

int8_t BlockFile[20] = "block.txt";

uint32_t	timebase = 8;

/****************************************************************************
* Gobal Variables
***************************************************************************/
extern BOOL		scaleVoltages;
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
* Callback Probe Interaction
*
* See ps6000aProbeInteractions (callback)
*
****************************************************************************/
void PREF4 CallBackProbeInteractions(int16_t handle, PICO_STATUS status, PICO_USER_PROBE_INTERACTIONS *probes, uint32_t	nProbes)
{
	uint32_t i = 0;

	userProbeInfo.status = status;
	userProbeInfo.numberOfProbes = nProbes;

	for (i = 0; i < nProbes; ++i)
	{
		userProbeInfo.userProbeInteractions[i].connected_ = probes[i].connected_;

		userProbeInfo.userProbeInteractions[i].channel_ = probes[i].channel_;
		userProbeInfo.userProbeInteractions[i].enabled_ = probes[i].enabled_;

		userProbeInfo.userProbeInteractions[i].probeName_ = probes[i].probeName_;

		userProbeInfo.userProbeInteractions[i].requiresPower_ = probes[i].requiresPower_;
		userProbeInfo.userProbeInteractions[i].isPowered_ = probes[i].isPowered_;

		userProbeInfo.userProbeInteractions[i].status_ = probes[i].status_;

		userProbeInfo.userProbeInteractions[i].probeOff_ = probes[i].probeOff_;

		userProbeInfo.userProbeInteractions[i].rangeFirst_ = probes[i].rangeFirst_;
		userProbeInfo.userProbeInteractions[i].rangeLast_ = probes[i].rangeLast_;
		userProbeInfo.userProbeInteractions[i].rangeCurrent_ = probes[i].rangeLast_;

		userProbeInfo.userProbeInteractions[i].couplingFirst_ = probes[i].couplingFirst_;
		userProbeInfo.userProbeInteractions[i].couplingLast_ = probes[i].couplingLast_;
		userProbeInfo.userProbeInteractions[i].couplingCurrent_ = probes[i].couplingCurrent_;

		userProbeInfo.userProbeInteractions[i].filterFlags_ = probes[i].filterFlags_;
		userProbeInfo.userProbeInteractions[i].filterCurrent_ = probes[i].filterCurrent_;
		userProbeInfo.userProbeInteractions[i].defaultFilter_ = probes[i].defaultFilter_;
	}

	g_probeStateChanged = 1;

}

/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void setDefaults(UNIT* unit)
{
	PICO_STATUS status;
	int32_t i;

	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		if (unit->channelSettings[PICO_CHANNEL_A + i].enabled == TRUE)
		{
			status = ps6000aSetChannelOn(unit->handle, (PICO_CHANNEL)(PICO_CHANNEL_A + i),
				(PICO_COUPLING)unit->channelSettings[PICO_CHANNEL_A + i].DCcoupled,
				(PICO_CONNECT_PROBE_RANGE)unit->channelSettings[PICO_CHANNEL_A + i].range,
				unit->channelSettings[PICO_CHANNEL_A + i].analogueOffset,
				PICO_BW_FULL);
			printf(status ? "SetDefaults:ps6000aSetChannelOn------ 0x%08lx \n" : "", status);
		}
		else
		{
			status = ps6000aSetChannelOff(unit->handle, (PICO_CHANNEL)(PICO_CHANNEL_A + i));
			printf(status ? "SetDefaults:ps6000aSetChannelOff------ 0x%08lx \n" : "", status);
		}
	}
}

/****************************************************************************
* ClearDataBuffers
*
* stops GetData writing values to memory that has been released
****************************************************************************/
PICO_STATUS clearDataBuffers(UNIT* unit)
{
	PICO_ACTION action_flag = PICO_CLEAR_ALL;
	PICO_STATUS status = 0;

	if ((status = ps6000aSetDataBuffers(unit->handle, PICO_CHANNEL_A, NULL, NULL, 0, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, action_flag)) != PICO_OK)
	{
		printf("ClearDataBuffers:ps6000aSetDataBuffers ------ 0x%08lx \n", status);
	}
	else
	{
		printf("Cleared all DataBuffers\n");
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
void blockDataHandler(UNIT* unit, int8_t* text, int32_t offset)
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
		//status = ps5000aGetValues(unit->handle, 0, (uint32_t*)&sampleCount, downSampleRatio, ratioMode, 0, NULL);
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
* SetTrigger
*
* - Used to call all the functions required to set up triggering
*
***************************************************************************/
PICO_STATUS SetTrigger(UNIT* unit,
	PICO_TRIGGER_CHANNEL_PROPERTIES* channelProperties,
	int16_t nChannelProperties,
	int16_t auxOutputEnable,
	PICO_CONDITION* triggerConditions,
	int16_t nTriggerConditions,
	PICO_DIRECTION* directions,
	int16_t nDirections,
	struct tPwq* pwq,
	uint32_t delay,
	int32_t autoTrigger_us)
{
	PICO_STATUS status;
	PICO_CONDITIONS_INFO info = PICO_CLEAR_CONDITIONS;
	PICO_CONDITIONS_INFO pwqInfo = PICO_CLEAR_CONDITIONS;

	if ((status = ps6000aSetTriggerChannelProperties(unit->handle,
		channelProperties,
		nChannelProperties,
		auxOutputEnable,
		autoTrigger_us)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetTriggerChannelProperties ------ Ox%08x \n", status);
		return status;
	}

	if (nTriggerConditions != 0)
	{
		info = (PICO_CONDITIONS_INFO)(PICO_CLEAR_CONDITIONS | PICO_ADD_CONDITION); // Clear and add trigger condition specified unless no trigger conditions have been specified
	}

	if ((status = ps6000aSetTriggerChannelConditions(unit->handle, triggerConditions, nTriggerConditions, info) != PICO_OK))
	{
		printf("SetTrigger:ps6000aSetTriggerChannelConditions ------ 0x%08x \n", status);
		return status;
	}

	if ((status = ps6000aSetTriggerChannelDirections(unit->handle, directions, nDirections)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetTriggerChannelDirections ------ 0x%08x \n", status);
		return status;
	}

	if ((status = ps6000aSetTriggerDelay(unit->handle, delay)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetTriggerDelay ------ 0x%08x \n", status);
		return status;
	}

	if ((status = ps6000aSetPulseWidthQualifierProperties(unit->handle,
		pwq->lower, pwq->upper, pwq->type)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetPulseWidthQualifierProperties ------ 0x%08x \n", status);
		return status;
	}

	//ps6000aSetPulseWidthQualifierDirections //////////////////////////////PASS ZERO DIRECTIONS???
	if ((status = ps6000aSetPulseWidthQualifierDirections(unit->handle,
		pwq->directions, pwq->nDirections)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetPulseWidthQualifierDirections ------ 0x%08x \n", status);
		return status;
	}

	// Clear and add pulse width qualifier condition, clear if no pulse width qualifier has been specified
	if (pwq->nConditions != 0)
	{
		pwqInfo = (PICO_CONDITIONS_INFO)(PICO_CLEAR_CONDITIONS | PICO_ADD_CONDITION);
	}

	if ((status = ps6000aSetPulseWidthQualifierConditions(unit->handle, pwq->conditions, pwq->nConditions, pwqInfo)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetPulseWidthQualifierConditions ------ 0x%08x \n", status);
		return status;
	}

	return status;
}

/****************************************************************************
* collectBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
void collectBlockImmediate(UNIT* unit)
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
* collectRapidBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
void collectRapidBlockImmediate(UNIT* unit)
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
* collectBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void collectBlockTriggered(UNIT* unit)
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

/****************************************************************************
* collectRapidBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void collectRapidBlockTriggered(UNIT* unit)
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

	struct tPicoDirection directions;
	directions.channel = conditions.source;
	directions.direction = PICO_RISING;
	directions.thresholdMode = PICO_LEVEL;

	//Create Pulse Width Qualifier structure with settings
	struct tPwq pulseWidth;
	memset(&pulseWidth, 0, sizeof(struct tPwq));//zero out pulseWidth

	printf("Collect RapidBlock triggered...\n");
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

	rapidblockDataHandler(unit, (int8_t*)"First 10 readings after trigger\n", 0);
}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void set_info(UNIT* unit)
{
	int8_t description[11][25] = { "Driver Version",
		"USB Version",
		"Hardware Version",
		"Variant Info",
		"Serial",
		"Cal Date",
		"Kernel Version",
		"Digital HW Version",
		"Analogue HW Version",
		"Firmware 1",
		"Firmware 2" };

	int16_t i = 0;
	int16_t requiredSize = 0;
	int8_t line[80];
	int32_t variant;
	PICO_STATUS status = PICO_OK;

	// Variables used for arbitrary waveform parameters
	int16_t			minArbitraryWaveformValue = 0;
	int16_t			maxArbitraryWaveformValue = 0;
	uint32_t		minArbitraryWaveformSize = 0;
	uint32_t		maxArbitraryWaveformSize = 0;

	//Initialise default unit properties and change when required
	unit->sigGen = SIGGEN_AWG;
	unit->firstRange = PICO_X1_PROBE_10MV;
	unit->lastRange = PICO_X1_PROBE_20V;
	unit->channelCount = DUAL_SCOPE;
	unit->digitalPortCount = 2;

	if (unit->handle)
	{
		printf("Device information:-\n\n");

		for (i = 0; i < 11; i++)
		{
			status = ps6000aGetUnitInfo(unit->handle, line, sizeof(line), &requiredSize, i);

			// info = 3 - PICO_VARIANT_INFO
			if (i == PICO_VARIANT_INFO)
			{
				variant = atoi(line);
				memcpy(&(unit->modelString), line, sizeof(unit->modelString) == 5 ? 5 : sizeof(unit->modelString));
				//memcpy(&(unit->modelString), line, sizeof(unit->modelString));

				unit->channelCount = (int16_t)line[1];
				unit->channelCount = unit->channelCount - 48; // Subtract ASCII 0 (48)

				// All models have 2 digital ports (MSO)
				unit->digitalPortCount = 2;

			}
			else if (i == PICO_BATCH_AND_SERIAL)	// info = 4 - PICO_BATCH_AND_SERIAL
			{
				memcpy(&(unit->serial), line, requiredSize);
			}

			printf("%s: %s\n", description[i], line);
		}

		printf("\n");

		


		// Set sig gen parameters
		// If device has Arbitrary Waveform Generator, find the maximum AWG buffer size
		/*
		status = ps6000aSigGenArbitraryMinMaxValues(unit->handle, &minArbitraryWaveformValue, &maxArbitraryWaveformValue, &minArbitraryWaveformSize, &maxArbitraryWaveformSize);
		unit->awgBufferSize = maxArbitraryWaveformSize;
		*/
	}
}

/****************************************************************************
* Select input voltage ranges for channels
****************************************************************************/
void setVoltages(UNIT* unit)
{
	PICO_STATUS status = PICO_OK;
	PICO_DEVICE_RESOLUTION resolution = PICO_DR_8BIT;

	int32_t i, ch;
	int32_t count = 0;
	int16_t numValidChannels = unit->channelCount; // Dependent on power setting - i.e. channel A & B if USB powered on 4-channel device
	int16_t numEnabledChannels = 0;
	int16_t retry = FALSE;

	// See what ranges are available... 
	for (i = unit->firstRange; i <= unit->lastRange; i++)
	{
		printf("%d -> %d mV\n", i, inputRanges[i]);
	}

	do
	{
		count = 0;

		do
		{
			// Ask the user to select a range
			printf("Specify voltage range (%d..%d)\n", unit->firstRange, unit->lastRange);
			printf("99 - switches channel off\n");

			for (ch = 0; ch < numValidChannels; ch++)
			{
				printf("\n");

				do
				{
					printf("Channel %c: ", 'A' + ch);
					fflush(stdin);
					//scanf_s("%hd", &(unit->channelSettings[ch].range));
					scanf_s("%d", &(unit->channelSettings[ch].range));

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
					unit->channelSettings[ch].range = PICO_X1_PROBE_20V - 1; //max range x1
				}
			}
			printf(count == 0 ? "\n** At least 1 channel must be enabled **\n\n" : "");
		} while (count == 0);	// must have at least one channel enabled

		status = ps6000aGetDeviceResolution(unit->handle, &resolution);

		// Verify that the number of enabled channels is valid for the resolution set.

		switch (resolution)
		{
		case PICO_DR_10BIT:

			if (count > 4)
			{
				printf("\nError: Only 4 channels may be enabled with 10-bit resolution set.\n");
				printf("Please switch off %d channel(s).\n", numValidChannels - 4);
				retry = TRUE;
			}
			else
			{
				retry = FALSE;
			}
			break;

		case PICO_DR_12BIT:

			if (count > 2)
			{
				printf("\nError: Only one channes may be enabled with 12-bit resolution set.\n");
				printf("Please switch off %d channel(s).\n", numValidChannels - 2);
				retry = TRUE;
			}
			else
			{
				retry = FALSE;
			}

			break;

		default:

			retry = FALSE;
			break;
		}

		printf("\n");
	} while (retry == TRUE);

	setDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
* setTimebase
* Select timebase, set time units as nano seconds
*
****************************************************************************/
void setTimebase(UNIT* unit)
{
	PICO_STATUS status = PICO_OK;
	PICO_STATUS powerStatus = PICO_OK;
	double timeInterval;//int32_t
	uint64_t maxSamples; //int32_t
	int32_t ch;

	uint32_t shortestTimebase;
	double timeIntervalSeconds;

	PICO_CHANNEL_FLAGS enabledChannelOrPortFlags = (PICO_CHANNEL_FLAGS)0;

	int16_t numValidChannels = unit->channelCount; // Dependent on power setting - i.e. channel A & B if USB powered on 4-channel device

	// Find the analogue channels that are enabled - if an MSO model is being used, this will need to be
	// modified to add channel flags for enabled digital ports
	for (ch = 0; ch < numValidChannels; ch++)
	{
		if (unit->channelSettings[ch].enabled)
		{
			enabledChannelOrPortFlags = enabledChannelOrPortFlags | (PICO_CHANNEL_FLAGS)(1 << ch);
		}
	}

	// Find the shortest possible timebase and inform the user.
	status = ps6000aGetMinimumTimebaseStateless(unit->handle, enabledChannelOrPortFlags, &shortestTimebase, &timeIntervalSeconds, unit->resolution);

	if (status != PICO_OK)
	{
		printf("setTimebase:ps6000aGetMinimumTimebaseStateless ------ 0x%08lx \n", status);
		if(status == 0x0000018c)
			printf("The channel combination is not valid for the ADC resolution (10/12bit)");
		return;
	}

	//printf("Shortest timebase index available %d (%.9f seconds).\n", shortestTimebase, timeIntervalSeconds);
	printf("Shortest timebase index available %d = %le seconds.\n", shortestTimebase, timeIntervalSeconds);
	//%le

	printf("Specify desired timebase: ");
	fflush(stdin);
	scanf_s("%lud", &timebase);
	//scanf_s("%llud", &timebase);

	do
	{
		status = ps6000aGetTimebase(unit->handle, timebase, BUFFER_SIZE, &timeInterval, &maxSamples, 0);

		if (status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION)
		{
			printf("SetTimebase: Error - Invalid number of channels for resolution.\n");
			return;
		}
		else if (status == PICO_OK)
		{
			// Do nothing
		}
		else
		{
			timebase++; // Increase timebase if the one specified can't be used. 
		}

	} while (status != PICO_OK);

	//printf("Timebase used %lu = %ld ns sample interval\n", timebase, timeInterval);
	//printf("Timebase used %lu = %le ns sample interval\n", timebase, timeInterval);
	printf("Timebase used %lu = %le seconds sample interval\n", timebase, timeInterval * 1e-9);
}

/****************************************************************************
* printResolution
*
* Outputs the resolution in text format to the console window
****************************************************************************/
void printResolution(PICO_DEVICE_RESOLUTION* resolution)
{
	switch (*resolution)
	{
	case PICO_DR_8BIT:

		printf("8 bits");
		break;

	case PICO_DR_10BIT:

		printf("10 bits");
		break;

	case PICO_DR_12BIT:

		printf("12 bits");
		break;

	case PICO_DR_14BIT:

		printf("14 bits");
		break;

	case PICO_DR_15BIT:

		printf("15 bits");
		break;

	case PICO_DR_16BIT:

		printf("16 bits");
		break;

	default:

		break;
	}

	printf("\n");
}

/****************************************************************************
* setResolution
* Set resolution for the device
*
****************************************************************************/
void setResolution(UNIT* unit)
{
	int16_t value = 0;
	int16_t i;
	int16_t numEnabledChannels = 0;
	int16_t retry;
	int32_t resolutionInput;

	PICO_STATUS status;
	PICO_DEVICE_RESOLUTION resolution;
	PICO_DEVICE_RESOLUTION newResolution = PICO_DR_8BIT;

	// Determine number of channels enabled
	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled == TRUE)
		{
			numEnabledChannels++;
		}
	}

	if (numEnabledChannels == 0)
	{
		printf("setResolution: Please enable channels.\n");
		return;
	}

	status = ps6000aGetDeviceResolution(unit->handle, &resolution);

	if (status == PICO_OK)
	{
		printf("Current resolution: ");
		printResolution(&resolution);
	}
	else
	{
		printf("setResolution:ps6000aGetDeviceResolution ------ 0x%08lx \n", status);
		return;
	}

	printf("\n");

	printf("Select device resolution:\n");
	printf("0: 8 bits\n");
	printf("1: 10 bits\n");
	printf("2: 12 bits\n");

	//if(numEnabledChannels <= 2)
	//{
	//	printf("3: 15 bits\n");
	//}


	retry = TRUE;

	do
	{
		/* if (numEnabledChannels == 1)
		{
			printf("Resolution [0...4]: ");
		}
		else if(numEnabledChannels == 2)
		{
			printf("Resolution [0...3]: ");
		}
		else
		{
			printf("Resolution [0...2]: ");
		} */
		printf("Resolution [0...2]: ");

		fflush(stdin);
		scanf_s("%lud", &resolutionInput);
		if (resolutionInput == 1)
			resolutionInput = PICO_DR_10BIT;
		if (resolutionInput == 2)
			resolutionInput = PICO_DR_12BIT;
		newResolution = (PICO_DEVICE_RESOLUTION)resolutionInput;

		// Verify if resolution can be selected for number of channels enabled

		if (newResolution == PICO_DR_12BIT && numEnabledChannels > 2)
		{
			printf("setResolution: 12 bit resolution can only be selected with 2 channel enabled.\n");
		}
		else if (newResolution == PICO_DR_10BIT && numEnabledChannels > 4)
		{
			printf("setResolution: 10 bit resolution can only be selected with a maximum of 4 channels enabled.\n");
		}
		else if (newResolution < PICO_DR_8BIT && newResolution > PICO_DR_10BIT)
		{
			printf("setResolution: Resolution index selected out of bounds.\n");
		}
		else
		{
			retry = FALSE;
		}
	} while (retry);

	printf("\n");

	status = ps6000aSetDeviceResolution(unit->handle, (PICO_DEVICE_RESOLUTION)newResolution);

	if (status == PICO_OK)
	{
		unit->resolution = newResolution;

		printf("Resolution selected: ");
		printResolution(&newResolution);

		// The maximum ADC value will change if transitioning from 8 bit to >= 12 bit or vice-versa
		status = ps6000aGetAdcLimits(unit->handle, newResolution, NULL, &value);
		unit->maxADCValue = value;
	}
	else
	{
		printf("setResolution:ps6000aSetDeviceResolution ------ 0x%08lx \n", status);
	}

}

/****************************************************************************
* CollectRapidBlock
*  This function demonstrates how to collect a set of captures using
*  rapid block mode.
****************************************************************************/
void rapidblockDataHandler(UNIT* unit, int8_t* text, int32_t offset)
{
	int16_t i;
	int16_t channel;

	uint64_t capture;

	int64_t nMaxSamples;
	double timeIndisposed;

	uint64_t nCaptures;
	uint64_t nSamples = 1000;
	uint64_t nCompletedCaptures;

	int16_t*** rapidBuffers;
	int16_t* overflow;

	PICO_STATUS status = 0;
	PICO_RATIO_MODE ratioMode = PICO_RATIO_MODE_RAW;//used for RunBlock()
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);//bitwise OR flags for first buffer that is set

	
	printf(scaleVoltages ? "mV\n" : "ADC Counts\n");
	printf("Press any key to abort\n");

	setDefaults(unit);

	//Set the number of captures
	nCaptures = 3;

	//Segment the memory
	status = ps6000aMemorySegments(unit->handle, nCaptures, &nMaxSamples);

	//Set the number of captures
	status = ps6000aSetNoOfCaptures(unit->handle, nCaptures);

	//Run
	timebase = 7; // 10 MS/s

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

	// Allocate memory
	rapidBuffers = (int16_t***)calloc(unit->channelCount, sizeof(int16_t*));
	overflow = (int16_t*)calloc(unit->channelCount * nCaptures, sizeof(int16_t));

	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			rapidBuffers[channel] = (int16_t**)calloc(nCaptures, sizeof(int16_t*));
			for (capture = 0; capture < nCaptures; capture++)
			{
				rapidBuffers[channel][capture] = (int16_t*)calloc(nSamples, sizeof(int16_t));
			}
		}
	}

	//for (channel = 0; channel < unit->channelCount; channel++)
	//{
		//if (unit->channelSettings[channel].enabled)
		//{
		//	for (capture = 0; capture < nCaptures; capture++)
		//	{
		//		rapidBuffers[channel][capture] = (int16_t*)calloc(nSamples, sizeof(int16_t));
		//	}
		//}
	//}

	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++)
			{
				status = ps6000aSetDataBuffers(unit->handle,
					(PICO_CHANNEL)channel,
					rapidBuffers[channel][capture],
					NULL,
					(int32_t)nSamples,
					PICO_INT16_T, //PICO_DATA_TYPE
					capture,
					ratioMode,
					action_flag);
				action_flag = PICO_ADD;//all subsequent calls use ADD!

				if (status != PICO_OK)
				{
					printf("RapidBlockDataHandler:ps6000aSetDataBuffers ------ 0x%08x, for channel %d \n", status, channel);
				}
			}
		}
	}

	// Get data
	status = ps6000aGetValuesBulk(unit->handle,
		0,						//Start Index for each segment
		&nSamples,				//Number of samples for each segment
		0,						//From Segment
		nCaptures - 1,			//To Segment
		1,						//Down Sample Ratio
		ratioMode,				//Down Sample Ratio mode
		overflow);				//Array of Channel overrage flags

	if (status == PICO_OK)
	{
		// Print first 10 samples from each capture
		for (capture = 0; capture < nCaptures; capture++)
		{
			printf("\nCapture %llu:-\n\n", capture + 1);

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
						if (rapidBuffers[channel][capture])//Check buffer is not NULL
						{//3.3e //6d
							printf("   %3.3e       ", scaleVoltages ?
								adc_to_mv(rapidBuffers[channel][capture][i],
									unit->channelSettings[PICO_CHANNEL_A + channel].range,
									unit->maxADCValue)														// If scaleVoltages, print mV value
								: rapidBuffers[channel][capture][i]);
						}// else print ADC Count
					}
				}
				printf("\n");
			}
		}
	}

	// Stop
	status = ps6000aStop(unit->handle);

	// Free memory
	clearDataBuffers(unit);
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
* displaySettings
* Displays information about the user configurable settings in this example
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void displaySettings(UNIT* unit)
{
	int32_t ch;
	int32_t voltage;
	PICO_STATUS status = PICO_OK;
	PICO_DEVICE_RESOLUTION resolution = PICO_DR_8BIT;

	printf("\nReadings will be scaled in %s\n", (scaleVoltages) ? ("millivolts") : ("ADC counts"));
	printf("\n");

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

	status = ps6000aGetDeviceResolution(unit->handle, &resolution);

	printf("Device Resolution: ");
	printResolution(&resolution);

}

/****************************************************************************
* openDevice
* Parameters
* - unit        pointer to the UNIT structure, where the handle will be stored
* - serial		pointer to the int8_t array containing serial number
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS openDevice(UNIT* unit, int8_t* serial)
{
	PICO_STATUS status;
	unit->resolution = PICO_DR_8BIT;

	if (serial == NULL)
	{
		status = ps6000aOpenUnit(&unit->handle, NULL, unit->resolution);
	}
	else
	{
		status = ps6000aOpenUnit(&unit->handle, serial, unit->resolution);
	}

	unit->openStatus = (int16_t)status;
	unit->complete = 1;

	return status;
}

/****************************************************************************
* handleDevice
* Parameters
* - unit        pointer to the UNIT structure, where the handle will be stored
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS handleDevice(UNIT* unit)
{
	int16_t value = 0;
	int32_t i;
	/////////struct tPwq pulseWidth;
	PICO_STATUS status;

	printf("Handle: %d\n", unit->handle);

	if (unit->openStatus != PICO_OK)
	{
		printf("Unable to open device\n");
		printf("Error code : 0x%08x\n", (uint32_t)unit->openStatus);
		while (!_kbhit());
		exit(99); // exit program
	}

	printf("Device opened successfully, cycle %d\n\n", ++cycles);

	// Setup device info - unless it's set already
	if (unit->model == MODEL_NONE)
	{
		set_info(unit);
	}

	// Turn off any digital ports (MSO models only)
	if (unit->digitalPortCount > 0)
	{
		printf("Turning off digital ports.\n");

		for (i = 0; i < unit->digitalPortCount; i++)
		{
			status = ps6000aSetDigitalPortOff(unit->handle, (PICO_CHANNEL)(i + PICO_PORT0));
		}
	}

	timebase = 0;

	status = ps6000aGetAdcLimits(unit->handle, PICO_DR_8BIT, NULL, &value);
	unit->maxADCValue = value;

	int16_t enabled_chs_limit = unit->channelCount;
	if (unit->channelCount > ENABLED_CHS_LIMIT)
	{
		enabled_chs_limit = ENABLED_CHS_LIMIT;
		printf("Limiting enabled channels to %d! (Starting at ChA)\n", enabled_chs_limit);
	}
	if(TURN_ON_EVERY_N_CH != 1)
		printf("Turning on every %d Channel\n", TURN_ON_EVERY_N_CH);

	for (i = 0; i < unit->channelCount; i++)
	{
		//define "TURN_ON_EVERY_N_CH" to either 2 or 4 (2 = Every odd Ch is enabled, 4 = Every 4th Ch enabled), set 1 to disable.
		if ( i % TURN_ON_EVERY_N_CH == 0 && i < enabled_chs_limit)
			unit->channelSettings[i].enabled = TRUE;
		else
			unit->channelSettings[i].enabled = FALSE;

		unit->channelSettings[i].DCcoupled = TRUE;
		unit->channelSettings[i].range = PICO_X1_PROBE_2V;
		unit->channelSettings[i].analogueOffset = 0.0f;
	}

	///////////memset(&pulseWidth, 0, sizeof(struct tPwq));

	setDefaults(unit);

	/* Trigger disabled	*/
	status = ps6000aSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

	return unit->openStatus;
}

/****************************************************************************
* closeDevice
****************************************************************************/
void closeDevice(UNIT* unit)
{
	ps6000aCloseUnit(unit->handle);
}