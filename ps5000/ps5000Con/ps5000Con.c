/*******************************************************************************
 *
 * Filename: ps5000Con.c
 *
 * Description:
 *   This is a console-mode program that demonstrates how to use the
 *   ps5000 driver API functions.
 *
 *	Supported PicoScope models:
 *
 *		PicoScope 5203 & 5204
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *    Collect a block using ETS
 *	  Collect samples using a rapid block capture with trigger 
 *    Collect a stream of data immediately
 *    Collect a stream of data when a trigger event occurs
 *    Set Signal Generator, using built in or custom signals
 *
  *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps5000.lib can be located
 *			Ensure that the ps5000Api.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps5000.lib to the project (Microsoft C only)
 *			 Add ps5000Api.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps5000 driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the ps5000Con application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2006-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "ps5000Api.h"
#define PREF4 __stdcall
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <libps5000/ps5000Api.h>
#ifndef PICO_STATUS
#include <libps5000/PicoStatus.h>
#define PREF4
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL
{
	FALSE, TRUE
} BOOL;

/* A function to detect a keyboard press on Linux */
int32_t _getch()
{
	struct termios oldt, newt;
	int32_t ch;
	int32_t bytesWaiting;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	do
	{
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
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return bytesWaiting;
}

int32_t fopen_s(FILE ** a, const char * b, const char * c)
{
	FILE * fp = fopen(b, c);
	*a = fp;
	return (fp > 0) ? 0 : -1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

#define BUFFER_SIZE 	1024
#define MAX_CHANNELS 4
#define QUAD_SCOPE 4
#define DUAL_SCOPE 2

typedef struct
{
	int16_t DCcoupled;
	PS5000_RANGE range;
	int16_t enabled;
} CHANNEL_SETTINGS;

typedef enum
{
	MODEL_NONE = 0, MODEL_PS5203 = 5203, MODEL_PS5204 = 5204
} MODEL_TYPE;

typedef struct tTriggerDirections
{
	enum enThresholdDirection channelA;
	enum enThresholdDirection channelB;
	enum enThresholdDirection channelC;
	enum enThresholdDirection channelD;
	enum enThresholdDirection ext;
	enum enThresholdDirection aux;
} TRIGGER_DIRECTIONS;

typedef struct tPwq
{
	PWQ_CONDITIONS * conditions;
	int16_t nConditions;
	enum enThresholdDirection direction;
	uint32_t lower;
	uint32_t upper;
	enum enPulseWidthType type;
} PWQ;

typedef struct
{
	int16_t handle;
	MODEL_TYPE model;
	PS5000_RANGE firstRange;
	PS5000_RANGE lastRange;
	unsigned char signalGenerator;
	unsigned char external;
	int16_t ChannelCount;
	CHANNEL_SETTINGS channelSettings[MAX_CHANNELS];
	PS5000_RANGE triggerRange;
} UNIT_MODEL;

uint32_t timebase = 8;
int16_t oversample = 1;
int32_t scaleVoltages = TRUE;

uint16_t inputRanges[PS5000_MAX_RANGES] =
{ 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000 };

int16_t g_ready = FALSE;
int64_t g_times[PS5000_MAX_CHANNELS];
int16_t g_timeUnit;
int32_t g_sampleCount;
uint32_t g_startIndex;
int16_t g_autoStop;
uint32_t g_trigAt;
int16_t g_trig;

typedef struct tBufferInfo
{
	UNIT_MODEL * unit;
	int16_t **driverBuffers;
	int16_t **appBuffers;

} BUFFER_INFO;

/****************************************************************************
 * Callback
 * used by ps5000 data streaimng collection calls, on receipt of data.
 * used to set global flags etc checked by user routines
 ****************************************************************************/
void PREF4 CallBackStreaming(int16_t handle,
		int32_t noOfSamples,
		uint32_t startIndex, //
		int16_t overflow, uint32_t triggerAt, int16_t triggered,
		int16_t autoStop, void * pParameter)
{
	int32_t channel;
	BUFFER_INFO * bufferInfo = NULL;

	if (pParameter != NULL)
	{
		bufferInfo = (BUFFER_INFO *)pParameter;
	}

	// used for streaming
	g_sampleCount = noOfSamples;
	g_startIndex = startIndex;
	g_autoStop = autoStop;

	// flag to say done reading data
	g_ready = TRUE;

	// flags to show if & where a trigger has occurred
	g_trig = triggered;
	g_trigAt = triggerAt;

	if (bufferInfo != NULL && noOfSamples)
	{
		for (channel = 0; channel < bufferInfo->unit->ChannelCount; channel++)
		{
			if (bufferInfo->unit->channelSettings[channel].enabled)
			{
				if (bufferInfo->appBuffers && bufferInfo->driverBuffers)
				{
					// Max buffers
					if (bufferInfo->appBuffers[channel * 2] && bufferInfo->driverBuffers[channel * 2])
					{
						memcpy_s(&bufferInfo->appBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
					}

					// Min buffers
					if (bufferInfo->appBuffers[channel * 2 + 1] && bufferInfo->driverBuffers[channel * 2 + 1])
					{
						memcpy_s(&bufferInfo->appBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
					}
				}
			}
		}
	}
}

/****************************************************************************
 * Callback
 * used by ps5000 data block collection calls, on receipt of data.
 * used to set global flags etc checked by user routines
 ****************************************************************************/
void PREF4 CallBackBlock(int16_t handle, PICO_STATUS status, void * pParameter)
{
	// flag to say done reading data
	g_ready = TRUE;
}

/****************************************************************************
 * SetDefaults - restore default settings
 ****************************************************************************/
void SetDefaults(UNIT_MODEL * unit)
{
	int32_t i;

	ps5000SetEts(unit->handle, PS5000_ETS_OFF, 0, 0, NULL); // Turn off ETS

	for (i = 0; i < unit->ChannelCount; i++) // reset channels to most recent settings
	{
		ps5000SetChannel(unit->handle, (PS5000_CHANNEL) (PS5000_CHANNEL_A + i),
				unit->channelSettings[PS5000_CHANNEL_A + i].enabled,
				unit->channelSettings[PS5000_CHANNEL_A + i].DCcoupled,
				unit->channelSettings[PS5000_CHANNEL_A + i].range);
	}
}

/****************************************************************************
 * adc_to_mv
 *
 * If the user selects scaling to millivolts,
 * Convert an 16-bit ADC count into millivolts
 ****************************************************************************/
int32_t adc_to_mv(int32_t raw, int ch)
{
	return (scaleVoltages) ? (raw * inputRanges[ch]) / PS5000_MAX_VALUE : raw;
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
	return ((mv * PS5000_MAX_VALUE) / inputRanges[ch]);
}

/****************************************************************************
 * BlockDataHandler
 * - Used by all block data routines except rapid block.
 * - acquires data (user sets trigger mode before calling), displays 10 items
 *   and saves all to data.txt
 * Input :
 * - unit : the unit to use.
 * - text : the text to display before the display of data slice
 * - offset : the offset into the data buffer to start the display's slice.
 ****************************************************************************/
void BlockDataHandler(UNIT_MODEL * unit, char * text, int32_t offset)
{
	int32_t i, j;
	int32_t timeInterval;
	int32_t sampleCount = BUFFER_SIZE;
	FILE * fp;
	int32_t maxSamples;
	int16_t * buffers[PS5000_MAX_CHANNEL_BUFFERS * 2];
	int32_t timeIndisposed;
	PICO_STATUS status;

	for (i = 0; i < unit->ChannelCount; i++)
	{
		buffers[i * 2] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
		buffers[i * 2 + 1] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
		ps5000SetDataBuffers(unit->handle, (PS5000_CHANNEL) i, buffers[i * 2],
				buffers[i * 2 + 1], sampleCount);
	}

	/*  find the maximum number of samples, the time interval (in timeUnits),
	 *		 the most suitable time units, and the maximum oversample at the current timebase*/
	while (ps5000GetTimebase(unit->handle, timebase, sampleCount,
			&timeInterval, oversample, &maxSamples, 0))
	{
		timebase++;
	}
	printf("timebase: %hd\toversample:%hd\n", timebase, oversample);

	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;
	status = ps5000RunBlock(unit->handle, 0, sampleCount, timebase, oversample,
			&timeIndisposed, 0, CallBackBlock, NULL);
	if (status != PICO_OK)
	{
		printf(
				"Immediately Block Mode: failed to call run_block successfully \n");
		return;
	}
	
	printf("Waiting for trigger...Press a key to abort\n");

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (g_ready)
	{
		ps5000GetValues(unit->handle, 0, (uint32_t*) &sampleCount, 1,
				RATIO_MODE_NONE, 0, NULL);

		/* Print out the first 10 readings, converting the readings to mV if required */
		printf(text);
		printf("Value (%s)\n", (scaleVoltages) ? ("mV") : ("ADC Counts"));

		for (i = offset; i < offset + 10; i++)
		{
			for (j = 0; j < unit->ChannelCount; j++)
			{
				if (unit->channelSettings[j].enabled)
				{
					printf("%d\t", adc_to_mv(buffers[j * 2][i],
							unit->channelSettings[PS5000_CHANNEL_A + j].range));
				}
			}
			printf("\n");
		}

		sampleCount = min(sampleCount, BUFFER_SIZE);

		fp = fopen("data.txt", "w");

		for (i = 0; i < sampleCount; i++)
		{
			for (j = 0; j < unit->ChannelCount; j++)
			{
				fprintf(fp, "%ld ", g_times[j] + (int64_t) (i * timeInterval));
				if (unit->channelSettings[j].enabled)
				{
					fprintf(
							fp,
							", %d, %d, %d, %d",
							buffers[j * 2][i],
							adc_to_mv(
									buffers[j * 2][i],
									unit->channelSettings[PS5000_CHANNEL_A + j].range),
							buffers[j * 2 + 1][i],
							adc_to_mv(
									buffers[j * 2 + 1][i],
									unit->channelSettings[PS5000_CHANNEL_A + j].range));
				}
			}
			fprintf(fp, "\n");
		}
		fclose(fp);
	}
	else
	{
		printf("data collection aborted\n");
		_getch();
	}

	ps5000Stop(unit->handle);

	for (i = 0; i < unit->ChannelCount * 2; i++)
	{
		free(buffers[i]);
	}
}

PICO_STATUS SetTrigger(int16_t handle,
		TRIGGER_CHANNEL_PROPERTIES * channelProperties,
		int16_t nChannelProperties, TRIGGER_CONDITIONS * triggerConditions,
		int16_t nTriggerConditions, TRIGGER_DIRECTIONS * directions, PWQ * pwq,
		uint32_t delay, int16_t auxOutputEnabled, int32_t autoTriggerMs)
{
	PICO_STATUS status;

	if ((status = ps5000SetTriggerChannelProperties(handle, channelProperties,
			nChannelProperties, auxOutputEnabled, autoTriggerMs)) != PICO_OK)
	{
		return status;
	}

	if ((status = ps5000SetTriggerChannelConditions(handle,
			(TRIGGER_CONDITIONS*) triggerConditions, nTriggerConditions))
			!= PICO_OK)
	{
		return status;
	}

	if ((status = ps5000SetTriggerChannelDirections(handle,
			directions->channelA, directions->channelB, directions->channelC,
			directions->channelD, directions->ext, directions->aux)) != PICO_OK)
	{
		return status;
	}

	if ((status = ps5000SetTriggerDelay(handle, delay)) != PICO_OK)
	{
		return status;
	}

	status
			= ps5000SetPulseWidthQualifier(handle, pwq->conditions,
					pwq->nConditions, pwq->direction, pwq->lower, pwq->upper,
					pwq->type);

	return status;
}

/****************************************************************************
 * CollectBlockImmediate
 *  this function demonstrates how to collect a single block of data
 *  from the unit (start collecting immediately)
 ****************************************************************************/
void CollectBlockImmediate(UNIT_MODEL * unit)
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
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0,
			0);

	BlockDataHandler(unit, "First 10 readings\n", 0);
}

/****************************************************************************
 * CollectBlockEts
 *  this function demonstrates how to collect a block of
 *  data using equivalent time sampling (ETS).
 ****************************************************************************/

void CollectBlockEts(UNIT_MODEL * unit)
{
	PICO_STATUS status;
	int64_t buffer[BUFFER_SIZE];
	int32_t ets_sampletime;
	int16_t triggerVoltage = mv_to_adc(100,
			unit->channelSettings[PS5000_CHANNEL_A].range); // ChannelInfo stores ADC counts
	struct tTriggerChannelProperties sourceDetails =
	{ triggerVoltage, triggerVoltage, 10, PS5000_CHANNEL_A, LEVEL };
	struct tTriggerConditions conditions =
	{ CONDITION_TRUE, CONDITION_DONT_CARE, CONDITION_DONT_CARE,
			CONDITION_DONT_CARE, CONDITION_DONT_CARE, CONDITION_DONT_CARE,
			CONDITION_DONT_CARE };
	uint32_t delay = 0;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));
	directions.channelA = RISING;

	printf("Collect ETS block...\n");
	printf("Collects when value rises past %dmV\n", adc_to_mv(
			sourceDetails.thresholdMajor,
			unit->channelSettings[PS5000_CHANNEL_A].range));
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	 * Rising edge
	 * Threshold = 1500mV
	 * 10% pre-trigger  (negative is pre-, positive is post-) */
	status = SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1,
			&directions, &pulseWidth, delay, 0, 0);

	/* printf("Set Trigger : %x" , status); */

	/* Enable ETS in fast mode, the computer will store 100 cycles but interleave only 10 */
	status
			= ps5000SetEts(unit->handle, PS5000_ETS_FAST, 20, 4,
					&ets_sampletime);
	/*printf("Set ETS : %x" , status);*/
	printf("ETS Sample Time is: %ld\n", ets_sampletime);

	//Set ETS times buffer
	status = ps5000SetEtsTimeBuffer(unit->handle, buffer, BUFFER_SIZE);

	BlockDataHandler(unit, "Ten readings after trigger\n", BUFFER_SIZE / 10 - 5); // 10% of data is pre-trigger
}

/****************************************************************************
 * CollectBlockTriggered
 *  this function demonstrates how to collect a single block of data from the
 *  unit, when a trigger event occurs.
 ****************************************************************************/
void CollectBlockTriggered(UNIT_MODEL * unit)
{
	int16_t triggerVoltage = mv_to_adc(100,
			unit->channelSettings[PS5000_CHANNEL_A].range); // ChannelInfo stores ADC counts
	struct tTriggerChannelProperties sourceDetails =
	{ triggerVoltage, triggerVoltage, 256 * 10, PS5000_CHANNEL_A, LEVEL };
	struct tTriggerConditions conditions =
	{ CONDITION_TRUE, CONDITION_DONT_CARE, CONDITION_DONT_CARE,
			CONDITION_DONT_CARE, CONDITION_DONT_CARE, CONDITION_DONT_CARE,
			CONDITION_DONT_CARE };
	struct tPwq pulseWidth;
	struct tTriggerDirections directions =
	{ RISING, NONE, NONE, NONE, NONE, NONE };
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block triggered...\n");
	printf("Collects when value rises past %dmV\n", adc_to_mv(
			sourceDetails.thresholdMajor,
			unit->channelSettings[PS5000_CHANNEL_A].range));
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	 * Rising edge
	 * Threshold = 100mV */
	SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1, &directions,
			&pulseWidth, 0, 0, 0);

	BlockDataHandler(unit, "Ten readings after trigger\n", 0);
}

/****************************************************************************
 * CollectBlockLogicTriggered
 *  this function demonstrates how to collect a single block of data from the
 *  unit using logic triggering (Trigger on channel A OR channel B)
 ****************************************************************************/
void CollectBlockLogicTriggered(UNIT_MODEL * unit)
{
	int16_t triggerVoltage = mv_to_adc(100,
			unit->channelSettings[PS5000_CHANNEL_A].range); // ChannelInfo stores ADC counts
	//Set properties for both channels A and B
	struct tTriggerChannelProperties sourceDetails[2] =
	{
	{ triggerVoltage, triggerVoltage, 256 * 10, PS5000_CHANNEL_A, LEVEL },
	{ triggerVoltage, triggerVoltage, 256 * 10, PS5000_CHANNEL_B, LEVEL } };
	//Trigger on channel A OR channel B
	struct tTriggerConditions conditions[2] =
	{
		{ CONDITION_TRUE, CONDITION_DONT_CARE, CONDITION_DONT_CARE,	CONDITION_DONT_CARE, CONDITION_DONT_CARE, CONDITION_DONT_CARE, CONDITION_DONT_CARE },
		{ CONDITION_DONT_CARE, CONDITION_TRUE, CONDITION_DONT_CARE,	CONDITION_DONT_CARE, CONDITION_DONT_CARE, CONDITION_DONT_CARE, CONDITION_DONT_CARE }
	};
	struct tPwq pulseWidth;
	struct tTriggerDirections directions =
	{ RISING, RISING, NONE, NONE, NONE, NONE };

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block logic triggering.\n");
	printf("Trigger on channel A OR channel B.\n");
	printf("Press a key to start...\n");
	_getch();

	SetDefaults(unit);

	SetTrigger(unit->handle, (TRIGGER_CHANNEL_PROPERTIES *) &sourceDetails, 2,
			(TRIGGER_CONDITIONS *) &conditions, 2, &directions, &pulseWidth, 0,
			0, 0);

	BlockDataHandler(unit, "Ten readings after trigger\n", 0);
}

/****************************************************************************
 * CollectRapidBlock
 *  this function demonstrates how to use rapid block mode.
 ****************************************************************************/
void CollectRapidBlock(UNIT_MODEL * unit)
{
	int32_t timeInterval;
	int32_t maxSamples;
	int32_t sampleCount = BUFFER_SIZE;
	PICO_STATUS status;
	int32_t timeIndisposed;
	int16_t **rapidBuffers;
	int16_t * overflow;
	int32_t i, j, k, ch, block;
	int32_t nSamples;
	FILE * fp;

	int16_t nCaptures = 10; //The number of blocks we wish to capture

	printf("Data are written to Rapid Block.txt...\n");
	printf("Press a key to start...\n");
	_getch();

	//Set fastest possible timebase
	while (ps5000GetTimebase(unit->handle, timebase, sampleCount,
			&timeInterval, oversample, &maxSamples, 0))
	{
		timebase++;
	}

	status = ps5000MemorySegments(unit->handle, nCaptures, &nSamples);

	status = ps5000SetNoOfCaptures(unit->handle, nCaptures);

	status = ps5000RunBlock(unit->handle, 0, sampleCount, timebase, oversample,
			&timeIndisposed, 0, CallBackBlock, NULL);

	rapidBuffers = malloc(nCaptures * sizeof(int16_t*) * unit->ChannelCount);
	overflow = (int16_t *) malloc(nCaptures * sizeof(int16_t));

	//Set data buffers for rapid block mode
	i = 0;
	for (ch = 0; ch < unit->ChannelCount; ch++)
	{
		for (block = 0; block < nCaptures; block++)
		{
			rapidBuffers[i] = (int16_t*) malloc(sampleCount * sizeof(int16_t));
			status = ps5000SetDataBufferBulk(unit->handle, ch, rapidBuffers[i],
					sampleCount, block);
			i++;
		}
	}

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (g_ready)
	{
		status = ps5000GetValuesBulk(unit->handle, &sampleCount, 0, nCaptures
				- 1, overflow);

		//Write buffers to a text file
		fp = fopen("Rapid Block.txt", "w");

		fprintf(fp, "Each column is one block of data\n\n");

		for (j = 0; j < sampleCount; j++)
		{
			for (i = 0; i < unit->ChannelCount * nCaptures; i++)
			{
				fprintf(fp, "\t%d,", rapidBuffers[i][j]);
			}
			fprintf(fp, "\n");
		}

		fclose(fp);
	}

	//Free buffers
	i = 0;
	for (ch = 0; ch < unit->ChannelCount; ch++)
	{
		for (block = 0; block < nCaptures; block++)
		{
			free(rapidBuffers[i]);
			i++;
		}

	}
	free(rapidBuffers);
	free(overflow);
}

/****************************************************************************
 * Initialise unit' structure with Variant specific defaults
 ****************************************************************************/
void get_info(UNIT_MODEL * unit)
{
	char description[6][25] =
	{ "Driver Version", "USB Version", "Hardware Version", "Variant Info",
			"Serial", "Error Code" };
	int16_t i, r = 0;
	char line[80];
	int32_t variant;

	PICO_STATUS status;

	if (unit->handle)
	{
		for (i = 0; i < 5; i++)
		{
			status = ps5000GetUnitInfo(unit->handle, line, sizeof(line), &r, i);
			//printf("Get the unit info: status = %d \n\n\n", status);
			if (i == 3)
			{
				variant = atoi(line);
			}
			printf("%s: %s\n", description[i], line);
		}

		switch (variant)
		{
		case MODEL_PS5203:
			unit->model = MODEL_PS5203;
			unit->external = TRUE;
			unit->signalGenerator = TRUE;
			unit->firstRange = PS5000_100MV;
			unit->lastRange = PS5000_20V;
			unit->ChannelCount = DUAL_SCOPE;
			break;
		case MODEL_PS5204:
			unit->model = MODEL_PS5204;
			unit->external = TRUE;
			unit->signalGenerator = TRUE;
			unit->firstRange = PS5000_100MV;
			unit->lastRange = PS5000_20V;
			unit->ChannelCount = DUAL_SCOPE;
			break;
		default:
			break;
		}
	}
}

/****************************************************************************
 * Select input voltage ranges for channels A and B
 ****************************************************************************/
void set_voltages(UNIT_MODEL * unit)
{
	int32_t i;
	int32_t ch;

	/* See what ranges are available... */
	for (i = unit->firstRange; i <= unit->lastRange; i++)
	{
		printf("%d -> %d mV\n", i, inputRanges[i]);
	}

	/* Ask the user to select a range */
	printf("Specify voltage range (%d..%d)\n", unit->firstRange,
			unit->lastRange);
	printf("99 - switches channel off\n");
	for (ch = 0; ch < unit->ChannelCount; ch++)
	{
		printf("\n");
		do
		{
			printf("Channel %c: ", 'A' + ch);
			fflush(stdin);
			scanf("%d", &unit->channelSettings[ch].range);
		} while (unit->channelSettings[ch].range != 99
				&& (unit->channelSettings[ch].range < unit->firstRange
						|| unit->channelSettings[ch].range > unit->lastRange));

		if (unit->channelSettings[ch].range != 99)
		{
			printf(" - %d mV\n", inputRanges[unit->channelSettings[ch].range]);
			unit->channelSettings[ch].enabled = TRUE;
		}
		else
		{
			printf("Channel Switched off\n");
			unit->channelSettings[ch].enabled = FALSE;
		}
	}
}

/****************************************************************************
 *
 * Select timebase, set oversample to on and time units as nano seconds
 *
 ****************************************************************************/
void SetTimebase(UNIT_MODEL unit)
{
	int32_t timeInterval;
	int32_t maxSamples;

	printf("Specify timebase (not 0): ");
	do
	{
		fflush(stdin);
		scanf("%u", &timebase);
	} while (timebase == 0);

	ps5000GetTimebase(unit.handle, timebase, BUFFER_SIZE, &timeInterval, 1,
			&maxSamples, 0);
	printf("Timebase %d - %ld ns\n", timebase, timeInterval);
	oversample = TRUE;
}

/****************************************************************************
 * Sets the signal generator
 * - allows user to set frequency and waveform
 * - allows for custom waveform (values 0..4095) of up to 8192 samples int32_t
 ***************************************************************************/
void SetSignalGenerator(UNIT_MODEL unit)
{
	PICO_STATUS status;
	int16_t waveform;
	int32_t frequency;
	char fileName[128];
	FILE * fp;
	int16_t arbitraryWaveform[8192];
	int16_t waveformSize = 0;
	uint32_t pkpk = 1000000;
	int32_t offset = 0;
	int16_t whitenoise = 0;
	char ch;

	memset(&arbitraryWaveform, 0, 8192);

	while (_kbhit()) // use up keypress
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
		printf("A:\tAWG WAVEFORM\t");
		printf("X:\tSigGen Off\n\n");

		ch = _getch();

		if (ch >= '0' && ch <= '9')
			waveform = ch - '0';
		else
			ch = toupper(ch);
	} while (ch != 'A' && ch != 'X' && (ch < '0' || ch > '9'));

	if (ch == 'X') // If we're going to turn off siggen
	{
		printf("Signal generator Off\n");
		waveform = 8; // DC Voltage
		pkpk = 0; // 0V
		waveformSize = 0;
	}
	else if (ch == 'A') // Set the AWG
	{
		waveformSize = 0;

		printf("Select a waveform file to load: ");
		scanf_s("%s", fileName, 128);
		if (fopen_s(&fp, fileName, "r") == 0)
		{ // Having opened file, read in data - one number per line (at most 8192 lines), with values in (0..4095)
			while (EOF != fscanf_s(fp, "%hi",
					(arbitraryWaveform + waveformSize)) && waveformSize++
					< 8192 - 1)
				;
			fclose(fp);
			printf("File successfully loaded\n");
		}
		else
		{
			printf("Invalid filename\n");
			return;
		}
	}
	else // Set one of the built in waveforms
	{
		switch (waveform)
		{
		case 8:
			do
			{
				printf("\nEnter offset in uV: (0 to 2500000)\n"); // Ask user to enter DC offset level;
				scanf_s("%lu", &offset);
			} while (offset < 0 || offset > 10000000);
			break;

		case 9:
			whitenoise = 1;
			break;

		default:
			whitenoise = 0;
			offset = 0;
		}
	}

	if (waveform < 8 || ch == 'A') // Find out frequency if required
	{
		do
		{
			printf("\nEnter frequency in Hz: (1 to 20000000)\n"); // Ask user to enter signal frequency;
			scanf_s("%lu", &frequency);
		} while (frequency <= 0 || frequency > 20000000);
	}

	if (waveformSize > 0)
	{
		double delta = ((1.0 * frequency * waveformSize) / 8192.0)
				* 4294967296.0 * 8e-9; // delta >= 10

		status = ps5000SetSigGenArbitrary(unit.handle, 0, 1000000,
				(uint32_t) delta, (uint32_t) delta, 0, 0, arbitraryWaveform,
				waveformSize, (SWEEP_TYPE) 0, 0, SINGLE, 0, 0, SIGGEN_RISING,
				SIGGEN_NONE, 0);

		printf(
				status
						 ? "\nps5000SetSigGenArbitrary: Status Error 0x%x \n"
						 : "", (uint32_t) status); // If status != 0, show the error
	}
	else
	{
		status = ps5000SetSigGenBuiltIn(unit.handle, offset, pkpk, waveform,
				(float) frequency, (float) frequency, 0, 0, (SWEEP_TYPE) 0,
				whitenoise, 0, 0, 0, 0, 0);
		printf(status ? "\nps5000SetSigGenBuiltIn: Status Error 0x%x \n" : "",
				(uint32_t) status); // If status != 0, show the error
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
void StreamDataHandler(UNIT_MODEL * unit, uint32_t preTrigger)
{
	int32_t i, j;
	uint32_t sampleCount = 50000; /*  Make sure buffer large enough */
	
	FILE * fp;
	int16_t * buffers[PS5000_MAX_CHANNEL_BUFFERS];
	int16_t * appBuffers[PS5000_MAX_CHANNEL_BUFFERS];

	PICO_STATUS status;

	uint32_t sampleInterval = 1;
	uint32_t totalSamples = 0;
	uint32_t triggeredAt = 0;

	BUFFER_INFO bufferInfo;

	for (i = 0; i < unit->ChannelCount; i++) // create data buffers
	{
		if (unit->channelSettings[i].enabled)
		{
			buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

			status = ps5000SetDataBuffers(unit->handle, (PS5000_CHANNEL)i, buffers[i * 2],
				buffers[i * 2 + 1], sampleCount);

			appBuffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			appBuffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
		}
		else
		{
			status = ps5000SetDataBuffers(unit->handle, (PS5000_CHANNEL)i, NULL,
				NULL, sampleCount);
		}
	}

	bufferInfo.unit = unit;
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;

	printf("Waiting for trigger...Press a key to abort\n");
	
	g_autoStop = FALSE;

	status = ps5000RunStreaming(unit->handle, &sampleInterval, PS5000_US, preTrigger, 1000000 - preTrigger, TRUE, 1, sampleCount);
	

	if (status != PICO_OK)
	{
		printf("ps5000Streaming: %d\n", status);
		return;
	}
	
	printf("Streaming data...Press a key to abort\n");

	fp = fopen("streaming_data.txt", "w");

	while (!_kbhit() && !g_autoStop)
	{
		/* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
		Sleep(10);

		g_ready = FALSE;
		
		status = ps5000GetStreamingLatestValues(unit->handle, CallBackStreaming, &bufferInfo);

		if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
		{
			if(g_trig)
			{
				triggeredAt = totalSamples + g_trigAt;		// calculate where the trigger occurred in the total samples collected
			}

			totalSamples += g_sampleCount;

			printf("Collected %i samples, index = %i Total: %d samples", g_sampleCount, g_startIndex, totalSamples);

			if (g_trig)
			{
				printf("Trig. at index %lu total %lu", g_trigAt, triggeredAt + 1);	// show where trigger occurred
			}

			printf("\n");

			for (i = g_startIndex; i < (int32_t) (g_startIndex + g_sampleCount); i++)
			{
				for (j = 0; j < unit->ChannelCount; j++)
				{
					if (unit->channelSettings[j].enabled)
					{
						fprintf(fp, "%d, %d, %d, %d,", appBuffers[j * 2][i],
								adc_to_mv(appBuffers[j * 2][i],
										unit->channelSettings[PS5000_CHANNEL_A
												+ j].range),
								appBuffers[j * 2 + 1][i], adc_to_mv(appBuffers[j * 2
										+ 1][i],
										unit->channelSettings[PS5000_CHANNEL_A
												+ j].range));
					}
				}
				fprintf(fp, "\n");
			}
		}
	}

	ps5000Stop(unit->handle);
	fclose(fp);

	if (!g_autoStop)
	{
		printf("Data collection aborted\n");
		_getch();
	}

	for (i = 0; i < unit->ChannelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			free(buffers[i * 2]);
			free(appBuffers[i * 2]);

			free(buffers[i * 2 + 1]);
			free(appBuffers[i * 2 + 1]);
		}
		
	}
}

/****************************************************************************
 * CollectStreamingImmediate
 *  this function demonstrates how to collect a stream of data
 *  from the unit (start collecting immediately)
 ***************************************************************************/
void CollectStreamingImmediate(UNIT_MODEL * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));

	SetDefaults(unit);

	printf("Collect streaming...\n");
	printf("Data is written to disk file (streaming_data.txt)\n");
	printf("Press a key to start\n");
	_getch();

	/* Trigger disabled	*/
	SetTrigger(unit->handle, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0,
			0);

	StreamDataHandler(unit, 0);
}

/****************************************************************************
 * CollectStreamingTriggered
 *  this function demonstrates how to collect a stream of data
 *  from the unit (start collecting on trigger)
 ***************************************************************************/
void CollectStreamingTriggered(UNIT_MODEL * unit)
{
	int16_t triggerVoltage = mv_to_adc(100,
			unit->channelSettings[PS5000_CHANNEL_A].range); // ChannelInfo stores ADC counts
	
	struct tTriggerChannelProperties sourceDetails =
	{ triggerVoltage, triggerVoltage, 256 * 10, PS5000_CHANNEL_A, LEVEL };
	
	struct tTriggerConditions conditions =
	{ CONDITION_TRUE, CONDITION_DONT_CARE, CONDITION_DONT_CARE,
			CONDITION_DONT_CARE, CONDITION_DONT_CARE, CONDITION_DONT_CARE,
			CONDITION_DONT_CARE };
	
	struct tPwq pulseWidth;
	
	struct tTriggerDirections directions =
	{ RISING, NONE, NONE, NONE, NONE, NONE };
	
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect streaming triggered...\n");
	printf("Data is written to disk file (streaming_data.txt)\n");
	printf("Press a key to start\n");
	_getch();

	SetDefaults(unit);

	/* Trigger enabled
	 * Rising edge
	 * Threshold = 100mV */

	SetTrigger(unit->handle, &sourceDetails, 1, &conditions, 1, &directions,
			&pulseWidth, 0, 0, 0);

	StreamDataHandler(unit, 100000);
}

int32_t FlashLed(UNIT_MODEL * unit)
{
	PICO_STATUS status;
	printf("Flash led ......\n");
	printf("Press a key to start\n");
	_getch();

	status = ps5000FlashLed(unit->handle, 3);
	printf("Flashing led: status = %d \n", status);
	if (status != PICO_OK)
		printf("Failed to flash the led: status = %d \n", status);
	else
		printf("Succeed to flash the led: status = %d \n", status);

	Sleep(2000);

	return 1;
}
/****************************************************************************
 *
 *
 ****************************************************************************/
int32_t main(void)
{
	char ch;
	int32_t i;
	PICO_STATUS status;
	UNIT_MODEL unit;

	printf("PS5000 driver example program\n");
	printf("Version 1.0\n\n");

	printf("\n\nOpening the device...\n");

	//open unit
	status = ps5000OpenUnit(&(unit.handle));
	printf("Handle: %d\n", unit.handle);
	if (status != PICO_OK)
	{
		printf("Unable to open device\n");
		printf("Error code : %d\n", status);
		while (!_kbhit())
			;
		exit(99); // exit program - nothing after this executes
	}

	printf("Device opened successfully\n\n");

	// setup devices
	get_info(&unit);
	timebase = 1;

	for (i = 0; i < MAX_CHANNELS; i++)
	{
		unit.channelSettings[i].enabled = TRUE;
		unit.channelSettings[i].DCcoupled = TRUE;
		unit.channelSettings[i].range = PS5000_5V;
	}

	// main loop - read key and call routine
	ch = ' ';
	while (ch != 'X')
	{
		printf("\n");
		printf("B - Immediate block\t\tV - Set voltages\n");
		printf("T - Triggered block\t\tI - Set timebase\n");
		printf("R - Immediate rapid block\tQ - Logic triggering block\n");
		printf("E - ETS block\t\t\tF - toggle signal generator on/off\n");
		printf("S - Immediate streaming\t\tA - ADC counts/mV\n");
		printf("W - Triggered streaming\n");
		printf("L - Flash the led\t\tU - Get unit info \n");
		printf("                                X - exit\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");
		switch (ch)
		{
		case 'B':
			CollectBlockImmediate(&unit);
			break;

		case 'T':
			CollectBlockTriggered(&unit);
			break;

		case 'Q':
			CollectBlockLogicTriggered(&unit);
			break;

		case 'S':
			CollectStreamingImmediate(&unit);
			break;

		case 'W':
			CollectStreamingTriggered(&unit);
			break;

		case 'R':
			CollectRapidBlock(&unit);
			break;

		case 'F':
			SetSignalGenerator(unit);
			break;

		case 'E':
			CollectBlockEts(&unit);
			break;

		case 'V':
			set_voltages(&unit);
			break;

		case 'I':
			SetTimebase(unit);
			break;

		case 'L':
			FlashLed(&unit);
			break;

		case 'U':
			get_info(&unit);
			break;

		case 'A':
			scaleVoltages = !scaleVoltages;
			if (scaleVoltages)
			{
				printf("Readings will be scaled in mV\n");
			}
			else
			{
				printf("Readings will be scaled in ADC counts\n");
			}
			break;

		case 'X':
			/* Handled by outer loop */
			break;

		default:
			printf("Invalid operation\n");
			break;
		}
	}

	ps5000CloseUnit(unit.handle);
}
