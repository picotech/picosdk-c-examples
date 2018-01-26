/*******************************************************************************
 *
 * Filename: ps3000acon.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use the
 *   PicoScope 3000 Series (ps3000a) driver functions.
 *
 *	Supported PicoScope models:
 *
 *		PicoScope 3204A/B/D
 *		PicoScope 3205A/B/D
 *		PicoScope 3206A/B/D
 *		PicoScope 3207A/B
 *		PicoScope 3204 MSO & D MSO
 *		PicoScope 3205 MSO & D MSO
 *		PicoScope 3206 MSO & D MSO
 *		PicoScope 3404A/B/D/D MSO
 *		PicoScope 3405A/B/D/D MSO
 *		PicoScope 3406A/B/D/D MSO
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *	  Collect a block of samples using Equivalent Time Sampling (ETS)
 *    Collect samples using a rapid block capture with trigger
 *    Collect a stream of data immediately
 *    Collect a stream of data when a trigger event occurs
 *    Set Signal Generator, using standard or custom signals
 *    Change timebase & voltage scales
 *    Display data in mV or ADC counts
 *    Handle power source changes (PicoScope 34XXA/B, 32XX D/D MSO, & 
 *		34XX D/D MSO devices only)
 *
 * Digital Examples (MSO variants only):  
 *    Collect a block of digital samples immediately
 *    Collect a block of digital samples when a trigger event occurs
 *    Collect a block of analogue & digital samples when analogue AND digital trigger events occurs
 *    Collect a block of analogue & digital samples when analogue OR digital trigger events occurs
 *    Collect a stream of digital data immediately
 *    Collect a stream of digital data and show aggregated values
 *
 *
 *	To build this application:
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps3000a.lib can be located
 *			Ensure that the ps3000aApi.h and PicoStatus.h files can be located
 *		
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add 32-/64-bit ps3000a.lib to the project
 *			 Add ps3000aApi.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps3000a driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the ps3000acon application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2011 - 2017 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "ps3000aApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <libps3000a-1.1/ps3000aApi.h>
#ifndef PICO_STATUS
#include <libps3000a-1.1/PicoStatus.h>
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

#define PREF4 __stdcall

int32_t cycles = 0;

#define BUFFER_SIZE 	1024

#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

// AWG Parameters

#define AWG_DAC_FREQUENCY			20e6		
#define AWG_DAC_FREQUENCY_PS3207B	100e6
#define	AWG_PHASE_ACCUMULATOR		4294967296.0

typedef enum
{
	ANALOGUE,
	DIGITAL,
	AGGREGATED,
	MIXED
}MODE;


typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
}CHANNEL_SETTINGS;

typedef enum
{
	SIGGEN_NONE = 0,
	SIGGEN_FUNCTGEN = 1,
	SIGGEN_AWG = 2
} SIGGEN_TYPE;

typedef struct tTriggerDirections
{
	PS3000A_THRESHOLD_DIRECTION channelA;
	PS3000A_THRESHOLD_DIRECTION channelB;
	PS3000A_THRESHOLD_DIRECTION channelC;
	PS3000A_THRESHOLD_DIRECTION channelD;
	PS3000A_THRESHOLD_DIRECTION ext;
	PS3000A_THRESHOLD_DIRECTION aux;
}TRIGGER_DIRECTIONS;

typedef struct tPwq
{
	PS3000A_PWQ_CONDITIONS_V2 * conditions;
	int16_t nConditions;
	PS3000A_THRESHOLD_DIRECTION direction;
	uint32_t lower;
	uint32_t upper;
	PS3000A_PULSE_WIDTH_TYPE type;
}PWQ;

typedef struct
{
	int16_t					handle;
	int8_t					model[8];
	PS3000A_RANGE			firstRange ;
	PS3000A_RANGE			lastRange;
	int16_t					channelCount;
	int16_t					maxValue;
	int16_t					sigGen;
	int16_t					ETS;
	int32_t					AWGFileSize;
	CHANNEL_SETTINGS		channelSettings [PS3000A_MAX_CHANNELS];
	int16_t					digitalPorts;
}UNIT;

uint32_t	timebase = 8;
int16_t     oversample = 1;
BOOL		scaleVoltages = TRUE;

uint16_t inputRanges [PS3000A_MAX_RANGES] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};

BOOL     	g_ready = FALSE;
int32_t 	g_times [PS3000A_MAX_CHANNELS] = {0, 0, 0, 0};
int16_t     g_timeUnit;
int32_t     g_sampleCount;
uint32_t	g_startIndex;
int16_t		g_autoStopped;
int16_t		g_trig = 0;
uint32_t	g_trigAt = 0;

char BlockFile[20]		= "block.txt";
char DigiBlockFile[20]	= "digiBlock.txt";
char StreamFile[20]		= "stream.txt";

typedef struct tBufferInfo
{
	UNIT * unit;
	MODE mode;
	int16_t **driverBuffers;
	int16_t **appBuffers;
	int16_t **driverDigBuffers;
	int16_t **appDigBuffers;

} BUFFER_INFO;


/****************************************************************************
* Streaming callback
* Used by ps3000a data streaming collection calls, on receipt of data.
* Used to set global flags etc. checked by user routines
****************************************************************************/
void PREF4 callBackStreaming(	int16_t handle,
	int32_t		noOfSamples,
	uint32_t	startIndex,
	int16_t		overflow,
	uint32_t	triggerAt,
	int16_t		triggered,
	int16_t		autoStop,
	void		*pParameter)
{
	int32_t channel;
	BUFFER_INFO * bufferInfo = NULL;

	if (pParameter != NULL)
	{
		bufferInfo = (BUFFER_INFO *) pParameter;
	}

	// used for streaming
	g_sampleCount	= noOfSamples;
	g_startIndex	= startIndex;
	g_autoStopped	= autoStop;

	// flag to say done reading data
	g_ready = TRUE;

	// flags to show if & where a trigger has occurred
	g_trig = triggered;
	g_trigAt = triggerAt;

	if (bufferInfo != NULL && noOfSamples)
	{
		if (bufferInfo->mode == ANALOGUE)
		{
			for (channel = 0; channel < bufferInfo->unit->channelCount; channel++)
			{
				if (bufferInfo->unit->channelSettings[channel].enabled)
				{
					if (bufferInfo->appBuffers && bufferInfo->driverBuffers)
					{
						if (bufferInfo->appBuffers[channel * 2]  && bufferInfo->driverBuffers[channel * 2])
						{
							memcpy_s (&bufferInfo->appBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
								&bufferInfo->driverBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
						}
						if (bufferInfo->appBuffers[channel * 2 + 1] && bufferInfo->driverBuffers[channel * 2 + 1])
						{
							memcpy_s (&bufferInfo->appBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
								&bufferInfo->driverBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
						}
					}
				}
			}
		}
		else if (bufferInfo->mode == AGGREGATED)
		{
			for (channel = 0; channel < bufferInfo->unit->digitalPorts; channel++)
			{
				if (bufferInfo->appDigBuffers && bufferInfo->driverDigBuffers)
				{
					if (bufferInfo->appDigBuffers[channel * 2] && bufferInfo->driverDigBuffers[channel * 2])
					{
						memcpy_s (&bufferInfo->appDigBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverDigBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
					}
					if (bufferInfo->appDigBuffers[channel * 2 + 1] && bufferInfo->driverDigBuffers[channel * 2 + 1])
					{
						memcpy_s (&bufferInfo->appDigBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverDigBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
					}
				}
			}
		}
		else if (bufferInfo->mode == DIGITAL)
		{
			for (channel = 0; channel < bufferInfo->unit->digitalPorts; channel++)
			{
				if (bufferInfo->appDigBuffers && bufferInfo->driverDigBuffers)
				{
					if (bufferInfo->appDigBuffers[channel] && bufferInfo->driverDigBuffers[channel])
					{
						memcpy_s (&bufferInfo->appDigBuffers[channel][startIndex], noOfSamples * sizeof(int16_t),
							&bufferInfo->driverDigBuffers[channel][startIndex], noOfSamples * sizeof(int16_t));
					}
				}
			}
		}
	}
}

/****************************************************************************
* Block Callback
* used by ps3000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackBlock( int16_t handle, PICO_STATUS status, void * pParameter)
{
	if (status != PICO_CANCELLED)
	{
		g_ready = TRUE;
	}
}

/****************************************************************************
* setDefaults - restore default settings
****************************************************************************/
void setDefaults(UNIT * unit)
{
	int32_t i;
	PICO_STATUS status;

	status = ps3000aSetEts(unit->handle, PS3000A_ETS_OFF, 0, 0, NULL);	// Turn off ETS
	printf(status?"SetDefaults:ps3000aSetEts------ 0x%08lx \n":"", status);

	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		status = ps3000aSetChannel(unit->handle, (PS3000A_CHANNEL)(PS3000A_CHANNEL_A + i),
			unit->channelSettings[PS3000A_CHANNEL_A + i].enabled,
			(PS3000A_COUPLING)unit->channelSettings[PS3000A_CHANNEL_A + i].DCcoupled,
			(PS3000A_RANGE)unit->channelSettings[PS3000A_CHANNEL_A + i].range, 0);

		printf(status?"SetDefaults:ps3000aSetChannel------ 0x%08lx \n":"", status);
	}
}

/****************************************************************************
* setDigitals - enable or disable Digital Channels
****************************************************************************/
PICO_STATUS setDigitals(UNIT *unit, int16_t state)
{
	PICO_STATUS status;

	int16_t logicLevel;
	float logicVoltage = 1.5;
	int16_t maxLogicVoltage = 5;

	int16_t timebase = 1;
	int16_t port;


	// Set logic threshold
	logicLevel =  (int16_t) ((logicVoltage / maxLogicVoltage) * PS3000A_MAX_LOGIC_LEVEL);

	// Enable Digital ports
	for (port = PS3000A_DIGITAL_PORT0; port <= PS3000A_DIGITAL_PORT1; port++)
	{
		status = ps3000aSetDigitalPort(unit->handle, (PS3000A_DIGITAL_PORT)port, state, logicLevel);
		printf(status?"SetDigitals:PS3000ASetDigitalPort(Port 0x%X) ------ 0x%08lx \n":"", port, status);
	}

	return status;
}

/****************************************************************************
* disableAnalogue - Disable Analogue Channels
****************************************************************************/
PICO_STATUS disableAnalogue(UNIT *unit)
{
	PICO_STATUS status;
	int16_t ch;

	// Turn off analogue channels, keeping settings
	for (ch = 0; ch < unit->channelCount; ch++)
	{
		unit->channelSettings[ch].enabled = FALSE;

		status = ps3000aSetChannel(unit->handle, (PS3000A_CHANNEL) ch, unit->channelSettings[ch].enabled, (PS3000A_COUPLING) unit->channelSettings[ch].DCcoupled, 
			(PS3000A_RANGE) unit->channelSettings[ch].range, 0);
		
		if(status != PICO_OK)
		{
			printf("disableAnalogue:ps3000aSetChannel(channel %d) ------ 0x%08lx \n", ch, status);
		}
	}
	return status;
}

/****************************************************************************
* restoreAnalogueSettings - Restores Analogue Channel settings
****************************************************************************/
PICO_STATUS restoreAnalogueSettings(UNIT *unit)
{
	PICO_STATUS status;
	int16_t ch;

	// Turn on analogue channels using previous settings
	for (ch = 0; ch < unit->channelCount; ch++)
	{
		status = ps3000aSetChannel(unit->handle, (PS3000A_CHANNEL) ch, unit->channelSettings[ch].enabled, (PS3000A_COUPLING) unit->channelSettings[ch].DCcoupled, 
			(PS3000A_RANGE) unit->channelSettings[ch].range, 0);

		if(status != PICO_OK)
		{
			printf("restoreAnalogueSettings:ps3000aSetChannel(channel %d) ------ 0x%08lx \n", ch, status);
		}
	}
	return status;
}


/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t ch, UNIT * unit)
{
	return (raw * inputRanges[ch]) / unit->maxValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t ch, UNIT * unit)
{
	return (mv * unit->maxValue) / inputRanges[ch];
}

/****************************************************************************************
* changePowerSource - function to handle switches between +5 V supply, and USB-only power
* Only applies to PicoScope 34xxA/B/D/D MSO units 
******************************************************************************************/
PICO_STATUS changePowerSource(int16_t handle, PICO_STATUS status)
{
	char ch;

	switch (status)
	{
		case PICO_POWER_SUPPLY_NOT_CONNECTED:			// User must acknowledge they want to power via USB

			do
			{
				printf("\n5 V power supply not connected");
				printf("\nDo you want to run using USB only Y/N?\n");
				ch = toupper(_getch());
				if(ch == 'Y')
				{
					printf("\nPowering the unit via USB\n");
					status = ps3000aChangePowerSource(handle, PICO_POWER_SUPPLY_NOT_CONNECTED);		// Tell the driver that's ok

					if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
					{
						status = changePowerSource(handle, status);
					}
				}
			}
			while(ch != 'Y' && ch != 'N');

			printf(ch == 'N'?"Please use the +5 V power supply to power this unit\n":"");
			break;

		case PICO_POWER_SUPPLY_CONNECTED:

			printf("\nUsing +5V power supply voltage\n");
			status = ps3000aChangePowerSource(handle, PICO_POWER_SUPPLY_CONNECTED);	// Tell the driver we are powered from +5 V supply
			break;

		case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:

			printf("\nUSB 3.0 device on non-USB 3.0 port.\n");
			status = ps3000aChangePowerSource(handle, PICO_USB3_0_DEVICE_NON_USB3_0_PORT);	// Tell the driver that it is ok for the device to be on a non-USB 3.0 port
				
			break;

		case PICO_POWER_SUPPLY_UNDERVOLTAGE:

			do
			{
				printf("\nUSB not supplying required voltage");
				printf("\nPlease plug in the +5 V power supply\n");
				printf("\nHit any key to continue, or Esc to exit...\n");
				ch = _getch();

				if (ch == 0x1B)	// ESC key
				{
					exit(0);
				}
				else
				{
					status = ps3000aChangePowerSource(handle, PICO_POWER_SUPPLY_CONNECTED);		// Tell the driver that's ok
				}
			}
			while (status == PICO_POWER_SUPPLY_REQUEST_INVALID);
			break;

	}
	return status;
}

/****************************************************************************
* clearDataBuffers
*
* stops GetData writing values to memory that has been released
****************************************************************************/
PICO_STATUS clearDataBuffers(UNIT * unit)
{
	int32_t i;
	PICO_STATUS status;

	for (i = 0; i < unit->channelCount; i++) 
	{
		if((status = ps3000aSetDataBuffers(unit->handle, (PS3000A_CHANNEL) i, NULL, NULL, 0, 0, PS3000A_RATIO_MODE_NONE)) != PICO_OK)
		{
			printf("ClearDataBuffers:ps3000aSetDataBuffers(channel %d) ------ 0x%08lx \n", i, status);
		}
	}


	for (i= 0; i < unit->digitalPorts; i++) 
	{
		if((status = ps3000aSetDataBuffer(unit->handle, (PS3000A_CHANNEL) (i + PS3000A_DIGITAL_PORT0), NULL, 0, 0, PS3000A_RATIO_MODE_NONE))!= PICO_OK)
		{
			printf("ClearDataBuffers:ps3000aSetDataBuffer(port 0x%X) ------ 0x%08lx \n", i + PS3000A_DIGITAL_PORT0, status);
		}
	}

	return status;
}

/****************************************************************************
* blockDataHandler
* - Used by all block data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all to block.txt
* Input :
* - unit : the unit to use.
* - text : the text to display before the display of data slice
* - offset : the offset into the data buffer to start the display's slice.
****************************************************************************/
void blockDataHandler(UNIT * unit, char * text, int32_t offset, MODE mode)
{
	int16_t retry;
	int16_t bit;

	uint16_t bitValue;
	uint16_t digiValue;
	
	int16_t * buffers[PS3000A_MAX_CHANNEL_BUFFERS];
	int16_t * digiBuffer[PS3000A_MAX_DIGITAL_PORTS];

	int32_t i, j;
	int32_t timeInterval;
	int32_t sampleCount = BUFFER_SIZE;
	int32_t maxSamples;
	int32_t timeIndisposed;

	FILE * fp = NULL;
	FILE * digiFp = NULL;
	
	PICO_STATUS status;
	PS3000A_RATIO_MODE ratioMode = PS3000A_RATIO_MODE_NONE;

	if (mode == ANALOGUE || mode == MIXED)		// Analogue or (MSO Only) MIXED 
	{
		for (i = 0; i < unit->channelCount; i++) 
		{
			if(unit->channelSettings[i].enabled)
			{
				buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
				buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

				status = ps3000aSetDataBuffers(unit->handle, (PS3000A_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, 0, ratioMode);

				printf(status?"BlockDataHandler:ps3000aSetDataBuffers(channel %d) ------ 0x%08lx \n":"", i, status);
			}
		}
	}

	if (mode == DIGITAL || mode == MIXED)		// (MSO Only) Digital or MIXED
	{
		for (i= 0; i < unit->digitalPorts; i++) 
		{
			digiBuffer[i] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

			status = ps3000aSetDataBuffer(unit->handle, (PS3000A_CHANNEL) (i + PS3000A_DIGITAL_PORT0), digiBuffer[i], sampleCount, 0, ratioMode);

			printf(status?"BlockDataHandler:ps3000aSetDataBuffer(port 0x%X) ------ 0x%08lx \n":"", i + PS3000A_DIGITAL_PORT0, status);
		}
	}

	/* Find the maximum number of samples and the time interval (in nanoseconds) */
	while (ps3000aGetTimebase(unit->handle, timebase, sampleCount, &timeInterval, oversample, &maxSamples, 0))
	{
		timebase++;
	}

	printf("\nTimebase: %lu  Sample interval: %ld ns \n", timebase, timeInterval);

	/* Start the device collecting, then wait for completion*/
	g_ready = FALSE;


	do
	{
		retry = 0;

		status = ps3000aRunBlock(unit->handle, 0, sampleCount, timebase, oversample, &timeIndisposed, 0, callBackBlock, NULL);

		if (status != PICO_OK)
		{
			if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED || 
				status == PICO_POWER_SUPPLY_UNDERVOLTAGE)       // PicoScope 340XA/B/D/D MSO devices...+5 V PSU connected or removed
			{
				status = changePowerSource(unit->handle, status);
				retry = 1;
			}
			else
			{
				printf("BlockDataHandler:ps3000aRunBlock ------ 0x%08lx \n", status);
				return;
			}
		}
	}
	while (retry);

	printf("Waiting for trigger...Press a key to abort\n");

	while (!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (g_ready) 
	{
		status = ps3000aGetValues(unit->handle, 0, (uint32_t*) &sampleCount, 1, ratioMode, 0, NULL);

		if (status != PICO_OK)
		{
			if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED || status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
			{
				if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
				{
					changePowerSource(unit->handle, status);
				}
				else
				{
					printf("\nPower Source Changed. Data collection aborted.\n");
				}
			}
			else
			{
				printf("BlockDataHandler:ps3000aGetValues ------ 0x%08lx \n", status);
			}
		}
		else
		{
			/* Print out the first 10 readings, converting the readings to mV if required */
			printf("%s\n",text);

			if (mode == ANALOGUE || mode == MIXED)		// If we are capturing analogue or MIXED (analogue + digital) data
			{
				printf("Channels are in %s\n\n", ( scaleVoltages ) ? ("mV") : ("ADC Counts"));

				for (j = 0; j < unit->channelCount; j++) 
				{
					if (unit->channelSettings[j].enabled) 
					{
						printf("Channel %c:    ", 'A' + j);
					}
				}

				printf("\n");
			}

			if (mode == DIGITAL || mode == MIXED)	// If we are capturing digital or MIXED data 
			{
				printf("Digital\n");
			}

			printf("\n");

			for (i = offset; i < offset+10; i++) 
			{
				if (mode == ANALOGUE || mode == MIXED)	// If we are capturing analogue or MIXED data
				{
					for (j = 0; j < unit->channelCount; j++) 
					{
						if (unit->channelSettings[j].enabled) 
						{
							printf("  %d     ", scaleVoltages ? 
								adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS3000A_CHANNEL_A + j].range, unit)	// If scaleVoltages, print mV value
								: buffers[j * 2][i]);																	// else print ADC Count
						}
					}
				}
				if (mode == DIGITAL || mode == MIXED)	// If we're doing digital or MIXED
				{
					digiValue = 0x00ff & digiBuffer[1][i];	// Mask Port 1 values to get lower 8 bits
					digiValue <<= 8;						// Shift by 8 bits to place in upper 8 bits of 16-bit word
					digiValue |= digiBuffer[0][i];			// Mask Port 0 values to get lower 8 bits and apply bitwise inclusive OR to combine with Port 1 values 
					printf("0x%04X", digiValue);

				}
				printf("\n");
			}

			if (mode == ANALOGUE || mode == MIXED)		// If we're doing analogue or MIXED
			{
				sampleCount = min(sampleCount, BUFFER_SIZE);

				fopen_s(&fp, BlockFile, "w");

				if (fp != NULL)
				{
					fprintf(fp, "Block Data log\n\n");
					fprintf(fp,"Results shown for each of the %d Channels are......\n",unit->channelCount);
					fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

					fprintf(fp, "Time  ");

					for (i = 0; i < unit->channelCount; i++) 
					{
						if (unit->channelSettings[i].enabled) 
						{
							fprintf(fp," Ch   Max ADC   Max mV   Min ADC   Min mV   ");
						}
					}

					fprintf(fp, "\n");

					for (i = 0; i < sampleCount; i++) 
					{
						fprintf(fp, "%d ", g_times[0] + (int32_t)(i * timeInterval));

						for (j = 0; j < unit->channelCount; j++) 
						{
							if (unit->channelSettings[j].enabled) 
							{
								fprintf(	fp,
									"Ch%C  %d = %+dmV, %d = %+dmV   ",
									'A' + j,
									buffers[j * 2][i],
									adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS3000A_CHANNEL_A + j].range, unit),
									buffers[j * 2 + 1][i],
									adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PS3000A_CHANNEL_A + j].range, unit));
							}
						}
						fprintf(fp, "\n");
					}
				}
				else
				{
					printf(	"Cannot open the file %s for writing.\n"
						"Please ensure that you have permission to access.\n", BlockFile);
				}
			}

			// Output digital values to separate file
			if (mode == DIGITAL || mode == MIXED)
			{
				fopen_s(&digiFp, DigiBlockFile, "w");

				if (digiFp != NULL)
				{
					fprintf(digiFp, "Block Digital Data log\n");
					fprintf(digiFp, "Digital Channels will be in the order D15...D0\n");

					fprintf(digiFp, "\n");

					for (i = 0; i < sampleCount; i++) 
					{
						digiValue = 0x00ff & digiBuffer[1][i];	// Mask Port 1 values to get lower 8 bits
						digiValue <<= 8;						// Shift by 8 bits to place in upper 8 bits of 16-bit word
						digiValue |= digiBuffer[0][i];			// Mask Port 0 values to get lower 8 bits and apply bitwise inclusive OR to combine with Port 1 values

						// Output data in binary form
						for (bit = 0; bit < 16; bit++)
						{
							// Shift value (32768 - binary 1000 0000 0000 0000), AND with value to get 1 or 0 for channel
							// Order will be D15 to D8, then D7 to D0

							bitValue = (0x8000 >> bit) & digiValue? 1 : 0;
							fprintf(digiFp, "%d, ", bitValue);
						}

						fprintf(digiFp, "\n");

					}
				}
			}

		} 
	}
	else 
	{
		printf("\nData collection aborted.\n");
		_getch();
	}

	if ((status = ps3000aStop(unit->handle)) != PICO_OK)
	{
		printf("BlockDataHandler:ps3000aStop ------ 0x%08lx \n", status);
	}

	if (fp != NULL)
	{
		fclose(fp);
	}

	if (digiFp != NULL)
	{
		fclose(digiFp);
	}

	if (mode == ANALOGUE || mode == MIXED)		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->channelCount; i++) 
		{
			if(unit->channelSettings[i].enabled)
			{
				free(buffers[i * 2]);
				free(buffers[i * 2 + 1]);

			}
		}
	}

	if (mode == DIGITAL || mode == MIXED)		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->digitalPorts; i++) 
		{
			free(digiBuffer[i]);
		}
	}
	
	clearDataBuffers(unit);
}

/****************************************************************************
* Stream Data Handler
* - Used by the two stream data examples - untriggered and triggered
* Inputs:
* - unit - the unit to sample on
* - preTrigger - the number of samples in the pre-trigger phase 
*					(0 if no trigger has been set)
***************************************************************************/
void streamDataHandler(UNIT * unit, uint32_t preTrigger, MODE mode)
{
	int16_t autostop;
	int16_t retry = 0;
	int16_t powerChange = 0;

	uint16_t portValue, portValueOR, portValueAND;

	int32_t i, j;
	int32_t bit;
	int32_t index = 0;
	int32_t totalSamples;

	uint32_t postTrigger;
	uint32_t sampleCount = 100000; /* Make sure buffer large enough */
	uint32_t sampleInterval;
	uint32_t downsampleRatio;
	uint32_t triggeredAt = 0;

	int16_t * buffers[PS3000A_MAX_CHANNEL_BUFFERS];
	int16_t * appBuffers[PS3000A_MAX_CHANNEL_BUFFERS];
	int16_t * digiBuffers[PS3000A_MAX_DIGITAL_PORTS];
	int16_t * appDigiBuffers[PS3000A_MAX_DIGITAL_PORTS];
	
	PICO_STATUS status;

	PS3000A_TIME_UNITS timeUnits;
	PS3000A_RATIO_MODE ratioMode;

	BUFFER_INFO bufferInfo;
	FILE * fp = NULL;


	if (mode == ANALOGUE)		// Analogue - collect raw data
	{
		for (i = 0; i < unit->channelCount; i++) 
		{
			if(unit->channelSettings[i].enabled)
			{
				buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
				buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

				status = ps3000aSetDataBuffers(unit->handle, (PS3000A_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, 0, PS3000A_RATIO_MODE_NONE);

				appBuffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
				appBuffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

				printf(status ? "StreamDataHandler:ps3000aSetDataBuffers(channel %ld) ------ 0x%08lx \n":"", i, status);
			}
		}

		downsampleRatio = 1;
		timeUnits = PS3000A_US;
		sampleInterval = 10;
		ratioMode = PS3000A_RATIO_MODE_NONE;
		postTrigger = 1000000;
		autostop = TRUE;
	}

	bufferInfo.unit = unit;
	bufferInfo.mode = mode;	
	bufferInfo.driverBuffers = buffers;
	bufferInfo.appBuffers = appBuffers;
	bufferInfo.driverDigBuffers = digiBuffers;
	bufferInfo.appDigBuffers = appDigiBuffers;

	if (mode == AGGREGATED)		// (MSO Only) AGGREGATED
	{
		for (i= 0; i < unit->digitalPorts; i++) 
		{
			digiBuffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			digiBuffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

			status = ps3000aSetDataBuffers(unit->handle, (PS3000A_CHANNEL) (i + PS3000A_DIGITAL_PORT0), digiBuffers[i * 2], digiBuffers[i * 2 + 1], sampleCount, 0, PS3000A_RATIO_MODE_AGGREGATE);

			appDigiBuffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			appDigiBuffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t)); 

			printf(status?"StreamDataHandler:ps3000aSetDataBuffer(channel %ld) ------ 0x%08lx \n":"", i, status);
		}

		downsampleRatio = 10;
		timeUnits = PS3000A_MS;
		sampleInterval = 10;
		ratioMode = PS3000A_RATIO_MODE_AGGREGATE;
		postTrigger = 10;
		autostop = FALSE;
	}

	if (mode == DIGITAL)		// (MSO Only) Digital 
	{
		for (i= 0; i < unit->digitalPorts; i++) 
		{
			digiBuffers[i] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			status = ps3000aSetDataBuffer(unit->handle, (PS3000A_CHANNEL) (i + PS3000A_DIGITAL_PORT0), digiBuffers[i], sampleCount, 0, PS3000A_RATIO_MODE_NONE);

			appDigiBuffers[i] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

			printf(status?"StreamDataHandler:ps3000aSetDataBuffer(channel %ld) ------ 0x%08lx \n":"", i, status);
		}

		downsampleRatio = 1;
		timeUnits = PS3000A_MS;
		sampleInterval = 10;
		ratioMode = PS3000A_RATIO_MODE_NONE;
		postTrigger = 10;
		autostop = FALSE;
	}

	if (autostop)
	{
		printf("\nStreaming Data for %lu samples", postTrigger / downsampleRatio);

		if (preTrigger)	// we pass 0 for preTrigger if we're not setting up a trigger
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
		printf("\nStreaming Data continually.\n\n");
	}

	g_autoStopped = FALSE;

	do
	{
		retry = 0;

		status = ps3000aRunStreaming(unit->handle, &sampleInterval, timeUnits, preTrigger, postTrigger, autostop, downsampleRatio,
			ratioMode, sampleCount);

		if(status != PICO_OK)
		{
			if(status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED || status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
			{
				status = changePowerSource(unit->handle, status);
				retry = 1;
			}
			else
			{
				printf("StreamDataHandler:ps3000aRunStreaming ------ 0x%08lx \n", status);
				return;
			}
		}
	}
	while(retry);

	printf("Streaming data...Press a key to stop\n");

	if (mode == ANALOGUE)
	{
		fopen_s(&fp, StreamFile, "w");

		if (fp != NULL)
		{
			fprintf(fp,"For each of the %d Channels, results shown are....\n",unit->channelCount);
			fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

			for (i = 0; i < unit->channelCount; i++) 
			{
				if (unit->channelSettings[i].enabled) 
				{
					fprintf(fp,"Ch  Max ADC  Max mV  Min ADC  Min mV   ");
				}
			}
			fprintf(fp, "\n");
		}
	}

	totalSamples = 0;

	while (!_kbhit() && !g_autoStopped)
	{
		// Register callback function with driver and check if data has been received
		g_ready = FALSE;

		status = ps3000aGetStreamingLatestValues(unit->handle, callBackStreaming, &bufferInfo);

		if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED || 
			status == PICO_POWER_SUPPLY_UNDERVOLTAGE) // PicoScope 340XA/B/D/D MSO devices...+5 V PSU connected or removed
		{
			if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
			{
				changePowerSource(unit->handle, status);
			}
			printf("\n\nPower Source Change");
			powerChange = 1;
		}

		index ++;

		if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
		{
			if (g_trig)
			{
				triggeredAt = totalSamples + g_trigAt;		// calculate where the trigger occurred in the total samples collected
			}

			totalSamples += g_sampleCount;

			printf("\nCollected %li samples, index = %lu, Total: %d samples ", g_sampleCount, g_startIndex, totalSamples);

			if (g_trig)
			{
				printf("Trig. at index %lu", triggeredAt);	// show where trigger occurred
			}

			for (i = g_startIndex; i < (int32_t)(g_startIndex + g_sampleCount); i++) 
			{
				if (mode == ANALOGUE)
				{
					if(fp != NULL)
					{
						for (j = 0; j < unit->channelCount; j++) 
						{
							if (unit->channelSettings[j].enabled) 
							{
								fprintf(	fp,
									"Ch%C  %d = %+dmV, %d = %+dmV   ",
									(char)('A' + j),
									appBuffers[j * 2][i],
									adc_to_mv(appBuffers[j * 2][i], unit->channelSettings[PS3000A_CHANNEL_A + j].range, unit),
									appBuffers[j * 2 + 1][i],
									adc_to_mv(appBuffers[j * 2 + 1][i], unit->channelSettings[PS3000A_CHANNEL_A + j].range, unit));
							}
						}

						fprintf(fp, "\n");
					}
					else
					{
						printf("Cannot open the file %s for writing.\n", StreamFile);
					}
				}

				if (mode == DIGITAL)
				{
					portValue = 0x00ff & appDigiBuffers[1][i];	// Mask Port 1 values to get lower 8 bits
					portValue <<= 8;							// Shift by 8 bits to place in upper 8 bits of 16-bit word
					portValue |= 0x00ff & appDigiBuffers[0][i]; // Mask Port 0 values to get lower 8 bits and apply bitwise inclusive OR to combine with Port 1 values  

					printf("\nIndex=%04lu: Value = 0x%04X  =  ",i, portValue);

					// Shift value (32768 - binary 1000 0000 0000 0000), AND with value to get 1 or 0 for channel
					// Order will be D15 to D8, then D7 to D0
					for (bit = 0; bit < 16; bit++)
					{
						printf( (0x8000 >> bit) & portValue? "1 " : "0 ");
					}
				}

				if (mode == AGGREGATED)
				{
					portValueOR = 0x00ff & appDigiBuffers[2][i];
					portValueOR <<= 8;
					portValueOR |= 0x00ff & appDigiBuffers[0][i];

					portValueAND = 0x00ff & appDigiBuffers[3][i];
					portValueAND <<= 8;
					portValueAND |= 0x00ff & appDigiBuffers[1][i];

					printf("\nIndex=%04lu: Bitwise  OR of last %ld readings = 0x%04X ",i,  downsampleRatio, portValueOR);
					printf("\nIndex=%04lu: Bitwise AND of last %ld readings = 0x%04X ",i,  downsampleRatio, portValueAND);
				}
			}
		}
	}

	ps3000aStop(unit->handle);

	if (!g_autoStopped && !powerChange)  
	{
		printf("\nData collection aborted.\n");
		_getch();
	}

	if(fp != NULL) 
	{
		fclose(fp);	
	}

	if (mode == ANALOGUE)		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->channelCount; i++) 
		{
			if(unit->channelSettings[i].enabled)
			{
				free(buffers[i * 2]);
				free(buffers[i * 2 + 1]);

				free(appBuffers[i * 2]);
				free(appBuffers[i * 2 + 1]);
			}
		}
	}

	if (mode == DIGITAL) 		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->digitalPorts; i++) 
		{
			free(digiBuffers[i]);
			free(appDigiBuffers[i]);
		}

	}

	if (mode == AGGREGATED) 		// Only if we allocated these buffers
	{
		for (i = 0; i < unit->digitalPorts * 2; i++) 
		{
			free(digiBuffers[i]);
			free(appDigiBuffers[i]);
		}
	}

	clearDataBuffers(unit);
}



/****************************************************************************
* setTrigger
*
* - Used to call aall the functions required to set up triggering
*
***************************************************************************/
PICO_STATUS setTrigger(	UNIT * unit,

						struct tPS3000ATriggerChannelProperties * channelProperties,
							int16_t nChannelProperties,
							PS3000A_TRIGGER_CONDITIONS_V2 * triggerConditionsV2,
							int16_t nTriggerConditions,
							TRIGGER_DIRECTIONS * directions,

						struct tPwq * pwq,
							uint32_t delay,
							int16_t auxOutputEnabled,
							int32_t autoTriggerMs,
							PS3000A_DIGITAL_CHANNEL_DIRECTIONS * digitalDirections,
							int16_t nDigitalDirections)
{
	PICO_STATUS status;

	if ((status = ps3000aSetTriggerChannelProperties(unit->handle,
		channelProperties,
		nChannelProperties,
		auxOutputEnabled,
		autoTriggerMs)) != PICO_OK) 
	{
		printf("SetTrigger:ps3000aSetTriggerChannelProperties ------ Ox%08lx \n", status);
		return status;
	}

	if ((status = ps3000aSetTriggerChannelConditionsV2(unit->handle, triggerConditionsV2, nTriggerConditions)) != PICO_OK) 
	{
		printf("SetTrigger:ps3000aSetTriggerChannelConditions ------ 0x%08lx \n", status);
		return status;
	}

	if ((status = ps3000aSetTriggerChannelDirections(unit->handle,
		directions->channelA,
		directions->channelB,
		directions->channelC,
		directions->channelD,
		directions->ext,
		directions->aux)) != PICO_OK) 
	{
		printf("SetTrigger:ps3000aSetTriggerChannelDirections ------ 0x%08lx \n", status);
		return status;
	}

	if ((status = ps3000aSetTriggerDelay(unit->handle, delay)) != PICO_OK) 
	{
		printf("SetTrigger:ps3000aSetTriggerDelay ------ 0x%08lx \n", status);
		return status;
	}

	if((status = ps3000aSetPulseWidthQualifierV2(unit->handle, 
		pwq->conditions,
		pwq->nConditions, 
		pwq->direction,
		pwq->lower, 
		pwq->upper, 
		pwq->type)) != PICO_OK)
	{
		printf("SetTrigger:ps3000aSetPulseWidthQualifier ------ 0x%08lx \n", status);
		return status;
	}

	if (unit->digitalPorts)					
	{
		if((status = ps3000aSetTriggerDigitalPortProperties(unit->handle, digitalDirections, nDigitalDirections)) != PICO_OK) 
		{
			printf("SetTrigger:ps3000aSetTriggerDigitalPortProperties ------ 0x%08lx \n", status);
			return status;
		}
	}
	return status;
}

/****************************************************************************
* collectBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
void collectBlockImmediate(UNIT * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block immediate...\n");
	printf("Press a key to start\n");
	_getch();

	setDefaults(unit);

	/* Trigger disabled	*/
	setTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	blockDataHandler(unit, "\nFirst 10 readings:\n", 0, ANALOGUE);
}

/****************************************************************************
* collectBlockEts
*  this function demonstrates how to collect a block of
*  data using equivalent time sampling (ETS).
****************************************************************************/
void collectBlockEts(UNIT * unit)
{
	PICO_STATUS status;
	int32_t ets_sampletime;
	int16_t	triggerVoltage = mv_to_adc(1000,	unit->channelSettings[PS3000A_CHANNEL_A].range, unit);
	uint32_t delay = 0;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	struct tPS3000ATriggerChannelProperties sourceDetails = {	triggerVoltage,
																256 * 10,
																triggerVoltage,
																256 * 10,
																PS3000A_CHANNEL_A,
																PS3000A_LEVEL };

	struct tPS3000ATriggerConditionsV2 conditions = {	PS3000A_CONDITION_TRUE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE };


	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));
	directions.channelA = PS3000A_RISING;

	printf("Collect ETS block...\n");

	printf("Collects when value rises past %d", scaleVoltages? 
		adc_to_mv(sourceDetails.thresholdUpper,	unit->channelSettings[PS3000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count

	printf(scaleVoltages? "mV\n" : "ADC Counts\n");
	printf("Press a key to start...\n");
	_getch();

	setDefaults(unit);

	// Trigger enabled
	// Rising edge
	// Threshold = 1000mV
	status = setTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, delay, 0, 0, 0, 0);

	status = ps3000aSetEts(unit->handle, PS3000A_ETS_FAST, 20, 4, &ets_sampletime);
	printf("ETS Sample Time is: %ld\n", ets_sampletime);

	blockDataHandler(unit, "Ten readings after trigger:\n", BUFFER_SIZE / 10 - 5, ANALOGUE); // 10% of data is pre-trigger

	status = ps3000aSetEts(unit->handle, PS3000A_ETS_OFF, 0, 0, &ets_sampletime);
}

/****************************************************************************
* collectBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void collectBlockTriggered(UNIT * unit)
{

	int16_t triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS3000A_CHANNEL_A].range, unit);

	struct tPS3000ATriggerChannelProperties sourceDetails = {	triggerVoltage,
																256 * 10,
																triggerVoltage,
																256 * 10,
																PS3000A_CHANNEL_A,
																PS3000A_LEVEL};

	struct tPS3000ATriggerConditionsV2 conditions = {	PS3000A_CONDITION_TRUE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE,
														PS3000A_CONDITION_DONT_CARE};

	struct tPwq pulseWidth;

	struct tTriggerDirections directions = {	PS3000A_RISING,
												PS3000A_NONE,
												PS3000A_NONE,
												PS3000A_NONE,
												PS3000A_NONE,
												PS3000A_NONE };

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect block triggered...\n");
	printf("Collects when value rises past %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS3000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count

	printf(scaleVoltages?"mV\n" : "ADC Counts\n");

	printf("Press a key to start...\n");
	_getch();

	setDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */
	setTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	blockDataHandler(unit, "Ten readings after trigger:\n", 0, ANALOGUE);
}

/****************************************************************************
* collectRapidBlock
*  this function demonstrates how to collect a set of captures using 
*  rapid block mode.
****************************************************************************/
void collectRapidBlock(UNIT * unit)
{
	int16_t i;
	int16_t retry;
	int16_t channel;
	int16_t *overflow;
	int16_t ***rapidBuffers;

	int32_t nMaxSamples;
	int32_t timeIndisposed;
	int32_t timeIntervalNs;
	int32_t maxSamples;

	uint32_t nSegments;
	uint32_t nCaptures;
	uint32_t capture;
	uint32_t nSamples = 1000;
	uint32_t nCompletedCaptures;
	uint32_t maxSegments;

	PICO_STATUS status;
	
	// Set level trigger on Channel A

	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS3000A_CHANNEL_A].range, unit);

	struct tPS3000ATriggerChannelProperties sourceDetails = {	triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS3000A_CHANNEL_A,
		PS3000A_LEVEL};

	struct tPS3000ATriggerConditionsV2 conditions = {	PS3000A_CONDITION_TRUE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE};

	struct tPwq pulseWidth;

	struct tTriggerDirections directions = {	PS3000A_RISING,
												PS3000A_NONE,
												PS3000A_NONE,
												PS3000A_NONE,
												PS3000A_NONE,
												PS3000A_NONE };

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect rapid block triggered...\n");
	printf("Collects when value rises past %d",	scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS3000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count

	printf(scaleVoltages?"mV\n" : "ADC Counts\n");
	printf("Press any key to abort\n");

	setDefaults(unit);

	// Trigger enabled
	setTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	// Find the maximum number of segments
	status = ps3000aGetMaxSegments(unit->handle, &maxSegments);

	printf("Max. number of segments for device: %d\n", maxSegments);

	// Set the number of segments
	nSegments = 64;

	// Set the number of captures
	nCaptures = 10;

	// Segment the memory
	status = ps3000aMemorySegments(unit->handle, nSegments, &nMaxSamples);

	// Set the number of captures
	status = ps3000aSetNoOfCaptures(unit->handle, nCaptures);

	// Verify timebase index is valid, sample interval will be device dependent
	timebase = 10; 

	/* Find the maximum number of samples and the time interval (in nanoseconds) */

	status = PICO_INVALID_TIMEBASE;

	do 
	{
		status = ps3000aGetTimebase(unit->handle, timebase, nSamples, &timeIntervalNs, oversample, &maxSamples, 0);

		if (status != PICO_OK)
		{
			timebase++;
		}
	}
	while (status != PICO_OK);

	printf("\nTimebase: %lu  Sample interval: %ld ns\n Max samples per channel per segment: %ld\n", timebase, timeIntervalNs, maxSamples);

	printf("Starting data capture for %d waveforms...\n", nCaptures);

	do
	{
		retry = 0;

		status = ps3000aRunBlock(unit->handle, 0, nSamples, timebase, 1, &timeIndisposed, 0, callBackBlock, NULL);

		if(status != PICO_OK)
		{
			if(status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED)
			{
				status = changePowerSource(unit->handle, status);
				retry = 1;
			}
			else
			{
				printf("RapidBlockDataHandler:ps3000aRunBlock ------ 0x%08lx \n", status);
				printf("Press any key to continue (data collection will be aborted).\n");
			}
		}
	}
	while(retry);

	//Wait until data ready
	g_ready = 0;

	while(!g_ready && !_kbhit())
	{
		Sleep(0);
	}

	if (!g_ready)
	{
		_getch();
		status = ps3000aStop(unit->handle);
		status = ps3000aGetNoOfCaptures(unit->handle, &nCompletedCaptures);
		printf("Rapid capture aborted. %lu complete blocks were captured\n", nCompletedCaptures);
		printf("\nPress any key...\n\n");
		_getch();

		if(nCompletedCaptures == 0)
		{
			return;
		}

		// Only display the blocks that were captured
		nCaptures = nCompletedCaptures;
	}

	//Allocate memory

	rapidBuffers = (int16_t ***) calloc(unit->channelCount, sizeof(int16_t*));
	overflow = (int16_t *) calloc(unit->channelCount * nCaptures, sizeof(int16_t));

	for (channel = 0; channel < unit->channelCount; channel++) 
	{
		if(unit->channelSettings[channel].enabled)
		{
			rapidBuffers[channel] = (int16_t **) calloc(nCaptures, sizeof(int16_t*));
		}
		else
		{
			rapidBuffers[channel] = NULL;
		}
	}

	for (channel = 0; channel < unit->channelCount; channel++) 
	{	
		if(unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++) 
			{
				rapidBuffers[channel][capture] = (int16_t *) calloc(nSamples, sizeof(int16_t));
			}
		}
	}

	for (channel = 0; channel < unit->channelCount; channel++) 
	{
		if(unit->channelSettings[channel].enabled)
		{
			for (capture = 0; capture < nCaptures; capture++) 
			{
				status = ps3000aSetDataBuffer(unit->handle, (PS3000A_CHANNEL)channel, rapidBuffers[channel][capture], nSamples, capture, PS3000A_RATIO_MODE_NONE);
			}
		}
	}

	// Retrieve data
	status = ps3000aGetValuesBulk(unit->handle, &nSamples, 0, nCaptures - 1, 1, PS3000A_RATIO_MODE_NONE, overflow);

	if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
		printf("\nPower Source Changed. Data collection aborted.\n");
	}

	if (status == PICO_OK)
	{
		//print first 10 samples from each capture
		for (capture = 0; capture < nCaptures; capture++)
		{
			printf("\nCapture %d:-\n\n", capture + 1);
			
			for (channel = 0; channel < unit->channelCount; channel++) 
			{
				if(unit->channelSettings[channel].enabled)
				{
					printf("Channel %c:\t", 'A' + channel);
				}
			}

			printf("\n");

			for(i = 0; i < 10; i++)
			{
				for (channel = 0; channel < unit->channelCount; channel++) 
				{
					if(unit->channelSettings[channel].enabled)
					{
						printf("   %6d       ", scaleVoltages ? 
							adc_to_mv(rapidBuffers[channel][capture][i], unit->channelSettings[PS3000A_CHANNEL_A +channel].range, unit)	// If scaleVoltages, print mV value
							: rapidBuffers[channel][capture][i]);																		// else print ADC Count
					}
				}
				printf("\n");
			}
		}
	}

	// Stop
	status = ps3000aStop(unit->handle);

	//Free memory
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
		if (unit->channelSettings[channel].enabled)
		{
			free(rapidBuffers[channel]);
		}
	}

	free(rapidBuffers);

	// Set nunmber of segments and captures back to 1
	status = ps3000aMemorySegments(unit->handle, (uint32_t) 1, &nMaxSamples); 
	status = ps3000aSetNoOfCaptures(unit->handle, 1);
}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void get_info(UNIT * unit)
{
	char description [11][25]= { "Driver Version",
		"USB Version",
		"Hardware Version",
		"Variant Info",
		"Serial",
		"Cal Date",
		"Kernel",
		"Digital H/W",
		"Analogue H/W",
		"Firmware 1",
		"Firmware 2"};

	int16_t i, r = 0;
	int8_t line [80];
	
	// Variables used for arbitrary waveform parameters
	int16_t			minArbitraryWaveformValue = 0;
	int16_t			maxArbitraryWaveformValue = 0;
	uint32_t		minArbitraryWaveformSize = 0;
	uint32_t		maxArbitraryWaveformSize = 0;

	PICO_STATUS status = PICO_OK;

	//Initialise default unit properties and change when required
	unit->sigGen		= SIGGEN_FUNCTGEN;
	unit->firstRange	= PS3000A_50MV;
	unit->lastRange		= PS3000A_20V;
	unit->channelCount	= DUAL_SCOPE;
	unit->ETS			= FALSE;
	unit->AWGFileSize	= MIN_SIG_GEN_BUFFER_SIZE;
	unit->digitalPorts	= 0;

	if (unit->handle) 
	{
		for (i = 0; i < 11; i++) 
		{
			status = ps3000aGetUnitInfo(unit->handle, line, sizeof (line), &r, i);

			if (i == 3) 
			{
				memcpy(unit->model, line, strlen((char*)(line))+1);

				// If 4 (analogue) channel variant 
				if (line[1] == '4')
				{
					unit->channelCount = QUAD_SCOPE;
				}

				// If ETS mode is enabled
				if (strlen(line) == 8 || line[3] != '4')
				{
					unit->ETS	= TRUE;
				}

				
				if (line[4] != 'A')
				{
					// Set Signal generator type if the device is not an 'A' model
					unit->sigGen = SIGGEN_AWG;

					// PicoScope 3000D and 3000D MSO models have a lower range of +/- 20mV
					if (line[4] == 'D')
					{
						unit->firstRange = PS3000A_20MV;
					}
				}

				// Check if MSO device
				if (strlen(line) >= 7)
				{
					line[4] = toupper(line[4]);
					line[5] = toupper(line[5]);
					line[6] = toupper(line[6]);

					if(strcmp(line + 4, "MSO") == 0 || strcmp(line + 5, "MSO") == 0 )
					{
						unit->digitalPorts = 2;
						unit->sigGen = SIGGEN_AWG;
					}
				}

				// If device has Arbitrary Waveform Generator, find the maximum AWG buffer size
				if (unit->sigGen == SIGGEN_AWG)
				{
					ps3000aSigGenArbitraryMinMaxValues(unit->handle, &minArbitraryWaveformValue, &maxArbitraryWaveformValue, &minArbitraryWaveformSize, &maxArbitraryWaveformSize);
					unit->AWGFileSize = maxArbitraryWaveformSize;
				}
			}
			printf("%s: %s\n", description[i], line);
		}		
	}
}

/****************************************************************************
* setVoltages
* Select input voltage ranges for channels
****************************************************************************/
void setVoltages(UNIT * unit)
{
	int32_t i, ch;
	int32_t count = 0;

	/* See what ranges are available... */
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
				scanf_s("%hd", &unit->channelSettings[ch].range);
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
				unit->channelSettings[ch].range = PS3000A_MAX_RANGES-1;
			}
		}
		printf(count == 0? "\n** At least 1 channel must be enabled **\n\n":"");
	}
	while(count == 0);	// must have at least one channel enabled

	setDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
*
* Select timebase, set oversample to on and time units as nano seconds
*
****************************************************************************/
void setTimebase(UNIT unit)
{
	int32_t timeInterval = 0;
	int32_t maxSamples = 0;

	PICO_STATUS status = PICO_INVALID_TIMEBASE;

	printf("Specify desired timebase: ");
	fflush(stdin);
	scanf_s("%lud", &timebase);

	do
	{
		status = ps3000aGetTimebase(unit.handle, timebase, BUFFER_SIZE, &timeInterval, 1, &maxSamples, 0);
		
		if (status != PICO_OK)
		{
			if(status == PICO_INVALID_CHANNEL)
			{
				printf("ps3000aGetTimebase: Status Error 0x%x\n", status);
				printf("Please enable an analogue channel (option V from the main menu).");
				return;

			}
			else
			{
				timebase++;  // Increase timebase if the one specified can't be used. 
			}
		}
	}
	while(status != PICO_OK);

	printf("Timebase used %lu = %ld ns sample interval\n", timebase, timeInterval);
	oversample = TRUE;
}

/****************************************************************************
* Sets the signal generator
* - allows user to set frequency and waveform
* - allows for custom waveform (values -32768..32767) 
* - of up to 8192 samples long (PicoScope 3x04B & PS3x05B), 
*	or 16384 samples long (PicoScope 3x06B),
*   or 32768 samples long (PicoScope 3207B, 3X0X D & D MSO devices)
******************************************************************************/
void setSignalGenerator(UNIT unit)
{
	char ch;
	char fileName [128];

	int16_t waveform;
	int16_t choice;
	int16_t *arbitraryWaveform;

	int32_t waveformSize = 0;
	int32_t offset = 0;

	uint32_t pkpk = 4000000; // +/- 2 V
	uint32_t delta = 0;

	double frequency = 1.0;

	FILE * fp = NULL;
	PICO_STATUS status;


	while (_kbhit())			// use up keypress
	{
		_getch();
	}

	do
	{
		printf("\nSignal Generator\n================\n");
		printf("0 - SINE         1 - SQUARE\n");
		printf("2 - TRIANGLE     3 - DC VOLTAGE\n");

		if(unit.sigGen == SIGGEN_AWG)
		{
			printf("4 - RAMP UP      5 - RAMP DOWN\n");
			printf("6 - SINC         7 - GAUSSIAN\n");
			printf("8 - HALF SINE    A - AWG WAVEFORM\n");
		}

		printf("F - SigGen Off\n\n");

		ch = _getch();

		if (ch >= '0' && ch <='9')
		{
			choice = ch -'0';
		}
		else
		{
			ch = toupper(ch);
		}
	}
	while(unit.sigGen == SIGGEN_FUNCTGEN && ch != 'F' && (ch < '0' || ch > '3') || 
			unit.sigGen == SIGGEN_AWG && ch != 'A' && ch != 'F' && (ch < '0' || ch > '8')  );

	if (ch == 'F')	// If we're going to turn off siggen
	{
		printf("Signal generator Off\n");
		waveform = 8;		// DC Voltage
		pkpk = 0;			// 0 V
		waveformSize = 0;
	}
	else
	{
		if (ch == 'A' && unit.sigGen == SIGGEN_AWG)		// Set the AWG
		{
			arbitraryWaveform = (int16_t*) malloc( unit.AWGFileSize * sizeof(int16_t));
			memset(arbitraryWaveform, 0, unit.AWGFileSize * sizeof(int16_t));

			waveformSize = 0;

			printf("Select a waveform file to load: ");
			scanf_s("%s", fileName, 128);

			if (fopen_s(&fp, fileName, "r") == 0) 
			{ 
				// Having opened file, read in data - one number per line (max 8192 lines for 
				// PicoScope 3X04B & 3X05B devices, 16384 for PicoScope 3X06B devices, 32768 for PicoScope 3207B, 
				// 3X0XD and 3X0XD MSO devices), with values in the range -32768..+32767
				while (EOF != fscanf_s(fp, "%hi", (arbitraryWaveform + waveformSize))&& waveformSize++ < unit.AWGFileSize - 1);
				fclose(fp);
				printf("File successfully loaded\n");
			} 
			else 
			{
				printf("Invalid filename\n");
				return;
			}
		}
		else			// Set one of the built in waveforms
		{
			switch (choice)
			{
				case 0:
					waveform = PS3000A_SINE;
					break;

				case 1:
					waveform = PS3000A_SQUARE;
					break;

				case 2:
					waveform = PS3000A_TRIANGLE;
					break;

				case 3:
					waveform = PS3000A_DC_VOLTAGE;
					do 
					{
						printf("\nEnter offset in uV: (0 to 2000000)\n"); // Ask user to enter DC offset level;
						scanf_s("%lu", &offset);
					} while (offset < 0 || offset > 2000000);
					break;

				case 4:
					waveform = PS3000A_RAMP_UP;
					break;

				case 5:
					waveform = PS3000A_RAMP_DOWN;
					break;

				case 6:
					waveform = PS3000A_SINC;
					break;

				case 7:
					waveform = PS3000A_GAUSSIAN;
					break;

				case 8:
					waveform = PS3000A_HALF_SINE;
					break;

				default:
					waveform = PS3000A_SINE;
					break;
			}
		}

		if(waveform < 8 || (ch == 'A' && unit.sigGen == SIGGEN_AWG))	// Find out frequency if required
		{
			do 
			{
				printf("\nEnter frequency in Hz: (1 to 1000000)\n");	// Ask user to enter signal frequency;
				scanf_s("%lf", &frequency);
			} while (frequency <= 0 || frequency > 1000000);
		}

		if (waveformSize > 0)		
		{
			ps3000aSigGenFrequencyToPhase(unit.handle, frequency, PS3000A_SINGLE, waveformSize, &delta);

			status = ps3000aSetSigGenArbitrary(	unit.handle, 
				0,				// offset voltage
				pkpk,			// PkToPk in microvolts. Max = 4000000 uV  (±2 V)
				delta,			// start delta
				delta,			// stop delta
				0, 
				0, 
				arbitraryWaveform, 
				waveformSize, 
				(PS3000A_SWEEP_TYPE)0,
				(PS3000A_EXTRA_OPERATIONS)0, 
				PS3000A_SINGLE, 
				0, 
				0, 
				PS3000A_SIGGEN_RISING,
				PS3000A_SIGGEN_NONE, 
				0);

			// If status != PICO_OK, show the error
			printf(status? "\nps3000aSetSigGenArbitrary: Status Error 0x%x \n":"", (uint32_t) status);	
		} 
		else 
		{
			status = ps3000aSetSigGenBuiltInV2(unit.handle, 
				offset, 
				pkpk, 
				waveform, 
				(double) frequency, 
				(double) frequency, 
				0, 
				0, 
				(PS3000A_SWEEP_TYPE)0, 
				(PS3000A_EXTRA_OPERATIONS)0, 
				0, 
				0, 
				(PS3000A_SIGGEN_TRIG_TYPE)0, 
				(PS3000A_SIGGEN_TRIG_SOURCE)0, 
				0);

			// If status != PICO_OK, show the error
			printf(status?"\nps3000aSetSigGenBuiltIn: Status Error 0x%x \n":"", (uint32_t)status);		
		}
	}
}


/****************************************************************************
* collectStreamingImmediate
*  this function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
***************************************************************************/
void collectStreamingImmediate(UNIT * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));

	setDefaults(unit);

	printf("Collect streaming...\n");
	printf("Data is written to disk file (stream.txt)\n");
	printf("Press a key to start\n");
	_getch();

	/* Trigger disabled	*/
	setTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	streamDataHandler(unit, 0, ANALOGUE);
}

/****************************************************************************
* collectStreamingTriggered
*  this function demonstrates how to collect a stream of data
*  from the unit (start collecting on trigger)
***************************************************************************/
void collectStreamingTriggered(UNIT * unit)
{
	int16_t triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS3000A_CHANNEL_A].range, unit); // ChannelInfo stores ADC counts
	struct tPwq pulseWidth;

	struct tPS3000ATriggerChannelProperties sourceDetails = {	triggerVoltage,
		256 * 10,
		triggerVoltage,
		256 * 10,
		PS3000A_CHANNEL_A,
		PS3000A_LEVEL };

	struct tPS3000ATriggerConditionsV2 conditions = {	PS3000A_CONDITION_TRUE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE,
		PS3000A_CONDITION_DONT_CARE };

	struct tTriggerDirections directions = {	PS3000A_RISING,
		PS3000A_NONE,
		PS3000A_NONE,
		PS3000A_NONE,
		PS3000A_NONE,
		PS3000A_NONE };

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	printf("Collect streaming triggered...\n");
	printf("Data is written to disk file (stream.txt)\n");
	printf("Press a key to start\n");
	_getch();
	
	setDefaults(unit);

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */
	setTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	streamDataHandler(unit, 10000, ANALOGUE);
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

	printf("\n\nReadings will be scaled in (%s)\n\n", (scaleVoltages)? ("mV") : ("ADC counts"));

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

	if(unit->digitalPorts > 0)
	{
		printf("Digital ports switched off.");
	}

	printf("\n");
}

/****************************************************************************
* openDevice 
* Parameters 
* - unit        pointer to the UNIT structure, where the handle will be stored
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS openDevice(UNIT *unit)
{
	int16_t value = 0;
	int32_t i;
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;
	
	PICO_STATUS status = ps3000aOpenUnit(&(unit->handle), NULL);

	if (status == PICO_POWER_SUPPLY_NOT_CONNECTED || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT )
	{
		status = changePowerSource(unit->handle, status);
	}

	printf("\nHandle: %d\n", unit->handle);

	if (status != PICO_OK) 
	{
		printf("Unable to open device\n");
		printf("Error code : 0x%08lx\n", status);
		while(!_kbhit());
		exit(99); // exit program
	}

	printf("Device opened successfully, cycle %d\n\n", ++cycles);

	// setup devices
	get_info(unit);
	timebase = 1;

	ps3000aMaximumValue(unit->handle, &value);
	unit->maxValue = value;

	for ( i = 0; i < unit->channelCount; i++) 
	{
		unit->channelSettings[i].enabled = TRUE;
		unit->channelSettings[i].DCcoupled = TRUE;
		unit->channelSettings[i].range = PS3000A_5V;
	}

	memset(&directions, 0, sizeof(struct tTriggerDirections));
	memset(&pulseWidth, 0, sizeof(struct tPwq));

	setDefaults(unit);

	/* Trigger disabled	*/
	setTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	return status;
}


/****************************************************************************
* closeDevice 
****************************************************************************/
void closeDevice(UNIT *unit)
{
	ps3000aCloseUnit(unit->handle);
}

/****************************************************************************
* andAnalogueDigital
* This function shows how to collect a block of data from the analogue
* ports and the digital ports at the same time, triggering when the 
* digital conditions AND the analogue conditions are met
*
* Returns       none
***************************************************************************/
void andAnalogueDigitalTriggered(UNIT * unit)
{
	int32_t channel = 0;

	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS3000A_CHANNEL_A].range, unit);

	PS3000A_TRIGGER_CHANNEL_PROPERTIES sourceDetails = {	triggerVoltage,			// thresholdUpper
															256 * 10,				// thresholdUpper Hysteresis
															triggerVoltage,			// thresholdLower
															256 * 10,				// thresholdLower Hysteresis
															PS3000A_CHANNEL_A,		// channel
															PS3000A_LEVEL};			// mode

	PS3000A_TRIGGER_CONDITIONS_V2 conditions = {	
													PS3000A_CONDITION_TRUE,					// Channel A
													PS3000A_CONDITION_DONT_CARE,			// Channel B
													PS3000A_CONDITION_DONT_CARE,			// Channel C
													PS3000A_CONDITION_DONT_CARE,			// Channel D
													PS3000A_CONDITION_DONT_CARE,			// external
													PS3000A_CONDITION_DONT_CARE,			// aux 
													PS3000A_CONDITION_DONT_CARE,			// pwq
													PS3000A_CONDITION_TRUE					// digital
												};

	TRIGGER_DIRECTIONS directions = {	
										PS3000A_ABOVE,				// Channel A
										PS3000A_NONE,				// Channel B
										PS3000A_NONE,				// Channel C
										PS3000A_NONE,				// Channel D
										PS3000A_NONE,				// external
										PS3000A_NONE };				// aux

		PS3000A_DIGITAL_CHANNEL_DIRECTIONS digDirections[2];		// Array size can be up to 16, an entry for each digital bit

		PWQ pulseWidth;
		memset(&pulseWidth, 0, sizeof(PWQ));

		// Set the Digital trigger so it will trigger when bit 15 is HIGH and bit 13 is HIGH
		// All non-declared bits are taken as PS3000A_DIGITAL_DONT_CARE
		//

		digDirections[0].channel = PS3000A_DIGITAL_CHANNEL_0;
		digDirections[0].direction = PS3000A_DIGITAL_DIRECTION_RISING;

		digDirections[1].channel = PS3000A_DIGITAL_CHANNEL_4;
		digDirections[1].direction = PS3000A_DIGITAL_DIRECTION_HIGH;

		printf("\nCombination Block Triggered\n");
		printf("Collects when value is above %d", scaleVoltages?
			adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS3000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
			: sourceDetails.thresholdUpper);																// else print ADC Count

		printf(scaleVoltages?"mV\n" : "ADC Counts\n");

		printf("AND \n");
		printf("Digital Channel  0   --- Rising\n");
		printf("Digital Channel  4   --- High\n");
		printf("Other Digital Channels - Don't Care\n");

		printf("Press a key to start...\n");
		_getch();

		for (channel = 0; channel < unit->channelCount; channel++)
		{
			unit->channelSettings[channel].enabled = TRUE;
		}

		setDefaults(unit);					// Enable Analogue channels.

		/* Trigger enabled
		* Rising edge
		* Threshold = 100mV */

		if (setTrigger(unit, &sourceDetails, 1, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, digDirections, 2) == PICO_OK)
		{
			blockDataHandler(unit, "\nFirst 10 readings:\n", 0, MIXED);
		}

		disableAnalogue(unit);			// Disable Analogue ports when finished
}


/****************************************************************************
* orAnalogueDigital
* This function shows how to collect a block of data from the analogue
* ports and the digital ports at the same time, triggering when either the 
* digital conditions OR the analogue conditions are met
*
* Returns       none
***************************************************************************/
void orAnalogueDigitalTriggered(UNIT * unit)
{
	int32_t channel = 0;

	int16_t	triggerVoltage = mv_to_adc(1000, unit->channelSettings[PS3000A_CHANNEL_A].range, unit);

	PS3000A_TRIGGER_CHANNEL_PROPERTIES sourceDetails = {	triggerVoltage,			// thresholdUpper
															256 * 10,				// thresholdUpper Hysteresis
															triggerVoltage,			// thresholdLower
															256 * 10,				// thresholdLower Hysteresis
															PS3000A_CHANNEL_A,		// channel
															PS3000A_LEVEL};			// mode

	PS3000A_TRIGGER_CONDITIONS_V2 conditions[2];

	TRIGGER_DIRECTIONS directions = {	PS3000A_RISING,			// Channel A
										PS3000A_NONE,			// Channel B
										PS3000A_NONE,			// Channel C
										PS3000A_NONE,			// Channel D
										PS3000A_NONE,			// external
										PS3000A_NONE };			// aux

	PS3000A_DIGITAL_CHANNEL_DIRECTIONS digDirections[2];		// Array size can be up to 16, an entry for each digital bit

	PWQ pulseWidth;

	conditions[0].channelA				= PS3000A_CONDITION_TRUE;					// Channel A
	conditions[0].channelB				= PS3000A_CONDITION_DONT_CARE;				// Channel B
	conditions[0].channelC				= PS3000A_CONDITION_DONT_CARE;				// Channel C
	conditions[0].channelD				= PS3000A_CONDITION_DONT_CARE;				// Channel D
	conditions[0].external				= PS3000A_CONDITION_DONT_CARE;				// external
	conditions[0].aux					= PS3000A_CONDITION_DONT_CARE;				// aux
	conditions[0].pulseWidthQualifier	= PS3000A_CONDITION_DONT_CARE;				// pwq
	conditions[0].digital				= PS3000A_CONDITION_DONT_CARE;				// digital

	conditions[1].channelA				= PS3000A_CONDITION_DONT_CARE;				// Channel A
	conditions[1].channelB				= PS3000A_CONDITION_DONT_CARE;				// Channel B
	conditions[1].channelC				= PS3000A_CONDITION_DONT_CARE;				// Channel C
	conditions[1].channelD				= PS3000A_CONDITION_DONT_CARE;				// Channel D
	conditions[1].external				= PS3000A_CONDITION_DONT_CARE;				// external
	conditions[1].aux					= PS3000A_CONDITION_DONT_CARE;				// aux
	conditions[1].pulseWidthQualifier	= PS3000A_CONDITION_DONT_CARE;				// pwq
	conditions[1].digital				= PS3000A_CONDITION_TRUE;					// digital

	memset(&pulseWidth, 0, sizeof(PWQ));

	// Set the Digital trigger so it will trigger when bit 15 is HIGH and bit 13 is HIGH
	// All non-declared bits are taken as PS3000A_DIGITAL_DONT_CARE
	//

	digDirections[0].channel = PS3000A_DIGITAL_CHANNEL_0;
	digDirections[0].direction = PS3000A_DIGITAL_DIRECTION_RISING;

	digDirections[1].channel = PS3000A_DIGITAL_CHANNEL_4;
	digDirections[1].direction = PS3000A_DIGITAL_DIRECTION_HIGH;

	printf("\nCombination Block Triggered\n");
	printf("Collects when value rises past %d", scaleVoltages?
		adc_to_mv(sourceDetails.thresholdUpper, unit->channelSettings[PS3000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
		: sourceDetails.thresholdUpper);																// else print ADC Count
	
	printf(scaleVoltages?"mV\n" : "ADC Counts\n");

	printf("OR \n");
	printf("Digital Channel  0   --- Rising\n");
	printf("Digital Channel  4   --- High\n");
	printf("Other Digital Channels - Don't Care\n");

	printf("Press a key to start...\n");
	_getch();

	for (channel = 0; channel < unit->channelCount; channel++)
	{
		unit->channelSettings[channel].enabled = TRUE;
	}

	setDefaults(unit); // Enable analogue channels

	/* Trigger enabled
	* Rising edge
	* Threshold = 1000mV */

	if (setTrigger(unit, &sourceDetails, 1, conditions, 2, &directions, &pulseWidth, 0, 0, 0, digDirections, 2) == PICO_OK)
	{

		blockDataHandler(unit, "\nFirst 10 readings:\n", 0, MIXED);
	}

	disableAnalogue(unit); // Disable Analogue ports when finished;
}

/****************************************************************************
* digitalBlockTriggered
* This function shows how to collect a block of data from the digital ports
* with triggering enabled
*
* Returns       none
***************************************************************************/

void digitalBlockTriggered(UNIT * unit)
{
	PWQ pulseWidth;
	TRIGGER_DIRECTIONS directions;

	PS3000A_DIGITAL_CHANNEL_DIRECTIONS digDirections[16];		// Array size can be up to 16, an entry for each digital bit

	PS3000A_TRIGGER_CONDITIONS_V2 conditions = {
		PS3000A_CONDITION_DONT_CARE,		// Channel A
		PS3000A_CONDITION_DONT_CARE,		// Channel B
		PS3000A_CONDITION_DONT_CARE,		// Channel C
		PS3000A_CONDITION_DONT_CARE,		// Channel D
		PS3000A_CONDITION_DONT_CARE,		// external
		PS3000A_CONDITION_DONT_CARE,		// aux
		PS3000A_CONDITION_DONT_CARE,		// pwq
		PS3000A_CONDITION_TRUE				// digital
	};


	printf("\nDigital Block Triggered\n");

	memset(&directions, 0, sizeof(TRIGGER_DIRECTIONS));
	memset(&pulseWidth, 0, sizeof(PWQ));

	printf("Collect block of data when the trigger occurs...\n");
	printf("Digital Channel  0   --- Rising\n");
	printf("Digital Channel 4   --- High\n");
	printf("Other Digital Channels - Don't Care\n");


	digDirections[1].channel = PS3000A_DIGITAL_CHANNEL_0;
	digDirections[1].direction = PS3000A_DIGITAL_DIRECTION_RISING;

	digDirections[0].channel = PS3000A_DIGITAL_CHANNEL_4;
	digDirections[0].direction = PS3000A_DIGITAL_DIRECTION_HIGH;


	if (setTrigger(unit, NULL, 0, &conditions, 1, &directions, &pulseWidth, 0, 0, 0, digDirections, 2) == PICO_OK)
	{
		printf("Press a key to start...\n");
		_getch();
		blockDataHandler(unit, "\nFirst 10 readings:\n", 0, DIGITAL);
	}
}


/****************************************************************************
* digitalBlockImmediate
* This function shows how to collect a block of data from the digital ports
* with triggering disabled
*
* Returns       none
***************************************************************************/
void digitalBlockImmediate(UNIT *unit)
{
	PWQ pulseWidth;
	TRIGGER_DIRECTIONS directions;
	PS3000A_DIGITAL_CHANNEL_DIRECTIONS digDirections;

	printf("\nDigital Block Immediate\n");
	memset(&directions, 0, sizeof(TRIGGER_DIRECTIONS));
	memset(&pulseWidth, 0, sizeof(PWQ));
	memset(&digDirections, 0, sizeof(PS3000A_DIGITAL_CHANNEL_DIRECTIONS));

	setTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, &digDirections, 0);

	printf("Press a key to start...\n");
	_getch();

	blockDataHandler(unit, "\nFirst 10 readings:\n", 0, DIGITAL);
}


/****************************************************************************
*  digitalStreamingAggregated
*  this function demonstrates how to collect a stream of Aggregated data
*  from the unit's Digital inputs (start collecting immediately)
***************************************************************************/
void digitalStreamingAggregated(UNIT * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));


	printf("Digital streaming with Aggregation...\n");
	printf("Press a key to start...\n");
	_getch();

	/* Trigger disabled	*/
	setTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	streamDataHandler(unit, 0, AGGREGATED);
}


/****************************************************************************
*  digitalStreamingImmediate
*  this function demonstrates how to collect a stream of data
*  from the unit's Digital inputs (start collecting immediately)
***************************************************************************/
void digitalStreamingImmediate(UNIT * unit)
{
	struct tPwq pulseWidth;
	struct tTriggerDirections directions;

	memset(&pulseWidth, 0, sizeof(struct tPwq));
	memset(&directions, 0, sizeof(struct tTriggerDirections));

	printf("Digital streaming...\n");
	printf("Press a key to start...\n");
	_getch();

	/* Trigger disabled	*/
	setTrigger(unit, NULL, 0, NULL, 0, &directions, &pulseWidth, 0, 0, 0, 0, 0);

	streamDataHandler(unit, 0, DIGITAL);
}


/****************************************************************************
* digitalMenu 
* Displays digital examples available
* Parameters 
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void digitalMenu(UNIT *unit)
{
	char ch;
	int16_t enabled = TRUE;
	int16_t disabled = !enabled;

	disableAnalogue(unit);					// Disable Analogue ports;
	setDigitals(unit, enabled);				// Enable Digital ports

	ch = ' ';
	while (ch != 'X')
	{
		printf("\n");
		printf("\nDigital Port Menu\n\n");
		printf("B - Digital Block Immediate\n");
		printf("T - Digital Block Triggered\n");
		printf("A - Analogue 'AND' Digital Triggered Block\n");
		printf("O - Analogue 'OR'  Digital Triggered Block\n");
		printf("S - Digital Streaming Mode\n");
		printf("V - Digital Streaming Aggregated\n");
		printf("X - Return to previous menu\n\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");
		switch (ch) 
		{
			case 'B':
				digitalBlockImmediate(unit);
				break;

			case 'T':
				digitalBlockTriggered(unit);
				break;

			case 'A':
				andAnalogueDigitalTriggered(unit);
				break;

			case 'O':
				orAnalogueDigitalTriggered(unit);
				break;

			case 'S':
				digitalStreamingImmediate(unit);
				break;

			case 'V':
				digitalStreamingAggregated(unit);
				break;
		}
	}

	setDigitals(unit, disabled);		// Disable Digital ports when finished
	restoreAnalogueSettings(unit);		// Restore analogue channel settings
}

/****************************************************************************
* main
* 
***************************************************************************/
int32_t main(void)
{
	char ch;
	PICO_STATUS status;
	UNIT unit;

	printf("PicoScope 3000 Series (A API) Driver Example Program\n");
	printf("\nOpening the device...\n");

	status = openDevice(&unit);

	ch = '.';

	while (ch != 'X')
	{
		displaySettings(&unit);

		printf("\n\n");
		printf("Please select one of the following options:\n\n");
		printf("B - Immediate block                           V - Set voltages\n");
		printf("T - Triggered block                           I - Set timebase\n");
		printf("E - Collect a block of data using ETS         A - ADC counts/mV\n");
		printf("R - Collect set of rapid captures\n");
		printf("S - Immediate streaming\n");
		printf("W - Triggered streaming\n");

		if(unit.sigGen != SIGGEN_NONE)
		{
			printf("G - Signal generator\n");
		}

		if(unit.digitalPorts > 0)
		{
			printf("D - Digital Ports menu\n");
		}

		printf("                                              X - Exit\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");

		switch (ch) 
		{
			case 'B':
				collectBlockImmediate(&unit);
				break;

			case 'T':
				collectBlockTriggered(&unit);
				break;

			case 'R':
				collectRapidBlock(&unit);
				break;

			case 'S':
				collectStreamingImmediate(&unit);
				break;

			case 'W':
				collectStreamingTriggered(&unit);
				break;

			case 'E':
				
				if (unit.ETS == FALSE)
				{
					printf("This model does not support ETS\n\n");
					break;
				}

				collectBlockEts(&unit);
				break;

			case 'G':
				
				if (unit.sigGen == SIGGEN_NONE)
				{
					printf("This model does not have a signal generator.\n\n");
					break;
				}

				setSignalGenerator(unit);
				break;

			case 'V':
				setVoltages(&unit);
				break;

			case 'I':
				setTimebase(unit);
				break;

			case 'A':
				scaleVoltages = !scaleVoltages;
				break;

			case 'D':

				if (unit.digitalPorts)
				{
					digitalMenu(&unit);
				}
				break;

			case 'X':
				break;

			default:
				printf("Invalid operation\n");
				break;
		}
	}
	
	closeDevice(&unit);

	return 1;
}
