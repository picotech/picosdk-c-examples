/*******************************************************************************
 *
 * Filename: Libpsospa.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 3XXXE Series (psospa) devices.
 *
 * Copyright (C) 2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include "../../shared/PicoUnit.h"
#include "../../shared/PicoScaling.h"

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

#include <libpsospa/psospaApi.h>
#ifndef PICO_STATUS
#include <libpsospa/PicoStatus.h>
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
const uint64_t constBufferSize = 12040;
/***************************************************************************/

/****************************************************************************
* Callback Probe Interaction
*
* See psospaProbeInteractions (callback)
*
****************************************************************************/
static void PREF4 CallBackProbeInteractions(int16_t handle, PICO_STATUS status, PICO_USER_PROBE_INTERACTIONS *probes, uint32_t	nProbes)
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
			printf(status ? "SetDefaults:psospaSetChannelOn------ 0x%08lx \n" : "", status);
		}
		else
		{
			status = psospaSetChannelOff(unit->handle, (PICO_CHANNEL)(PICO_CHANNEL_A + i));
			printf(status ? "SetDefaults:psospaSetChannelOff------ 0x%08lx \n" : "", status);
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
		printf("ClearDataBuffers:psospaSetDataBuffers ------ 0x%08lx \n", status);
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
	PICO_CONDITIONS_INFO info = PICO_CLEAR_CONDITIONS;
	PICO_CONDITIONS_INFO pwqInfo = PICO_CLEAR_CONDITIONS;

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
		info = (PICO_CONDITIONS_INFO)(PICO_CLEAR_CONDITIONS | PICO_ADD_CONDITION); // Clear and add trigger condition specified unless no trigger conditions have been specified
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

	//psospaSetPulseWidthQualifierDirections //////////////////////////////PASS ZERO DIRECTIONS???
	if ((status = psospaSetPulseWidthQualifierDirections(unit->handle,
		pwq->directions, pwq->nDirections)) != PICO_OK)
	{
		printf("SetTrigger:psospaSetPulseWidthQualifierDirections ------ 0x%08x \n", status);
		return status;
	}

	// Clear and add pulse width qualifier condition, clear if no pulse width qualifier has been specified
	if (pwq->nConditions != 0)
	{
		pwqInfo = (PICO_CONDITIONS_INFO)(PICO_CLEAR_CONDITIONS | PICO_ADD_CONDITION);
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
	unit->digitalPortCount = 0;

	if (unit->handle)
	{
		printf("Device information:-\n\n");

		for (i = 0; i < 11; i++)
		{
			status = psospaGetUnitInfo(unit->handle, line, sizeof(line), &requiredSize, i);

			// info = 3 - PICO_VARIANT_INFO
			if (i == PICO_VARIANT_INFO)
			{
				variant = atoi(line);
				memcpy(&(unit->modelString), line, sizeof(unit->modelString) == 5 ? 5 : sizeof(unit->modelString));
				//memcpy(&(unit->modelString), line, sizeof(unit->modelString));

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
	status = psospaGetMinimumTimebaseStateless(unit->handle, enabledChannelOrPortFlags, &shortestTimebase, &timeIntervalSeconds, unit->resolution);

	if (status != PICO_OK)
	{
		printf("setTimebase:psospaGetMinimumTimebaseStateless ------ 0x%08lx \n", status);
		if(status == 0x0000018c)
			printf("The channel combination is not valid for the ADC resolution (10/12bit)");
		return;
	}

	printf("Shortest timebase index available %d = %le seconds.\n", shortestTimebase, timeIntervalSeconds);

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

	status = psospaGetDeviceResolution(unit->handle, &resolution);

	if (status == PICO_OK)
	{
		printf("Current resolution: ");
		printResolution(&resolution);
	}
	else
	{
		printf("setResolution:psospaGetDeviceResolution ------ 0x%08lx \n", status);
		printf("Check the number of channels enabled.\n");
		printf("Check Max. timebase for Resolution\n");
		return;
	}

	printf("\n");

	printf("Select device resolution:\n");
	printf("0: 8 bits\n");
	printf("1: 10 bits\n");

	retry = TRUE;
	do
	{
		printf("Resolution [0...1]: ");

		fflush(stdin);
		scanf_s("%lud", &resolutionInput);
		if (resolutionInput == 0)
			resolutionInput = PICO_DR_8BIT;
		if (resolutionInput == 1)
			resolutionInput = PICO_DR_10BIT;
		newResolution = (PICO_DEVICE_RESOLUTION)resolutionInput;

		// Verify if resolution can be selected for number of channels enabled
		/*
		if (newResolution == PICO_DR_12BIT && numEnabledChannels > 2)
		{
			printf("setResolution: 12 bit resolution can only be selected with 2 channel enabled.\n");
		}
		else if (newResolution == PICO_DR_10BIT && numEnabledChannels > 4)
		{
			printf("setResolution: 10 bit resolution can only be selected with a maximum of 4 channels enabled.\n");
		}
		else
		*/
		if (newResolution < PICO_DR_8BIT && newResolution > PICO_DR_10BIT)
		{
			printf("setResolution: Resolution index selected out of bounds.\n");
		}
		else
		{
			retry = FALSE;
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
		printf("setResolution:psospaSetDeviceResolution ------ 0x%08lx \n", status);
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
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_20MHZ)	//Not 6428E-D
				printf("bandwithLimit: 20MHz, ");
			if (unit->channelSettings[ch].bandwithLimit == PICO_BW_200MHZ)	//64x5E and 64x6E only
				printf("bandwithLimit: 200MHz, ");
			printf("analogueOffset: %g\n", unit->channelSettings[ch].analogueOffset);
		}
	}
	printf("\n");

	status = psospaGetDeviceResolution(unit->handle, &resolution);

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
		status = psospaOpenUnit(&unit->handle, NULL, unit->resolution, NULL);
	}
	else
	{
		status = psospaOpenUnit(&unit->handle, serial, unit->resolution, NULL);
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
PICO_STATUS handleDevice(GENERICUNIT* unit)
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
			status = psospaSetDigitalPortOff(unit->handle, (PICO_CHANNEL)(i + PICO_PORT0));
		}
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
		printf("Limiting enabled channels to %d! (Starting at ChA)\n", enabled_chs_limit);
	}
	if(TURN_ON_EVERY_N_CH != 1)
		printf("Turning on every %d Channels\n", TURN_ON_EVERY_N_CH);

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

	//memset(&pulseWidth, 0, sizeof(struct tPwq));

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