/*******************************************************************************
 *
 * Filename: pl1000con.c
 *
 * Description:
 *   This is a console-mode program that demonstrates how to use the
 *   PicoLog 1000 Series (pl1000) driver API functions.
 *
 * Supported PicoLog 1000 Series devices:
 *
 *		PicoLog 1012
 *		PicoLog 1216
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *    Use windowing to collect a sequence of overlapped blocks
 *    Write a continuous stream of data to a disk file
 *    Take individual readings
 *	  Set PWM
 *	  Set digital outputs
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit pl1000.lib can be located
 *			Ensure that the pl1000Api.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add pl1000.lib to the project (Microsoft C only)
 *			 Add pl1000Api.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libpl1000 driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the pl1000con application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 *  Copyright (C) 2009-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 *******************************************************************************/

#include <stdio.h>
#ifdef WIN32
/* Headers for Windows */
#include <conio.h>
#include "pl1000Api.h"
#include <windows.h>
#include <stdlib.h>
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <libpl1000-1.0/pl1000Api.h>
#ifndef PICO_STATUS
#include <libpl1000-1.0/PicoStatus.h>

#define PREF4
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)
#define _stricmp(a,b) strcasecmp(a,b)

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
        newt.c_lflag &= ~( ICANON | ECHO );
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
#define TRUE		1
#define FALSE		0

#define MAX_BLOCK_SIZE 8192
#define PL1000_12_CHANNEL 12
#define PL1000_16_CHANNEL 16

int32_t		scale_to_mv = TRUE;
uint16_t	max_adc_value;
int16_t		g_handle;
int16_t		isReady;
int16_t		d0State, d1State, d2State, d3State = 0;
uint16_t  numDeviceChannels;
PICO_STATUS status;

/****************************************************************************
 *
 * adc_to_mv
 *
 * If the user selects scaling to millivolts,
 * Convert a 10- or 12-bit ADC count into millivolts
 *
 ****************************************************************************/
int32_t adc_to_mv (int32_t raw)										
{		
	int32_t	scaled;

	if (scale_to_mv)
	{
		scaled = raw * 2500 / max_adc_value;
	}
	else
	{
		scaled = raw;
	}

	return scaled;
}

/****************************************************************************
 *
 * mv_to_adc
 *
 * Convert a millivolt value into an 10-bit ADC count
 *
 *  (useful for setting trigger thresholds)
 *
 ****************************************************************************/
int32_t mv_to_adc (int32_t mv)
{
	return mv * max_adc_value / 2500;
}

void printChannelsHeader(FILE * fp, int16_t * channels, int16_t numChannels)
{
	int16_t i;

	if(fp != NULL)
	{
		for(i = 0; i < numChannels; i++)
		{
			fprintf (fp, "Ch%3d\t", channels[i]);
		}

		fprintf(fp, "\n");
	}
}

/****************************************************************************
 *
 * collect_block_immediate()
 *
 *  This function demonstrates how to collect a single block of data
 *  from the unit (start collecting immediately)
 *
 ****************************************************************************/
void collect_block_immediate (void)
{
	uint32_t	i = 0;
	uint16_t	j = 0;
	int16_t		channels [] = {(int16_t) PL1000_CHANNEL_1, (int16_t) PL1000_CHANNEL_2 };
	uint32_t	nSamples = 1000; // Should be equal to nChannels * nSamplesPerChannel
	int16_t		nChannels = 2;
	uint32_t	nSamplesPerChannel = 500;
	uint32_t	nSamplesCollected;
	uint16_t *  samples = (uint16_t *) calloc(nSamples, sizeof(uint16_t));	// Size of array should be equal to nChannels * nSamplesPerChannel
	uint32_t	usForBlock = 1000000;	// 1s
	uint16_t 	overflow = 0;
	uint32_t	triggerIndex = 0;
	uint32_t	samplingIntervalUs = 0;
	FILE *		fp;

	printf ("Collect immediate block ...\n");
	printf ("Press a key to start\n");
	_getch();

	// Set the trigger (disabled)
	status = pl1000SetTrigger(g_handle, FALSE, 0, 0, 0, 0, 0, 0, 0);
	
	// Set sampling rate and channels
	status = pl1000SetInterval(g_handle, &usForBlock, nSamplesPerChannel, channels, nChannels);

	samplingIntervalUs = (usForBlock / (nSamplesPerChannel * nChannels));

	printf("\n");
	printf("Collecting %d samples per channel over %d microseconds.\n", nSamplesPerChannel, usForBlock);
	printf("Sampling interval: %d us\n", samplingIntervalUs);
	printf("\n");

	// Print sampling interval to be used

	fopen_s(&fp, "pl1000_block.txt", "w");

	if(fp != NULL)
	{
		printChannelsHeader(fp, channels, nChannels);
	}
	
	// Run
	status = pl1000Run(g_handle, nSamplesPerChannel, BM_SINGLE);

	// Wait until unit is ready
	isReady = 0; 
		
	while(isReady == 0)
	{
		status = pl1000Ready(g_handle, &isReady);
	}

	nSamplesCollected = nSamplesPerChannel;

	status = pl1000GetValues(g_handle, samples, &nSamplesCollected, &overflow, &triggerIndex);

	// Print out the first 10 readings, converting the readings to mV if required
	printf ("First 10 readings of %i\n\n", nSamplesCollected);

	for (i = 0; i < 10; i++)
	{
		printf ("%d\n", adc_to_mv(samples[i]));
	}
		
	for (i = 0; i < nSamplesCollected; i++)
	{
		for (j = 0; j < nChannels; j++)
		{
			fprintf (fp, "%d\t", adc_to_mv(samples[(i * nChannels) + j]));
		}

		fprintf(fp, "\n");
	}
		
	printf("\n");

	fclose(fp);
	status = pl1000Stop(g_handle);

}

/****************************************************************************
 *
 * collect_block_triggered()
 *
 *  This function demonstrates how to collect a single block of data from the
 *  unit, when a trigger event occurs.
 *
 ****************************************************************************/
void collect_block_triggered (void)
{
  uint32_t	i = 0;
  uint32_t  j = 0;
	int16_t		channels [] = {(int16_t) PL1000_CHANNEL_1};
	uint32_t	nSamples = 10000; // Should be equal to nChannels * nSamplesPerChannel
	int16_t		nChannels = 1;
	uint32_t	nSamplesPerChannel = nSamples / nChannels;
	uint32_t	nSamplesCollected;
	uint16_t *  samples = (uint16_t *) calloc(nSamples, sizeof(uint16_t));
	uint32_t	usForBlock = 1000000;
	uint16_t 	overflow = 0;
	uint32_t	triggerIndex = 0;
	uint32_t	samplingIntervalUs = 0;
	FILE *		fp;

	printf ("Collect block triggered...\n");
	printf ("Collects when value rises past 1 V\n");
	printf ("Press a key to start...\n");
	_getch();

	//Set the trigger
	status = pl1000SetTrigger(g_handle, TRUE, 0, 0, PL1000_CHANNEL_1, 0, mv_to_adc(1000), 0, -50);
	
	//set sampling rate and channels
	status = pl1000SetInterval(g_handle, &usForBlock, nSamplesPerChannel, channels, nChannels);

	samplingIntervalUs = (usForBlock / (nSamplesPerChannel * nChannels));

	printf("\n");
	printf("Collecting %d samples per channel over %d microseconds.\n", nSamplesPerChannel, usForBlock);
	printf("Sampling interval: %d us\n", samplingIntervalUs);
	printf("\n");

	printf ("Trigger delay is set to -50%% (trigger event in centre of block)\n");
	printf ("\nWaiting for trigger...\n\n");
	printf ("Press a key to abort\n");

	fopen_s(&fp, "pl1000_triggered_block.txt", "w");

	if(fp != NULL)
	{
		printChannelsHeader(fp, channels, nChannels);
	}

	// Run
	status = pl1000Run(g_handle, nSamplesPerChannel, BM_SINGLE);

	// Wait until unit is ready
	isReady = 0;
	
	while(isReady == 0 && (!_kbhit ()))
	{
		status = pl1000Ready(g_handle, &isReady);
	}

	nSamplesCollected = nSamplesPerChannel;

	status = pl1000GetValues(g_handle, samples, &nSamplesCollected, &overflow, &triggerIndex);

	// Print out the first 10 readings, converting the readings to mV if required
	printf ("5 readings either side of trigger event (%i samples collected)\n\n", nSamplesCollected);
	
	for (i = triggerIndex - 5; i < triggerIndex + 6; i++)
	{
		printf ("%d\n", adc_to_mv(samples[i]));
	}
	
	for (i = 0; i < nSamplesCollected; i++)
	{
    for (j = 0; j < (uint32_t) nChannels; j++)
    {
      fprintf(fp, "%d\t", adc_to_mv(samples[(i * nChannels) + j]));
    }

    fprintf(fp, "\n");
	}

	printf("\n");

	fclose(fp);
	status = pl1000Stop(g_handle);
}


/****************************************************************************
 *
 * collect_windowed_blocks()
 *
 *  This function demonstrates how to use windowed blocks.
 *
 ****************************************************************************/
void collect_windowed_blocks (void)
{
  uint32_t	i = 0;;
  uint32_t j = 0;
	int16_t		channels [] = {(int16_t) PL1000_CHANNEL_1};
	uint32_t	nSamples = 1000; // Should be equal to nChannels * nSamplesPerChannel
	int16_t		nChannels = 1;
	uint32_t	nSamplesPerChannel = nSamples / nChannels;
	uint32_t	nSamplesCollected;
	uint16_t * samples = (uint16_t *) calloc(nSamples, sizeof(uint16_t)); // Size of array should be equal to nChannels * nSamplesPerChannel
	uint32_t	usForBlock = 10000000;	// 10 seconds
	uint16_t	overflow = 0;
	uint32_t	triggerIndex = 0;
	int16_t		nLines = 0;
	uint32_t	samplingIntervalUs = 0;
	FILE *		fp;

	printf ("Collect windowed block...\n");
	printf ("First block appears after 10 seconds,\n");
	printf ("then 10 second blocks are collected every second\n");
	printf ("Press a key to start\n");
	_getch();

	// Set the trigger (disabled)
	status = pl1000SetTrigger(g_handle, FALSE, 0, 0, 0, 0, 0, 0, 0);
	
	// Set sampling rate and channels
	status = pl1000SetInterval(g_handle, &usForBlock, nSamplesPerChannel, channels, nChannels);
	
	samplingIntervalUs = (usForBlock / (nSamplesPerChannel * nChannels));

	printf("\n");
	printf("Sampling interval: %d us\n", samplingIntervalUs);
	printf("\n");

	// Start streaming
	status = pl1000Run(g_handle, nSamplesPerChannel, BM_WINDOW);

	// Wait until unit is ready
	printf ("Waiting for first block...\n");
	isReady = 0;
	
	while (isReady == 0)
	{
		status = pl1000Ready(g_handle, &isReady);
	}

	printf("Press any key to stop\n");
	fopen_s(&fp, "pl1000_windowed_blocks.txt", "w");
  
	while (!_kbhit())
	{
    nSamplesCollected = nSamplesPerChannel;

		status = pl1000GetValues(g_handle, samples, &nSamplesCollected, &overflow, &triggerIndex);

		printf("%d values\n", nSamplesCollected);
		
		if (nLines == 20)
		{
				printf("Press any key to stop\n");
				nLines = 0;
		}
    else
    {
      nLines++;
    }

		for (i = 0; i < nSamplesCollected; i++)
		{
      for (j = 0; j < (uint32_t) nChannels; j++)
      {
        fprintf(fp, "%d\t", adc_to_mv(samples[(i * nChannels) + j]));
      }

      fprintf(fp, "\n");
		}

		Sleep(1000);		// Wait 1 second before collecting next 10 second block.
	}

	fclose(fp);
	status = pl1000Stop(g_handle);

	_getch();
}

/****************************************************************************
 *
 * collect_streaming()
 * 
 *  This function demonstrates how to use streaming data collection.
 *
 ****************************************************************************/

void collect_streaming (void)
{
  uint32_t	i = 0;
  uint32_t j = 0;
	int16_t		channels [] = {1};
	uint32_t	nSamples = 1000; // Should be equal to nChannels * nSamplesPerChannel
	int16_t		nChannels = 1;
	uint32_t	nSamplesPerChannel = nSamples / nChannels;
	uint32_t	nSamplesCollected = 0;
	uint16_t *  samples = (uint16_t *) calloc(nSamples, sizeof(uint16_t)); // Size of array should be equal to nChannels * nSamplesPerChannel
	uint32_t	usForBlock = 1000000;
	uint16_t  overflow = 0;
	uint32_t	triggerIndex = 0;
	int16_t		nLines = 0;
	uint32_t	totalSamplesCollected = 0;
	uint32_t	samplingIntervalUs = 0;
	FILE *		fp;
	
	printf ("Collect streaming...\n");
	printf ("Data is written to disk file (pl1000_streaming.txt)\n");
	printf ("Press a key to start\n");
	_getch();
		
	// Set the trigger (disabled)
	status = pl1000SetTrigger(g_handle, FALSE, 0, 0, 0, 0, 0, 0, 0);
	
	// Set sampling rate and channels
	status = pl1000SetInterval(g_handle, &usForBlock, nSamplesPerChannel, channels, nChannels);

	samplingIntervalUs = (usForBlock / (nSamplesPerChannel * nChannels));

	printf("\n");
	printf("Sampling interval: %d us\n", samplingIntervalUs);
	printf("\n");

	// Start streaming
	status = pl1000Run(g_handle, nSamplesPerChannel, BM_STREAM);

	// Wait until unit is ready
	isReady = 0;
	
	while(isReady == 0)
	{
		status = pl1000Ready(g_handle, &isReady);
	}

	printf("Press any key to stop\n");
	fopen_s(&fp, "pl1000_streaming.txt", "w");
  
	while(!_kbhit())
	{
		nSamplesCollected = nSamplesPerChannel;

		status = pl1000GetValues(g_handle, samples, &nSamplesCollected, &overflow, &triggerIndex);

		totalSamplesCollected = totalSamplesCollected + nSamplesCollected;
		printf("Collected %d values per channel, total per channel: %d\n", nSamplesCollected, totalSamplesCollected);

		if (nLines == 20)
		{
			printf("Press any key to stop\n");
			nLines = 0;
		}
		else
		{
			nLines++;
		}

		for (i = 0; i < nSamplesCollected; i++)
		{
      for (j = 0; j < (uint32_t) nChannels; j++)
      {
        fprintf(fp, "%d\t", adc_to_mv(samples[(i * nChannels) + j]));
      }

      fprintf(fp, "\n");
		}

		Sleep(100);
	}
	fclose(fp);
	status = pl1000Stop(g_handle);

	_getch();
}

/****************************************************************************
 *
 * Collect_individual()
 *
 *  This function demonstrates how to collect a single reading from each
 *  channel for a defined number of readings.
 *
 ****************************************************************************/
void collect_individual (void)
{
	int32_t		sample_no;
	int32_t		c;
	int16_t		value;
  
	printf ("Collect individual...\n");
	printf ("Takes individual readings under program control\n");
	printf ("Sample from all channels\n");
	printf ("Press a key to start\n");
	_getch();

	sample_no = 20;
	
	while (!_kbhit ())
	{
		Sleep (100);
		
		if (++sample_no > 20)
		{
			sample_no = 0;
			printf ("Press any key to stop\n\n");
			
			for (c = (int32_t) PL1000_CHANNEL_1; c <= numDeviceChannels; c++)
			{
				printf ("ch%02d  ", c);
			}
			printf ("\n");
		}
		
		for (c = (int32_t) PL1000_CHANNEL_1; c <= numDeviceChannels; c++)
		{
			value = 0;
			pl1000GetSingle(g_handle, c, &value);
			printf ("%5d ", adc_to_mv (value));
		}
		
		printf ("\n");
	}
	
	_getch ();
}

/****************************************************************************
 *
 * outputToggle()
 *
 *  Set/clear digital outputs
 *
 ****************************************************************************/
void outputToggle(PL1000_DO_CH doChannel)
{
	switch(doChannel)
	{
		case PL1000_DO_CHANNEL_0:
		{
			if (d0State == 0)
			{
				d0State = 1;
			}
			else
			{
				d0State = 0;
			}
			
			pl1000SetDo(g_handle, d0State, doChannel);
			break;
		}
		case PL1000_DO_CHANNEL_1:
		{
			if (d1State == 0)
			{
				d1State = 1;
			}
			else
			{
				d1State = 0;
			}
						
			pl1000SetDo(g_handle, d1State, doChannel);
			break;
		}
		case PL1000_DO_CHANNEL_2:
		{
			if (d2State == 0)
			{
				d2State = 1;
			}
			else
			{
				d2State = 0;
			}
			
			pl1000SetDo(g_handle, d2State, doChannel);
			break;
		}
		case PL1000_DO_CHANNEL_3:
		{
			if (d3State == 0)
			{
				d3State = 1;
			}
			else
			{
				d3State = 0;
			}

			pl1000SetDo(g_handle, d3State, doChannel);
			break;
		}
	}
}

/****************************************************************************
 * 
 * displayOutputStates()
 *
 * Display digital output states
 *
 ****************************************************************************/
void displayOutputStates()
{
		printf("\nDigital Outputs\n");
		printf("===============\n");
		printf("D0\tD1\tD2\tD3\t\n");
		printf("%i\t%i\t%i\t%i\t\n\n", d0State, d1State, d2State, d3State);
}

/****************************************************************************
 *
 * pwm()
 *
 * Set pulse width modulation
 *
 ****************************************************************************/
void pwm()
{
  PICO_STATUS status = PICO_OK;
	int32_t period = 0;
	int32_t cycle = 0;

	printf("\n----------PWM----------\n");
	do
	{
		printf("Enter period (100 to 1800 microseconds):");
		scanf_s("%d", &period);
		fflush(stdin);
	}while(period < 100 || period > 1800);

	do
	{
		printf("Enter duty cycle (0 to 100%%):");
		scanf_s("%d", &cycle);
		fflush(stdin);
	}while(cycle < 0 || cycle > 100);
		
	status = pl1000SetPulseWidth(g_handle, (uint16_t) period, (uint8_t) cycle);

  if (status != PICO_OK)
  {
    printf("\nUnable to set pulse-width modulated output - status code %d (0x%08lx)\n", status, status);
  }
}

/****************************************************************************
 *
 * Main function
 *
 ****************************************************************************/

void main (void)
{
	int8_t	ch;
	int8_t	info[80];
	int16_t	requiredSize = 0;
  uint16_t variant = 0;
  const int8_t * PICOLOG_1012 = "PicoLog1012";
  const int8_t * PICOLOG_1216 = "PicoLog1216";

	printf ("PicoLog 1000 Series (pl1000) Driver Example Program\n");
	printf ("Version 1.3\n\n");

	printf ("\nOpening the device...\n");
	status = pl1000OpenUnit(&g_handle);
	
	if (status != PICO_OK)
	{
		printf ("Unable to open device\nPress any key\n");
		_getch();
		exit(99);
	}
	else
	{
		// Set all digital outputs to zero
		pl1000SetDo(g_handle, 0, PL1000_DO_CHANNEL_0);
		pl1000SetDo(g_handle, 0, PL1000_DO_CHANNEL_1);
		pl1000SetDo(g_handle, 0, PL1000_DO_CHANNEL_2);
		pl1000SetDo(g_handle, 0, PL1000_DO_CHANNEL_3);

		printf ("Device opened successfully\n\n");
		status = pl1000GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_VARIANT_INFO);
		printf("Model:\t\t\t %s\n", info);

    // Find the number of channels on the device
    if (_stricmp(info, PICOLOG_1012) == 0)
    {
      numDeviceChannels = PL1000_12_CHANNEL;
    }
    else if (_stricmp(info, PICOLOG_1216) == 0)
    {
      numDeviceChannels = PL1000_16_CHANNEL;
    }
    else
    {
      printf("Invalid variant. Exiting application.\n");
      exit(99);
    }

		status = pl1000GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_BATCH_AND_SERIAL);
		printf("Serial Number:\t\t %s\n", info);
		
		status = pl1000GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_CAL_DATE);
		printf("Calibration Date:\t %s\n", info);
		
		status = pl1000GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_USB_VERSION);
		printf("USB version:\t\t %s\n", info);
		
		status = pl1000GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_HARDWARE_VERSION);
		printf("Hardware version:\t %s\n", info);
		
		status = pl1000GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_DRIVER_VERSION);
		printf("pl1000.dll version:\t %s\n", info);
		
		status = pl1000GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_KERNEL_VERSION);
		printf("Kernel version:\t\t %s\n", info);

		status = pl1000GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_FIRMWARE_VERSION_1);
		printf("Firmware:\t\t %s\n", info);
		
		// Find the maximum ADC count for the device

		status = pl1000MaxValue(g_handle, &max_adc_value);  

		ch = ' ';

		while (ch != 'X')
		{
			printf ("\n");
			printf ("Select an operation\n");
			printf ("B - Immediate block\t\tA - Toggle ADC/mV\n");
			printf ("T - Triggered block\t\tP - Set PWM\n");
			printf ("W - Windowed block\t\tD - Display digital output states\n");
			printf ("S - Streaming\t\t\t0,1,2,3 - Toggle digital output\n");
			printf ("I - Individual reading\t\tX - exit\n");
			ch = toupper (_getch());
			printf ("\n");

			switch (ch)
			{
				case 'B':
					collect_block_immediate ();
					break;

				case 'T':
					collect_block_triggered ();
					break;

				case 'W':
					collect_windowed_blocks ();
					break;

				case 'S':
					collect_streaming ();
					break;

				case 'I':
					collect_individual ();
					break;
				
				case 'P':
					pwm();
					break;
				
				case 'D':
					displayOutputStates();
					break;

				case '0':
					outputToggle(PL1000_DO_CHANNEL_0);
					displayOutputStates();
					break;

				case '1':
					outputToggle(PL1000_DO_CHANNEL_1);
					displayOutputStates();
					break;

				case '2':
					outputToggle(PL1000_DO_CHANNEL_2);
					displayOutputStates();
					break;

				case '3':
					outputToggle(PL1000_DO_CHANNEL_3);
					displayOutputStates();
					break;

				case 'A':
					scale_to_mv = !scale_to_mv;

					if (scale_to_mv)
					{
						printf ("Values will be displayed in mV\n");
					}
					else
					{
						printf ("Values will be displayed in ADC counts\n");
					}
					break;

				case 'X':
				/* Handled by outer loop */
				break;

				default:
				printf ("Invalid operation\n");
				break;
			}
		}
		
		pl1000CloseUnit(g_handle);
	}
}
