/*******************************************************************************
 *
 * Filename: ps6000aCon.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use some of 
 *	 the PicoScope 6000 Series (ps6000a) driver API functions to perform operations
 *	 using a PicoScope 6000 Oscilloscope.
 *
 *	Supported PicoScope models:
 *
 *		PicoScope 6000a API devices
 *
 * Examples:
 *   Collect a block of samples immediately
 *   Change timebase & voltage scales
 *   Display data in mV or ADC counts
 *	 Handle power source changes
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express/Community Edition) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps6000a.lib can be located
 *			Ensure that the ps6000aApi.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps6000a.lib to the project (Microsoft C only)
 *			 Add ps6000aApi.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps6000a driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the ps6000aCon application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2023-2024 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>

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

int32_t fopen_s(FILE ** a, const int8_t * b, const int8_t * c)
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

#define BUFFER_SIZE 	1024

#define OCTA_SCOPE		8
#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

#define MAX_PICO_DEVICES 64
#define TIMED_LOOP_STEP 500

#define PS6000A_MAX_CHANNELS 8 //analog chs only

typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
	float analogueOffset;
}CHANNEL_SETTINGS;

typedef enum
{
	MODEL_NONE = 0,//this is used
//models values not used in the code
} MODEL_TYPE;

typedef enum
{
	SIGGEN_NONE = 0,
	SIGGEN_FUNCTGEN = 1,
	SIGGEN_AWG = 2
} SIGGEN_TYPE;

typedef struct
{
	int16_t handle;
	MODEL_TYPE				model;
	int8_t						modelString[8];
	int8_t						serial[10];
	int16_t						complete;
	int16_t						openStatus;
	int16_t						openProgress;
	PICO_CONNECT_PROBE_RANGE			firstRange;
	PICO_CONNECT_PROBE_RANGE			lastRange;
	int16_t						channelCount;
	int16_t						maxADCValue;
	PICO_WAVE_TYPE				sigGen;
	int16_t						hasHardwareETS;
	uint16_t					awgBufferSize;
	CHANNEL_SETTINGS	channelSettings [PS6000A_MAX_CHANNELS];
	PICO_DEVICE_RESOLUTION	resolution;
	int16_t						digitalPortCount;
}UNIT;

uint32_t timebase = 8;
BOOL			scaleVoltages = TRUE;

uint16_t inputRanges [PICO_X1_PROBE_RANGES] = {
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
												20000};

int16_t			g_autoStopped;
int16_t   	g_ready = FALSE;
uint64_t 		g_times [PS6000A_MAX_CHANNELS];
int16_t     g_timeUnit;
int32_t     g_sampleCount;
uint32_t		g_startIndex;
int16_t			g_trig = 0;
uint32_t		g_trigAt = 0;
int16_t			g_overflow = 0;

int8_t blockFile[20] = "block.txt";


typedef struct tBufferInfo
{
	UNIT * unit;
	int16_t **driverBuffers;
	int16_t **appBuffers;

} BUFFER_INFO;

/****************************************************************************
* Callback
* used by ps6000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackBlock(int16_t handle, PICO_STATUS status, void* pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
	}
}

/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void setDefaults(UNIT * unit)
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
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t rangeIndex, UNIT * unit)
{
	return (raw * inputRanges[rangeIndex]) / unit->maxADCValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t rangeIndex, UNIT * unit)
{
	return (mv * unit->maxADCValue) / inputRanges[rangeIndex];
}

/****************************************************************************
* ClearDataBuffers
*
* stops GetData writing values to memory that has been released
****************************************************************************/
PICO_STATUS clearDataBuffers(UNIT * unit)
{
	int32_t i;
	PICO_ACTION action_flag = (PICO_CLEAR_ALL | PICO_ADD);
	PICO_STATUS status = 0;

	for (i = 0; i < unit->channelCount; i++) 
	{
		if(unit->channelSettings[i].enabled)
		{
			if ( (status = ps6000aSetDataBuffers(unit->handle, (PICO_CHANNEL) i, NULL, NULL, 0, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, action_flag)) != PICO_OK)
			{
				printf("clearDataBuffers:ps6000aSetDataBuffers(channel %d) ------ 0x%08lx \n", i, status);
			}
			action_flag = PICO_ADD;
		}
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
* - etsModeSet : a flag to indicate if ETS mode has been set on the device
****************************************************************************/
void blockDataHandler(UNIT* unit, int8_t* text, int32_t offset, int16_t etsModeSet)
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
				if(status != PICO_OK)
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
			status ==  PICO_CHANNEL_COMBINATION_NOT_VALID_IN_THIS_RESOLUTION)
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

		printf("\nTimebase: %lu  SampleInterval: %le ns\n", timebase, timeInterval);


	/* Start it collecting, then wait for completion*/
	g_ready = FALSE;

	do
	{
		retry = 0;

		status = ps6000aRunBlock(unit->handle, 0, sampleCount, timebase, &timeIndisposed, 0, callBackBlock, NULL);

		if (status != PICO_OK)
		{
				printf("BlockDataHandler:ps6000aRunBlock ------ 0x%08lx \n", status);
				return;
		}
	} while (retry);

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
					{
						printf("  %6d     ", scaleVoltages ?
							adc_to_mv(buffers[j * 2][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit)	// If scaleVoltages, print mV value
							: buffers[j * 2][i]);																	// else print ADC Count
					}
				}

				printf("\n");
			}

			sampleCount = min(sampleCount, BUFFER_SIZE);

			fopen_s(&fp, blockFile, "w");

			if (fp != NULL)
			{
				fprintf(fp, "Block Data log\n\n");

				fprintf(fp, "Results shown for each of the %d Channels are......\n", unit->channelCount);
				fprintf(fp, "Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

				fprintf(fp, "Time (ns) ");
				
				for (i = 0; i < unit->channelCount; i++)
				{
					if (unit->channelSettings[i].enabled)
					{
						fprintf(fp, " Ch    Max ADC   Max mV  Min ADC   Min mV   ");
					}
				}
				fprintf(fp, "\n");

				for (i = 0; i < sampleCount; i++)
				{
						fprintf(fp, "%I64u ", g_times[0] + (uint64_t)(i * timeInterval));


					for (j = 0; j < unit->channelCount; j++)
					{
						if (unit->channelSettings[j].enabled)
						{
							fprintf(fp,
								"Ch%C  %6d = %+6dmV, %6d = %+6dmV   ",
								'A' + j,
								buffers[j * 2][i],
								adc_to_mv(buffers[j * 2][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit),
								buffers[j * 2 + 1][i],
								adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit));
						}
					}

					fprintf(fp, "\n");
				}

			}
			else
			{
				printf("Cannot open the file %s for writing.\n"
					"Please ensure that you have permission to access the file.\n", blockFile);
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
void collectBlockImmediate(UNIT* unit)
{
	PICO_STATUS status = PICO_OK;

	printf("Collect block immediate...\n");
	printf("Press a key to start\n");
	_getch();

	setDefaults(unit);

	/* Trigger disabled	*/
	status = ps6000aSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

	blockDataHandler(unit, (int8_t*)"First 10 readings\n", 0, FALSE);
}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void set_info(UNIT * unit)
{
	int8_t description [11][25]= { "Driver Version",
		"USB Version",
		"Hardware Version",
		"Variant Info",
		"Serial",
		"Cal Date",
		"Kernel Version",
		"Digital HW Version",
		"Analogue HW Version",
		"Firmware 1",
		"Firmware 2"};

	int16_t i = 0;
	int16_t requiredSize = 0;
	int8_t line [80];
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
			status = ps6000aGetUnitInfo(unit->handle, line, sizeof (line), &requiredSize, i);

			// info = 3 - PICO_VARIANT_INFO
			if (i == PICO_VARIANT_INFO) 
			{
				variant = atoi(line);
				memcpy(&(unit->modelString), line, sizeof(unit->modelString)==5?5:sizeof(unit->modelString));

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
	}
}

/****************************************************************************
* Select input voltage ranges for channels
****************************************************************************/
void setVoltages(UNIT * unit)
{
	PICO_STATUS status = PICO_OK;
	PICO_DEVICE_RESOLUTION resolution = PICO_DR_8BIT;

	int32_t i, ch;
	int32_t count = 0;
	int16_t numValidChannels = unit->channelCount;
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
					unit->channelSettings[ch].range = PICO_X1_PROBE_20V -1; //max range x1
				}
			}
			printf(count == 0? "\n** At least 1 channel must be enabled **\n\n":"");
		}
		while (count == 0);	// must have at least one channel enabled

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
	}
	while (retry == TRUE);

	setDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
* setTimebase
* Select timebase, set time units as nano seconds
*
****************************************************************************/
void setTimebase(UNIT * unit)
{
	PICO_STATUS status = PICO_OK;
	PICO_STATUS powerStatus = PICO_OK;
	double timeInterval;
	uint64_t maxSamples;
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
		return;
	}

	printf("Shortest timebase index available %d (%.9f seconds).\n", shortestTimebase, timeIntervalSeconds);
	
	printf("Specify desired timebase: ");
	fflush(stdin);
	scanf_s("%lud", &timebase);

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

	}
	while (status != PICO_OK);

	printf("Timebase used %lu = %le ns sample interval\n", timebase, timeInterval);
}

/****************************************************************************
* printResolution
*
* Outputs the resolution in text format to the console window
****************************************************************************/
void printResolution(PICO_DEVICE_RESOLUTION* resolution)
{
	switch(*resolution)
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
			printf("ADC Resolution Unknown!");
			break;
	}

	printf("\n");
}

/****************************************************************************
* setResolution
* Set resolution for the device
*
****************************************************************************/
void setResolution(UNIT * unit)
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
	for(i = 0; i < unit->channelCount; i++)
	{
		if(unit->channelSettings[i].enabled == TRUE)
		{
			numEnabledChannels++;
		}
	}

	if(numEnabledChannels == 0)
	{
		printf("setResolution: Please enable channels.\n");
		return;
	}

	status = ps6000aGetDeviceResolution(unit->handle, &resolution);

	if(status == PICO_OK)
	{
		printf("Current resolution: ");
		printResolution(&resolution);
	}
	else
	{
		printf("setResolution:ps6000aGetDeviceResolution ------ 0x%08lx \n", status);
		printf("Check the number and pairs of channels enabled. (Try A, E instead of A, B)\n");
		printf("Check Max. timebase for Resolution\n");
		printf("Is this a FlexRes Model?\n");
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
		else if(newResolution < PICO_DR_8BIT && newResolution > PICO_DR_10BIT)
		{
			printf("setResolution: Resolution index selected out of bounds.\n");
		}
		else
		{
			retry = FALSE;
		}	
	}
	while(retry);
	
	printf("\n");

	status = ps6000aSetDeviceResolution(unit->handle, (PICO_DEVICE_RESOLUTION) newResolution);

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
		printf("Check the number and pairs of channels enabled. (Try A, E instead of A, B)\n");
		printf("Check Max. timebase for Resolution\n");
		printf("Is this a FlexRes Model?\n");
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
void displaySettings(UNIT *unit)
{
	int32_t ch;
	int32_t voltage;
	PICO_STATUS status = PICO_OK;
	PICO_DEVICE_RESOLUTION resolution = PICO_DR_8BIT;

	printf("\nReadings will be scaled in %s\n", (scaleVoltages)? ("millivolts") : ("ADC counts"));
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
PICO_STATUS openDevice(UNIT *unit, int8_t *serial)
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

	unit->openStatus = (int16_t) status;
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
PICO_STATUS handleDevice(UNIT * unit)
{
	int16_t value = 0;
	int32_t i;
	PICO_STATUS status;

	printf("Handle: %d\n", unit->handle);
	
	if (unit->openStatus != PICO_OK)
	{
		printf("Unable to open device\n");
		printf("Error code : 0x%08x\n", (uint32_t) unit->openStatus);
		while(!_kbhit());
		exit(99); // exit program
	}

	printf("Device opened successfully, cycle %d\n\n", ++cycles);
	
	// Setup device info - unless it's set already
	if (unit->model == MODEL_NONE)
	{
		set_info(unit);
	}

	// Turn off any digital ports
	if (unit->digitalPortCount > 0)
	{
		printf("Turning off digital ports.");

		for (i = 0; i < unit->digitalPortCount; i++)
		{
			status = ps6000aSetDigitalPortOff(unit->handle, (PICO_CHANNEL)(i + PICO_PORT0) );
		}
	}
	
	timebase = 1;

	status = ps6000aGetAdcLimits(unit->handle, PICO_DR_8BIT, NULL, &value);
	unit->maxADCValue = value;

	for (i = 0; i < unit->channelCount; i++)
	{
		unit->channelSettings[i].enabled = TRUE;
		unit->channelSettings[i].DCcoupled = PICO_DC_50OHM; //options: PICO_DC_50OHM, PICO_AC, PICO_DC (1Mohm)
		unit->channelSettings[i].range = PICO_X1_PROBE_500MV;
		unit->channelSettings[i].analogueOffset = 0.0f;
	}

	setDefaults(unit);

	/* Trigger disabled	*/
	status = ps6000aSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);

	return unit->openStatus;
}

/****************************************************************************
* closeDevice 
****************************************************************************/
void closeDevice(UNIT *unit)
{
	ps6000aCloseUnit(unit->handle);
}

/****************************************************************************
* mainMenu
* Controls default functions of the seelected unit
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void mainMenu(UNIT *unit)
{
	int8_t ch = '.';
	while (ch != 'X')
	{
		displaySettings(unit);

		printf("\n\n");
		printf("Please select operation:\n\n");

		printf("B - Immediate block                           V - Set voltages\n");
		printf("                                              I - Set timebase\n");
		printf("                                              A - ADC counts/mV\n");		
		printf("                                              D - Set resolution\n");
		printf("                                              X - Exit\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");

		switch (ch) 
		{
			case 'B':
				collectBlockImmediate(unit);
				break;

			case 'V':
				setVoltages(unit);
				break;

			case 'I':
				setTimebase(unit);
				break;

			case 'A':
				scaleVoltages = !scaleVoltages;
				break;

			case 'D':
				setResolution(unit);
				break;

			case 'X':
				break;

			default:
				printf("Invalid operation\n");
				break;
		}
	}
}


/****************************************************************************
* main
*
***************************************************************************/
int32_t main(void)
{
	int8_t ch;
	uint16_t devCount = 0, listIter = 0,	openIter = 0;
	//device indexer -  64 chars - 64 is maximum number of picoscope devices handled by driver
	int8_t devChars[] =
			"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#";
	PICO_STATUS status = PICO_OK;
	UNIT allUnits[MAX_PICO_DEVICES];

	printf("PicoScope 6000 Series (ps6000a) Driver Example Program\n");
	printf("\nEnumerating Units...\n");

	do
	{
		status = openDevice(&(allUnits[devCount]), NULL);
		
		if (status == PICO_OK)
		{
			allUnits[devCount++].openStatus = (int16_t) status;
		}

	} while(status != PICO_NOT_FOUND);

	if (devCount == 0)
	{
		printf("Picoscope devices not found\n");
		return 1;
	}

	// if there is only one device, open and handle it here
	if (devCount == 1)
	{
		printf("Found one device, opening...\n\n");
		status = allUnits[0].openStatus;

		if (status == PICO_OK )
		{
			set_info(&allUnits[0]);
			status = handleDevice(&allUnits[0]);
		}

		if (status != PICO_OK)
		{
			printf("Picoscope devices open failed, error code 0x%x\n",(uint32_t)status);
			return 1;
		}

		mainMenu(&allUnits[0]);
		closeDevice(&allUnits[0]);
		printf("Exit...\n");
		return 0;
	}
	else
	{
		// More than one unit
		printf("Found %d devices, initializing...\n\n", devCount);

		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (allUnits[listIter].openStatus == PICO_OK )
			{
				set_info(&allUnits[listIter]);
				openIter++;
			}
		}
	}
	
	// None
	if (openIter == 0)
	{
		printf("Picoscope devices init failed\n");
		return 1;
	}
	// Just one - handle it here
	if (openIter == 1)
	{
		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (!(allUnits[listIter].openStatus == PICO_OK ))
			{
				break;
			}
		}
		
		printf("One device opened successfully\n");
		printf("Model\t: %s\nS/N\t: %s\n", allUnits[listIter].modelString, allUnits[listIter].serial);
		status = handleDevice(&allUnits[listIter]);
		
		if (status != PICO_OK)
		{
			printf("Picoscope device open failed, error code 0x%x\n", (uint32_t)status);
			return 1;
		}
		
		mainMenu(&allUnits[listIter]);
		closeDevice(&allUnits[listIter]);
		printf("Exit...\n");
		return 0;
	}
	printf("Found %d devices, pick one to open from the list:\n", devCount);

	for (listIter = 0; listIter < devCount; listIter++)
	{
		printf("%c) Picoscope %7s S/N: %s\n", devChars[listIter],
				allUnits[listIter].modelString, allUnits[listIter].serial);
	}

	printf("ESC) Cancel\n");

	ch = '.';
	
	// If escape
	while (ch != 27)
	{
		ch = _getch();
		
		// If escape
		if (ch == 27)
			continue;
		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (ch == devChars[listIter])
			{
				printf("Option %c) selected, opening Picoscope %7s S/N: %s\n",
						devChars[listIter], allUnits[listIter].modelString,
						allUnits[listIter].serial);
				
				if ((allUnits[listIter].openStatus == PICO_OK ))
				{
					status = handleDevice(&allUnits[listIter]);
				}
				
				if (status != PICO_OK)
				{
					printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status);
					return 1;
				}

				mainMenu(&allUnits[listIter]);

				printf("Found %d devices, pick one to open from the list:\n",devCount);
				
				for (listIter = 0; listIter < devCount; listIter++)
				{
					printf("%c) Picoscope %7s S/N: %s\n", devChars[listIter],
							allUnits[listIter].modelString,
							allUnits[listIter].serial);
				}
				
				printf("ESC) Cancel\n");
			}
		}
	}

	for (listIter = 0; listIter < devCount; listIter++)
	{
		closeDevice(&allUnits[listIter]);
	}

	printf("Exit...\n");
	
	return 0;
}
