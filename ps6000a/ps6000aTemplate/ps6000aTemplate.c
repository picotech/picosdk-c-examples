/*******************************************************************************
 *
 * Filename: ps6000aTemplate.c
 *
 * Description:
 * 
 * Main code for template example for PicoScope 6XXXE Series (ps6000a) driver.
 * THIS FILE IS NOT INTENDED TO BE MODIFIED BY THE USER.
 * For User Setup See UserSetup.c file to set up devicce and capture parameters
 *
 *	Supported PicoScope models:
 *
 *      All 6XXXE model numbers and any
 *		PicoScope 6000a API units
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
 * Copyright (C) 2026 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <math.h>

#include "../shared/Libps6000a.h"
#include "../shared/LibStreamingps6000a.h"
#include "../../shared/PicoScaling.h"
#include "./UserSetup.h"

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
#include <unistd.h>
#include <stdlib.h>

#include <ps6000aApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#include "./UserSetup.c"
#endif

#endif

/****************************************************************************
* Refernce Global Variables
***************************************************************************/

extern BOOL		scaleVoltages; //defined and used in Libps6000a.c
/***************************************************************************/

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

	printf("PicoScope 6000E Series (ps6000a) Driver Example \n");
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

		userSetup(&allUnits[0]);

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
		
		userSetup(&allUnits[listIter]);
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

				userSetup(&allUnits[listIter]);

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
