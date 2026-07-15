/*******************************************************************************
 *
 * Filename: psospaStreaming.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use some of 
 *	 the PicoScope 3XXXE Series (psospa) driver API functions to perform operations
 *	 using a PicoScope 3XXXE Oscilloscope.
 *
 *	Supported PicoScope models:
 *
 *      All 3XXXE model numbers and any
 *		PicoScope psospa API units
 *
 * Examples:
 *   Collect Streaming blocks of samples immediately
 *   Collect Streaming blocks of samples when a trigger event occurs
 * 
 *   With the following options:
 *   -Change timebase & voltage scales
 *   -Display data in mV or ADC counts
 *	 -Handle power source changes
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express/Community Edition) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x64)
 *			Ensure that the 64-bit psospa.lib can be located
 *			Ensure that the psospaApi.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 64-bit console mode application
 *			 Add this file to the project
 *			 Add psospa.lib to the project (Microsoft C only)
 *			 Add psospaApi.h and PicoStatus.h to the project
 *			 Build the project
 *
 * Copyright (C) 2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <math.h>

#include "../shared/Libpsospa.h"
#include "../shared/LibStreamingpsospa.h"

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "psospaApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>

#include <psospaApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif

#endif

/****************************************************************************
* Refernce Global Variables
***************************************************************************/

extern BOOL				scaleVoltages; //defined and used in Libpsospa.c
extern const uint64_t	constBufferSize; //defined and used in Libpsospa.c
/***************************************************************************/

/****************************************************************************
* mainMenu
* Controls default functions of the seelected unit
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
static void mainMenu(GENERICUNIT*unit)
{
	int8_t ch = '.';
	while (ch != 'X')
	{
		displaySettings(unit);

		printf("\n\n");
		printf("Streaming Mode Example\n");
		printf("Please select operation:\n\n");

		printf("S - Immediate Streaming                       V - Set Voltages\n");
		printf("T - Triggered Streaming                       I - SetTimebase\n");
		printf("J - GetMoreData Stopped                       A - ADC counts/mV\n");	
		printf("                                              D - Set Resolution\n");
		if (unit->digitalPortCount != 0)
			printf("                                              M - Set Digital Ports (MSO)\n");
		printf("                                              X - Exit\n");
		printf("Operation:");

		ch = toupper(_getch());

		printf("\n\n");

		switch (ch) 
		{
			case 'S':
				// Trigger disabled
				PICO_STATUS status = psospaSetSimpleTrigger(unit->handle, 0, PICO_CHANNEL_A, 0, PICO_RISING, 0, 0);
				streamDataHandler(unit,
					0,						// noOfPreTriggerSamples - Used by RunStreaming()
					constBufferSize,		// noOfPostTriggerSamples - Used by RunStreaming()
					1,						// idealTimeInterval - Used by RunStreaming()
					PICO_US,				// sampleIntervalTimeUnits - Used by RunStreaming()
					constBufferSize,		// nSamples - Set the number of samples per capture - Used by SetDataBuffers()
					PICO_RATIO_MODE_RAW,	// ratioMode - Used by SetDataBuffers()
					1,						// downSampleRatio - Used by SetDataBuffers()
					0,						// autostop
					FILE_TXT,				// Save data as CSV file
					TRUE);					// imagefile - create image file of data
				break;

			case 'T':
				SetupTrigger(unit);
				streamDataHandler(unit,
					1024,					// noOfPreTriggerSamples - Used by RunStreaming()
					constBufferSize - 1024,	// noOfPostTriggerSamples - Used by RunStreaming()
					1,						// idealTimeInterval - Used by RunStreaming()
					PICO_US,				// sampleIntervalTimeUnits - Used by RunStreaming()
					constBufferSize,		// nSamples - Set the number of samples per capture - Used by SetDataBuffers()
					PICO_RATIO_MODE_RAW,	// ratioMode - Used by SetDataBuffers()
					1,						// downSampleRatio - Used by SetDataBuffers()
					1,						// autostop
					FILE_TXT,				// Save data as CSV file
					TRUE);					// imagefile - create image file of data
				break;

			case 'J':
				GetMoreDataHandler(unit,
					PICO_RATIO_MODE_RAW,
					1,
					constBufferSize,
					FILE_TXT,
					TRUE);					// imagefile - create image file of data
				break;

			case 'V':
				setVoltages(unit);
				break;

			case 'M':
				if (unit->digitalPortCount != 0)
				{
					setDigitalPorts(unit);
				}			
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
int main(void)
{
	int ch;
	unsigned int devCount = 0, listIter = 0,	openIter = 0;
	//device indexer -  64 chars - 64 is maximum number of picoscope devices handled by driver
	char devChars[] =
			"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#";
	PICO_STATUS status = PICO_OK;
	GENERICUNIT* allUnits = (GENERICUNIT*)calloc(MAX_PICO_DEVICES, sizeof(GENERICUNIT));
	if(allUnits != NULL)
		allUnits[0] = (GENERICUNIT){ 0 }; // Initialize first element to zero

	printf("PicoScope 3000E Series (psospa) Driver Example \n");
	printf("\nEnumerating Units...\n");

	do
	{
		if(allUnits != NULL)
			status = openDevice(&(allUnits[devCount]), NULL);
		else
			return 1;
		
		if (status == PICO_OK)
		{
			allUnits[devCount++].openStatus = (int16_t) status;
		}

	} while(status != PICO_NOT_FOUND);

	if (devCount == 0)
	{
		printf("Picoscope devices not found\n");
		free(allUnits);
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
			status = handleDevice(&allUnits[0], NULL);
		}

		if (status != PICO_OK)
		{
			printf("Picoscope devices open failed, error code 0x%x\n",(uint32_t)status);
			free(allUnits);
			return 1;
		}

		mainMenu(&allUnits[0]);

		closeDevice(&allUnits[0]);
		printf("Exit...\n");
		free(allUnits);
		return 0;
	}
	else
	{
		// More than one unit
		printf("Found %u devices, initializing...\n\n", devCount);

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
		free(allUnits);
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
		status = handleDevice(&allUnits[listIter], NULL);
		
		if (status != PICO_OK)
		{
			printf("Picoscope device open failed, error code 0x%x\n", (uint32_t)status);
			free(allUnits);
			return 1;
		}
		
		mainMenu(&allUnits[listIter]);
		closeDevice(&allUnits[listIter]);
		printf("Exit...\n");
		free(allUnits);
		return 0;
	}
	printf("Found %u devices, pick one to open from the list:\n", devCount);

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
					status = handleDevice(&allUnits[listIter], NULL);
				}
				
				if (status != PICO_OK)
				{
					printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status);
					free(allUnits);
					return 1;
				}

				mainMenu(&allUnits[listIter]);

				printf("Found %u devices, pick one to open from the list:\n",devCount);
				
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
	free(allUnits);
	return 0;
}
