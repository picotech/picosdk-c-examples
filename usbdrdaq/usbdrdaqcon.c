/*******************************************************************************
 *
 *	Filename: usbdrdaqcon.c
 *
 *	Description:
 *    This is a console-mode program that demonstrates how to use the
 *    USB DrDAQ driver API functions.
 *
 *	There are the following examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *    Use windowing to collect a sequence of overlapped blocks
 *    Write a continuous stream of data to a file
 *    Take individual readings
 *		 Set the signal generator
 *		 Set digital outputs
 *		 Get states of digital inputs
 *		 Set PWM
 *		 Pulse counting
 *		 Set the RGB LED
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (Win32/x64)
 *			Ensure that the 32-/64-bit usbdrdaq.lib can be located
 *			Ensure that the usbDrDaqApi.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			Set up a project for a 32-bit/64-bit console mode application
 *			Add this file, usbDrDaqApi.h, PicoStatus.h and 
 *			USBDrDAQ.lib (Microsoft C only) to the project and then
 *			build it.
 *
 *  Linux platforms:
 *
 *		Ensure that the libusbdrdaq driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the usbdrdaqcon application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 *	Copyright (C) 2010 - 2017 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>

// Define bool type
typedef enum enBOOL
{
	FALSE, TRUE
} bool;

#ifdef WIN32
/* Headers for Windows */
#include "windows.h"
#include <conio.h>

#include "usbDrDaqApi.h"

#define PREF4 __stdcall

#else

#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <libusbdrdaq-1.0/usbDrDaqApi.h>
#ifndef PICO_STATUS
#include <libusbdrdaq-1.0/PicoStatus.h>

#define PREF4
#endif

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

int32_t				scale_to_mv;
uint16_t			max_adc_value;
int16_t				g_handle;
int16_t				isReady;
int16_t				d1State, d2State, d3State, d4State = 0;
PICO_STATUS			status;
USB_DRDAQ_INPUTS	channel;



/****************************************************************************
*
* adc_to_mv
*
* If the user selects scaling to millivolts,
* Convert a 12-bit ADC count into millivolts
*
****************************************************************************/
float adc_to_mv (float raw)										
{		
	float scaled = 0.0;

	if (scale_to_mv)
	{
		scaled = (float) ((raw / max_adc_value) * 2500);
	}
	else
	{
		scaled = (float) raw;
	}
	return scaled;
}

/****************************************************************************
*
* mv_to_adc
*
* Convert a millivolt value into an ADC count
*
*  (useful for setting trigger thresholds)
*
****************************************************************************/
int16_t mv_to_adc (int16_t mv)
{
	return (mv / 2500) * max_adc_value;
}

/****************************************************************************
*
* channel_select
*  Allow user to select channel
*
****************************************************************************/
void channel_select()
{
	int32_t value;
	PICO_STATUS status = PICO_OK;

	printf("\n");
	printf("1:  External Sensor 1\n");
	printf("2:  External Sensor 2\n");
	printf("3:  External Sensor 3\n");
	printf("4:  Scope Channel\n");
	printf("5:  PH\n");
	printf("6:  Resistance\n");
	printf("7:  Light\n");
	printf("8:  Thermistor\n");
	printf("9:  Microphone Waveform\n");
	printf("10: Microphone Level\n");
	printf("\n");
	
	do
	{
		printf("Enter channel number: ");
		scanf_s("%d", &value);
	}while (value < USB_DRDAQ_CHANNEL_EXT1 || value > USB_DRDAQ_MAX_CHANNELS);

	channel = (USB_DRDAQ_INPUTS)value;

	// Set temperature compensation if PH channel is enabled
	if (channel == USB_DRDAQ_CHANNEL_PH)
	{
		status = UsbDrDaqPhTemperatureCompensation(g_handle, TRUE);

		if (status != PICO_OK)
		{
			printf("channel_select:- UsbDrDaqPhTemperatureCompensation: %d\n", status);
		}
	}
}

/****************************************************************************
*
* Collect_block_immediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
*
****************************************************************************/

void collect_block_immediate (void)
{
	uint32_t	i;
  uint32_t  j;
	uint32_t	nSamples = 1000;
	int16_t		nChannels = 1;
	uint32_t	nSamplesPerChannel = nSamples / nChannels;
	uint32_t	nSamplesCollected;
	float		samples[1000] = {0.0}; 
	uint32_t	usForBlock = 1000000;
	uint16_t	overflow;
	uint32_t	triggerIndex = 0;
	FILE *		fp;

	printf ("Collect block immediate (channel %d)...\n", channel);
	printf ("Press a key to start\n");
	_getch();

	// Set the trigger (disabled)
	status = UsbDrDaqSetTrigger(g_handle, FALSE, 0, 0, 0, 0, 0, 0, 0);

	// Set sampling rate and channels
	status = UsbDrDaqSetInterval(g_handle, &usForBlock, nSamplesPerChannel, &channel, nChannels);

	printf("Press any key to stop\n");
	fopen_s(&fp, "usb_dr_daq_block_immediate.txt", "w");
	
	while (!_kbhit())
	{
		// Run
		status = UsbDrDaqRun(g_handle, nSamplesPerChannel, BM_SINGLE);

		// Wait until unit is ready
		isReady = 0;
		
		while (isReady == 0)
		{
			status = UsbDrDaqReady(g_handle, &isReady);
		}

		nSamplesCollected = nSamplesPerChannel;
		status = UsbDrDaqGetValuesF(g_handle, samples, &nSamplesCollected, &overflow, &triggerIndex);

		// Print out the first 10 readings, converting the readings to mV if required
		printf ("First 10 readings of each channel (press any key to stop)\n\n");
		
		for (i = 0; i < 10; i++)
		{
      for (j = 0; j < (uint32_t) nChannels; j++)
      {
        printf("%.3f\n", adc_to_mv(samples[(i * nChannels) + j]));
      }

      printf("\n");
		}
		
		for (i = 0; i < nSamplesCollected; i++)
		{
      for (j = 0; j < (uint32_t) nChannels; j++)
      {
        fprintf(fp, "%.3f\t", adc_to_mv(samples[(i * nChannels) + j]));
      }

      fprintf(fp, "\n");
		}
		Sleep(100);

		printf("\n");
	}
	fclose(fp);
	status = UsbDrDaqStop(g_handle);
	_getch();
}

/****************************************************************************
*
* Collect_block_triggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
*
****************************************************************************/

void collect_block_triggered (void)
{
	uint32_t	i;
  uint32_t  j;
	uint32_t	nSamples = 1000;
	int16_t		nChannels = 1;
	uint32_t	nSamplesPerChannel = nSamples / nChannels;
	uint32_t	nSamplesCollected;
  float		samples[1000] = {0.0};
	uint32_t	usForBlock = 1000000;
	uint16_t	overflow;
	uint32_t	triggerIndex = 0;
	int32_t		threshold;
	FILE *		fp;

	printf ("Collect block triggered (channel %d)...\n", channel);
	printf ("Enter threshold: ");
	scanf_s("%d", &threshold);
	printf ("\nPress a key to start...\n");
	_getch();

	// Set the trigger
	status = UsbDrDaqSetTrigger(g_handle, TRUE, 0, 0, channel, (uint16_t) threshold, 16000, 0, -50);

	// Set sampling rate and channels
	status = UsbDrDaqSetInterval(g_handle, &usForBlock, nSamplesPerChannel, &channel, nChannels);

	printf ("Trigger delay is set to -50%% (trigger event in centre of block)\n");
	printf ("\nWaiting for trigger...\n\n");
	printf ("Press a key to abort\n");

	fopen_s(&fp, "usb_dr_daq_block_triggered.txt", "w");

	// Run
	status = UsbDrDaqRun(g_handle, nSamplesPerChannel, BM_SINGLE);

	// Wait until unit is ready
	isReady = 0;

	while (isReady == 0 && (!_kbhit ()))
	{
		status = UsbDrDaqReady(g_handle, &isReady);
	}

	nSamplesCollected = nSamplesPerChannel;
	status = UsbDrDaqGetValuesF(g_handle, samples, &nSamplesCollected, &overflow, &triggerIndex);

	//Print out the first 10 readings, converting the readings to mV if required
	printf ("5 readings either side of trigger event (%i samples collected per channel)\n", nSamplesCollected);
	for (i = triggerIndex-5; i < triggerIndex+6; i++)
	{
		printf ("%.3f\n", adc_to_mv(samples[i]));
	}

  for (i = 0; i < nSamplesCollected; i++)
  {
    for (j = 0; j < (uint32_t) nChannels; j++)
    {
      fprintf(fp, "%.3f\t", adc_to_mv(samples[(i * nChannels) + j]));
    }

    fprintf(fp, "\n");
  }

	fclose(fp);
	status = UsbDrDaqStop(g_handle);
}


/****************************************************************************
*
* Collect_windowed_blocks
*
* This function demonstrates how to use windowed blocks.
*
****************************************************************************/

void collect_windowed_blocks (void)
{
	uint32_t	i;
  uint32_t  j;
	uint32_t	nSamples = 1000;
	int16_t		nChannels = 1;
	uint32_t	nSamplesPerChannel = nSamples / nChannels;
	uint32_t	nSamplesCollected;
  float		samples[1000] = {0.0};
	uint32_t	usForBlock = 10000000;	//10 seconds
	uint16_t	overflow;
	uint32_t	triggerIndex = 0;
	int16_t		nLines = 0;
	FILE *		fp;

	printf ("Collect windowed block (channel %d)...\n", channel);
	printf ("First block appears after 10 seconds,\n");
	printf ("then 10 second blocks are collected every second\n");
	printf ("Press a key to start\n");
	_getch();

	//Set the trigger (disabled)
	status = UsbDrDaqSetTrigger(g_handle, FALSE, 0, 0, 0, 0, 0, 0, 0);

	//set sampling rate and channels
	status = UsbDrDaqSetInterval(g_handle, &usForBlock, nSamplesPerChannel, &channel, nChannels);

	//Start 
	status = UsbDrDaqRun(g_handle, nSamplesPerChannel, BM_WINDOW);

	//Wait until unit is ready
	printf ("Waiting for first block...\n");
	isReady = 0;
	
	while (isReady == 0)
	{
		status = UsbDrDaqReady(g_handle, &isReady);
	}

	printf("Press any key to stop\n");
	fopen_s(&fp, "usb_dr_daq_block_windowed.txt", "w");

	while (!_kbhit())
	{
		nSamplesCollected = nSamplesPerChannel;
		status = UsbDrDaqGetValuesF(g_handle, samples, &nSamplesCollected, &overflow, &triggerIndex);

		printf("%d values\n", nSamplesCollected);
		
    if(nLines == 20)
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
        fprintf(fp, "%.3f\t", adc_to_mv(samples[(i * nChannels) + j]));
      }

      fprintf(fp, "\n");
    }

		Sleep(1000);		//Wait 1 second before collecting next 10 second block.

		printf("\n");
	}

	fclose(fp);
	status = UsbDrDaqStop(g_handle);

	_getch();
}

/****************************************************************************
*
* Collect_streaming
* 
* This function demonstrates how to use streaming.
*
****************************************************************************/

void collect_streaming (void)
{
	uint32_t	i;
  uint32_t  j;
	uint32_t	nSamples = 1000;
	int16_t		nChannels = 1;
	uint32_t	nSamplesPerChannel = nSamples / nChannels;
	uint32_t	nSamplesCollected;
  float		samples[1000] = {0.0};
	uint32_t	usForBlock = 1000000;
	uint16_t	overflow;
	uint32_t	triggerIndex = 0;
	int16_t		nLines = 0;
	FILE *		fp;

	printf ("Collect streaming (channel %d)...\n", channel);
	printf ("Data is written to disk file (usb_dr_daq_streaming.txt)\n");
	printf ("Press a key to start\n");
	_getch();

	//Set the trigger (disabled)
	status = UsbDrDaqSetTrigger(g_handle, FALSE, 0, 0, 0, 0, 0, 0, 0);

	//set sampling rate and channels
	status = UsbDrDaqSetInterval(g_handle, &usForBlock, nSamplesPerChannel, &channel, nChannels);

	//Start streaming
	status = UsbDrDaqRun(g_handle, nSamplesPerChannel, BM_STREAM);

	//Wait until unit is ready
	isReady = 0;
	
	while (isReady == 0)
	{
		status = UsbDrDaqReady(g_handle, &isReady);
	}

	printf("Press any key to stop\n");
	fopen_s(&fp, "usb_dr_daq_streaming.txt", "w");

	while (!_kbhit())
	{
		nSamplesCollected = nSamplesPerChannel;
		status = UsbDrDaqGetValuesF(g_handle, samples, &nSamplesCollected, &overflow, &triggerIndex);

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
        fprintf(fp, "%.3f\t", adc_to_mv(samples[(i * nChannels) + j]));
      }

      fprintf(fp, "\n");
    }

		Sleep(100);

	}
	fclose(fp);
	status = UsbDrDaqStop(g_handle);

	_getch();
}

/****************************************************************************
*
* Collect_individual
*
****************************************************************************/

void collect_individual (void)
{
	int32_t		sample_no;
	int32_t		c;
	float		value;
	uint16_t	overflow;

	printf ("Collect individual...\n");
	printf ("Takes individual readings under program control\n");
	printf ("Sample from all channels\n");
	printf ("Press a key to start\n");
	_getch();

	sample_no = 20;
	
	while (!_kbhit ())
	{
		Sleep(100);
		
		if (++sample_no > 20)	//Print headings again
		{
			sample_no = 0;
			printf ("\nPress any key to stop\n \n");
			
			for (c = 1; c <= USB_DRDAQ_MAX_CHANNELS; c++)
			{
				printf ("ch%d\t", c);
			}
		}
		
		for (c = 1; c <= USB_DRDAQ_MAX_CHANNELS; c++)
		{
			value = 0;
			UsbDrDaqGetSingleF(g_handle, (USB_DRDAQ_INPUTS) c, &value, &overflow);
			printf ("%.2f\t", adc_to_mv (value));
		}
	}
	_getch ();
}

/****************************************************************************
*
* Set/clear digital outputs
*
****************************************************************************/
void outputToggle(USB_DRDAQ_GPIO IOChannel)
{
	switch(IOChannel)
	{
		case USB_DRDAQ_GPIO_1:
		{
			if (d1State)
			{
				d1State = 0;
			}
			else
			{
				d1State = 1;
			}

			UsbDrDaqSetDO(g_handle, IOChannel, d1State);
			break;
		}
		case USB_DRDAQ_GPIO_2:
		{
			if (d2State)
			{
				d2State = 0;
			}
			else
			{
				d2State = 1;
			}

			UsbDrDaqSetDO(g_handle, IOChannel, d2State);
			break;
		}
		case USB_DRDAQ_GPIO_3:
		{
			if (d3State)
			{
				d3State = 0;
			}
			else
			{
				d3State = 1;
			}

			UsbDrDaqSetDO(g_handle, IOChannel, d3State);
			break;
		}
		case USB_DRDAQ_GPIO_4:
		{
			if (d4State)
			{
				d4State = 0;
			}
			else
			{
				d4State = 1;
			}

			UsbDrDaqSetDO(g_handle, IOChannel, d4State);
			break;
		}
	}
}

/****************************************************************************
*
* Display digital output states
*
****************************************************************************/
void displayOutputStates()
{
	printf("\nDigital Outputs\n");
	printf("===============\n");
	printf("GPIO 1\tGPIO 2\tGPIO 3\tGPIO 4\t\n");
	printf("%i\t%i\t%i\t%i\t\n\n", d1State, d2State, d3State, d4State);
}

/****************************************************************************
*
* Set pulse width modulation
*
****************************************************************************/
void pwm()
{
	int32_t period = 0;
	int32_t cycle = 0;
	int32_t IOChannel;

	printf("\n----------PWM----------\n");
	do
	{
		printf("Select GPIO channel (1 or 2):");
		scanf_s("%d", &IOChannel);
		fflush(stdin);
	}while (IOChannel < USB_DRDAQ_GPIO_1 || IOChannel > USB_DRDAQ_GPIO_2);

	do
	{
		printf("Enter period (0 to 65535 microseconds):");
		scanf_s("%d", &period);
		fflush(stdin);
	}while (period < 0 || period > 65535);

	do
	{
		printf("Enter duty cycle (0 to 100%%):");
		scanf_s("%d", &cycle);
		fflush(stdin);
	}while (cycle < 0 || cycle > 100);
		
	UsbDrDaqSetPWM(g_handle, (USB_DRDAQ_GPIO) IOChannel, (uint16_t)period, (uint8_t)cycle);

	// Clear state of channel we are using
	if (IOChannel == USB_DRDAQ_GPIO_1)
	{
		d1State = 0;
	}
	else if (IOChannel == USB_DRDAQ_GPIO_2)
	{
		d2State = 0;
	}
}

/****************************************************************************
*
* Read digital inputs
*
****************************************************************************/
void DigitalInput()
{
	int16_t value;
	USB_DRDAQ_GPIO channel;

	printf("Press any key to stop...\n");

	while (!_kbhit())
	{
		for (channel = USB_DRDAQ_GPIO_1; channel <= USB_DRDAQ_GPIO_4; channel++)
		{
			UsbDrDaqGetInput(g_handle, channel, 0, &value);				//Not using pull-up resistor
			printf("%d\t", value);
		}
		printf("\n");
		
		Sleep(500);
	}
	_getch();

	//reset digital output status
	d1State = d2State = d3State = d4State = 0;
}

/****************************************************************************
*
* Pulse counting
*
****************************************************************************/
void pulseCounting()
{
	int32_t value, direction;
	USB_DRDAQ_GPIO IOChannel;
	int16_t count;

	do
	{
		printf("Select GPIO (1 or 2):");
		scanf_s("%d", &value);
	}while (value != USB_DRDAQ_GPIO_1 && value != USB_DRDAQ_GPIO_2);

	printf("\n");
	
	IOChannel = (USB_DRDAQ_GPIO)value;
	
	do
	{
		printf("Select direction (0: rising. 1: falling):");
		scanf_s("%d", &direction);
	}while (direction != 0 && direction != 1);

	printf("\n");

	printf("Press any key to start counting pulses\n");
	_getch();
	
	UsbDrDaqStartPulseCount(g_handle, IOChannel, (int16_t)direction);

	printf("Press any key to stop...\n");

	while (!_kbhit())
	{
		Sleep(1000);
		
		UsbDrDaqGetPulseCount(g_handle, IOChannel, &count);
		printf("%d\n", count);
	}
	_getch();

	//reset digital output status
	if(IOChannel == USB_DRDAQ_GPIO_1)
	{
		d1State = 0;
	}
	else if(IOChannel == USB_DRDAQ_GPIO_2)
	{
		d2State = 0;
	}
}


/****************************************************************************
*
* Set the signal generator
*
****************************************************************************/
void sigGen()
{
	USB_DRDAQ_WAVE	waveType;
	int32_t frequency = 0;
	int32_t pToP = 0;
	int32_t value, offset;

	printf("0: Sine\n");
	printf("1: Square\n");
	printf("2: Triangle\n");
	printf("3: Ramp Up\n");
	printf("4: Rampe Down\n");
	printf("5: DC\n");
	printf("99: OFF\n");

	do
	{
		printf("\nSelect wave type:");
		scanf_s("%d", &value);
	}while ((value < 0 || value > 5) && value != 99);

	if(value == 99)
	{
		UsbDrDaqStopSigGen(g_handle);
		return;
	}

	waveType = (USB_DRDAQ_WAVE)value;

	do
	{
		printf("\nEnter offset (microvolts):");
		scanf_s("%d", &offset);
	}while (offset < -1500000 || offset > 1500000);

	if(waveType != USB_DRDAQ_DC)		//Frequency and peak to peak are ignored for DC output
	{
		do
		{
			printf("\nEnter frequency (0 to 20,000 Hz):");
			scanf_s("%d", &frequency);
		}while (frequency < 0 || frequency > 20000);

		do
		{
			printf("Enter peak-to-peak amplitude (microvolts)");
			scanf_s("%d", &pToP);
		}while (pToP < 0|| pToP > 3000000);
	}

	UsbDrDaqSetSigGenBuiltIn(g_handle, (int32_t)offset, (uint32_t)pToP, (int16_t)frequency, waveType);
}


/****************************************************************************
*
* Get available scaling for current channel and allow user to select
* the scaling to use
*
****************************************************************************/
void channelScaling()
{
	PICO_STATUS status = PICO_OK;

	const int16_t namesSize = 1000;
	int16_t nScales, currentScale, i;
	int8_t * names = (int8_t *) calloc(namesSize, sizeof(int8_t));
	int8_t	ch;
	int32_t selectedScaleNo;
	int16_t index = 0;

	float min = 0.0;
	float max = 0.0;
	int16_t places = 0;
	int16_t divider = 0;

	// Obtain the scaling values
	status = UsbDrDaqGetScalings(g_handle, channel, &nScales, &currentScale, names, namesSize);

	printf("%d scale(s) available for channel %d:\n\n", nScales, channel);

	i = 0;
	printf("%d: ", index);

	while (names[i] != NULL)
	{
		if (names[i] == 13) //Carriage return
		{
			printf("\n");
			index++;

			// Print an index value for the next scaling value
			if (i < (namesSize - 1) && names[i + 1] != NULL)
			{
				printf("%d: ", index);
			}
		}
		else
		{
			printf("%c", names[i]);
		}

		i++;
	}

	if (nScales > 1)
	{
		printf("\ncurrent scale: %d\n\n", currentScale);

		printf("Press 'C' to change scale or any other key to continue\n");

		ch = toupper(_getch());

		if(ch == 'C')
		{
			do
			{
				printf("Select scale (0 to %d): ", nScales - 1);
				scanf_s("%d", &selectedScaleNo);
				printf("\n");
			}while (selectedScaleNo < 0 || selectedScaleNo > nScales - 1);

			status = UsbDrDaqSetScalings(g_handle, channel, (int16_t) selectedScaleNo);
		}
	}

	// Obtain and display scaling information
	status = UsbDrDaqGetChannelInfo(g_handle, &min, &max, &places, &divider, channel);

	printf("\nChannel Information:-\n\n");
	printf("Min: %.*f\n", places, min);
	printf("Max: %.*f\n", places, max);
	printf("Decimal Places: %d\n", places);
	printf("Divider: %d\n", divider);
}

/****************************************************************************
*
* Enable or disable the RGB LED
*
****************************************************************************/
void led()
{
	int32_t enable, red, green, blue;

	printf("\n");
	printf("0: Disable RGB LED\n");
	printf("1: Enable RGB LED\n");

	do
	{
		printf("\n>");
		scanf_s("%d", &enable);
	}while (enable < 0 || enable > 1);

	UsbDrDaqEnableRGBLED(g_handle, (int16_t)enable);

	if (enable)
	{
		do
		{
			printf("\nEnter Red value (0 to 255):");
			scanf_s("%d", &red);
		}while (red < 0);					//Doesn't matter if > 255

		do
		{
			printf("\nEnter Green value (0 to 255):");
			scanf_s("%d", &green);
		}while (green < 0);
		
		do
		{
			printf("\nEnter Blue value (0 to 255):");
			scanf_s("%d", &blue);
		}while (blue < 0);	

		UsbDrDaqSetRGBLED(g_handle, (uint16_t)red, (uint16_t)green, (uint16_t)blue);
	}
}

/****************************************************************************
*
*
****************************************************************************/

void main (void)
{
	int8_t	ch;
	int8_t	info[80];
	int16_t infoLength = 80;
	int16_t	requiredSize = 0;

	printf ("USB DrDAQ Driver Example Program\n");
	printf ("Version 1.4\n\n");

	printf ("\nOpening the device...\n");
	status = UsbDrDaqOpenUnit(&g_handle);

	if (status != PICO_OK)
	{
		printf ("Unable to open device\nPress any key\n");
		_getch();
		return;
	}
	else
	{
		printf ("Device opened successfully:\n\n");
		
		status = UsbDrDaqGetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_VARIANT_INFO);
		printf("Model:\t\t\t %s\n", info);
		
		status = UsbDrDaqGetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_BATCH_AND_SERIAL);
		printf("Serial Number:\t\t %s\n", info);
		
		status = UsbDrDaqGetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_CAL_DATE);
		printf("Calibration Date:\t %s\n", info);
		
		status = UsbDrDaqGetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_USB_VERSION);
		printf("USB Version:\t\t %s\n", info);
		
		status = UsbDrDaqGetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_HARDWARE_VERSION);
		printf("Hardware version:\t %s\n", info);
		
		status = UsbDrDaqGetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_DRIVER_VERSION);
		printf("USBDrDAQ.dll version:\t %s\n", info);
		
		status = UsbDrDaqGetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_KERNEL_VERSION);
		printf("Kernel version:\t\t %s\n", info);

		status = UsbDrDaqGetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_FIRMWARE_VERSION_1);
		printf("Firmware:\t\t %s\n", info);

		//Set to scope channel initially
		channel = USB_DRDAQ_CHANNEL_SCOPE;

		ch = ' ';

		while (ch != 'X')
		{
			printf ("\n");
			printf ("Select an operation:\n\n");
			printf ("B - Immediate block\t\t1, 2, 3, 4 - Toggle digital outputs\n");
			printf ("T - Triggered block\t\tP - Set PWM\n");
			printf ("W - Windowed block\t\tD - Get digital input states\n");
			printf ("S - Streaming\t\t\tE - Pulse counting\n");
			printf ("C - Select channel\t\tF - Set signal generator\n");
			printf ("G - Channel scaling\t\tH - Set RGB LED\n");
			printf ("A - Select mV or ADC counts\n");
			printf ("I - Individual reading\t\tX - Exit\n");
			ch = toupper (_getch());
			printf ("\n");

			switch (ch)
			{

			case 'C':
				channel_select();
				break;

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

			case 'D':
				DigitalInput();
				break;
							
			case 'E':
				pulseCounting();
				break;
							
			case 'F':
				sigGen();
				break;

			case 'P':
				pwm();
				break;

			case 'G':
				channelScaling();
				break;

			case 'H':
				led();
				break;

			case '1':
				outputToggle(USB_DRDAQ_GPIO_1);
				displayOutputStates();
				break;

			case '2':
				outputToggle(USB_DRDAQ_GPIO_2);
				displayOutputStates();
				break;

			case '3':
				outputToggle(USB_DRDAQ_GPIO_3);
				displayOutputStates();
				break;

			case '4':
				outputToggle(USB_DRDAQ_GPIO_4);
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
				break;

			default:
				printf ("Invalid operation\n");
				break;
			}
		}

		UsbDrDaqCloseUnit(g_handle);
	}
}