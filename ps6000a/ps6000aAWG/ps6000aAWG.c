/*******************************************************************************
 *
 * Filename: ps6000aAWG.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use some of 
 *	 the PicoScope 6XXXE Series (ps6000a) driver API functions to perform operations
 *	 using a PicoScope 3XXXE Oscilloscope.
 *
 *	Supported PicoScope models:
 *
 *      All 6XXXE model numbers and any
 *		PicoScope ps6000a API units
 *
 * Example:
 *   Demo of controlling the Signal generator and AWG
 *   
 * 
 *   With the following options:
 *   -Change frequencies & voltage scales
 *   -Display in V & Hz
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express/Community Edition) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x64)
 *			Ensure that the 64-bit ps6000a.lib can be located
 *			Ensure that the ps6000aApi.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 64-bit console mode application
 *			 Add this file to the project
 *			 Add ps6000a.lib to the project (Microsoft C only)
 *			 Add ps6000aApi.h and PicoStatus.h to the project
 *			 Build the project
 *
 * Copyright (C) 2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include <math.h>

#include "../shared/Libps6000a.h"
#include "../shared/LibStreamingps6000a.h"
#include "../shared/LibAWGps6000a.h"

/* Headers for Windows */
#ifdef _WIN32
#include "ps6000aApi.h"
#include "windows.h"
#include <conio.h>
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
#endif

#endif

/****************************************************************************
* Refernce Global Variables
***************************************************************************/

extern BOOL		scaleVoltages; //defined and used in Libps6000a.c
/***************************************************************************/

/****************************************************************************
* mainMenu
* Controls default functions of the seelected unit
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
static void mainMenu(GENERICUNIT*unit, SIG_GEN_SETTINGS* sigGenSettings)
{
	int8_t ch = '.';
	while (ch != 'X')
	{
		//displaySettings(unit); //printf the current channel settings of the device

		printf("\n\n");
		printf("|\t\t\t\tAWG Mode Example\t\t\t\t|\n");
		printsigGenSettings(unit, sigGenSettings);
		printf("\n");
		printf("Please select operation:\n");
		printf("--- Wave types --------------------------------\n");
		printf("S - Sine wave\n");
		printf("Q - Square wave\n");
		printf("R - Triangle wave\n");
		//printf("U - Ramp Up\n");
		//printf("D - Ramp Down\n");
		printf("A - AWG\n");
		printf("C - DC\n");
		printf("--- Output Settings ---------------------------\n");
		printf("F - Set Frequency\n");
		printf("V - Set Peak voltage Output\n");
		printf("O - Set Offset voltage Output\n");
		printf("--- Sweep Settings ----------------------------\n");
		printf("E - Toggle ON / OFF\n");
		printf("Y - Set Stop Frequency\n");
		printf("I - Set frequency increment (Hz)\n");
		printf("N - Set increment time (ms)\n");
		printf("--- Triggering --------------------------------\n");
		printf("T - Toggle ON / OFF\n");
		printf("B - Trigger on Ext\n");
		printf("M - Manual trigger\n");
		printf("--- AWG ---------------------------------------\n");
		printf("L - Load AWG file\n");
		printf("X-- EXIT --------------------------------------\n");
		printf("Operation:");
		sigGenSettings->Enabled = TRUE; //set enabled to TRUE after every operation
		ch = toupper(_getch());

		printf("\n\n");

		switch (ch) 
		{
			case 'S':
				SineWave(unit, sigGenSettings);
				break;

			case 'Q':
				SquareWave(unit, sigGenSettings);
				break;

			case 'R':
				TriangleWave(unit, sigGenSettings);
				break;

			case 'C':
				dc(unit, sigGenSettings);
				break;

			case 'A':
				AWG(unit, sigGenSettings);
				break;

			case 'L':
				AWGLoadFile(unit, sigGenSettings);
				break;

			case 'V':
				AWGSetPeaktoPVoltage(unit, sigGenSettings);
				break;

			case 'O':
				AWGSetOffsetVoltage(unit, sigGenSettings);
				break;

			case 'F': 
				AWGSetFrequency(unit, sigGenSettings);
				break;

			case 'Y':
				AWGSetFrequencyStop(unit, sigGenSettings);
				break;

			case 'I':
				AWGSetFrequencyInc(unit, sigGenSettings);
				break;

			case 'N':
				AWGSetSweepTimeInc(unit, sigGenSettings);
				break;

			case 'E':
				SweepOnOff(unit, sigGenSettings);
				break;

			case 'T':
				SigGenTriggerOnOff(unit, sigGenSettings);
				break;

			case 'M':
				SigGenTriggerNow(unit, sigGenSettings);
				break;

			case 'B':
				SigGenTriggerExt(unit, sigGenSettings);
				break;
				
			case 'X': //Exit the application
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
	SIG_GEN_SETTINGS sigGenSettings[MAX_PICO_DEVICES] = { 0 };

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
			status = handleDevice(&allUnits[0], &sigGenSettings[0]);
		}

		if (status != PICO_OK)
		{
			printf("Picoscope devices open failed, error code 0x%x\n",(uint32_t)status);
			free(allUnits);
			return 1;
		}

		mainMenu(&allUnits[0], &sigGenSettings[0]);
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
		status = handleDevice(&allUnits[0], &sigGenSettings[0]);
		
		if (status != PICO_OK)
		{
			printf("Picoscope device open failed, error code 0x%x\n", (uint32_t)status);
			free(allUnits);
			return 1;
		}
		
		mainMenu(&allUnits[listIter], &sigGenSettings[listIter]);
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
					status = handleDevice(&allUnits[0], &sigGenSettings[0]);
				}
				
				if (status != PICO_OK)
				{
					printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status);
					free(allUnits);
					return 1;
				}

				mainMenu(&allUnits[listIter], &sigGenSettings[listIter]);

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
