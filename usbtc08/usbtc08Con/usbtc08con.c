/******************************************************************************
 *
 * Filename: usbtc08con.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to setup and 
 *	 collect data from a USB TC-08 Thermocouple Data Logger using the 
 *	 usbtc08 driver API functions.
 *
 * Examples:
 *    Collect a single reading from each channel
 *    Collect readings continuously from each channel
 *
 * To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit usbtc08.lib can be located
 *			Ensure that the usbtc08.h file can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add usbtc08.lib to the project (Microsoft C only)
 *			 Add usbtc08.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libusbtc08 driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the usbtc08con application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2007 - 2017 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/
#include <stdio.h>

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "usbtc08.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#include <libusbtc08-1.8/usbtc08.h>

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

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
	newt.c_lflag &= ~(ICANON | ECHO);
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
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return bytesWaiting;
}

int32_t fopen_s(FILE ** a, const char * b, const char * c)
{
	FILE * fp = fopen(b, c);
	*a = fp;
	return (fp > 0) ? 0 : -1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

#define PREF4 __stdcall

#define BUFFER_SIZE 1000	// Buffer size to be used for streaming mode captures

int32_t main(void)
{
	int16_t handle = 0;									/* The handle to a TC-08 returned by usb_tc08_open_unit() or usb_tc08_open_unit_progress() */
	int8_t selection = 0;								/* User selection from the main menu */
	
	float temp[USBTC08_MAX_CHANNELS + 1] = {0.0};		/* Buffer to store single temperature readings from the TC-08 */
	float * temp_buffer[USBTC08_MAX_CHANNELS + 1];		/* 2D array to be used for streaming data capture */
	int32_t times_buffer[BUFFER_SIZE] = {0};			/* Array to hold the time of the conversion of the first channel for each set of readings in streaming mode captures */
	int16_t overflows[USBTC08_MAX_CHANNELS + 1] = {0};	/* Array to hold overflow flags for each channel in streaming mode captures */

	int32_t	channel = 0; 								/* Loop counter for channels */
	uint32_t reading = 0;								/* Loop counter for readings */
	int32_t retVal = 0;									/* Return value from driver calls indication success / error */
	USBTC08_INFO unitInfo;								/* Struct to hold unit information */
	
	uint32_t numberOfReadings = 0;							/* Number of readings to collect in streaming mode */
	int32_t readingsCollected = 0;							/* Number of readings collected at a time in streaming mode */
	uint32_t totalReadings[USBTC08_MAX_CHANNELS + 1] = {0};	/* Total readings collected in streaming mode */

	/* Print header information */
	printf ("Pico Technology USB TC-08 Console Example Program\n");
	printf ("-------------------------------------------------\n\n");
	printf ("Looking for USB TC-08 devices on the system.\n\n");
	printf ("Progress: ");
	
	
	/* Try to open one USB TC-08 unit, if available 
	 * The simplest way to open the unit like is this:
	 *
	 *   handle = usb_tc08_open_unit();
	 *
	 * but that will cause your program to wait while the driver downloads
	 * firmware to any connected TC-08 units. If you're making an 
	 * interactive application, it's better to use 
	 * usb_tc08_open_unit_async() which returns immediately and allows you to 
	 * display some sort of progress indication to the user as shown below: 
	 */
	retVal = usb_tc08_open_unit_async();
	
	/* Make sure no errors occurred opening the unit */
	if (!retVal) 
	{
		printf ("\n\nError opening unit. Exiting.\n");
		return -1;
	}

	/* Display a text "progress bar" while waiting for the unit to open */
	while ((retVal = usb_tc08_open_unit_progress(&handle, NULL)) == USBTC08_PROGRESS_PENDING)
	{
		/* Update our "progress bar" */
		printf("|");
		fflush(stdout);
		Sleep(200);
	}
		
	/* Determine whether a unit has been opened */
	if (retVal != USBTC08_PROGRESS_COMPLETE || handle <= 0) 
	{
		printf ("\n\nNo USB TC-08 units could be opened. Exiting.\n");
		return -1;
	} 
	else 
	{
		printf ("\n\nUSB TC-08 opened successfully.\n");
	}
	
	/* Get the unit information */
	unitInfo.size = sizeof(unitInfo);
	usb_tc08_get_unit_info(handle, &unitInfo);
	
	printf("\nUnit information:\n");
	printf("Driver: %s \nSerial: %s \nCal date: %s \n", unitInfo.DriverVersion, unitInfo.szSerial, unitInfo.szCalDate);

	/* Set up all channels */
	retVal = usb_tc08_set_channel(handle, 0,'C');

	for (channel = 1; channel < (USBTC08_MAX_CHANNELS + 1); channel++)
	{
		retVal &= usb_tc08_set_channel(handle, channel,'K');
	}
	
	/* Make sure this was successful */
	if (retVal)
	{
		printf("\nEnabled all channels, selected Type K thermocouple.\n");
	} 
	else 
	{
		printf ("\n\nError setting up channels. Exiting.\n");
		usb_tc08_close_unit(handle);
		Sleep(2000);
		return -1;
	}
	
	/* Main menu loop */
	do 
	{
		printf("\nPlease select one of the following options and press <Enter>\n");
		printf("------------------------------------------------------------\n\n");
		printf("S - Single reading on all channels\n");
		printf("C - Continuous reading on all channels\n");
		printf("X - Close the USB TC08 and exit \n");
		
		while (0 == scanf_s(" %c", &selection, 1))
		{
			; /* Do nothing until a character is entered */ 
		}
		
		switch (selection) 
		{
		
			case 'S':
			case 's': /* Single reading mode */
				printf("Getting single reading for each channel...");
				fflush(stdout);

				/* Request the reading */
				usb_tc08_get_single(handle, temp, NULL, USBTC08_UNITS_CENTIGRADE);
				
				printf(" done!\n\nCJC      : %3.2f C\n", temp[0]);
				
				for (channel = (int32_t) USBTC08_CHANNEL_1; channel < USBTC08_MAX_CHANNELS + 1; channel++)
				{
					printf("Channel %d: %3.2f C\n", channel, temp[channel]);
				}

				break;
			
		
			case 'C':
			case 'c': /* Continuous (Streaming) mode */

				// Setup data buffers 
				for(channel = (int32_t) USBTC08_CHANNEL_CJC; channel < (USBTC08_MAX_CHANNELS + 1); channel++)
				{
					temp_buffer[channel] = (float *) calloc(BUFFER_SIZE, sizeof(float));
					totalReadings[channel] = 0;
				}

				printf("Entering streaming mode.\n");

				do 
				{
				  printf("Enter number of readings to collect per channel:\n"); // Ask user to enter number of readings
				  scanf_s("%u", &numberOfReadings);
				} while (numberOfReadings < 0 && numberOfReadings <= ULONG_MAX);

				printf("Press any key to stop data collection.\n\n");
				printf("Time    CJC    Ch1    Ch2    Ch3    Ch4    Ch5    Ch6    Ch7    Ch8\n");
				
				/* Set the unit running */
				usb_tc08_run(handle, usb_tc08_get_minimum_interval_ms(handle));

				while(totalReadings[USBTC08_CHANNEL_1] <= numberOfReadings && !_kbhit())
				{
					for (channel = 0; channel < (USBTC08_MAX_CHANNELS + 1); channel++) 
					{
						do
						{
							// Request temperature data, a negative value indicates an error
							readingsCollected = usb_tc08_get_temp(handle, temp_buffer[channel], times_buffer, BUFFER_SIZE, &overflows[channel], channel, USBTC08_UNITS_CENTIGRADE, 1);
						}
						while(readingsCollected == 0);

						/* Must check for errors (e.g. device could be unplugged) */
						if (readingsCollected < 0) 
						{
							printf ("\n\nError while streaming.\n");
							usb_tc08_stop(handle);
							Sleep(2000);
							return -1;
						}

						totalReadings[channel] = totalReadings[channel] + (uint32_t) readingsCollected;
					}

					// Print to screen
					for (reading = 0; reading < (uint32_t) readingsCollected; reading++)
					{
						printf("%6d ", times_buffer[reading]); 

						for (channel = 0; channel < USBTC08_MAX_CHANNELS + 1; channel++) 
						{
							printf("%6.2f ", temp_buffer[channel][reading]);
						}

						printf("\n");
					}

					// Wait for 5 seconds before asking for more data
					Sleep(5000);

				}

				usb_tc08_stop(handle);
				break;
		}
		
	} while (selection != 'X' && selection != 'x');
	
	/* Close the TC-08 */
	usb_tc08_close_unit(handle);
	
	return 0;
}
