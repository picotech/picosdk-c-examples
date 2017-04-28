/*******************************************************************************
 *
 *  Filename: picohrdlcon.c
 *
 *  Description:
 *		This is a console-mode program that demonstrates how to use the
 *		picohrdl driver API functions for the PicoLog ADC-20 and ADC-24 
 *		High Resolution Data Loggers.
 *
 *  There are five examples:
 *		Collect a block of samples immediately
 *		Collect a block of samples when a trigger event occurs
 *		Use windowing to collect a sequence of overlapped blocks
 *		Write a continuous stream of data to a disk file
 *		Take individual readings
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit picohrdl.lib can be located
 *			Ensure that the HRDL.h file can be located
 *
 *		Otherwise:
 *
 *			Set up a project for a 32-/64-bit console mode application
 *			Add this file to the project
 *			Add the appropriate 32-/64-bit picohrdl.lib to the project (Microsoft C only)
 *			Build the project
 *
 *  Copyright (C) 2004 - 2017 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/
#include <stdio.h>
#ifdef WIN32
#include <conio.h>
#include <windows.h>
#include "HRDL.h"

#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "libpicohrdl-1.0/HRDL.h"

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

struct structChannelSettings 
{
	int16_t enabled;
	HRDL_RANGE range;
	int16_t singleEnded;
} g_channelSettings[HRDL_MAX_ANALOG_CHANNELS + 1];

#define BUFFER_SIZE 1000

int32_t		g_times[BUFFER_SIZE];
int32_t		g_values[BUFFER_SIZE];

int32_t		g_scaleTo_mv;
int16_t		g_device;
int16_t		g_doSet;
int16_t		g_maxNoOfChannels;

double inputRangeV [] = {1, 2, 4, 8, 16, 32, 64};

/****************************************************************************
*
* ResetChannels
*
* Switches all the channels to off
* The voltage level to 2500mV range,
* All to single ended
*
****************************************************************************/
void RestChannels()
{
	int32_t i;

	for (i = HRDL_DIGITAL_CHANNELS; i <= HRDL_MAX_ANALOG_CHANNELS; i++)
	{
		g_channelSettings[i].enabled = 0;
		g_channelSettings[i].range = (HRDL_RANGE) 0;
		g_channelSettings[i].singleEnded = 1;
	}
}

/****************************************************************************
*
* AdcTo_mv
*
* If the user selects scaling to millivolts,
* Convert an ADC count into millivolts
*
****************************************************************************/
float AdcToMv (HRDL_INPUTS channel, int32_t raw)
{ 
	int32_t maxAdc = 0;
	int32_t minAdc = 0;

	if (channel < HRDL_ANALOG_IN_CHANNEL_1 || channel > HRDL_MAX_ANALOG_CHANNELS)
	{
		return 0;
	}

	if (raw == -1)
	{
		return -1;
	}

	HRDLGetMinMaxAdcCounts(g_device, &minAdc, &maxAdc, channel);

	// To convert from adc to V you need to use the following equation
	//            maxV - minV
	//   raw =  ---------------
	//          maxAdc - minAdc
	//
	// if we assume that V and adc counts are bipolar and symmetrical about 0, the 
	// equation reduces to the following:
	//            maxV
	//   raw =  --------
	//           maxAdc 
	//
    
	//
	// Note the use of the maxAdc count for the HRDL in the equation below:  
	//
	// maxAdc is always 1 adc count short of the advertised full voltage scale
	// minAdc is always exactly the advertised minimum voltage scale.
	//
	// It is this way to ensure that we have an adc value that
	// equates to exactly zero volts.
	//
	// maxAdc     == maxV
	// 0 adc      == 0 V
	// minAdc     == minV
	// 

	if (g_scaleTo_mv)
	{
		return (float)  ((double) raw * 2500.0 / (double)inputRangeV[g_channelSettings[channel].range]) / (double)(maxAdc);
	}
	else
	{
		return (float) raw;
	}

}
/****************************************************************************
*
* CollectBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the device (start collecting immediately)
*
****************************************************************************/   
void CollectBlockImmediate (void)
{
	int32_t		i;
	int16_t		overflow;
	int16_t		channel;
	static		int16_t ok;
	int8_t		strError[80];
	int16_t		timeCount;
	int16_t		noOfActiveChannels;
	int16_t		status = 1;

	printf("\nCollect block immediate...\n");
	printf("Press a key to start\n");
	_getch(); 

	for (i = HRDL_ANALOG_IN_CHANNEL_1; i <= g_maxNoOfChannels; i++)
	{
		status = HRDLSetAnalogInChannel(g_device,
										(int16_t)i,
										g_channelSettings[i].enabled, 
										(int16_t) g_channelSettings[i].range,
										g_channelSettings[i].singleEnded);

		if (status == 0)
		{
			HRDLGetUnitInfo(g_device, strError, (int16_t) 80, HRDL_SETTINGS);
			printf("Error occurred: %s\n\n", strError);
			return;
		}
	}

	//
	// Collect data at one hundred milli second intervals. The enabled channels will be
	// converted at 60ms intervals. This means that the channels will be 
	// converted at closer intervals to each other but a penalty will be 
	// paid in terms of noise free resolution.
	//
	// To increase noise free resolution, the conversion time should be increased.
	//

	status = HRDLSetInterval(g_device, 61, HRDL_60MS);

	if (status == 0)
	{
		HRDLGetUnitInfo(g_device, strError, (int16_t)80, HRDL_SETTINGS);
		printf("Error occurred: %s\n\n", strError);
		return;
	}

	//
	// Start it collecting,
	//  then wait for completion
	//
	
	status = HRDLRun(g_device, BUFFER_SIZE, (int16_t) HRDL_BM_BLOCK);

	if (status == 0)
	{
		HRDLGetUnitInfo(g_device, strError, (int16_t) 80, HRDL_SETTINGS);
		printf("Error occurred: %s\n\n", strError);
		return;
	}

	printf("Waiting for device to complete collection.");

	while(!HRDLReady(g_device))
	{
		Sleep(1000);
		printf(".");
	}

	printf("\n");

	//
	// Should be done now...
	// get the times (in milli secondss)
	// and the analog values (in ADC counts)
	//
	HRDLGetNumberOfEnabledChannels(g_device, &noOfActiveChannels);
	noOfActiveChannels = noOfActiveChannels + (int16_t)(g_channelSettings[HRDL_DIGITAL_CHANNELS].enabled);
	HRDLGetTimesAndValues(g_device, g_times, g_values, &overflow, (int32_t) (BUFFER_SIZE / noOfActiveChannels));

	//
	// Print out the first 10 readings,
	//  converting the readings to mV if required
	//
	printf("First 5 readings\n");
	printf("Time\t");
	
	for (channel = HRDL_DIGITAL_CHANNELS; channel <= HRDL_MAX_ANALOG_CHANNELS; channel++)
	{
		if (g_channelSettings[channel].enabled && channel == HRDL_DIGITAL_CHANNELS)
		{
			printf("1234\t");
		}
		else if (g_channelSettings[channel].enabled)
		{
			printf("Ch%d\t", channel);
		}
	}

	printf ("\n(ms)\t");

	for (channel = HRDL_DIGITAL_CHANNELS; channel <= HRDL_MAX_ANALOG_CHANNELS; channel++)
	{
		if (g_channelSettings[channel].enabled && channel == HRDL_DIGITAL_CHANNELS)
		{
			printf(" DO \t");
		}
		else if (g_channelSettings[channel].enabled)
		{
			if (g_scaleTo_mv)
			{
				printf ("(mV)\t");
			}
			else
			{
				printf ("(ADC)\t");
			}
		}
	}
	
	printf ("\n");
	
	//
	// Check to see if an overflow occured during the last data collection
	//
	if (overflow)
	{
		printf("An over voltage occured during the last data run.\n\n");
	}

	timeCount = 0;
	
	// display the first 10 readings for each active channel
	for (i = 0; i < 10 * noOfActiveChannels;)
	{	
		printf ("%ld\t", g_times [timeCount]); 
		
		for (channel = HRDL_DIGITAL_CHANNELS; channel <= HRDL_MAX_ANALOG_CHANNELS; channel++)
		{
			if (g_channelSettings[channel].enabled)
			{
				if (channel == HRDL_DIGITAL_CHANNELS)
				{
					printf("%d%d%d%d\t",  0x01 & (g_values [i]), 0x01 & (g_values [i] >> 0x1), 0x01 & (g_values [i] >> 0x2), 0x01 & (g_values [i] >> 0x3));
					i++;
				}
				else
				{
					printf ("%f\t", AdcToMv ((HRDL_INPUTS) channel, g_values [i++])); 
				}
			}
		}
		printf("\n");   
		timeCount++;
	}                                                                           
}
/****************************************************************************
*
* CollectWindowedBlocks
*  this function demonstrates how to use windowed blocks.
*
* Windowing is useful If you are collecting data slowly (say 10 seconds
* per block), but you want to analyse the data every second.
*
* Each call to HRDLGetTimesAndValues returns the most recent 10 seconds
* of data.
*
****************************************************************************/
#define WINDOWEDBLOCK 16
void CollectWindowedBlocks(void)
{
	int32_t		i;
	int16_t		channel;
	int32_t		noOfReadings;
	int16_t		channelDistribution;
	int16_t		noOfActiveChannels;
	int8_t		strError[80];
	int16_t		status = 1;

	printf("\nCollect windowed block...\n");
	printf("First block appears after 16 seconds,\n");
	printf("Subsequent blocks every second...\n");
	printf("Press a key to start\n");
	_getch();

	for (i = HRDL_ANALOG_IN_CHANNEL_1; i <= g_maxNoOfChannels; i++)
	{
		status = HRDLSetAnalogInChannel(g_device,
										(int16_t)i,
										g_channelSettings[i].enabled, 
										(int16_t) g_channelSettings[i].range,
										g_channelSettings[i].singleEnded);

		if (status == 0)
		{
			HRDLGetUnitInfo(g_device, strError, (int16_t)80, HRDL_SETTINGS);
			printf("Error occurred: %s\n\n", strError);
			return;
		}
	}

	//
	// Collect data at 1 second intervals, with maximum resolution
	//
	HRDLSetInterval(g_device, 1000, HRDL_660MS);

	//
	// Start it collecting,
	// then wait for completion of first block
	//
	status = HRDLRun(g_device, WINDOWEDBLOCK, (int16_t) HRDL_BM_WINDOW);

	if (status == 0)
	{
		HRDLGetUnitInfo(g_device, strError, (int16_t) 80, HRDL_SETTINGS);
		printf("Error occurred: %s\n\n", strError);
		return;
	}

	printf("Waiting for first block...\n");

	while (!HRDLReady(g_device))
	{
		Sleep (100);
	}

	printf("Collected first block\n\n");

	//
	// From here on, we can get data whenever we want...
	// we will receive the last 10 seconds data.
	//
	
	while (!_kbhit ())
	{
		for (channel = HRDL_DIGITAL_CHANNELS; channel <= HRDL_MAX_ANALOG_CHANNELS; channel++)
		{
			if (g_channelSettings[channel].enabled && channel == HRDL_DIGITAL_CHANNELS)
			{
				printf("1234\t");
			}
			else if (g_channelSettings[channel].enabled)
			{
				printf("Ch%d\t", channel);
			}
		}

		printf("\n");
		noOfReadings = HRDLGetValues(g_device, g_values, NULL, WINDOWEDBLOCK);

		//
		// Print out the first 5 readings
		//
		channelDistribution = 0;
		HRDLGetNumberOfEnabledChannels(g_device, &noOfActiveChannels);
		noOfActiveChannels = noOfActiveChannels + (int16_t)(g_channelSettings[HRDL_DIGITAL_CHANNELS].enabled);

		for (i = 0; i < noOfReadings * noOfActiveChannels;)
		{
			for (channel = 0; channel < HRDL_MAX_ANALOG_CHANNELS + 1; channel++)
			{
				if (!g_channelSettings[channel].enabled)
				{
					continue;
				}

				if (g_channelSettings[channel].enabled && channel == HRDL_DIGITAL_CHANNELS)
				{
					printf("%d%d%d%d\t",  0x01 & (g_values [i]), 0x01 & (g_values [i] >> 0x1), 0x01 & (g_values [i] >> 0x2), 0x01 & (g_values [i] >> 0x3));
					i++;
				}
				else
				{
					printf ("%f\t%f", AdcToMv ((HRDL_INPUTS) channel, g_values [i]));
					i++;
				}
			}

			printf("\n");
		}
    
		printf ("Press any key to stop\n\n");

		//
		// Wait a second before asking again      
		// 
		Sleep (1000); 
	}

	HRDLStop(g_device);

	_getch();
}

/****************************************************************************
*
* CollectStreaming
*  this function demonstrates how to use streaming.
*
* In this mode, you can collect data continuously.
*
* This example writes data to disk...
* don't leave it running too long or it will fill your disk up!
*
* Each call to HRDLGetValues returns the readings since the last call
*
****************************************************************************/

void CollectStreaming (void)
{
	int32_t		i;
	int32_t		blockNo;
	FILE *		fp;
	int32_t		nValues;
	int16_t		channel;
	int16_t		numberOfActiveChannels;
	int8_t		strError[80];
	int16_t		status = 1;

	printf("Collect streaming...\n");
	printf("Data is written to disk file (test.csv)\n");
	printf("Press a key to start\n");
	_getch();

	for (i = HRDL_ANALOG_IN_CHANNEL_1; i <= g_maxNoOfChannels; i++)
	{
		status = HRDLSetAnalogInChannel(g_device,
			                         (int16_t)i,
										g_channelSettings[i].enabled, 
										(int16_t) g_channelSettings[i].range,
										g_channelSettings[i].singleEnded);

		if (status == 0)
		{
			HRDLGetUnitInfo(g_device, strError, (int16_t) 80, HRDL_SETTINGS);
			printf("Error occurred: %s\n\n", strError);
			return;
		}
	}

	//
	// Collect data at 1 second intervals, with maximum resolution
	//
	HRDLSetInterval(g_device, 61, HRDL_60MS);

	printf("Starting data collection...\n");

	//
	// Start it collecting,
	status = HRDLRun(g_device, BUFFER_SIZE, (int16_t) HRDL_BM_STREAM);

	if (status == 0)
	{
		HRDLGetUnitInfo(g_device, strError, (int16_t) 80, HRDL_SETTINGS);
		printf("Error occurred: %s\n\n", strError);
		return;
	}

	while (!HRDLReady(g_device))
	{
		Sleep (1000);
	}

	//
	// From here on, we can get data whenever we want...
	//
	blockNo = 0;
	fopen_s(&fp, "test.csv", "w");
  
	if (!fp)
	{
		printf("Error opening output file.");
		return;
	}

	HRDLGetNumberOfEnabledChannels(g_device, &numberOfActiveChannels);
	numberOfActiveChannels = numberOfActiveChannels + (int16_t)(g_channelSettings[HRDL_DIGITAL_CHANNELS].enabled);
  
	while (!_kbhit())
	{ 
		nValues = HRDLGetValues(g_device, g_values, NULL, BUFFER_SIZE/numberOfActiveChannels);
		printf ("%d values\n", nValues);

		for (i = 0; i < nValues * numberOfActiveChannels;)
		{
			for (channel = HRDL_DIGITAL_CHANNELS; channel <= HRDL_MAX_ANALOG_CHANNELS; channel++)
			{
				//
				// Print the channel headers to file
				//
				if (nValues && channel == HRDL_DIGITAL_CHANNELS && g_channelSettings[channel].enabled)
				{
					fprintf (fp, "Digital IO (1 2 3 4):,");
				}
				else if (nValues && g_channelSettings[channel].enabled)
				{
					fprintf (fp, "Channel %d:,", channel);
				}

				//
				// Print to file the new readings
				//

				if (channel == HRDL_DIGITAL_CHANNELS && g_channelSettings[channel].enabled)
				{
					fprintf (fp, "%d %d %d %d,", 0x01 & (g_values [i]),
													0x01 & (g_values [i] >> 0x1),
													0x01 & (g_values [i] >> 0x2),
													0x01 & (g_values [i] >> 0x3));
					i++;
				}
				else if (g_channelSettings[channel].enabled)
				{
					fprintf (fp, "%f,", AdcToMv ((HRDL_INPUTS) channel, g_values [i]));
					i++;
				}
			}

			if (nValues && g_channelSettings[channel].enabled)
			{
  				fprintf (fp, "\n");
			}
		}

		if ((blockNo++  % 20) == 0)
		{
			printf ("Press any key to stop\n");

			if (nValues)
			{
				fprintf (fp, "\n");
			}
		
			//
			// Wait 100ms before asking again
			//

			Sleep (100);
		}

	}

	fclose (fp);
	HRDLStop(g_device);
	_getch ();   
}
/****************************************************************************
*
* CollectSingle using blocking Api Calls
*  This function demonstrates how to collect analogue values one at a time.
*  This function also demonstrates how to set and discover what digital IO 
*	values.
*
* In this mode, you can collect data and manage your own timing
*
*  
*
****************************************************************************/
void CollectSingleBlocked (void)
{
	int16_t channel;
	int32_t value;

	printf("\n");

	// Get the analogue input mesarements
	for (channel = HRDL_ANALOG_IN_CHANNEL_1; channel <= HRDL_MAX_ANALOG_CHANNELS; channel++)
	{              
		if (!g_channelSettings[channel].enabled)
		{
			continue;
		}

		if (!HRDLGetSingleValue(g_device, channel, HRDL_2500_MV, HRDL_660MS, TRUE, NULL,&value))
		{
			printf ("Channel %d not converted\n", channel); 
		}
		else
		{
			printf ("Channel %d:\t%f\n", channel, AdcToMv ((HRDL_INPUTS) channel, value)); 
		}
	}

	if (g_channelSettings[HRDL_DIGITAL_CHANNELS].enabled)
	{
		// If digital IO is available on this unit, check the status of the inputs    
		if (HRDLGetSingleValue(g_device, HRDL_DIGITAL_CHANNELS, 0, 0, 0, NULL, &value))
		{
			printf("Digital Channel %d %d\n", 1, (value & HRDL_DIGITAL_IO_CHANNEL_1) == HRDL_DIGITAL_IO_CHANNEL_1);
			printf("Digital Channel %d %d\n", 2, (value & HRDL_DIGITAL_IO_CHANNEL_2) == HRDL_DIGITAL_IO_CHANNEL_2);
			printf("Digital Channel %d %d\n", 3, (value & HRDL_DIGITAL_IO_CHANNEL_3) == HRDL_DIGITAL_IO_CHANNEL_3);
			printf("Digital Channel %d %d\n", 4, (value & HRDL_DIGITAL_IO_CHANNEL_4) == HRDL_DIGITAL_IO_CHANNEL_4);
		}
	}
    
}

/****************************************************************************
*
* CollectSingle using blocking Api Calls
*  this function demonstrates how to collect analogue values one at a time.
*  This function also demonstrates how to set and discover what digital IO values.
*
* In this mode, you can collect data and manage your own timing
*
****************************************************************************/
void CollectSingleUnblocked (void)
{
	int8_t strError[80];
	int16_t channel;
	int32_t value;

	printf("\n");

	// Get the analogue input measurements
	for (channel = HRDL_ANALOG_IN_CHANNEL_1; channel <= HRDL_MAX_ANALOG_CHANNELS; channel++)
	{              
		if (!g_channelSettings[channel].enabled)
		{
			continue;
		}

		if (!HRDLCollectSingleValueAsync(g_device, channel, HRDL_2500_MV, HRDL_660MS, TRUE))
		{
			HRDLGetUnitInfo(g_device, strError, (int16_t)80, HRDL_SETTINGS);
			printf("Error occurred: %s\n\n", strError);
			return;
		}

		// When using this set of functions it is possible to
		// perform other tasks and not just sleep
		while(!HRDLReady(g_device))
		{
			Sleep(50);
		}

		if (!HRDLGetSingleValueAsync(g_device, &value, NULL))
		{
			printf ("Channel %d not converted\n", channel); 
		}
		else
		{
			printf ("Channel %d:\t%f\n", channel, AdcToMv ((HRDL_INPUTS) channel, value)); 
		}
	}

	if (g_channelSettings[HRDL_DIGITAL_CHANNELS].enabled)
	{
		// If digital IO is available on this unit, check the status of the inputs    
		if (HRDLGetSingleValue(g_device, HRDL_DIGITAL_CHANNELS, 0, 0, 0, NULL, &value))
		{
			printf("Digital Channel %d %d\n", 1, (value & HRDL_DIGITAL_IO_CHANNEL_1) == HRDL_DIGITAL_IO_CHANNEL_1);
			printf("Digital Channel %d %d\n", 2, (value & HRDL_DIGITAL_IO_CHANNEL_2) == HRDL_DIGITAL_IO_CHANNEL_2);
			printf("Digital Channel %d %d\n", 3, (value & HRDL_DIGITAL_IO_CHANNEL_3) == HRDL_DIGITAL_IO_CHANNEL_3);
			printf("Digital Channel %d %d\n", 4, (value & HRDL_DIGITAL_IO_CHANNEL_4) == HRDL_DIGITAL_IO_CHANNEL_4);
		}
	}
    
}

/****************************************************************************
*
* SetAnalogChannels
*  this function demonstrates how to detect available input range and set it.
*  We will first check to see if a channel is available, then check what ranges
*  are available and then check to see if differential mode is supported for thet
*  channel.
*
****************************************************************************/
void SetAnalogChannels(void)
{ 
	int32_t channel;
	int16_t range;
	int16_t available;
	int16_t status = 1;

	printf("\n");

	//
	// Iterate through the channels on the device and if one is available give the
	// user the option of using one;
	//
	// A channel may not be available if:
	//
	// a) it is not available on the current device,
	// b) the input is a secondary differential input and is already in use for a differential channel or
	// c) the input is a primary differential input and cannot be used for a differential channel because the
	//    secondary input of the pair is already in use for a single ended channel.
	//
	// Primary inputs for differential pairs are odd channel numbers eg  1, 3, 5, etc. Their corresponding
	// secondary numbers are primary channel number + 1 eg 1 and 2, 3 and 4, etc.
	//
	// You should firstly make sure that the channel is available on the current unit
	// and secondly ensure that the input, or both inputs for a differential channel,
	// are not already in use for another channel.


	for (channel = HRDL_ANALOG_IN_CHANNEL_1; channel <= g_maxNoOfChannels; channel++)
	{
		printf("%2d - Channel %d\n", channel, channel); 
	}

	//
	// Let the user select the channel

	printf("Select a channel..\n");
	channel = 0;
  
	do 
	{
		scanf_s("%d", &channel);  

	} while (channel < HRDL_ANALOG_IN_CHANNEL_1 || channel > g_maxNoOfChannels);

	printf("Enable the channel? (Y\\N)\n\n");  
	g_channelSettings[channel].enabled = (int16_t) toupper(_getch()) == 'Y';

	//
	// Disable the channel if the user does not require it

	if (!g_channelSettings[channel].enabled)
	{
		printf("Channel %d disabled\n\n", channel);
		HRDLSetAnalogInChannel(g_device, (int16_t)channel, (int16_t) 0, (int16_t) HRDL_1250_MV, (int16_t) 1);
		return;
	}          

	//
	// Iterate through all the input ranges, if the range is available on
	// the current device, give the user the option of using it.

	available = 0;

	for (range = 0; range < HRDL_MAX_RANGES; range++)
	{
		status = HRDLSetAnalogInChannel(g_device, (int16_t) channel, (int16_t) 1, (int16_t) range, (int16_t) 1);

		if (status == 1)
		{
			printf("%d - %dmV\n", range, (int32_t)(2500/inputRangeV[range]));    
			available = 1;
		}
	}

	if (!available)
	{
		printf("Channel is not available for use:\n ");
		
		if ((channel & 0x01) && g_channelSettings[channel + 1].enabled )  // odd channels
		{
			printf("The channel cannot be enabled because it is a primary differential channel  \
						and its corresponding secondary channel is already in use for a single ended measurement\n");           
		}
		else if (g_channelSettings[channel - 1].enabled)
		{
			printf("The channel cannot be enabled because it is a secondary differential channel  \
						and is already in use for a differential measurement\n");
		}
		else
		{
			printf("This channel cannot be enabled because it is not available on this Pico HRDL variant\n");
		}                                         
		return;
	}

	//
	// Let the user select the range
	printf("Select Range...\n");
	
	do 
	{
		g_channelSettings[channel].range = (HRDL_RANGE) (_getch() - '0');

	} while (!HRDLSetAnalogInChannel(g_device, (int16_t) channel, (int16_t) 1, (int16_t) g_channelSettings[channel].range, (int16_t) 1));

	//
	// See if it possible to use this channel as differential.
	// 
	// It may not be used as differential if this input is a secondary differential input
	// or this input is a primary differential input and the corresponding secondary input
	// is already in use for a single ended channel.

	if (HRDLSetAnalogInChannel(g_device, (int16_t)channel, (int16_t) 1, (int16_t) g_channelSettings[channel].range, (int16_t) 0))
	{
		printf("Single ended? (Y\\N)");  
		g_channelSettings[channel].singleEnded = (int16_t) toupper(_getch()) == 'Y';
	}
	else
	{
		g_channelSettings[channel].singleEnded = 1;           
	}

	HRDLSetAnalogInChannel(g_device, (int16_t) channel, (int16_t) 1, (int16_t) g_channelSettings[channel].range, g_channelSettings[channel].singleEnded);

	// Let the user know what they have set
	printf("\nChannel %d, %dmV range, %s\n\n", channel, (int32_t)(2500 / inputRangeV[g_channelSettings[channel].range]), g_channelSettings[channel].singleEnded ? "single ended" : "differential");

}
/****************************************************************************
*
* SetDigitalChannels
*  this function demonstrates how to detect available digital inputs.
*
****************************************************************************/
void SetDigitalChannels(void)
{
	int16_t channel = 0;
	int16_t directionOut = 0;
	int16_t pinState = 0;
	int16_t status = 1;

	printf("\n");

	// Check to see if the digital channels are available on this variant

	status = HRDLSetDigitalIOChannel(g_device, directionOut, pinState, 1);

	if (status == 0)
	{
		printf("No Digital IO available on this device.\n");
		return;
	}

	// Iterate through the channels, asking for direction and digital out pin state
	for (channel = 0; channel < HRDL_MAX_DIGITAL_CHANNELS; channel++)
	{
		printf("Set digital %d, direction input? (Y/N)\n", channel + 1);

		if (toupper(_getch()) == 'N')
		{
			directionOut += 0x01 << channel;
			printf("Set digital out %d, high? (Y/N)\n", channel + 1);

			if (toupper(_getch()) == 'Y')
			{
				pinState += 0x01 << channel;
			}
		}
	}
	
	HRDLSetDigitalIOChannel(g_device, directionOut, pinState, 1);
	g_channelSettings[HRDL_DIGITAL_CHANNELS].enabled = TRUE;
	printf("Digital channels set.\n");
}

/****************************************************************************
*
* OpenDevice
*  this function demonstrates how to open the next avaiable unit
*
****************************************************************************/
int16_t OpenDevice(int16_t async)
{
	int16_t device = 0;

	if (async)
	{ 
		//
		// Start the Asynchronous opening routine
		//
		if (HRDLOpenUnitAsync())
		{
			//
			// You can now go and do other things while the unit is opening
			//                                   
			while (HRDLOpenUnitProgress(&device, NULL) == HRDL_OPEN_PROGRESS_PENDING)
			{
				printf(".");
				Sleep(500);
			}

			printf("\n");
		}
	}
	else
	{    
		//
		// Start the opening routine, this will block until the 
		// device open routine completes
		//
		device = HRDLOpenUnit();    
	} 

	return device;
}

/****************************************************************************
*
* SelectUnit
*  this function demonstrates how to open all available units and
*  select the required one
*
****************************************************************************/
int16_t SelectUnit(void)
{
	int16_t devices[HRDL_MAX_PICO_UNITS];
	int8_t line[80];

	int16_t async; 
	int16_t i;   

	int16_t deviceToUse = 0;
	int16_t ndevicesFound = 0;     

	printf("\n\nOpen devices Asynchronously (Y/N)?"); 
	async = (int16_t) toupper (_getch ()) == 'Y';

	printf ("\n\nOpening devices.\n");
	
	for (i = 0; i < HRDL_MAX_UNITS; i++)
	{
		devices[i] = OpenDevice(async);

		//
		// if the device is available give the user the option of using it
		//
		if (devices[i] > 0)
		{
			HRDLGetUnitInfo(devices[i], line, sizeof (line), HRDL_BATCH_AND_SERIAL);
			printf("%d: %s\n", i + 1, line);         
			ndevicesFound++;
		}
		else
		{
			HRDLGetUnitInfo(devices[i], line, sizeof (line), HRDL_ERROR);

			if (atoi(line) == HRDL_NOT_FOUND)
			{
				printf("%d: No Unit Found\n", i + 1);               
			}
			else
			{
				printf("%d: %s\n", i+1, line);               
			}
		}
	}

	//
	// If there is more than one device available, let the user choose now
	//          
	if (ndevicesFound > 1)
	{           
		//
		// Now let the user choose a device to use
		//
		printf("Choose the unit from selection above\n");
		
		do
		{
			deviceToUse = ((int16_t) _getch() - '0') - 1;

		} while( (deviceToUse < 0 || deviceToUse > HRDL_MAX_PICO_UNITS ) && devices[deviceToUse] > 0);

		//
		// Finally, close all the units that we didnt want
		//
		for (i = 0; i < HRDL_MAX_PICO_UNITS; i++)
		{
			if ( (i != deviceToUse) && (devices[i] > 0))
			{
				HRDLCloseUnit(devices[i]);
			}
		}    
	}
	else if (ndevicesFound == 1)
	{
		//
		// Select the only device found
		//
		for (i = 0; i < HRDL_MAX_PICO_UNITS; i++)
		{
			if (devices[i] > 0)
			{
				deviceToUse = i;
				break;
			}              
		}         
	}

	return ndevicesFound > 0 ? devices[deviceToUse] : 0;

} 

/****************************************************************************
*
*
****************************************************************************/
void main (void)
{ 
	int32_t		ok = 0;
	int8_t 		line [80];  
	int16_t		lineNo;

	int8_t		ch;

	int8_t description[7][25] = { "Driver Version    :", 
									"USB Version       :", 
									"Hardware Version  :", 
									"Variant Info      :", 
									"Batch and Serial  :", 
									"Calibration Date  :", 
									"Kernel Driver Ver.:"};

	g_doSet = FALSE;
	printf("HRDL driver example program for ADC-20/24 data loggers\n");
	printf("Version 1.2\n");
	printf("Copyright 2004 - 2017 Pico Technology Ltd.\n");
  
	memset(g_channelSettings, 0, sizeof(g_channelSettings));

	g_device = SelectUnit();

	ok = g_device > 0;                             

	if (!ok)
	{
		printf("Unable to open device\n");
		HRDLGetUnitInfo(g_device, line, sizeof (line), HRDL_ERROR);
		printf("%s\n", line);
		exit(99);
	}
	else
	{
		printf("Device opened successfully.\n\n");   
		printf("Device Information\n");
		printf("==================\n\n");

		//
		// Get all the information related to the device
		//
		for (lineNo = 0; lineNo < HRDL_ERROR; lineNo++)
		{
			HRDLGetUnitInfo(g_device, line, sizeof (line), lineNo);
				
			if (lineNo == HRDL_VARIANT_INFO)
			{
				switch(atoi(line))
				{
					case 20:
						g_maxNoOfChannels = 8;
						break;
					
					case 24:
						g_maxNoOfChannels = 16;
						break;
					
					default:
						printf("Invalid unit type returned from driver");
						return;
				}
			}
			
			if (lineNo == HRDL_VARIANT_INFO)
			{
				printf("%s ADC-%s\n", description[lineNo], line);
			}
			else
			{
				printf("%s %s\n", description[lineNo], line);
			}
		}

		printf("\n");

		printf ("Convert ADC counts to mV? (Y/N): \n");
		g_scaleTo_mv = toupper (_getch ()) == 'Y';

		printf ("Reject 50Hz mains noise? (Y/N): \n");
		
		if (toupper (_getch ()) == 'Y')
		{
			HRDLSetMains(g_device, 1);
		}
		else
		{ 
			HRDLSetMains(g_device, 0);
		}

		SetAnalogChannels();

	ch = ' ';  

	while (ch != 'X')
	{
		printf("\n");
		printf("Select an operation:\n\n");
		printf("B - Immediate block\n");
		printf("W - Windowed block\n");
		printf("S - Streaming\n");
		printf("U - Single readings\n");
		printf("A - Set analog channels \n");
		printf("D - Set digital channels \n");
		printf("X - Exit\n");
		printf("Operation: ");
		ch = (int8_t) toupper(_getch());
		printf("\n");
		
		switch (ch)
		{
			case 'B':
			CollectBlockImmediate();
			break;

			case 'W':
			CollectWindowedBlocks();
			break;

			case 'S':
			CollectStreaming();
			break;  

			case 'R':
			CollectSingleBlocked();
			break;

			case 'U':
			CollectSingleUnblocked();
			break;

			case 'A':
			SetAnalogChannels();
			break;

			case 'D':
			SetDigitalChannels();
			break;

			case 'X':
			/* Handled by outer loop */
			break;

			default:
			printf("Invalid operation\n");
			break;
			}
		}

		// 
		// Close the device so that it is available for other apps
		//
		HRDLCloseUnit(g_device);
	}   
}
