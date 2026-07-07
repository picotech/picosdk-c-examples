/*******************************************************************************
 *
 * Filename: ps4000aBlock.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use some of 
 *	 the PicoScope 4000 Series (ps4000a) driver API functions to perform operations
 *	 using a PicoScope 4000 Oscilloscope.
 *
 *	Supported PicoScope models:
 *
 *      All 4XXX model numbers and any
 *		PicoScope 4000a API units
 *
 * Examples:
 *   Collect a block of samples immediately
 *   Collect a block of samples when a trigger event occurs
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
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps4000a.lib can be located
 *			Ensure that the ps4000aApi.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps4000a.lib to the project (Microsoft C only)
 *			 Add ps4000aApi.h and PicoStatus.h to the project
 *			 Build the project
 *
 * Copyright (C) 2026 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <math.h>

#include "../shared/Libps4000a.h"
#include "../shared/LibBlockps4000a.h"

/* Headers for Windows */
#ifdef _WIN32
#include "ps4000aApi.h"
#include "windows.h"
#include <conio.h>
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>

#include <ps4000aApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif

#endif

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL				scaleVoltages;
extern const uint64_t	constBufferSize; //defined and used in Libps4000a.c
extern int16_t			g_probeStateChanged;
/***************************************************************************/

/****************************************************************************
* mainMenu
* Controls default functions of the seelected unit
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
static void mainMenu(GENERICUNIT *unit)
{
	int8_t ch = '.';
	while (ch != 'X')
	{
		displaySettings(unit);

		printf("\n\n");
		printf("Block Mode Example\n");
		printf("Please select operation:\n\n");

		printf("B - Immediate Block                           V - Set Voltages\n");
		printf("T - Triggered Block                           I - SetTimebase\n");
		printf("J - Get Data More                             A - ADC counts/mV\n");	
		if (unit->model == MODEL_4444)
			printf("O - Triggered BlockOverlapped (looped blocks) D - Set Resolution\n");
		else
			printf("O - Triggered BlockOverlapped (looped blocks)\n");
		printf("                                              X - Exit\n");
		printf("Operation:");

		ch = toupper(_getch());

		if (g_probeStateChanged == 1)
		{
			printf("\nProbe state change detected.\n");
			g_probeStateChanged = 0;
			// ProbestoSettings() -
			// Looks at the callback probe updated array and updates the unit channel settings accordingly
			// Modifies the channel settings - Modify the function as needed for your application.
			ProbestoSettings(unit);
			setDefaults(unit); // Put these changes into effect
		}

		printf("\n\n");

		switch (ch) 
		{
			case 'B':
				// Trigger disabled
				PICO_STATUS status = ps4000aSetSimpleTrigger(unit->handle, 0, PS4000A_CHANNEL_A, 0, PICO_RISING, 0, 0);
				blockDataHandler(unit,
									0,						// noOfPreTriggerSamples - on Device
									constBufferSize,		// noOfPostTriggerSamples - on Device
									0,						// idealTimeInterval - 0 find max. sample rate
									constBufferSize,		// nSamples - PC buffer size
									PS4000A_RATIO_MODE_NONE,	// ratioMode - Used by Buffer
									1,						// downSampleRatio - Used by Buffer
									FILE_TXT,
									TRUE);					// imagefile - create image file of data				
				break;

			case 'O':
				SetupTrigger(unit);
				blockOverlappedDataHandler(unit,
									0,						// noOfPreTriggerSamples - on Device
									constBufferSize,		// noOfPostTriggerSamples - on Device
									0,						// idealTimeInterval - 0 find max. sample rate
									constBufferSize,		// nSamples - PC buffer size
									PS4000A_RATIO_MODE_NONE,	// ratioMode - Used by Buffer
									1);						// downSampleRatio - Used by Buffer
				break;

			case 'T':
				SetupTrigger(unit);
				blockDataHandler(unit,
									0,						// noOfPreTriggerSamples - on Device
									constBufferSize,		// noOfPostTriggerSamples - on Device
									0,						// idealTimeInterval - 0 find max. sample rate
									constBufferSize,		// nSamples - PC buffer size
									PS4000A_RATIO_MODE_NONE,	// ratioMode - Used by Buffer
									1,						// downSampleRatio - Used by Buffer
									FILE_TXT,
									TRUE);					// imagefile - create image file of data	
				break;

			case 'J':	
				GetMoreDataHandler(unit,
									PS4000A_RATIO_MODE_DECIMATE,	// ratioMode - Used by Buffer
									16,						// downSampleRatio - Used by Buffer
									constBufferSize,       // nSamples - PC buffer size
									FILE_TXT,
									TRUE);					// imagefile - create image file of data
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
				if (unit->model == MODEL_4444)
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
	GENERICUNIT* allUnits = (GENERICUNIT*)calloc(MAX_PICO_DEVICES, sizeof(GENERICUNIT));
	if(allUnits != NULL)
		allUnits[0] = (GENERICUNIT){ 0 }; // Initialize first element to zero

	printf("PicoScope 4000 Series (ps4000a) Driver Example \n");
	printf("\nEnumerating Units...\n");

	int16_t count = 0;
	int8_t serials[100];
	int16_t serialsLength = 100;

	status = ps4000aEnumerateUnits(&count, serials, &serialsLength);

	if (status == PICO_OK)
	{
		printf("Found %d devices - serial numbers: %s\n", count, serials);
	}

	do
	{
		if(allUnits != NULL)
			status = openDevice(&(allUnits[devCount]), NULL);
		else
			return 1;
		
		if (status == PICO_OK || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_NOT_CONNECTED)
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

		if (status == PICO_OK || status == PICO_POWER_SUPPLY_NOT_CONNECTED || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			set_info(&allUnits[0]);
			status = handleDevice(&allUnits[0], NULL);
		}

		if (status != PICO_OK && status != PICO_POWER_SUPPLY_NOT_CONNECTED)
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
					status = handleDevice(&allUnits[listIter], NULL);
				}
				
				if (status != PICO_OK)
				{
					printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status);
					free(allUnits);
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
	free(allUnits);
	return 0;
}
