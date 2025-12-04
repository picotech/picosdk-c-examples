/*******************************************************************************
 *
 * Filename: Libps6000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 6XXXE Series (ps6000a) devices.
 *
 * Copyright (C) 2023-2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>

#include "../../shared/PicoUnit.h"
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

int32_t cycles = 0;

int16_t			g_probeStateChanged = 0;
USER_PROBE_INFO userProbeInfo;

/****************************************************************************
* Gobal Variables
***************************************************************************/
BOOL		scaleVoltages = TRUE;
uint32_t	timebase = 0;
const uint64_t constBufferSize = 131072; //128kB
int16_t   		g_ready = FALSE;
/***************************************************************************/

/****************************************************************************
* Callback Probe Interaction
*
* See ps6000aProbeInteractions (callback)
*
****************************************************************************/
void PREF4 callBackProbeInteractions(int16_t handle,
	PICO_STATUS status, PICO_USER_PROBE_INTERACTIONS *probes, uint32_t	nProbes)
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
* callBackBlockReady
* used by ps6000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackBlockReady(
	int16_t				handle,
	PICO_STATUS		status,
	PICO_POINTER	pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
	}
}

/****************************************************************************
* callBackDataReady
* used by ps6000a for Async data collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackDataReady(
	int16_t    					handle,
	PICO_STATUS					status,
	uint64_t     				noOfSamples,
	int16_t    					overflow,
	PICO_POINTER				pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
	}
}

/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void setDefaults(GENERICUNIT* unit)
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
				unit->channelSettings[PICO_CHANNEL_A + i].bandwithLimit);
			printf(status ? "SetDefaults:ps6000aSetChannelOn------ 0x%08lx \n" : "", status);
		}
		else
		{
			status = ps6000aSetChannelOff(unit->handle, (PICO_CHANNEL)(PICO_CHANNEL_A + i));
			printf(status ? "SetDefaults:ps6000aSetChannelOff------ 0x%08lx \n" : "", status);
		}
	}


	for (i = 0; i < unit->digitalPortCount; i++) // reset channels to most recent settings
	{
		int16_t temp_threshold[8];
		for (int16_t b = 0; b < 8; b++)
		{
			temp_threshold[b] = (int16_t)(unit->digitalChannelSettings[i].threshold[b] * 32767 / 8);
			// fixed +/-8v range for digital ports
		}
		
		if (unit->digitalChannelSettings[i].enabled == TRUE)
		{
			status = ps6000aSetDigitalPortOn(unit->handle,
				(PICO_CHANNEL)(PICO_PORT0 + i),
				&temp_threshold[i],
				(sizeof(unit->digitalChannelSettings[i].threshold) / sizeof(unit->digitalChannelSettings[i].threshold[0])),
				PICO_NORMAL_100MV);

			printf(status ? "SetDefaults:ps6000aSetDigitalPortOn------ 0x%08lx \n" : "", status);
		}
		else
		{
			status = ps6000aSetDigitalPortOff(unit->handle, (PICO_CHANNEL)(PICO_PORT0 + i));
			printf(status ? "SetDefaults:ps6000aSetDigitalPortOff------ 0x%08lx \n" : "", status);
		}
	}
}

/****************************************************************************
* ClearDataBuffers
*
* stops GetData writing values to memory that has been released
****************************************************************************/
PICO_STATUS clearDataBuffers(GENERICUNIT* unit)
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
* SetTrigger
*
* - Used to call all the functions required to set up triggering
*
***************************************************************************/
PICO_STATUS SetTrigger(GENERICUNIT* unit,
	PICO_TRIGGER_CHANNEL_PROPERTIES* channelProperties,
	int16_t nChannelProperties,
	PICO_AUXIO_MODE auxOutputMode,
	PICO_CONDITION* triggerConditions,
	int16_t nTriggerConditions,
	PICO_DIRECTION* directions,
	int16_t nDirections,
	struct tPwq* pwq,
	uint32_t delay,
	int32_t autoTrigger_us)
{
	PICO_STATUS status;
	PICO_ACTION info = PICO_CLEAR_ALL;
	PICO_ACTION pwqInfo = PICO_CLEAR_ALL;

	if ((status = ps6000aSetTriggerChannelProperties(unit->handle,
		channelProperties,
		nChannelProperties,
		0,		//No longer used, set to 0, set by ps6000aSetAuxIoMode()
		autoTrigger_us)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetTriggerChannelProperties ------ Ox%08x \n", status);
		return status;
	}

	if (nTriggerConditions != 0)
	{
		info = (PICO_ACTION)(PICO_CLEAR_ALL | PICO_ADD);
		// Clear and add trigger condition specified unless no trigger conditions have been specified
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

	if ((status = ps6000aSetPulseWidthQualifierDirections(unit->handle,
		pwq->directions, pwq->nDirections)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetPulseWidthQualifierDirections ------ 0x%08x \n", status);
		return status;
	}

	// Clear and add pulse width qualifier condition, clear if no pulse width qualifier has been specified
	if (pwq->nConditions != 0)
	{
		pwqInfo = (PICO_ACTION)(PICO_CLEAR_ALL | PICO_ADD);
	}

	if ((status = ps6000aSetPulseWidthQualifierConditions(unit->handle, pwq->conditions, pwq->nConditions, pwqInfo)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetPulseWidthQualifierConditions ------ 0x%08x \n", status);
		return status;
	}
	if ((status = ps6000aSetAuxIoMode(unit->handle,
		auxOutputMode)) != PICO_OK)
	{
		printf("SetTrigger:ps6000aSetAuxIoMode ------ Ox%08x \n", status);
		return status;
	}
	return status;
}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void set_info(GENERICUNIT* unit)
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
	unit->sigGenfeature = SIGGEN_AWG;
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
void setVoltages(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;
	PICO_DEVICE_RESOLUTION resolution = PICO_DR_8BIT;

	int32_t i, ch;
	int32_t count = 0;
	int16_t numValidChannels = unit->channelCount; //
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
* Set digital ports (PORT1, PORT1) and voltage threshold
****************************************************************************/
void setDigitalPorts(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;
	PICO_DEVICE_RESOLUTION resolution = PICO_DR_8BIT;

	int32_t ch, i = 0;
	int32_t count = 0;
	int16_t numValidChannels = unit->digitalPortCount;
	int16_t retry = FALSE;

	do
	{
		//do
		//{
			// Ask the user to select a range
		printf("Please connect MSO pods before setting port and pins!\n");
		printf("Specify voltage pin threshold -8V to +8V\n");
		printf("99 - switches pin off\n");

		for (ch = 0; ch < numValidChannels; ch++)
		{
			count = 0; 
			printf("Digital Port%d: ", ch);
			for (i = 0; i < 8; i++)
			{
				printf("\n");
				do
				{
					printf("Digital Pin%d: ", i);
					fflush(stdin);
					scanf_s("%lf", &unit->digitalChannelSettings[ch].threshold[i]);
					// Set the threshold for the digital channel

				} while (((unit->digitalChannelSettings[ch].threshold[i] > 99.1f) || (unit->digitalChannelSettings[ch].threshold[i] < 98.9f)) &&
					((unit->digitalChannelSettings[ch].threshold[i] > 8.0f) ||
						(unit->digitalChannelSettings[ch].threshold[i] < -8.0f))
					);

				if ((unit->digitalChannelSettings[ch].threshold[i] > 99.1f) || (unit->digitalChannelSettings[ch].threshold[i] < 98.9f))
				{
					printf("Port threshold: %+3.3e V\n", unit->digitalChannelSettings[ch].threshold[i]);
					unit->digitalChannelSettings[ch].enabled = TRUE;
					count++;
				}
				else
				{
					printf("Entered '99' - Setting Pin threshold to 0V\n");
					unit->digitalChannelSettings[ch].threshold[i] = 0.0f;	// Set threshold to 0V
				}
			}
			if (count == 0)
			{
				printf("No pins used on Port%d - Channel Switched off\n", ch);
				unit->digitalChannelSettings[ch].enabled = FALSE;	// Set digital channel off
				unit->digitalChannelSettings[ch].threshold[i] = 0.0f;	// Set threshold to 0V
			}
		}
		//printf(count == 0 ? "\n** At least 1 channel must be enabled **\n\n" : "");
	//} while (count == 0);	// must have at least one channel enabled

		status = ps6000aGetDeviceResolution(unit->handle, &resolution);

		printf("\n");
	} while (retry == TRUE);

	setDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
* setTimebase
* Select timebase, set time units asi seconds
*
****************************************************************************/
void setTimebase(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;
	PICO_STATUS powerStatus = PICO_OK;
	double timeInterval;//int32_t
	//uint64_t maxSamples; //int32_t
	int32_t ch;

	uint32_t shortestTimebase;
	double timeIntervalSeconds;

	PICO_CHANNEL_FLAGS enabledChannelOrPortFlags = (PICO_CHANNEL_FLAGS)0;

	int16_t numValidChannels = unit->channelCount;

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

	printf("Shortest timebase index available %d = %le seconds.\n", shortestTimebase, timeIntervalSeconds);

	printf("Specify desired timeInterval (in the format Ne-XX, example 1us -> 1e-06): ");
	fflush(stdin);
	double timeIntervalRequested = 0;
	scanf_s("%le", &timeIntervalRequested);

	status = ps6000aNearestSampleIntervalStateless(unit->handle,
		enabledChannelOrPortFlags,	//enabledChannelFlags,
		timeIntervalRequested,		//timeIntervalRequested,
		unit->resolution,			//resolution,
		&timebase,					//*timebase,
		&timeInterval				//*timeIntervalAvailable
		);

		if (status != PICO_OK)//(status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION)
		{
			printf("NearestSampleIntervalStateless: Error - Invalid number of channels for resolution.\n");
			return;
		}
		else
		{
			// Do nothing
		}

	printf("Timebase used %lu = %le seconds sample interval\n", timebase, timeInterval);
	unit->timeInterval = timeInterval;
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
void setResolution(GENERICUNIT* unit)
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

	retry = TRUE;
	do
	{
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
* displaySettings
* Displays information about the user configurable settings in this example
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void displaySettings(GENERICUNIT* unit)
{
	int32_t ch;
	int32_t voltage;
	PICO_STATUS status = PICO_OK;
	PICO_DEVICE_RESOLUTION resolution = PICO_DR_8BIT;

	printf("\nTrigger values will be scaled in %s\n", (scaleVoltages) ? ("Millivolts(mV)") : ("ADC counts"));

	for (ch = 0; ch < unit->channelCount; ch++)
	{
		if (!(unit->channelSettings[ch].enabled))
		{
			printf("Channel %c Range: Off\n", 'A' + ch);
		}
		else
		{
			voltage = inputRanges[unit->channelSettings[ch].range];
			printf("Channel %c Range: ", 'A' + ch);

			if (voltage < 1000)
			{
				printf("%dmV, ", voltage);
			}
			else
			{
				printf("%dV, ", voltage / 1000);
			}
			if(unit->channelSettings[ch].DCcoupled == PICO_DC)
				printf("Coupling: DC, ");
			if (unit->channelSettings[ch].DCcoupled == PICO_AC)
				printf("Coupling: AC, ");
			if (unit->channelSettings[ch].DCcoupled == PICO_DC_50OHM)
				printf("Coupling: 50Ohm, ");
			if ( unit->channelSettings[ch].bandwithLimit == PICO_BW_FULL)
				printf("bandwithLimit: FULL, ");
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_20MHZ)	//Not 6428E-D
				printf("bandwithLimit: 20MHz, ");
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_200MHZ)	//64x5E and 64x6E only
				printf("bandwithLimit: 200MHz, ");
			printf("analogueOffset: %g\n", unit->channelSettings[ch].analogueOffset);
		}
	}
		printf("\nDigital Ports:\n");
		for (ch = 0; ch < unit->digitalPortCount; ch++)
		{
			if (!(unit->digitalChannelSettings[ch].enabled))
			{
				printf("Digital Port %d: Off\n", ch);
			}
			else
			{
				printf("Digital Port %d:\n", ch);
				for(int16_t i= 0; i < 8; i++)
					printf("\tPin%d: Threshold: %.3fV\n",i, unit->digitalChannelSettings[ch].threshold[i]);
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
PICO_STATUS openDevice(GENERICUNIT* unit, int8_t* serial)
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
PICO_STATUS handleDevice(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
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

	double temp_timeIntervalns;
	do
	{
		status = ps6000aGetTimebase(unit->handle, timebase, constBufferSize, &temp_timeIntervalns, NULL, 0);

		if (status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION)
		{
			printf("SetTimebase: Error - Invalid number of channels for resolution.\n");
			return status;
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

	unit->timeInterval = temp_timeIntervalns * 1e-9;

	status = ps6000aGetAdcLimits(unit->handle, PICO_DR_8BIT, NULL, &value);
	unit->maxADCValue = value;

	int16_t enabled_chs_limit = unit->channelCount;
	if (unit->channelCount > ENABLED_CHS_LIMIT)
	{
		enabled_chs_limit = ENABLED_CHS_LIMIT;
		//printf("Limiting enabled channels to %d! (Starting at ChA)\n", enabled_chs_limit);
	}
	//if(TURN_ON_EVERY_N_CH != 1)
	//	printf("Turning on every %d Channels\n", TURN_ON_EVERY_N_CH);
	// Turn off any digital ports (MSO models only)
	if (unit->digitalPortCount > 0)
	{
		printf("Turning off digital ports.\n");

		for (i = 0; i < unit->digitalPortCount; i++)
		{
			status = ps6000aSetDigitalPortOff(unit->handle, (PICO_CHANNEL)(i + PICO_PORT0));
		}
	}
	for (i = 0; i < unit->channelCount; i++)
	{
		//define "TURN_ON_EVERY_N_CH" to either 2 or 4 (2 = Every odd Ch is enabled, 4 = Every 4th Ch enabled), set 1 to disable.
		if ( i % TURN_ON_EVERY_N_CH == 0 && i < enabled_chs_limit)
			unit->channelSettings[i].enabled = TRUE;
		else
			unit->channelSettings[i].enabled = FALSE;

		unit->channelSettings[i].DCcoupled = PICO_DC;	// PICO_AC, PICO_DC, PICO_DC_50OHM
		unit->channelSettings[i].range = PICO_X1_PROBE_1V;
		unit->channelSettings[i].analogueOffset = 0.0f;
		unit->channelSettings[i].bandwithLimit = PICO_BW_FULL; // PICO_BW_FULL, PICO_BW_20MHZ, PICO_BW_200MHZ
	}
	for (i = 0; i < unit->digitalPortCount; i++) // reset channels to most recent settings
	{
		unit->digitalChannelSettings[i].enabled = FALSE;		//turn off digital channels
		unit->digitalChannelSettings[i].threshold[0] = 0.0f;	// Set threshold to 0V
	}

	unit->CapturesComplete = 0; // used by GetMoreDataHandler()

	if (sigGenSettings != NULL)
	{
		//Set default Signal Generator settings /AWG settings
		///////////
		sigGenSettings->Enabled = 0;
		//
		sigGenSettings->PeakVolts = 2.0f;
		sigGenSettings->Offset = 0.0f;
		sigGenSettings->Frequency = 1000.0f; // 1.0e3;
		// Sweep settings
		sigGenSettings->FrequencyStop = 2000.0f;
		sigGenSettings->FrequencyIncrement = 100.0f;   //double* frequencyIncrement(Hz),
		sigGenSettings->DwellTime = 0.1f;              //double* dwellTime (s)
		sigGenSettings->SweepType = PICO_UP;
		// Waveform settings
		sigGenSettings->AWGBufferSize = 0;
		//sigGenSettings->AWGBuffer = (int16_t*)calloc(maxAwgBufferLeght, sizeof(int16_t));
		sigGenSettings->AWGBuffer = NULL;
		// Trigger settings
		sigGenSettings->triggerSource = PICO_SIGGEN_NONE;
		sigGenSettings->triggerType = PICO_SIGGEN_RISING;
		sigGenSettings->cycles = 1; // Number of cycles to output
		sigGenSettings->autoTrigPicoSecs = 0; // Auto trigger in pico seconds (0 = no auto trigger)
	}
	setDefaults(unit);

	/* Trigger disabled	*/
	status = ps6000aSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

	return unit->openStatus;
}

/****************************************************************************
* closeDevice
****************************************************************************/
void closeDevice(GENERICUNIT* unit)
{
	ps6000aCloseUnit(unit->handle);
}

/****************************************************************************
* GetMoreDataHandler
* - Used by all data routines
* - acquires data, displays 10 items
*   and saves all data to a file.
* Input :
* - unit : the unit to use.
* - noOfPreTriggerSamples : number of samples to capture before trigger.
* - autostop : 1 to stop when trigger condition is met, 0 to continue until user stops.
****************************************************************************/
void GetMoreDataHandler(GENERICUNIT* unit,
						PICO_RATIO_MODE ratioMode,
						uint64_t downSampleRatio,
						uint64_t nSamples) // Set the number of raw samples
{
	int32_t index = 0;
	int16_t channel = 0;
	PICO_STATUS status = PICO_OK;
	//Set the number buffers from previous Rapid block capture.
	if (unit->CapturesComplete == 0)
	{
		printf("No Captures done - Exiting MoreDataHandler()\n");
		return;
	}
	uint64_t nCaptures = unit->CapturesComplete;
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);	// bitwise OR flags for first buffer that is set

	//Define acquisition Settings

	//Buffers settings
	//Use scope acquisition settings for data download
	struct tbuffer_settings bufferSettings = { 0 };
	bufferSettings.startIndex = 0;
	bufferSettings.downSampleRatioMode = ratioMode;
	bufferSettings.downSampleRatio = downSampleRatio;
	bufferSettings.nSamples = nSamples;

	//Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
	struct tmultiBufferSizes multiBufferSizes;// to store buffer sizes
	int16_t*** minBuffersStopped;
	int16_t*** maxBuffersStopped;
	pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffersStopped, &maxBuffersStopped, &multiBufferSizes);

	printf("\nRequesting More Data.");
	//printf("\nNumber of PreTriggerSamples: %lld", noOfPreTriggerSamples);

	//Save and print Sample Internal set (in seconds)
	//unit->timeInterval = (idealTimeInterval * (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15));
	printf("\nsample Internal: %g seconds\n", unit->timeInterval);
	//print number of Samples
	printf("%llu Samples\n", nSamples);
	uint64_t printTriggerSample = 0;

	// SetDataBuffers with API
	if (nCaptures == 1) // only 1 segment, for block and streaming download
		SetAllDataBuffers(unit, &bufferSettings, &minBuffersStopped, &maxBuffersStopped, &multiBufferSizes, 0, (CAPTURE_MODE)BLOCK, 0);
	else // > 1 segment, Rapid download only
		SetAllDataBuffers(unit, &bufferSettings, &minBuffersStopped, &maxBuffersStopped, &multiBufferSizes, 0, (CAPTURE_MODE)RAPID_BLOCK, 0);
	
	printf("\nPress any key to abort.");
	printf("\nWaiting for Data ");

	g_ready = FALSE; // reset flag

	if (unit->CapturesComplete == 1)
	{
		status = ps6000aGetValuesAsync(unit->handle,
			0,				// startIndex
			bufferSettings.nSamples,
			downSampleRatio,
			ratioMode,
			0,				// segmentIndex
			callBackDataReady,	// pointer to Data callback
			NULL);			// pParameter

		if (status != PICO_OK)
		{
			printf(status ? "blockDataHandler:ps6000aGetValuesAsync ------ 0x%08lx \n" : "", status);
			return;
		}
	}
	else // > 1 segment
	{
		status = ps6000aGetValuesBulkAsync(unit->handle,
			0,				// startIndex
			bufferSettings.nSamples,// noOfSamples
			0,				// From Segment
			nCaptures - 1,	// To Segment
			downSampleRatio,
			ratioMode,
			callBackDataReady,	// pointer to Data callback
			NULL);			// pParameter

		if (status != PICO_OK)
		{
			printf(status ? "blockDataHandler:ps6000aGetValuesBulkAsync ------ 0x%08lx \n" : "", status);
			return;
		}
	}
	//wait for capture to complete or for user to abort
	while (!g_ready && !_kbhit())
	{
		Sleep(500);
		printf(". ");
	}

	printf("\nFinished Data download");
	//Write one segment to a file as captured
	printf("\nWriting Buffer Set of channels to a file.\n");

	//Create file name string
	char startOfFileName2[] = "MoreDataStopped";
	char buf[58 + (3 * sizeof(int))];
	size_t buf_size = sizeof(buf) / sizeof(buf[0]);
	//snprintf(buf, buf_size, "%s%d.txt", startOfFileName, (int)capture);
	snprintf(buf, buf_size, "%s", startOfFileName2);

	//Get scaling Info for each channel
	struct tPicoProbeScaling enabledChannelsScaling[PS6000A_MAX_CHANNELS] = { 0 };
	struct tPicoProbeScaling channelRangeInfoTemp;
	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			getRangeScaling(unit->channelSettings[PICO_CHANNEL_A + 0].range, &channelRangeInfoTemp);
			enabledChannelsScaling[channel] = channelRangeInfoTemp;
		}
	}

	WriteArrayToFilesGeneric(
		unit,
		minBuffersStopped,
		maxBuffersStopped,
		multiBufferSizes,
		enabledChannelsScaling,
		buf,
		0,		// streamingDataTriggerInfoTemp.triggerAt_, // Triggersample
		NULL,	// No overflow flags
		NULL);	// Set default full range if NULL

	// Release Buffer memory from API
	clearDataBuffers(unit);

	// Free buffers
	pico_release_multibuffers(unit, &minBuffersStopped, &maxBuffersStopped, &multiBufferSizes);
}

/****************************************************************************
* SetupTrigger
* This function sets up an advanced trigger on Channel A, rising, at +50% of the channel range.
* Inputs :
* - unit : the unit to use.
* Returns       none
****************************************************************************/
void SetupTrigger(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;

	//Set triggerLevelADC to +50% of set channel voltage range
	int16_t triggerLevelADC = mv_to_adc((double)inputRanges[unit->channelSettings[PICO_CHANNEL_A].range] / 2,
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
		PICO_AUXIO_INPUT,	//auxIoMode
		&conditions, 1,		//conditions		//nConditions
		&directions, 1,		//directions		//nDirections
		&pulseWidth,		//PWQ
		0, 0);				//TrigDelay //AutoTrigger_us
}

/****************************************************************************
* SetAllDataBuffers
* This function Setups all the data buffers for enabled channels and segments.
* Inputs :
* - unit : the unit to use.
* - bufferSettings : structure containing buffer settings
* - minBuffers : 3D array of pointers to int16_t buffers for minimum ADC values
* - maxBuffers : 3D array of pointers to int16_t buffers for maximum ADC values
* - multiBufferSizes : structure containing buffer sizes
* - StreamBufToSet : in streaming mode, the buffer number to set (0 to (multiBufferSizes->numberOfBuffers -1) )
* - CaptureMode : enum to indicate if BLOCK, RAPID_BLOCK or STREAMING mode
* - Reset_action : if 0, first buffer set uses CLEAR_ALL | ADD, if not 0, first buffer set uses ADD only (used in streaming mode)
* Returns       none
****************************************************************************/
void SetAllDataBuffers(GENERICUNIT* unit,
	struct tbuffer_settings* bufferSettings, 
	int16_t**** minBuffers, 
	int16_t**** maxBuffers, 
	struct tmultiBufferSizes* multiBufferSizes,
	uint64_t StreamBufToSet,
	enum enCaptureMode CaptureMode,
	int16_t Reset_action)
{
	uint64_t waveform;
	uint64_t nCaptures;
	int16_t channel;
	uint64_t capture;
	PICO_STATUS status = PICO_OK;
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);//bitwise OR flags for first buffer that is set

 	if (CaptureMode != (enum enCaptureMode)STREAMING )
	{
		nCaptures = multiBufferSizes->numberOfBuffers;
	}
	else // Streaming mode - only set one buffer at a time
	{
		waveform = 0;
		// force "for loop" to only use one buffer set
		nCaptures = StreamBufToSet + 1;
		if (Reset_action != 0) // Set action flag to ADD on subsequent calls into this function
			action_flag = PICO_ADD; // all subsequent calls use ADD!
	}

	//printf("\nCalling SetDataBuffers() for Channel(s) - ");
	// SetDataBuffers with API
	for (channel = 0; channel < unit->channelCount; channel++)
	{
		if (unit->channelSettings[channel].enabled)
		{
			if (CaptureMode != (enum enCaptureMode)STREAMING)
			{
				capture = 0;
			}
			else // Streaming mode - only set one buffer at a time
			{
				capture = StreamBufToSet; // force "for loop" to only use one buffer set
			}

			for (capture; capture < nCaptures; capture++)
			{
				if (CaptureMode != (enum enCaptureMode)STREAMING)
					waveform = capture;
				status = ps6000aSetDataBuffers(unit->handle,
					(PICO_CHANNEL)channel,
					(*maxBuffers)[capture][channel],
					(*minBuffers)[capture][channel],
					multiBufferSizes->maxBufferSize,
					PICO_INT16_T, //PICO_DATA_TYPE
					waveform,
					bufferSettings->downSampleRatioMode,
					action_flag);
				action_flag = PICO_ADD;//all subsequent calls use ADD!
				if (status != PICO_OK)
				{
					printf("SetAllDataBuffers:ps6000aSetDataBuffers ------ 0x%08x, for channel %c \n", status, PICO_CHANNEL_A + channel);
					return;
				}
				//if (capture == 0)
				//	printf("%c,", 'A' + channel);
			}
		}
	}
	//digital channels
	for (channel = 0; channel < unit->digitalPortCount; channel++)
	{
		if (unit->digitalChannelSettings[channel].enabled)
		{
			if (CaptureMode != (enum enCaptureMode)STREAMING)
			{
				capture = 0;
			}
			else // Streaming mode - only set one buffer at a time
			{
				capture = StreamBufToSet; // force "for loop" to only use one buffer set
			}

			for (capture; capture < nCaptures; capture++)
			{ 
				if (CaptureMode != (enum enCaptureMode)STREAMING)
					waveform = capture; 
				status = ps6000aSetDataBuffers(unit->handle,
					PICO_PORT0 + (PICO_CHANNEL)channel,
					(*maxBuffers)[capture][channel + unit->channelCount],
					(*minBuffers)[capture][channel + unit->channelCount],
					multiBufferSizes->maxBufferSize,
					PICO_INT16_T,
					waveform,			//waveform number
					bufferSettings->downSampleRatioMode,
					action_flag);
				action_flag = PICO_ADD;//all subsequent calls use ADD!
				if (status != PICO_OK)
				{
					printf(status ? "SetAllDataBuffers:psospaSetDataBuffers(PORT %d) ------ 0x%08lx \n" : "", PICO_PORT0 + channel, status);
					return;
				}
				//if (capture == 0)
				//	printf("PORT%d,", channel);
			}
		}
	}
	//printf("\n");
}