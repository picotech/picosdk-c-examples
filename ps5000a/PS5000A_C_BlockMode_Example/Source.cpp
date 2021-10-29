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
#include <vector>
#include <iostream>
#include <fstream>
#include "ps5000aApi.h"

#include "GenericMethods.h"

using namespace std;

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

int32_t fopen_s(FILE** a, const int8_t* b, const int8_t* c)
{
  FILE* fp = fopen(b, c);
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

// int32_t     g_sampleCount;
// uint32_t		g_startIndex;

#define OBJ
#define STRUCTURED

/****************************************************************************
* main
*
***************************************************************************/
int32_t main(void)
{

  getStatusCode(67);
  getStatusCode();


  PICO_STATUS status = PICO_OK;
  int16_t handle = 0;
  bool usbPowered = 0;

  const int32_t NO_OF_SAMPLES = 100;

  status = ps5000aOpenUnit(&handle, NULL, PS5000A_DEVICE_RESOLUTION::PS5000A_DR_15BIT);
  if (PICO_OK != status) {
    std::cout << "ERROR : Open Unit : " << status << std::endl;
    getStatusCode(status);
  }
  if (PICO_POWER_SUPPLY_NOT_CONNECTED == status) {
    usbPowered = 1;
    status = ps5000aChangePowerSource(handle, status);
  }

  if (PICO_OK != status) {
    std::cout << "ERROR : Open Unit : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }

  status = ps5000aSetChannel(handle, PS5000A_CHANNEL_A, 1, PS5000A_DC, PS5000A_1V, 0);
  if (PICO_OK != status) {
    std::cout << "ERROR : Set Channel A : " << status << std::endl;
    return -1;
  }

  status = ps5000aSetChannel(handle, PS5000A_CHANNEL_B, 1, PS5000A_DC, PS5000A_1V, 0);
  if (PICO_OK != status) {
    std::cout << "ERROR : Set Channel B : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }

  if (0 == usbPowered) {
    status = ps5000aSetChannel(handle, PS5000A_CHANNEL_C, 0, PS5000A_DC, PS5000A_1V, 0);
    if (PICO_OK != status) {
      std::cout << "ERROR : Set Channel C : " << status << std::endl;
      getStatusCode(status);
      return -1;
    }

    status = ps5000aSetChannel(handle, PS5000A_CHANNEL_D, 0, PS5000A_DC, PS5000A_1V, 0);
    if (PICO_OK != status) {
      std::cout << "ERROR : Set Channel D : " << status << std::endl;
      getStatusCode(status);
      return -1;
    }
  }

  int16_t* bufferA = (int16_t*)calloc(NO_OF_SAMPLES, sizeof(int16_t));
  int16_t* bufferB = (int16_t*)calloc(NO_OF_SAMPLES, sizeof(int16_t));
  status = ps5000aSetDataBuffer(handle, PS5000A_CHANNEL_A, bufferA, NO_OF_SAMPLES, 0, PS5000A_RATIO_MODE_NONE);
  if (PICO_OK != status) {
    std::cout << "ERROR : Set Buffer Channel A : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }
  status = ps5000aSetDataBuffer(handle, PS5000A_CHANNEL_B, bufferB, NO_OF_SAMPLES, 0, PS5000A_RATIO_MODE_NONE);
  if (PICO_OK != status) {
    std::cout << "ERROR : Set Buffer Channel B : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }

  int timeIntervalNS;
  int32_t maxSamples;
  uint32_t TIMEBASE = 4;
  status = ps5000aGetTimebase(handle, TIMEBASE, NO_OF_SAMPLES, &timeIntervalNS, &maxSamples, 0);
  if (PICO_OK != status) {
    std::cout << "ERROR : Set Trigger : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }

  status = ps5000aSetSimpleTrigger(handle, 1, PS5000A_CHANNEL_A, 100, PS5000A_THRESHOLD_DIRECTION::PS5000A_RISING, 0, 5000);
  if (PICO_OK != status) {
    std::cout << "ERROR : Set Trigger : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }

  status = ps5000aSetSigGenBuiltInV2(handle,
    0,
    1000000,
    PS5000A_SINE,
    100,
    100,
    1,
    1,
    PS5000A_UP,
    PS5000A_ES_OFF,
    0,
    0,
    PS5000A_SIGGEN_RISING,
    PS5000A_SIGGEN_NONE,
    0);
  if (PICO_OK != status) {
    std::cout << "ERROR : AWG Signal Genertion : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }

  constexpr int32_t NUMBER_OF_CHANNELS = 1;
  auto* timeIndisposedMs = new int32_t(NUMBER_OF_CHANNELS);
  status = ps5000aRunBlock(handle, 10, NO_OF_SAMPLES - 10, TIMEBASE, timeIndisposedMs, 0, NULL, NULL);
  if (PICO_OK != status) {
    std::cout << "ERROR : RunBlock : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }

  int16_t isReady = 0;
  while (0 == isReady && PICO_OK == status) {
    status = ps5000aIsReady(handle, &isReady);
    std::cout << "PS3 IsReady : " << isReady << std::endl;
    if (PICO_OK != status) {
      std::cout << "Error : IsReady Issue : " << status << std::endl;
      getStatusCode(status);
      return -1;
    }
    Sleep(1);
  }

  int noSamples;
  status = ps5000aGetValues(handle, 0, (uint32_t*)&noSamples, 1, PS5000A_RATIO_MODE_NONE, 0, nullptr);
  if (PICO_OK != status) {
    std::cout << "Error : Get Values Issue : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }

  std::cout << "Print Buffer A : " << std::endl;
  for (auto sampleIndex = 0; sampleIndex < noSamples; sampleIndex++)
    std::cout << sampleIndex << ";" << bufferA[sampleIndex] << std::endl;

  std::cout << std::endl;
  std::cout << "Print Buffer B : " << std::endl;
  for (auto sampleIndex = 0; sampleIndex < noSamples; sampleIndex++)
    std::cout << sampleIndex << ";" << bufferB[sampleIndex] << std::endl;

  status = ps5000aCloseUnit(handle);
  if (PICO_OK != status) {
    std::cout << "Error : Close Unit : " << status << std::endl;
    getStatusCode(status);
    return -1;
  }


  return 0;
}