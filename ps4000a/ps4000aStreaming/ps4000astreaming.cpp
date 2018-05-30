/*******************************************************************************
 *
 * Filename: ps4000astreaming.cpp
 *
 * Description:
 *   This is a console mode program that demonstrates how to use the
 *   PicoScope 4000 Series (ps4000a) driver functions to stream data immediately 
 *	 or with a trigger.
 *
 *	Supported PicoScope models:
 *
 *		PicoScope 4225 & 4425
 *		PicoScope 4444
 *		PicoScope 4824
 *
 * Examples:
 *
 *  Setting Up channels
 *	Collect stream data immediately
 *	Collect stream data with trigger
 *	Setting up trigger using simple trigger
 *	Setting up a trigger using individual trigger calls
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps4000a.lib can be located
 *			Ensure that the ps4000aApi.h, PicoConnectProbes.h and PicoStatus.h 
 *			files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps4000a.lib to the project (Microsoft C only)
 *			 Add ps4000aApi.h, PicoConnectProbes.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps4000a driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. 
 *		Edit the configure.ac and Makefile.am files as required.
 *		In a terminal window, use the following commands to build the application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2013-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include "windows.h"
#include <conio.h>
#include "ps4000aApi.h"

#define OCTO_SCOPE		8
#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

const uint32_t	bufferLength = 100000;
PICO_STATUS			status = PICO_OK;
int64_t					g_totalSamples = 0;
int16_t    			g_ready = FALSE;
int32_t      		g_sampleCount = 0;
uint32_t				g_startIndex;
int16_t					g_autoStop;
int16_t					g_trig = 0;
uint32_t				g_trigAt = 0;
int16_t					g_probeStateChanged = 0;

typedef enum
{
	MODEL_NONE = 0,
	MODEL_PS4824 = 0x12d8,
	MODEL_PS4225 = 0x1081,
	MODEL_PS4425 = 0x1149,
	MODEL_PS4444 = 0x115C
} MODEL_TYPE;

typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
	float analogueOffset;
}CHANNEL_SETTINGS;

typedef enum
{
	SIGGEN_NONE = 0,
	SIGGEN_FUNCTGEN = 1,
	SIGGEN_AWG = 2
} SIGGEN_TYPE;

typedef struct
{
	int16_t						handle;
	MODEL_TYPE				model;
	int8_t						modelString[8];
	int8_t						serial[11];
	int16_t						complete;
	int16_t						openStatus;
	int16_t						openProgress;
	PS4000A_RANGE			firstRange;
	PS4000A_RANGE			lastRange;
	int16_t						channelCount;
	int16_t						maxADCValue;
	SIGGEN_TYPE				sigGen;
	int16_t						hasETS;
	uint16_t					AWGFileSize;
	CHANNEL_SETTINGS	channelSettings[PS4000A_MAX_CHANNELS];
	uint16_t					hasFlexibleResolution;
	uint16_t					hasIntelligentProbes;
}UNIT;

// Struct to store intelligent probe information
typedef struct tUserProbeInfo
{
		PICO_STATUS status;
		PS4000A_USER_PROBE_INTERACTIONS userProbeInteractions[PS4000A_MAX_4_CHANNELS];
		uint32_t numberOfProbes;

}USER_PROBE_INFO;

typedef struct tBufferInfo
{
	UNIT			*unit;
	int16_t		**driverBuffers;
	int16_t		**appBuffers;

} BUFFER_INFO;

uint32_t inputRanges[] = {
							10,
							20,
							50,
							100,
							200,
							500,
							1000,
							2000,
							5000,
							10000,
							20000,
							50000,
							100000,
							200000 };

USER_PROBE_INFO userProbeInfo;


/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t rangeIndex, UNIT * unit)
{
	return (raw * (int32_t) inputRanges[rangeIndex]) / unit->maxADCValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
* (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t rangeIndex, UNIT * unit)
{
	return (mv * unit->maxADCValue) / (int16_t) inputRanges[rangeIndex];
}

/****************************************************************************
* Callback
* Used by ps4000a data streaming collection calls, on receipt of data.
* Used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 CallBackStreaming
(
	int16_t handle,
	int32_t noOfSamples,
	uint32_t startIndex,
	int16_t overflow,
	uint32_t triggerAt,
	int16_t triggered,
	int16_t autoStop,
	void * pParameter
)
{
	BUFFER_INFO * bufferInfo = NULL;

	if (pParameter != NULL)
	{
		bufferInfo = (BUFFER_INFO *)pParameter;
	}

	// Used for streaming
	g_sampleCount = noOfSamples;
	g_startIndex = startIndex;
	g_autoStop = autoStop;

	// Flag to say done reading data
	g_ready = TRUE;

	// Flags to show if & where a trigger has occurred
	g_trig = triggered;
	g_trigAt = triggerAt;

	if (bufferInfo != NULL && noOfSamples)
	{
		for (int channel = 0; channel < bufferInfo->unit->channelCount; channel++)
		{
			if (bufferInfo->unit->channelSettings[channel].enabled)
			{
				if (bufferInfo->appBuffers && bufferInfo->driverBuffers)
				{
					if (bufferInfo->appBuffers[channel * 2] && bufferInfo->driverBuffers[channel * 2])
					{
						memcpy_s(&bufferInfo->appBuffers[channel * 2][0], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
					}

					// Min buffers
					if (bufferInfo->appBuffers[channel * 2 + 1] && bufferInfo->driverBuffers[1])
					{
						memcpy_s(&bufferInfo->appBuffers[channel * 2 + 1][0], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
					}

				}
			}
		}
	}
}

/****************************************************************************
* Probe Interaction Callback
*
* See ps4000aProbeInteractions (callback)
*
****************************************************************************/
void PREF4 CallBackProbeInteractions(int16_t handle, PICO_STATUS status, PS4000A_USER_PROBE_INTERACTIONS * probes, uint32_t	nProbes)
{
		uint32_t i = 0;

		userProbeInfo.status = status;
		userProbeInfo.numberOfProbes = nProbes;

		for (i = 0; i < nProbes; ++i)
		{
				userProbeInfo.userProbeInteractions[i].connected = probes[i].connected;

				userProbeInfo.userProbeInteractions[i].channel = probes[i].channel;
				userProbeInfo.userProbeInteractions[i].enabled = probes[i].enabled;

				userProbeInfo.userProbeInteractions[i].probeName = probes[i].probeName;

				userProbeInfo.userProbeInteractions[i].requiresPower_ = probes[i].requiresPower_;
				userProbeInfo.userProbeInteractions[i].isPowered_ = probes[i].isPowered_;

				userProbeInfo.userProbeInteractions[i].status_ = probes[i].status_;

				userProbeInfo.userProbeInteractions[i].probeOff = probes[i].probeOff;

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
* SetDefaults - set up channel voltage scales, coupling and offset
****************************************************************************/
void SetDefaults(UNIT *unit)
{

	for (int32_t ch = 0; ch < unit->channelCount; ch++)
	{
		status = ps4000aSetChannel(unit->handle,																						// Handle to select the correct device
			(PS4000A_CHANNEL)(PS4000A_CHANNEL_A + ch),																				// channel
			unit->channelSettings[PS4000A_CHANNEL_A + ch].enabled,														// If channel is enabled or not
			(PS4000A_COUPLING)unit->channelSettings[PS4000A_CHANNEL_A + ch].DCcoupled,				// If AC or DC coupling
			(PICO_CONNECT_PROBE_RANGE) unit->channelSettings[PS4000A_CHANNEL_A + ch].range,		// The voltage scale of the channel
			unit->channelSettings[PS4000A_CHANNEL_A + ch].analogueOffset);										// Analogue offset

		printf(status ? "SetDefaults:ps4000aSetChannel------ 0x%08lx for channel %i\n" : "", status, ch);
	}
}

/****************************************************************************
* OpenDevice
* Parameters
* - unit        pointer to the UNIT structure, where the handle will be stored
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS OpenDevice(UNIT *unit)
{
	int8_t ch = 'Y';
	char line[80] = { 0 };
	
	int16_t requiredSize;
	int16_t minArbitraryWaveformValue = 0;
	int16_t maxArbitraryWaveformValue = 0;

	int32_t variant;

	uint32_t minArbitraryWaveformBufferSize = 0;
	uint32_t maxArbitraryWaveformBufferSize = 0;

	// Can call this function multiple times to open multiple devices 
	// Note that the unit.handle is specific to each device make sure you don't overwrite them
	status = ps4000aOpenUnit(&unit->handle, nullptr);
	
	if (unit->handle == 0)
	{
		return status;
	}

	// Check status code returned for power status codes
	switch (status)
	{
		case PICO_OK: // No need to change power source

			break;

		case PICO_POWER_SUPPLY_NOT_CONNECTED:

		do
		{
			printf("\n5 V power supply not connected.");
			printf("\nDo you want to run using USB only Y/N?\n");

			ch = toupper(_getch());

			if (ch == 'Y')
			{
				printf("\nPower OK\n\n");
				status = ps4000aChangePowerSource(unit->handle, PICO_POWER_SUPPLY_NOT_CONNECTED);		// Tell the driver that's ok
			}

		} while (ch != 'Y' && ch != 'N');

		printf(ch == 'N' ? "Please set correct USB connection setting for this device\n" : "");
		break;

		case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:	// User must acknowledge they want to power via USB

			do
			{
				printf("\nUSB 3.0 device on non-USB 3.0 port.\n");
				status = ps4000aChangePowerSource(unit->handle, PICO_USB3_0_DEVICE_NON_USB3_0_PORT);		// Tell the driver that's ok

			} while (ch != 'Y' && ch != 'N');

			printf(ch == 'N' ? "Please set correct USB connection setting for this device\n" : "");
			break;

		default:

			return status;
	}


	// Device only has these min and max values
	unit->firstRange = PS4000A_10MV;
	unit->lastRange = PS4000A_50V;

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


	for (int i = 0; i <= 10; i++)
	{
		status = ps4000aGetUnitInfo(unit->handle, (int8_t *) line, sizeof(line), &requiredSize, i);

		// info = 3 - PICO_VARIANT_INFO
		if (i == PICO_VARIANT_INFO)
		{
			variant = atoi(line);
			memcpy(&(unit->modelString), line, sizeof(unit->modelString) == 5 ? 5 : sizeof(unit->modelString));
		}
		else if (i == PICO_BATCH_AND_SERIAL)	// info = 4 - PICO_BATCH_AND_SERIAL
		{
			memcpy(&(unit->serial), line, requiredSize);
		}

		printf("%s: %s\n", description[i], line);
	}

	printf("\n");

	// Find the maxiumum AWG buffer size
	status = ps4000aSigGenArbitraryMinMaxValues(unit->handle, &minArbitraryWaveformValue, &maxArbitraryWaveformValue, &minArbitraryWaveformBufferSize, &maxArbitraryWaveformBufferSize);

	switch (variant)
	{
	case MODEL_PS4824:
		unit->model = MODEL_PS4824;
		unit->sigGen = SIGGEN_AWG;
		unit->firstRange = PS4000A_10MV;
		unit->lastRange = PS4000A_50V;
		unit->channelCount = OCTO_SCOPE;
		unit->hasETS = FALSE;
		unit->AWGFileSize = maxArbitraryWaveformBufferSize;
		unit->hasFlexibleResolution = 0;
		unit->hasIntelligentProbes = 0;
		break;

	case MODEL_PS4225:
		unit->model = MODEL_PS4225;
		unit->sigGen = SIGGEN_NONE;
		unit->firstRange = PS4000A_50MV;
		unit->lastRange = PS4000A_200V;
		unit->channelCount = DUAL_SCOPE;
		unit->hasETS = FALSE;
		unit->AWGFileSize = 0;
		unit->hasFlexibleResolution = 0;
		unit->hasIntelligentProbes = 0;
		break;

	case MODEL_PS4425:
		unit->model = MODEL_PS4425;
		unit->sigGen = SIGGEN_NONE;
		unit->firstRange = PS4000A_50MV;
		unit->lastRange = PS4000A_200V;
		unit->channelCount = QUAD_SCOPE;
		unit->hasETS = FALSE;
		unit->AWGFileSize = 0;
		unit->hasFlexibleResolution = 0;
		unit->hasIntelligentProbes = 0;
		break;

	case MODEL_PS4444:
		unit->model = MODEL_PS4444;
		unit->sigGen = SIGGEN_NONE;
		unit->firstRange = PS4000A_10MV;
		unit->lastRange = PS4000A_50V;
		unit->channelCount = QUAD_SCOPE;
		unit->hasETS = FALSE;
		unit->AWGFileSize = 0;
		unit->hasFlexibleResolution = 0;
		unit->hasIntelligentProbes = 1;
		break;

	default:
		unit->model = MODEL_NONE;
		break;
	}

	// Register probe interaction callback 
	if (unit->hasIntelligentProbes)
	{
			status = ps4000aSetProbeInteractionCallback(unit->handle, CallBackProbeInteractions);

			// Wait for information to populate (callback will be called twice initially)
			Sleep(2000);

	}

	for (int ch = 0; ch < unit->channelCount; ch++)
	{
		unit->channelSettings[ch].enabled = (ch == 0);
		unit->channelSettings[ch].DCcoupled = TRUE;
		unit->channelSettings[ch].range = PS4000A_5V;
		unit->channelSettings[ch].analogueOffset = 0.0f;
	}

	ps4000aMaximumValue(unit->handle, &unit->maxADCValue);
	SetDefaults(unit);
	return status;
}

/****************************************************************************
* Select input voltage ranges for channels
****************************************************************************/
void SetVoltages(UNIT *unit)
{
	int32_t i, ch, count = 0;
	for (i = unit->firstRange; i <= unit->lastRange; i++)
	{
		printf("%d -> %d mV\n", i, inputRanges[i]);
	}

	do
	{
		/* Ask the user to select a range */
		printf("Specify voltage range (%d..%d)\n", unit->firstRange, unit->lastRange);
		printf("99 - switches channel off\n");
		for (ch = 0; ch < unit->channelCount; ch++)
		{
			printf("\n");
			do
			{
				printf("Channel %c: ", 'A' + ch);
				fflush(stdin);
				scanf_s("%hd", &(unit->channelSettings[ch].range));
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
				unit->channelSettings[ch].range = PS4000A_10MV;
			}
		}
		printf(count == 0 ? "\n** At least 1 channel must be enabled **\n\n" : "");
	} while (count == 0);	// Must have at least one channel enabled

	SetDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
* Stream Data Handler
* - Used by the two stream data examples - untriggered and triggered
* Inputs:
* - unit - the unit to sample on
***************************************************************************/
void StreamDataHandler(UNIT * unit)
{
	int16_t autostop;
	int16_t powerChange = 0;

	int16_t * buffers[PS4000A_MAX_CHANNEL_BUFFERS];
	int16_t * appBuffers[PS4000A_MAX_CHANNEL_BUFFERS];

	int32_t i, j;
	int32_t index = 0;
	int32_t totalSamples = 0;

	uint32_t downsampleRatio;
	uint32_t preTrigger;
	uint32_t postTrigger;
	uint32_t sampleInterval;
	uint32_t triggeredAt = 0;

	FILE * fp = NULL;
	BUFFER_INFO bufferInfo;

	PICO_STATUS status;
	PS4000A_TIME_UNITS timeUnits;
	PS4000A_RATIO_MODE ratioMode;

	
	// Setup data and temporary application buffers to copy data into
	for (i = 0; i < unit->channelCount; i++)
	{

		if (unit->channelSettings[PS4000A_CHANNEL_A + i].enabled)
		{

			buffers[i * 2] = (int16_t*)calloc(bufferLength, sizeof(int16_t));
			buffers[i * 2 + 1] = (int16_t*)calloc(bufferLength, sizeof(int16_t));
			status = ps4000aSetDataBuffers(unit->handle, (PS4000A_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], bufferLength, 0, PS4000A_RATIO_MODE_NONE);

			appBuffers[i * 2] = (int16_t*)calloc(bufferLength, sizeof(int16_t));
			appBuffers[i * 2 + 1] = (int16_t*)calloc(bufferLength, sizeof(int16_t));

			printf(status ? "StreamDataHandler:ps4000aSetDataBuffers(channel %ld) ------ 0x%08lx \n" : "", i, status);
		}
	}

	downsampleRatio = 1;
	timeUnits = PS4000A_US;
	sampleInterval = 1;
	ratioMode = PS4000A_RATIO_MODE_NONE;
	preTrigger = 0;
	postTrigger = 1000000;
	autostop = TRUE;

	bufferInfo.unit = unit;
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;

	if (autostop)
	{
		printf("\nStreaming Data for %lu samples", postTrigger / downsampleRatio);
		
		if (preTrigger) // We pass 0 for preTrigger if we're not setting up a trigger
		{
			printf(" after the trigger occurs\nNote: %lu Pre Trigger samples before Trigger arms\n\n", preTrigger / downsampleRatio);
		}
		else
		{
			printf("\n\n");
		}
	}
	else
	{
		printf("\nStreaming Data continually\n\n");
	}

	g_autoStop = FALSE;

	printf("Collect streaming...\n");
	printf("Data is written to disk file (stream.txt)\n");
	printf("Press a key to start\n");
	_getch();

	do
	{
    // For streaming we use sample interval rather than timebase used in ps4000aRunBlock
    status = ps4000aRunStreaming(unit->handle, &sampleInterval, timeUnits, preTrigger, postTrigger, autostop, downsampleRatio, ratioMode,
      bufferLength);

		if (status != PICO_OK)
		{
			if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED || status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
			{
				status = ps4000aChangePowerSource(unit->handle, status);
			}
			else
			{
				printf("StreamDataHandler:ps4000aRunStreaming ------ 0x%08lx \n", status);
				return;
			}
		}
	} while (status != PICO_OK);

	printf("Streaming data...Press a key to stop\n");
	fopen_s(&fp, "stream.txt", "w");

	if (fp != NULL)
	{
		fprintf(fp, "For each of the %d Channels, results shown are....\n", unit->channelCount);
		fprintf(fp, "Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

		for (i = 0; i < unit->channelCount; i++)
		{
			if (unit->channelSettings[i].enabled)
			{
				fprintf(fp, "   Max ADC    Max mV  Min ADC  Min mV   ");
			}
		}
		fprintf(fp, "\n");
	}

	while (!_kbhit() && !g_autoStop)
	{
		/* Poll until data is received. Until then, ps4000aGetStreamingLatestValues() wont call the callback */
		Sleep(0);
		g_ready = FALSE;

		status = ps4000aGetStreamingLatestValues(unit->handle, CallBackStreaming, &bufferInfo);

		index++;

		if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
		{
			if (g_trig)
			{
				triggeredAt = totalSamples + g_trigAt;		// Calculate where the trigger occurred in the total samples collected
			}

			totalSamples += g_sampleCount;
			printf("\nCollected %3li samples, index = %5lu, Total: %7d samples", g_sampleCount, g_startIndex, totalSamples);

			if (g_trig)
			{
				printf("Trig. at index %lu total %lu", g_trigAt, triggeredAt);	// Show where trigger occurred

			}

			for (i = 0; i < (uint32_t)(g_sampleCount); i++)
			{

				if (fp != NULL)
				{
					for (j = 0; j < unit->channelCount; j++)
					{
						if (unit->channelSettings[j].enabled)
						{
							fprintf(fp,
								"Ch%C  %7d = %7dmV, %7d = %7dmV   ",
								(char)('A' + j),
								appBuffers[j * 2][i],
								adc_to_mv(appBuffers[j * 2][i], unit->channelSettings[PS4000A_CHANNEL_A + j].range, unit),
								appBuffers[j * 2 + 1][i],
								adc_to_mv(appBuffers[j * 2 + 1][i], unit->channelSettings[PS4000A_CHANNEL_A + j].range, unit));
						}
					}

					fprintf(fp, "\n");
				}
				else
				{
					printf("\nCannot open the file %s for writing.\n", "stream.txt");
					return;
				}

			}
		}
	}

	ps4000aStop(unit->handle);

	if (fp != NULL)
	{

		fclose(fp);
	}

	if (!g_autoStop)
	{
		printf("\nData collection aborted\n");
		_getch();
	}
	else
	{
		printf("\nData collection complete.\n\n");
	}

	for (i = 0; i < unit->channelCount; i++)
	{
		if (unit->channelSettings[i].enabled)
		{
			free(buffers[i * 2]);
			free(appBuffers[i * 2]);

			free(buffers[i * 2 + 1]);
			free(appBuffers[i * 2 + 1]);

			if ((status = ps4000aSetDataBuffers(unit->handle, (PS4000A_CHANNEL)i, NULL, NULL, 0, 0, PS4000A_RATIO_MODE_NONE)) != PICO_OK)
			{
				printf("ClearDataBuffers:ps4000aSetDataBuffers(channel %d) ------ 0x%08lx \n", i, status);
			}
		}
	}
}

/****************************************************************************
* CollectStreamingImmediate
*  this function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
***************************************************************************/
void CollectStreamingImmediate(UNIT * unit)
{
	status = ps4000aSetSimpleTrigger(unit->handle, 0, PS4000A_CHANNEL_A, 0, PS4000A_RISING, 0, 0);

	// Disable trigger, no need to call three functions to trun off trigger
	if (status!= PICO_OK)
	{
		printf("Error seeting trigger, Error Code: 0x%08lx\n", status);
		return;
	}

	StreamDataHandler(unit);
	return;
}

/****************************************************************************
* CollectStreamingImmediate
*  this function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
***************************************************************************/
void CollectStreamingTriggered(UNIT * unit)
{
	PS4000A_CONDITION *_condition = new PS4000A_CONDITION;
	PS4000A_DIRECTION *direction = new PS4000A_DIRECTION;
	PS4000A_TRIGGER_CHANNEL_PROPERTIES *properties = new PS4000A_TRIGGER_CHANNEL_PROPERTIES;

	PS4000A_CHANNEL ch = PS4000A_CHANNEL_A;
	PS4000A_THRESHOLD_DIRECTION _direction = PS4000A_RISING;
	int16_t threshold = mv_to_adc(-1000, unit->channelSettings[PS4000A_CHANNEL_A].range, unit); 

	// If this array contains more than one channel for example was initialised as _condition[2] the 
	// channels could be combined as an AND logic; please remember to increase nConditions accordingly.
	// Channels are DONT_CARE if not stated
	_condition->condition = PS4000A_CONDITION_TRUE;
	_condition->source = ch;

	direction->channel = ch;
	direction->direction = _direction;

	properties->channel = ch;
	properties->thresholdLower = threshold;
	properties->thresholdLowerHysteresis = 0;
	properties->thresholdMode = PS4000A_LEVEL;
	properties->thresholdUpper = threshold;
	properties->thresholdUpperHysteresis = 0;


	// Calling PS4000A_CLEAR|PS4000A_ADD removes all previous conditions and sets this new one
	// If you wish to OR channels this functions will have to be called again but with only PS4000A_ADD
	if (status = ps4000aSetTriggerChannelConditions(unit->handle, _condition, 1, (PS4000A_CONDITIONS_INFO)(PS4000A_CLEAR | PS4000A_ADD)) != PICO_OK)
	{
		printf("Error setting Trigger Channel Conditions, Error Code: 0x%08lx\n", status);
		return;
	}

	// Only ever need to call this once all channel can be set up if we make the directions into an array to hold all channels
	if (status = ps4000aSetTriggerChannelDirections(unit->handle, direction, 1) != PICO_OK)
	{
		printf("Error setting Trigger Channel Directions, Error Code: 0x%08lx\n", status);
		return;
	}

	// Only ever need to call this once all channel can be set up if we make the properties into an array
	if (status = ps4000aSetTriggerChannelProperties(unit->handle, properties, 1, 0, 0) != PICO_OK)
	{
		printf("Error setting Trigger Channel Properties, Error Code: 0x%08lx\n", status);
		return;
	}

	StreamDataHandler(unit);
}

int main(void)
{
	int8_t ch = '.';
	UNIT unit;

	printf("PicoScope 4000 Series (ps4000a) Driver Streaming Data Collection Example Program\n\n");

	status = OpenDevice(&unit);
	
	if (status != PICO_OK) // If unit not found or open no need to continue
	{
		printf("Picoscope devices failed to open or select power source\n error code: 0x%08lx\n", status);
		_getch();
		return 0;
	}


	while (ch != 'X')
	{
		printf("\n\n");
		printf("S - Immediate streaming                       V - Set voltages\n");
		printf("T - Triggered streaming\n");
		printf("                                              X - Exit\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");
		switch (ch)
		{
		case 'S':
			CollectStreamingImmediate(&unit);
			break;

		case 'T':
			CollectStreamingTriggered(&unit);
			break;

		case 'V':
			SetVoltages(&unit);
			break;

		case 'X':
			break;

		default:
			printf("Invalid operation\n");
			break;
		}
	}
	ps4000aCloseUnit(unit.handle);
	return 1;
}
