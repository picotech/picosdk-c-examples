/*******************************************************************************
 *
 * Filename: ps5000aBlockMSOCon.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use some of
 *	 the PicoScope 5000 Series (ps5000a) driver API functions to perform operations
 *	 using a PicoScope 5000 Series Flexible Resolution Mixed Signal Oscilloscope.
 *
 *	Supported PicoScope models:
 *
 *		PicoScope 5242D MSO & 5442D MSO
 *		PicoScope 5243D MSO & 5443D MSO
 *		PicoScope 5244D MSO & 5444D MSO
 *
 * Examples:
 *   Collect a block of samples when a trigger event occurs
 *	 Handle power source changes
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express/Community Edition) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps5000a.lib can be located
 *			Ensure that the ps5000aApi.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps5000a.lib to the project (Microsoft C only)
 *			 Add ps5000aApi.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps5000a driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. Edit the configure.ac and Makefile.am files to use this file.
 *		In a terminal window, use the following commands to build the
 *		ps5000aBlockMSOCon application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2018-2019 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include <math.h>
#include "ps5000aApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <libps5000a-1.1/ps5000aApi.h>
#ifndef PICO_STATUS
#include <libps5000a-1.1/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL { FALSE, TRUE } BOOL;

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
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  setbuf(stdin, NULL);
  ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return bytesWaiting;
}

int32_t fopen_s(FILE ** a, const int8_t * b, const int8_t * c)
{
  FILE * fp = fopen(b, c);
  *a = fp;
  return (fp > 0) ? 0 : -1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

int32_t cycles = 0;

#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

#define MAX_DIGITAL_PORTS 2

#define MAX_PICO_DEVICES 64
#define TIMED_LOOP_STEP 500

typedef struct tChannelSettings
{
  PS5000A_COUPLING coupling;
  PS5000A_RANGE range;
  int16_t enabled;
  float analogueOffset;
}CHANNEL_SETTINGS;

uint16_t inputRanges[PS5000A_MAX_RANGES] = {
                        10,
                        20,
                        50,
                        100,
                        200,
                        500,
                        1000,
                        2000,
                        5000,
                        10000,
                        20000,
                        50000 };

int16_t   	g_ready = FALSE;
int32_t     g_sampleCount;
uint32_t		g_startIndex;
int16_t			g_overflow = 0;

int8_t blockFile[20] = "block.txt";
int8_t digiBlockFile[20] = "digiBlock.txt";

/****************************************************************************
* Callback
* used by ps5000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackBlock(int16_t handle, PICO_STATUS status, void * pParameter)
{
  if (status != PICO_CANCELLED)
  {
    g_ready = TRUE;
  }
}

/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t rangeIndex, int16_t maxADCValue)
{
  return (raw * inputRanges[rangeIndex]) / maxADCValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t rangeIndex, int16_t maxADCValue)
{
  return (mv * maxADCValue) / inputRanges[rangeIndex];
}

/****************************************************************************
* main
*
***************************************************************************/
int32_t main(void)
{
  PICO_STATUS status = PICO_OK;
  int16_t handle = 0;

  printf("PicoScope 5000 Series (ps5000a) Driver MSO Block Capture Example Program\n\n");

  // Establish connection to device
  // ------------------------------

  // Open the connection to the device
  status = ps5000aOpenUnit(&handle, NULL, PS5000A_DR_8BIT);

  // Handle power status codes
  if (status != PICO_OK)
  {
    if (status == PICO_POWER_SUPPLY_NOT_CONNECTED || PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
    {
      status = ps5000aChangePowerSource(handle, status);
    }
    else
    {
      fprintf(stderr, "No device found - status code %d\n", status);
      return -1;
    }
  }

  // Display unit information and determine if it is a 4-channel model

  int16_t requiredSize = 0;
  int8_t	line[80];
  int32_t channelCount = 0;
  int32_t digitalPortCount = 0;
  int32_t i = 0;

  int8_t description[11][25] = { "Driver Version",
                                  "USB Version",
                                  "Hardware Version",
                                  "Variant Info",
                                  "Serial",
                                  "Cal Date",
                                  "Kernel Version",
                                  "Digital HW Version",
                                  "Analogue HW Version",
                                  "Firmware 1",
                                  "Firmware 2" };

  printf("Device information:-\n\n");

  for (i = 0; i < 11; i++)
  {
    status = ps5000aGetUnitInfo(handle, line, 80, &requiredSize, i);

    // info = 3 - PICO_VARIANT_INFO
    if (i == PICO_VARIANT_INFO)
    {
      channelCount = (int16_t)line[1];
      channelCount = channelCount - 48; // Subtract ASCII 0 (48)

      // Determine if the device is an MSO
      if (strstr(line, "MSO") != NULL)
      {
        digitalPortCount = 2;
      }
      else
      {
        digitalPortCount = 0;
        fprintf(stderr, "This example is for PicoScope 5000 Series Mixed Signal Oscilloscopes.\n");
        printf("Exiting application...\n");
        Sleep(5000);
        return -1;
      }

    }

    printf("%s: %s\n", description[i], line);
  }

  // Find max ADC value
  int16_t maxADCValue = 0;
  status = ps5000aMaximumValue(handle, &maxADCValue);

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aMaximumValue ------ 0x%08lx \n", status);
    return -1;
  }

  // Channel setup
  // -------------

  // Setup analogue channels

  struct tChannelSettings channelSettings[PS5000A_MAX_CHANNELS];
  int32_t ch = 0;

  // Find the current power source - to determine if the power supply is connected for 4-channel models
  //
  // If the power supply is not connected on a 4-channel device, only channels A and B can be used. 
  // MSO digital ports will still be available.
  int32_t numAvailableChannels = channelCount;

  if (channelCount == QUAD_SCOPE)
  {
    status = ps5000aCurrentPowerSource(handle);

    if (status == PICO_POWER_SUPPLY_NOT_CONNECTED)
    {
      numAvailableChannels = DUAL_SCOPE;
    }
  }

  for (ch = 0; ch < numAvailableChannels; ch++)
  {
    channelSettings[ch].enabled = 1;
    channelSettings[ch].coupling = PS5000A_DC;
    channelSettings[ch].range = PS5000A_5V;
    channelSettings[ch].analogueOffset = 0.0f;

    status = ps5000aSetChannel(handle, (PS5000A_CHANNEL)ch, channelSettings[ch].enabled, channelSettings[ch].coupling,
      channelSettings[ch].range, channelSettings[ch].analogueOffset);

    if (status != PICO_OK)
    {
      fprintf(stderr, "ps5000aSetChannel (ch %d) ------ 0x%08lx \n", ch, status);
      return -1;
    }
  }

  // Set digital ports

  int16_t logicLevel = (int16_t)((1.5 / 5) * maxADCValue);

  status = ps5000aSetDigitalPort(handle, PS5000A_DIGITAL_PORT0, 1, logicLevel);

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aSetDigitalPort (PORT0) ------ 0x%08lx \n", status);
    return -1;
  }

  status = ps5000aSetDigitalPort(handle, PS5000A_DIGITAL_PORT1, 1, logicLevel);

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aSetDigitalPort (PORT1) ------ 0x%08lx \n", status);
    return -1;
  }

  // Trigger setup
  // -------------

  // Set up trigger on digital channel 15, falling edge, with auto trigger of 1 second

  // Set the condition for the port to which the channel belongs
  struct tPS5000ACondition digitalCondition;

  digitalCondition.source = PS5000A_DIGITAL_PORT1;
  digitalCondition.condition = PS5000A_CONDITION_TRUE;

  status = ps5000aSetTriggerChannelConditionsV2(handle, &digitalCondition, 1, (PS5000A_CONDITIONS_INFO)(PS5000A_CLEAR | PS5000A_ADD));

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aSetTriggerChannelConditionsV2 ------ 0x%08lx \n", status);
    return -1;
  }

  // Set digital channel properties
  struct tPS5000ADigitalChannelDirections digitalDirection;
  digitalDirection.channel = PS5000A_DIGITAL_CHANNEL_15;
  digitalDirection.direction = PS5000A_DIGITAL_DIRECTION_FALLING;

  status = ps5000aSetTriggerDigitalPortProperties(handle, &digitalDirection, 1);

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aSetTriggerDigitalPortProperties ------ 0x%08lx \n", status);
    return -1;
  }

  status = ps5000aSetAutoTriggerMicroSeconds(handle, 1000000);
  
  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aSetAutoTriggerMicroSeconds ------ 0x%08lx \n", status);
    return -1;
  }


  // Setup data buffers
  // ------------------

  // Data buffers for raw data collection
  int16_t * buffers[PS5000A_MAX_CHANNELS];
  int16_t * digiBuffers[MAX_DIGITAL_PORTS];

  int32_t preTriggerSamples = 100;
  int32_t postTriggerSamples = 10000;
  int32_t totalSamples = preTriggerSamples + postTriggerSamples;

  uint32_t downSampleRatio = 1;
  PS5000A_RATIO_MODE ratioMode = PS5000A_RATIO_MODE_NONE;

  // Set buffers for the analogue channels
  for (ch = 0; ch < numAvailableChannels; ch++)
  {
    if (channelSettings[ch].enabled)
    {
      buffers[ch] = (int16_t*)calloc(totalSamples, sizeof(int16_t));

      status = ps5000aSetDataBuffer(handle, (PS5000A_CHANNEL)ch, buffers[ch], totalSamples, 0, ratioMode);

      if (status != PICO_OK)
      {
        fprintf(stderr, "ps5000aSetDataBuffer ------ 0x%08lx \n", status);
        return -1;
      }
    }
  }

  // Set buffers for the digital ports
  digiBuffers[0] = (int16_t*)calloc(totalSamples, sizeof(int16_t));

  status = ps5000aSetDataBuffer(handle, PS5000A_DIGITAL_PORT0, digiBuffers[0], totalSamples, 0, ratioMode);

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aSetDataBuffer (PORT0) ------ 0x%08lx \n", status);
    return -1;
  }

  digiBuffers[1] = (int16_t*)calloc(totalSamples, sizeof(int16_t));

  status = ps5000aSetDataBuffer(handle, PS5000A_DIGITAL_PORT1, digiBuffers[1], totalSamples, 0, ratioMode);

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aSetDataBuffer (PORT1) ------ 0x%08lx \n", status);
    return -1;
  }

  // Query timebase function to obtain the time interval
  // ---------------------------------------------------

  float timeInterval = 0;
  int32_t maxSamples = 0;
  uint32_t timebase = 127;

  status = ps5000aGetTimebase2(handle, timebase, totalSamples, &timeInterval, &maxSamples, 0);

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aGetTimebase2 ------ 0x%08lx \n", status);
    return -1;
  }

  printf("\nTimebase: %d, time interval: %.1f ns\n\n", timebase, timeInterval);

  // Data collection
  // ---------------

  printf("Starting data collection...\n");

  // Start device collecting, then wait for completion
  g_ready = FALSE;
  int32_t timeIndisposed = 0;

  status = ps5000aRunBlock(handle, preTriggerSamples, postTriggerSamples, timebase, &timeIndisposed, 0, callBackBlock, NULL);

  while (!g_ready && !_kbhit())
  {
    Sleep(1);
  }

  if (g_ready)
  {
    // Can retrieve data using different ratios and ratio modes from driver
    status = ps5000aGetValues(handle, 0, (uint32_t*)&totalSamples, downSampleRatio, ratioMode, 0, &g_overflow);

    if (status != PICO_OK)
    {
      fprintf(stderr, "ps5000aGetValues ------ 0x%08lx \n", status);
      return -1;
    }

    printf("Data collection complete - collected %d samples per channel.\n", totalSamples);

    // Output data to file

    FILE * fp;
    FILE * digiFp;

    // Analogue data
    fopen_s(&fp, blockFile, "w");

    if (fp != NULL)
    {
      fprintf(fp, "Block Data log\n\n");
      fprintf(fp, "Results shown for each of the %d channels are displayed in ADC Count & millivolts.\n\n", numAvailableChannels);

      for (i = 0; i < totalSamples; i++)
      {
        for (ch = 0; ch < numAvailableChannels; ch++)
        {
          if (channelSettings[ch].enabled)
          {
            fprintf(fp, "Ch%C\t %d\t %d\t", 'A' + ch, buffers[ch][i],
              adc_to_mv(buffers[ch][i], channelSettings[PS5000A_CHANNEL_A + ch].range, maxADCValue));
          }
        }

        fprintf(fp, "\n");
      }

      fclose(fp);
    }
    else
    {
      printf("Cannot open file %s for writing.\n", blockFile);
    }

    // Digital data
    fopen_s(&digiFp, digiBlockFile, "w");

    int16_t bit;

    uint16_t bitValue;
    uint16_t digiValue;

    if (digiFp != NULL)
    {
      fprintf(digiFp, "Block Digital Data log\n");
      fprintf(digiFp, "Digital Channels will be in the order D15...D0\n");

      fprintf(digiFp, "\n");

      for (i = 0; i < totalSamples; i++)
      {
        digiValue = 0x00ff & digiBuffers[1][i];	// Mask Port 1 values to get lower 8 bits
        digiValue <<= 8;												// Shift by 8 bits to place in upper 8 bits of 16-bit word
        digiValue |= digiBuffers[0][i];					// Mask Port 0 values to get lower 8 bits and apply bitwise inclusive OR to combine with Port 1 values

        // Output data in binary form
        for (bit = 0; bit < 16; bit++)
        {
          // Shift value (32768 - binary 1000 0000 0000 0000), AND with value to get 1 or 0 for channel
          // Order will be D15 to D8, then D7 to D0

          bitValue = (0x8000 >> bit) & digiValue ? 1 : 0;
          fprintf(digiFp, "%d, ", bitValue);
        }

        fprintf(digiFp, "\n");
      }

      fclose(digiFp);
    }
    else
    {
      printf("Cannot open file %s for writing.\n", digiBlockFile);
    }
  }
  else
  {
    printf("Data collection cancelled.\n");
  }

  status = ps5000aStop(handle);

  if (status != PICO_OK)
  {
    fprintf(stderr, "ps5000aStop ------ 0x%08lx \n", status);
    return -1;
  }

  // Free memory allocated for buffers

  for (ch = 0; ch < numAvailableChannels; ch++)
  {
    if (channelSettings[ch].enabled)
    {
      free(buffers[ch]);
    }
  }

  free(digiBuffers[0]);
  free(digiBuffers[1]);

  printf("\n");

  // Close connection to device
  // --------------------------

  status = ps5000aCloseUnit(handle);

  printf("Exit...\n");

  Sleep(2000);

  return 0;
}
