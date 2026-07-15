/*******************************************************************************
 *
 * Filename: Libpsospa.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope Series for (psospa) devices.
 *
 * Copyright (C) 2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

#include "../../shared/PicoUnit.h"
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
 * Model Lookup Table and function to return model enum from string
 ***************************************************************************/
typedef struct { char* key; MODEL_TYPE val; } t_symstruct;
t_symstruct lookuptable[] = {
	{ "3415E",		MODEL_3415E }, // 3415E, 3415E MSO
	{ "3415EMSO",	MODEL_3415E_MSO },
	{ "3416E",		MODEL_3416E }, // 3416E, 3416E MSO
	{ "3416EMSO",	MODEL_3416E_MSO },
	{ "3417E",		MODEL_3417E }, // 3417E, 3417E MSO
	{ "3417EMSO",	MODEL_3417E_MSO },
	{ "3418E",		MODEL_3418E }, // 3418E, 3418E MSO
	{ "3418EMSO",	MODEL_3418E_MSO },
	{ "5462E",		MODEL_5462E }, // 5462E, 5462E MSO
	{ "5462EMSO",	MODEL_5462E_MSO },
	{ "5463E",		MODEL_5463E }, // 5463E, 5463E MSO
	{ "5463EMSO",	MODEL_5463E_MSO },
	{ "5464E",		MODEL_5464E }, // 5464E, 5464E MSO
	{ "5464EMSO",	MODEL_5464E_MSO },
	{ "5462+",		MODEL_5462Ep }, // 5462Ep, 5462Ep MSO
	{ "5462E+MSO",	MODEL_5462Ep_MSO },
	{ "5463E+",		MODEL_5463Ep }, // 5463Ep, 5463Ep MSO
	{ "5463E+MSO",	MODEL_5463Ep_MSO },
	{ "5464E+",		MODEL_5464Ep }, // 5464Ep, 5464Ep MSO
	{ "5464E+MSO",	MODEL_5464Ep_MSO}
};

#define NKEYS (sizeof(lookuptable)/sizeof(t_symstruct))
MODEL_TYPE keyfromstring(char* key)
{
	int i;
	for (i = 0; i < NKEYS; i++) {
		if (strcmp(lookuptable[i].key, key) == 0)
			return lookuptable[i].val;
	}
	return BADKEY;
}

/****************************************************************************
* Callback Probe Interaction
*
* See psospaProbeInteractions (callback)
*
****************************************************************************/
void PREF4 callBackProbeInteractions(int16_t handle,
	PICO_STATUS status, PICO_USER_PROBE_INTERACTIONS *probes, uint32_t	nProbes)
{
	uint32_t i = 0;

	userProbeInfo.status = status;
	userProbeInfo.numberOfProbes = nProbes;

    // Store probe interactions in global struct for user access
    // Copy probe interactions to global struct for user access, using channel as index
    for (i = 0; i < nProbes; ++i)
    {
        userProbeInfo.userProbeInteractions[probes[i].channel_] = probes[i];
    }
    g_probeStateChanged = 1;

}

/****************************************************************************
* callBackBlockReady
* used by psospa data block collection calls, on receipt of data.
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
* used by psospa for Async data collection calls, on receipt of data.
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
			status = psospaSetChannelOn(unit->handle, (PICO_CHANNEL)(PICO_CHANNEL_A + i),
				(PICO_COUPLING)unit->channelSettings[PICO_CHANNEL_A + i].DCcoupled,
				unit->channelSettings[PICO_CHANNEL_A + i].rangeMin,
				unit->channelSettings[PICO_CHANNEL_A + i].rangeMax,
				unit->channelSettings[PICO_CHANNEL_A + i].rangeType,
				unit->channelSettings[PICO_CHANNEL_A + i].analogueOffset,
				unit->channelSettings[PICO_CHANNEL_A + i].bandwithLimit);
			printf(status ? "SetDefaults:psospaSetChannelOn------ 0x%08x \n" : "", status);
		}
		else
		{
			status = psospaSetChannelOff(unit->handle, (PICO_CHANNEL)(PICO_CHANNEL_A + i));
			printf(status ? "SetDefaults:psospaSetChannelOff------ 0x%08x \n" : "", status);
		}
	}
	for (i = 0; i < unit->digitalPortCount; i++) // reset channels to most recent settings
	{
		if (unit->digitalChannelSettings[i].enabled == TRUE)
		{
			status = psospaSetDigitalPortOn(unit->handle,
												(PICO_CHANNEL)(PICO_PORT0 + i),
												unit->digitalChannelSettings[i].threshold[0]);
			
			printf(status ? "SetDefaults:psospaSetDigitalPortOn------ 0x%08x \n" : "", status);
		}
		else
		{
			status = psospaSetDigitalPortOff(unit->handle, (PICO_CHANNEL)(PICO_PORT0 + i));
			printf(status ? "SetDefaults:psospaSetDigitalPortOff------ 0x%08x \n" : "", status);
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

	if ((status = psospaSetDataBuffers(unit->handle, PICO_CHANNEL_A, NULL, NULL, 0, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, action_flag)) != PICO_OK)
	{
		printf("ClearDataBuffers:psospaSetDataBuffers ------ 0x%08x \n", status);
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

	if ((status = psospaSetTriggerChannelProperties(unit->handle,
		channelProperties,
		nChannelProperties,
		autoTrigger_us)) != PICO_OK)
	{
		printf("SetTrigger:psospaSetTriggerChannelProperties ------ Ox%08x \n", status);
		return status;
	}

	if (nTriggerConditions != 0)
	{
		info = (PICO_ACTION)(PICO_CLEAR_ALL | PICO_ADD);
		// Clear and add trigger condition specified unless no trigger conditions have been specified
	}

	if ((status = psospaSetTriggerChannelConditions(unit->handle, triggerConditions, nTriggerConditions, info) != PICO_OK))
	{
		printf("SetTrigger:psospaSetTriggerChannelConditions ------ 0x%08x \n", status);
		return status;
	}

	if ((status = psospaSetTriggerChannelDirections(unit->handle, directions, nDirections)) != PICO_OK)
	{
		printf("SetTrigger:psospaSetTriggerChannelDirections ------ 0x%08x \n", status);
		return status;
	}

	if ((status = psospaSetTriggerDelay(unit->handle, delay)) != PICO_OK)
	{
		printf("SetTrigger:psospaSetTriggerDelay ------ 0x%08x \n", status);
		return status;
	}

	if ((status = psospaSetPulseWidthQualifierProperties(unit->handle,
		pwq->lower, pwq->upper, pwq->type)) != PICO_OK)
	{
		printf("SetTrigger:psospaSetPulseWidthQualifierProperties ------ 0x%08x \n", status);
		return status;
	}

	if ((status = psospaSetPulseWidthQualifierDirections(unit->handle,
		pwq->directions, pwq->nDirections)) != PICO_OK)
	{
		printf("SetTrigger:psospaSetPulseWidthQualifierDirections ------ 0x%08x \n", status);
		return status;
	}

	// Clear and add pulse width qualifier condition, clear if no pulse width qualifier has been specified
	if (pwq->nConditions != 0)
	{
		pwqInfo = (PICO_ACTION)(PICO_CLEAR_ALL | PICO_ADD);
	}

	if ((status = psospaSetPulseWidthQualifierConditions(unit->handle, pwq->conditions, pwq->nConditions, pwqInfo)) != PICO_OK)
	{
		printf("SetTrigger:psospaSetPulseWidthQualifierConditions ------ 0x%08x \n", status);
		return status;
	}

	if ((status = psospaSetAuxIoMode(unit->handle,
		auxOutputMode)) != PICO_OK)
	{
		printf("SetTrigger:psospaSetAuxIoMode ------ Ox%08x \n", status);
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
			status = psospaGetUnitInfo(unit->handle, line, sizeof(line), &requiredSize, i);

			// info = 3 - PICO_VARIANT_INFO
			if (i == PICO_VARIANT_INFO)
			{
				memcpy(unit->modelString, line, sizeof(unit->modelString));
				unit->modelString[sizeof(unit->modelString) - 1] = '\0'; // Ensure null termination

				// Extract channel count from the variant info string
				unit->channelCount = (int16_t)line[1];
				unit->channelCount = unit->channelCount - 48; // Subtract ASCII 0 (48)

				// Determine if the device is an MSO
				if (strstr(line, "MSO") != NULL)
				{
					unit->digitalPortCount = 2;
				}
				else
				{
					unit->digitalPortCount = 0;
				}
			}
			else if (i == PICO_BATCH_AND_SERIAL)	// info = 4 - PICO_BATCH_AND_SERIAL
			{
				memcpy(&(unit->serial), line, requiredSize);
			}

			printf("%s: %s\n", description[i], line);
		}
		printf("\n");

		switch (keyfromstring(unit->modelString))
		{
		case MODEL_3415E: /* 4Ch,  Scope */
			printf("Model is 3415E\n");
			unit->model = MODEL_3415E;
			break;
		case MODEL_3415E_MSO: /* 4Ch,  MSO */
			printf("Model is 3415E_MSO\n");
			unit->model = MODEL_3415E_MSO;
			break;
		case MODEL_3416E: /* 4Ch,  Scope */
			printf("Model is 3416E\n");
			unit->model = MODEL_3416E;
			break;
		case MODEL_3416E_MSO: /* 4Ch,  MSO */
			printf("Model is 3416E_MSO\n");
			unit->model = MODEL_3416E_MSO;
			break;
		case MODEL_3417E: /* 4Ch,  Scope */
			printf("Model is 3417E\n");
			unit->model = MODEL_3417E;
			break;
		case MODEL_3417E_MSO: /* 4Ch,  MSO */
			printf("Model is 3417E_MSO\n");
			unit->model = MODEL_3417E_MSO;
			break;
		case MODEL_3418E: /* 4Ch,  Scope */
			printf("Model is 3418E\n");
			unit->model = MODEL_3418E;
			break;
		case MODEL_3418E_MSO: /* 4Ch,  MSO */
			printf("Model is 3418E_MSO\n");
			unit->model = MODEL_3418E_MSO;
			break;
		case MODEL_5462E: /* 4Ch,  Scope */
			printf("Model is 5462E\n");
			unit->model = MODEL_5462E;
			break;
		case MODEL_5462E_MSO: /* 4Ch,  MSO */
			printf("Model is 5462E_MSO\n");
			unit->model = MODEL_5462E_MSO;
			break;
		case MODEL_5463E: /* 4Ch,  Scope */
			printf("Model is 5463E\n");
			unit->model = MODEL_5463E;
			break;
		case MODEL_5463E_MSO: /* 4Ch,  MSO */
			printf("Model is 5463E_MSO\n");
			unit->model = MODEL_5463E_MSO;
			break;
		case MODEL_5464E: /* 4Ch,  Scope */
			printf("Model is 5464E\n");
			unit->model = MODEL_5464E;
			break;
		case MODEL_5464E_MSO: /* 4Ch,  MSO */
			printf("Model is 5464E_MSO\n");
			unit->model = MODEL_5464E_MSO;
			break;
		case MODEL_5462Ep: /* 4Ch+,  Scope */
			printf("Model is 5462E+\n");
			unit->model = MODEL_5462Ep;
			break;
		case MODEL_5462Ep_MSO: /* 4Ch+,  MSO */
			printf("Model is 5462E+ MSO\n");
			unit->model = MODEL_5462Ep_MSO;
			break;
		case MODEL_5463Ep: /* 4Ch+,  Scope */
			printf("Model is 5463E+\n");
			unit->model = MODEL_5463Ep;
			break;
		case MODEL_5463Ep_MSO: /* 4Ch+,  MSO */
			printf("Model is 5463E+ MSO\n");
			unit->model = MODEL_5463Ep_MSO;
			break;
		case MODEL_5464Ep: /* 4Ch+,  Scope */
			printf("Model is 5464E+\n");
			unit->model = MODEL_5464Ep;
			break;
		case MODEL_5464Ep_MSO: /* 4Ch+,  MSO */
			printf("Model is 5464E+ MSO\n");
			unit->model = MODEL_5464Ep_MSO;
			break;

		case BADKEY: /* failed lookup */
			printf("Model not found or not referenced!, using defaults\n");
			break;
		}

		// Set sig gen parameters
		// If device has Arbitrary Waveform Generator, find the maximum AWG buffer size
		/*
		status = psospaSigGenArbitraryMinMaxValues(unit->handle, &minArbitraryWaveformValue, &maxArbitraryWaveformValue, &minArbitraryWaveformSize, &maxArbitraryWaveformSize);
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

	// See what ranges are available... 
	for (i = unit->firstRange; i <= unit->lastRange; i++)
	{
		printf("%d -> %d mV\n", i, inputRanges[i]);
	}

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
				scanf_s("%d", &(unit->channelSettings[ch].range));

			} while (unit->channelSettings[ch].range != 99 && (unit->channelSettings[ch].range < unit->firstRange || unit->channelSettings[ch].range > unit->lastRange));

			if (unit->channelSettings[ch].range != 99)
			{
				printf(" - %d mV\n", inputRanges[unit->channelSettings[ch].range]);
				unit->channelSettings[ch].rangeMax = (int64_t)inputRanges[unit->channelSettings[ch].range] * 1000000;// convert mV to nV
				unit->channelSettings[ch].rangeMin = (int64_t)inputRanges[unit->channelSettings[ch].range] * -1000000;// convert mV to nV
				unit->channelSettings[ch].rangeType = PICO_X1_PROBE_NV;//x1 probe
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

	status = psospaGetDeviceResolution(unit->handle, &resolution);

	printf("\n");

	setDefaults(unit);	// Put these changes into effect
}
/****************************************************************************
* Set digital ports (PORT1, PORT1) and voltage threshold
****************************************************************************/
void setDigitalPorts(GENERICUNIT* unit)
{
	PICO_STATUS status = PICO_OK;
	PICO_DEVICE_RESOLUTION resolution = PICO_DR_8BIT;

	int32_t ch = 0;
	int32_t count = 0;
	int16_t numValidChannels = unit->digitalPortCount;

		count = 0;
		//do
		//{
			// Ask the user to select a range
			printf("Specify voltage port threshold -5V to +5V\n");
			printf("99 - switches port off\n");

			for (ch = 0; ch < numValidChannels; ch++)
			{
				printf("\n");

				do
				{
					printf("Digital Port%c: ", '0' + ch);
					fflush(stdin);
					scanf_s("%lf", &unit->digitalChannelSettings[ch].threshold[0]);
					// Set the threshold for the digital channel

				} while ( ((unit->digitalChannelSettings[ch].threshold[0] > 99.1f) || (unit->digitalChannelSettings[ch].threshold[0] < 98.9f)) &&
					((   unit->digitalChannelSettings[ch].threshold[0] > 5.0f) || 
					(unit->digitalChannelSettings[ch].threshold[0] < -5.0f))
					);

				if ( (unit->digitalChannelSettings[ch].threshold[0] > 99.1f) ||	(unit->digitalChannelSettings[ch].threshold[0] < 98.9f) )
				{
					printf("Port threshold: %+3.3e V\n", unit->digitalChannelSettings[ch].threshold[0]);
					unit->digitalChannelSettings[ch].enabled = TRUE;
					count++;
				}
				else
				{
					printf("Channel Switched off\n");
					unit->digitalChannelSettings[ch].enabled = FALSE;	// Set digital channel off
					unit->digitalChannelSettings[ch].threshold[0] = 0.0f;	// Set threshold to 0V
				}
			}
			//printf(count == 0 ? "\n** At least 1 channel must be enabled **\n\n" : "");
		//} while (count == 0);	// must have at least one channel enabled

		status = psospaGetDeviceResolution(unit->handle, &resolution);

		printf("\n");

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
	status = psospaGetMinimumTimebaseStateless(unit->handle, enabledChannelOrPortFlags, &shortestTimebase, &timeIntervalSeconds, unit->resolution);

	if (status != PICO_OK)
	{
		printf("setTimebase:psospaGetMinimumTimebaseStateless ------ 0x%08x \n", status);
		return;
	}

	printf("Shortest timebase index available %" PRIu32 " = %le seconds.\n", shortestTimebase, timeIntervalSeconds);

	printf("Specify desired timeInterval (in the format Ne-XX, example 1us -> 1e-06): ");
	fflush(stdin);
	double timeIntervalRequested = 0;
	scanf_s("%le", &timeIntervalRequested);
	uint8_t roundFaster = 1; // If 0 = timebase slower than requested, If 1 = timebase faster than requested

	status = psospaNearestSampleIntervalStateless(unit->handle,
		enabledChannelOrPortFlags,	//enabledChannelFlags,
		timeIntervalRequested,		//timeIntervalRequested,
		roundFaster,				//roundFaster,
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

	printf("Timebase used %" PRIu32 " = %le seconds sample interval\n", timebase, timeInterval);
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
	int resolutionInput;

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

	status = psospaGetDeviceResolution(unit->handle, &resolution);

	if (status == PICO_OK)
	{
		printf("Current resolution: ");
		printResolution(&resolution);
	}
	else
	{
		printf("setResolution:psospaGetDeviceResolution ------ 0x%08x \n", status);
		printf("Check the number of channels enabled.\n");
		printf("Check Max. timebase for Resolution\n");
		return;
	}

	printf("\n");

	printf("Select device resolution:\n");
	printf("0: 8 bits\n");
	printf("1: 10 bits\n");
	printf("2: 16 bits\n");

	retry = TRUE;
	do
	{
		printf("Resolution [0...2]: ");
		fflush(stdin);
		scanf_s("%d", &resolutionInput);

		switch (resolutionInput)
		{
		case 0:
			newResolution = PICO_DR_8BIT;
			//if (unit->model == 3XXXE || unit->model == 5XXXE+)
			if ((unit->model == MODEL_3415E ||
				unit->model == MODEL_3415E_MSO ||
				unit->model == MODEL_3416E ||
				unit->model == MODEL_3416E_MSO ||
				unit->model == MODEL_3417E ||
				unit->model == MODEL_3417E_MSO ||
				unit->model == MODEL_3418E ||
				unit->model == MODEL_3418E_MSO ||
				unit->model == MODEL_5462Ep ||
				unit->model == MODEL_5462Ep_MSO ||
				unit->model == MODEL_5463Ep ||
				unit->model == MODEL_5463Ep_MSO ||
				unit->model == MODEL_5464Ep ||
				unit->model == MODEL_5464Ep_MSO
				))
			{
				retry = FALSE;
			}
			else
				printf("setResolution: Invalid resolution for this model.\n");
			break;
		case 1:
			newResolution = PICO_DR_10BIT;
			// if (unit->model == 3XXXE)
			if ((unit->model == MODEL_3415E ||
				unit->model == MODEL_3415E_MSO ||
				unit->model == MODEL_3416E ||
				unit->model == MODEL_3416E_MSO ||
				unit->model == MODEL_3417E ||
				unit->model == MODEL_3417E_MSO ||
				unit->model == MODEL_3418E ||
				unit->model == MODEL_3418E_MSO
				))
			{
				retry = FALSE;
			}
			else
				printf("setResolution: Invalid resolution for this model.\n");
			break;
		case 2:
			newResolution = PICO_DR_16BIT;
			//if(unit->model == 5XXXE || unit->model == 5XXXE+)
			if ((unit->model == MODEL_5462E ||
				unit->model == MODEL_5462E_MSO ||
				unit->model == MODEL_5463E ||
				unit->model == MODEL_5463E_MSO ||
				unit->model == MODEL_5464E ||
				unit->model == MODEL_5464E_MSO ||
				unit->model == MODEL_5462Ep ||
				unit->model == MODEL_5462Ep_MSO ||
				unit->model == MODEL_5463Ep ||
				unit->model == MODEL_5463Ep_MSO ||
				unit->model == MODEL_5464Ep ||
				unit->model == MODEL_5464Ep_MSO
				))
			{
				retry = FALSE;
			}
			else
				printf("setResolution: Invalid resolution for this model.\n");
			break;
		default:
			printf("setResolution: Invalid resolution index.\n");
			retry = TRUE;
			break;
		}
	} while (retry);

	printf("\n");
	status = psospaSetDeviceResolution(unit->handle, (PICO_DEVICE_RESOLUTION)newResolution);

	if (status == PICO_OK)
	{
		unit->resolution = newResolution;

		printf("Resolution selected: ");
		printResolution(&newResolution);

		// The maximum ADC value will change if transitioning from 8 bit to >= 12 bit or vice-versa
		status = psospaGetAdcLimits(unit->handle, newResolution, NULL, &value);
		unit->maxADCValue = value;
	}
	else
	{
		printf("setResolution:psospaSetDeviceResolution ------ 0x%08x \n", status);
		printf("Check the number of channels enabled.\n");
		printf("Check Max. timebase for Resolution\n");
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
	int32_t mVolts;  //int32_t voltage;
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
			
			mVolts = (int32_t)(unit->channelSettings[ch].rangeMax / 1000000);

			//printf("Channel %c Voltage Range = ", 'A' + ch);
			printf("Channel %c Range: ", 'A' + ch);
			if (unit->channelSettings[ch].rangeType <= PICO_X1_PROBE_NV)
				printf("x1 Probe, ");

			if (mVolts < 1000)
			{
				printf("%dmV, ", mVolts);
			}
			else
			{
				printf("%dV, ", mVolts / 1000);
			}
			if(unit->channelSettings[ch].DCcoupled == PICO_DC)
				printf("Coupling: DC, ");
			if (unit->channelSettings[ch].DCcoupled == PICO_AC)
				printf("Coupling: AC, ");
			if (unit->channelSettings[ch].DCcoupled == PICO_DC_50OHM)
				printf("Coupling: 50Ohm, ");
			if ( unit->channelSettings[ch].bandwithLimit == PICO_BW_FULL)
				printf("bandwithLimit: FULL, ");
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_20MHZ)
				printf("bandwithLimit: 20MHz, ");
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_50MHZ)
				printf("bandwithLimit: 50MHz, ");
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_100MHZ)
				printf("bandwithLimit: 100MHz, ");
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_200MHZ)
				printf("bandwithLimit: 200MHz, ");
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_500MHZ)
				printf("bandwithLimit: 500MHz, ");
			printf("analogueOffset: %g\n", unit->channelSettings[ch].analogueOffset);
		}
	}
	if (unit->digitalPortCount != 0)
	{
		printf("\nDigital Ports:\n");
		for (ch = 0; ch < unit->digitalPortCount; ch++)
		{
			if (!(unit->digitalChannelSettings[ch].enabled))
			{
				printf("Digital Port %c: Off\n", '0' + ch);
			}
			else
			{
				printf("Digital Port %c: Threshold: %.3fV\n", '0' + ch, unit->digitalChannelSettings[ch].threshold[0]);
			}
		}
	}
	printf("\n");

	status = psospaGetDeviceResolution(unit->handle, &resolution);

	printf("Device Resolution: ");
	printResolution(&resolution);
}

/****************************************************************************
* print_pico_usb_power_delivery
* Parameters
* - pd        pointer to the PICO_USB_POWER_DELIVERY structure
*
* Returns       none
***************************************************************************/
void print_pico_usb_power_delivery(const PICO_USB_POWER_DELIVERY* pd) {
	if (pd == NULL) {
		printf("Error: Null structure pointer.\n");
		return;
	}
	// uint8_t can be printed using %u (it promotes to int) or the PRIu8 macro
	printf("Valid (= 0):             %u\n", pd->valid_);
	printf("Bus Voltage (mV):        %" PRIu32 " mV\n", pd->busVoltagemV_);
	printf("Rp Current Limit (mA):   %" PRIu32 " mA\n", pd->rpCurrentLimitmA_);
	printf("Partner Connected:       %u\n", pd->partnerConnected_);
	printf("CC Polarity:             %u\n", pd->ccPolarity_);
	printf("Attached Device Type:    ");
	switch(pd->attachedDevice_)
	{
	case 0:
		printf("None\n");
		break;
	case 2:
		printf("Source\n");
		break;
	case 3:
		printf("Debug\n");
		break;
	default:
		printf("Unknown\n");
		break;
	}
	printf("Contract Exists:         %u\n", pd->contractExists_);
	printf("Current PDO:             0x%08X\n", pd->currentPdo_); // Printed in Hexadecimal format often used for registers/PDOs
	printf("Current RDO:             0x%08X\n", pd->currentRdo_); // Printed in Hexadecimal format often used for registers/RDOs
	printf("--------------------------------------\n");
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
	PICO_USB_POWER_DETAILS *powerDetails = &(unit->powerDetails);

	if (serial == NULL)
	{
		status = psospaOpenUnit(&unit->handle, NULL, unit->resolution, powerDetails);
	}
	else
	{
		status = psospaOpenUnit(&unit->handle, serial, unit->resolution, powerDetails);
	}
	// If the device doesn't support 8 bit resolution, try 16 bit as some devices only support 16 bit and not 8 bit
	if (status == PICO_RESOLUTION_NOT_SUPPORTED_BY_VARIANT)
	{
		unit->resolution = PICO_DR_16BIT;
		if (serial == NULL)
		{
			status = psospaOpenUnit(&unit->handle, NULL, unit->resolution, powerDetails);
		}
		else
		{
			status = psospaOpenUnit(&unit->handle, serial, unit->resolution, powerDetails);
		}
	}
	// If USB power fails, print the information to the console
	if (unit->powerDetails.powerErrorLikely_)
	{
		//printf("USB power Error Likely:  %u\n", unit->powerDetails.powerErrorLikely_);
		printf("--- USB Power Delivery Status: DATA PORT---\n");
		print_pico_usb_power_delivery(&(unit->powerDetails.dataPort_));
		printf("--- USB Power Delivery Status: POWER PORT---\n");
		print_pico_usb_power_delivery(&(unit->powerDetails.powerPort_));
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
		status = psospaGetTimebase(unit->handle, timebase, constBufferSize, &temp_timeIntervalns, NULL, 0);

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

	status = psospaGetAdcLimits(unit->handle, PICO_DR_8BIT, NULL, &value);
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
			status = psospaSetDigitalPortOff(unit->handle, (PICO_CHANNEL)(i + PICO_PORT0));
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

		//Set "range" to match rangeMax/Min values for legacy code!
		unit->channelSettings[i].range = PICO_X1_PROBE_1V;//
		unit->channelSettings[i].rangeMax = inputRanges[PICO_X1_PROBE_1V] * 1000000;//1v range - convert mV to nV
		unit->channelSettings[i].rangeMin = inputRanges[PICO_X1_PROBE_1V] * -1000000;//1v range

		unit->channelSettings[i].rangeType = PICO_X1_PROBE_NV;//x1 probe
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
	status = psospaSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

	return unit->openStatus;
}

/****************************************************************************
* closeDevice
****************************************************************************/
void closeDevice(GENERICUNIT* unit)
{
	psospaCloseUnit(unit->handle);
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
						uint64_t nSamples,
                        FILE_TYPE filetype, // Set the number of raw samples
                        BOOL imagefile
                        ) 
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
	// Create Overflow Array Buffer(s)
	int16_t* FileOverflow;
	FileOverflow = (int16_t*)calloc(nCaptures, sizeof(int16_t));
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
	printf("%" PRIu64 " Samples\n", nSamples);
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
		status = psospaGetValuesAsync(unit->handle,
			0,				// startIndex
			bufferSettings.nSamples,
			downSampleRatio,
			ratioMode,
			0,				// segmentIndex
			callBackDataReady,	// pointer to Data callback
			NULL);			// pParameter

		if (status != PICO_OK)
		{
			printf("blockDataHandler:psospaGetValues ------ 0x%08x \n", status);
			return;
		}
	}
	else // > 1 segment
	{
		status = psospaGetValuesBulkAsync(unit->handle,
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
			printf("blockDataHandler:psosp0aGetValuesBulkAsync ------ 0x%08x \n", status);
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
	struct tPicoProbeScaling enabledChannelsScaling[PSOSPA_MAX_CHANNELS] = { 0 };
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

		if (imagefile == TRUE)
		{
		printf("\nWriting Capture to image file.\n");
		WriteArrayToImage(
		  unit,
		  minBuffersStopped,
		  maxBuffersStopped,
		  multiBufferSizes,
		  enabledChannelsScaling,
		  buf,
		  0, // streamingDataTriggerInfoTemp.triggerAt_, // Triggersample
		  NULL, // No overflow flags
		  0, // plotChannelMask: 0 = all enabled channels
		  NULL); // Set default full range if NULL
		}
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
				status = psospaSetDataBuffers(unit->handle,
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
					printf("SetAllDataBuffers:psospaSetDataBuffers ------ 0x%08x, for channel %c \n", status, PICO_CHANNEL_A + channel);
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
				status = psospaSetDataBuffers(unit->handle,
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
					printf("SetAllDataBuffers:psospaSetDataBuffers(PORT %d) ------ 0x%08x \n", PICO_PORT0 + channel, status);
					return;
				}
				//if (capture == 0)
				//	printf("PORT%d,", channel);
			}
		}
	}
	//printf("\n");
}
