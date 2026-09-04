/******************************************************************************
 *
 * Filename: usbtc08Con.c
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
 *		the usbtc08Con application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2007-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "usbtc08.h"
#else
#include <sys/types.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <libusbtc08/usbtc08.h>

#define Sleep(a) usleep(1000*a)
/* This example only ever uses %c and %u conversions, never an unbounded %s, so
 * mapping scanf_s onto scanf here introduces no overflow. */
#define scanf_s scanf // flawfinder: ignore

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
#endif

/* Number of channels reported by the driver: the cold junction (channel 0)
 * plus the eight thermocouple inputs. */
#define NUM_TC08_CHANNELS (USBTC08_MAX_CHANNELS + 1)

/* Buffer size to be used for streaming mode captures. usb_tc08_get_temp() never
 * returns more than USBTC08_MAX_SAMPLE_BUFFER readings in a single call, so
 * sizing the buffers from the driver's own constant keeps the allocation and the
 * buffer length passed to the driver permanently in step. */
#define BUFFER_SIZE USBTC08_MAX_SAMPLE_BUFFER

/****************************************************************************
* flushStdin
*
* Discards the remainder of the current input line.
*
* Called after scanf() so that input a conversion rejected is not read again on
* the next pass. Note that fflush(stdin) is undefined behaviour and must not be
* used for this.
****************************************************************************/
static void flushStdin(void)
{
	int c;

	do
	{
		c = getchar();
	}
	while (c != '\n' && c != EOF);
}

/****************************************************************************
* freeChannelBuffers
*
* Releases the per-channel streaming buffers allocated for continuous mode.
*
* Safe to call when only some of the buffers were allocated: the array starts as
* all-NULL and each pointer is cleared as it is freed.
****************************************************************************/
static void freeChannelBuffers(float * buffers[])
{
	int channel;

	for (channel = 0; channel < NUM_TC08_CHANNELS; channel++)
	{
		free(buffers[channel]);
		buffers[channel] = NULL;
	}
}

int main(void)
{
	int16_t handle = 0;									/* The handle to a TC-08 returned by usb_tc08_open_unit() or usb_tc08_open_unit_progress() */
	int16_t progress = 0;								/* Percentage progress reported while a unit is opening */
	char selection = 0;									/* User selection from the main menu */

	float temp[NUM_TC08_CHANNELS] = {0.0f};				/* Buffer to store single temperature readings from the TC-08 */
	float * temp_buffer[NUM_TC08_CHANNELS] = {NULL};	/* 2D array to be used for streaming data capture */
	int32_t times_buffer[BUFFER_SIZE] = {0};			/* Array to hold the time of the conversion of the first channel for each set of readings in streaming mode captures */
	int16_t overflows[NUM_TC08_CHANNELS] = {0};			/* Per-channel overflow flag; usb_tc08_get_temp writes one per call */
	int16_t singleOverflowFlags = 0;					/* usb_tc08_get_single writes ONE bit field here, one bit per channel */

	int channel = 0; 									/* Loop counter for channels */
	int reading = 0;									/* Loop counter for readings */
	int32_t retVal = 0;									/* Return value from driver calls indication success / error */
	USBTC08_INFO unitInfo;								/* Struct to hold unit information */

	uint32_t numberOfReadings = 0;							/* Number of readings to collect in streaming mode */
	int32_t readingsCollected = 0;							/* Number of readings collected at a time in streaming mode */
	int32_t readingsThisPass = 0;							/* Readings that every channel returned in the current pass */
	uint32_t totalReadings[NUM_TC08_CHANNELS] = {0};		/* Total readings collected in streaming mode */

	int32_t minimumIntervalMs = 0;	/* Minimum time interval between samples. */
	int32_t actualIntervalMs = 0;	/* Interval the driver applied when streaming started. */

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
		printf ("\n\nError opening unit: %d. Exiting.\n", usb_tc08_get_last_error(0));
		return -1;
	}

	/* Display a text "progress bar" while waiting for the unit to open.
	 * A real variable is passed for the progress argument rather than NULL so
	 * the driver always has somewhere to write it. */
	while ((retVal = usb_tc08_open_unit_progress(&handle, &progress)) == USBTC08_PROGRESS_PENDING)
	{
		/* Update our "progress bar" */
		printf("|");
		fflush(stdout);
		Sleep(200);
	}

	/* Determine whether a unit has been opened */
	if (retVal != USBTC08_PROGRESS_COMPLETE || handle <= 0)
	{
		printf ("\n\nNo USB TC-08 units could be opened: %d. Exiting.\n", usb_tc08_get_last_error(0));
		return -1;
	}
	else
	{
		printf ("\n\nUSB TC-08 opened successfully.\n");
	}

	/* Get the unit information. The struct is cleared first so that nothing
	 * uninitialised is printed if the call fails. */
	memset(&unitInfo, 0, sizeof(unitInfo));
	unitInfo.size = sizeof(unitInfo);

	if (usb_tc08_get_unit_info(handle, &unitInfo))
	{
		printf("\nUnit information:\n");
		printf("Driver: %s \nSerial: %s \nCal date: %s \n", unitInfo.DriverVersion, unitInfo.szSerial, unitInfo.szCalDate);
	}
	else
	{
		printf("\nUnable to read the unit information: %d\n", usb_tc08_get_last_error(handle));
	}

	/* Set up all channels. Channel 0 is the cold junction, which takes the
	 * dedicated 'C' type; channels 1 to 8 are the thermocouple inputs. */
	retVal = usb_tc08_set_channel(handle, USBTC08_CHANNEL_CJC, 'C');

	for (channel = USBTC08_CHANNEL_1; channel < NUM_TC08_CHANNELS && retVal; channel++)
	{
		/* Each call is tested in turn rather than accumulating the results with
		 * '&='. The driver documents "non-zero means success", so bitwise ANDing
		 * two different non-zero codes could report a failure that never
		 * happened. */
		retVal = usb_tc08_set_channel(handle, (int16_t) channel, 'K');
	}

	/* Make sure this was successful */
	if (retVal)
	{
		printf("\nEnabled all channels, selected Type K thermocouple.\n");
	}
	else
	{
		printf ("\n\nError setting up channels: %d. Exiting.\n", usb_tc08_get_last_error(handle));
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

		if (scanf_s(" %c", &selection, 1) != 1)
		{
			/* End of input, or an unrecoverable stream error. Leave the menu
			 * rather than spinning on a stdin that will never yield another
			 * character. */
			printf("\nNo more input available. Exiting.\n");
			break;
		}

		flushStdin();

		switch (selection)
		{

			case 'S':
			case 's': /* Single reading mode */
				printf("Getting single reading for each channel...");
				fflush(stdout);

				/* Request the reading. A real variable is passed rather than NULL
				 * so the driver always has somewhere to record over-range
				 * channels. Note the argument is named overflow_flags in
				 * usbtc08.h, plural: it is a single bit field covering all the
				 * thermocouple channels - bit 0 is channel 1 and the cold
				 * junction has no bit - unlike usb_tc08_get_temp's singular
				 * per-channel overflow. */
				retVal = usb_tc08_get_single(handle, temp, &singleOverflowFlags, USBTC08_UNITS_CENTIGRADE);

				if (!retVal)
				{
					printf(" failed: %d\n", usb_tc08_get_last_error(handle));
					break;
				}

				/* The cold junction cannot go over range and has no bit in the
				 * field, so no marker is shown for it. */
				printf(" done!\n\nCJC      : %3.2f C\n", temp[USBTC08_CHANNEL_CJC]);

				for (channel = USBTC08_CHANNEL_1; channel < NUM_TC08_CHANNELS; channel++)
				{
					/* Bit 0 of the field is channel 1, so channel N is bit N-1. */
					printf("Channel %d: %3.2f C%s\n", channel, temp[channel],
						(singleOverflowFlags & (1 << (channel - USBTC08_CHANNEL_1))) ? "  (over range)" : "");
				}

				break;


			case 'C':
			case 'c': /* Continuous (Streaming) mode */

				/* Setup data buffers, one per channel including the cold
				 * junction. Bail out if an allocation fails: the driver would
				 * otherwise be handed a NULL buffer to write into. */
				for(channel = USBTC08_CHANNEL_CJC; channel < NUM_TC08_CHANNELS; channel++)
				{
					temp_buffer[channel] = (float *) calloc(BUFFER_SIZE, sizeof(float));
					totalReadings[channel] = 0;

					if (temp_buffer[channel] == NULL)
					{
						printf("\nUnable to allocate the channel buffers. Exiting.\n");
						freeChannelBuffers(temp_buffer);
						usb_tc08_close_unit(handle);
						return -1;
					}
				}

				printf("Entering streaming mode.\n");

				printf("Enter number of readings to collect per channel: ");

				if (scanf_s("%" SCNu32, &numberOfReadings) != 1 || numberOfReadings == 0)
				{
					printf("A number of readings greater than zero is required.\n");
					flushStdin();
					freeChannelBuffers(temp_buffer);
					break;
				}

				flushStdin();

				minimumIntervalMs = usb_tc08_get_minimum_interval_ms(handle);

				if (minimumIntervalMs <= 0)
				{
					printf("Unable to read the minimum sampling interval: %d\n", usb_tc08_get_last_error(handle));
					freeChannelBuffers(temp_buffer);
					break;
				}

				/* Set the unit running. The return value is the interval the
				 * driver actually applied, or 0 if streaming failed to start. */
				actualIntervalMs = usb_tc08_run(handle, minimumIntervalMs);

				if (actualIntervalMs <= 0)
				{
					printf("Unable to start streaming: %d\n", usb_tc08_get_last_error(handle));
					freeChannelBuffers(temp_buffer);
					break;
				}

				printf("Sampling interval: %" PRId32 " ms\n", actualIntervalMs);
				printf("Press any key to stop data collection.\n\n");
				printf("Time    CJC    Ch1    Ch2    Ch3    Ch4    Ch5    Ch6    Ch7    Ch8\n");

				while(totalReadings[USBTC08_CHANNEL_1] < numberOfReadings && !_kbhit())
				{
					/* Track the smallest count returned across the channels in
					 * this pass. The driver reports a count per channel and
					 * those counts can differ, so only rows that every channel
					 * actually returned are printed. */
					readingsThisPass = BUFFER_SIZE;

					for (channel = USBTC08_CHANNEL_CJC; channel < NUM_TC08_CHANNELS; channel++)
					{
						do
						{
							// Request temperature data, a negative value indicates an error
							readingsCollected = usb_tc08_get_temp(handle, temp_buffer[channel], times_buffer, BUFFER_SIZE,
								&overflows[channel], (int16_t) channel, USBTC08_UNITS_CENTIGRADE, 1);
						}
						while(readingsCollected == 0 && !_kbhit());

						/* Must check for errors (e.g. device could be unplugged) */
						if (readingsCollected < 0)
						{
							printf ("\n\nError while streaming: %d\n", usb_tc08_get_last_error(handle));
							usb_tc08_stop(handle);
							freeChannelBuffers(temp_buffer);
							usb_tc08_close_unit(handle);
							Sleep(2000);
							return -1;
						}

						totalReadings[channel] = totalReadings[channel] + (uint32_t) readingsCollected;

						if (readingsCollected < readingsThisPass)
						{
							readingsThisPass = readingsCollected;
						}
					}

					// Print to screen
					for (reading = 0; reading < readingsThisPass; reading++)
					{
						printf("%6" PRId32 " ", times_buffer[reading]);

						for (channel = USBTC08_CHANNEL_CJC; channel < NUM_TC08_CHANNELS; channel++)
						{
							printf("%6.2f ", temp_buffer[channel][reading]);
						}

						printf("\n");
					}

					// Wait for 5 seconds before asking for more data
					Sleep(5000);

				}

				usb_tc08_stop(handle);
				freeChannelBuffers(temp_buffer);
				break;

			case 'X':
			case 'x':
				/* Handled by the loop condition below */
				break;

			default:
				printf("Invalid operation\n");
				break;
		}

	} while (selection != 'X' && selection != 'x');

	/* Close the TC-08 */
	usb_tc08_close_unit(handle);

	return 0;
}
