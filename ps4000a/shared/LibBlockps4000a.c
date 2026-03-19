/*******************************************************************************
 *
 * Filename: Libps4000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 4XXXE Series (ps4000a) devices,
 *   for Block captures.
 *
 * Copyright (C) 2013-2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"
#include "../../shared/PicoScaling.h"
#include "./Libps4000a.h"
#include <stdio.h>


/* Headers for Windows */
#ifdef _WIN32
#include "ps4000aApi.h"
#else
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>


#include <libps4000a/ps4000aApi.h>
#ifndef PICO_STATUS
#include <libps4000a/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000 * a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a, b, c, d) memcpy(a, c, d)

typedef enum enBOOL { FALSE, TRUE } BOOL;

/* A function to detect a keyboard press on Linux */
int32_t _getch() {
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

int32_t _kbhit() {
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

int32_t fopen_s(FILE **a, const char *b, const char *c) {
  FILE *fp = fopen(b, c);
  *a = fp;
  return (fp > 0) ? 0 : -1;
}

/* A function to get a single character on Linux */
#define max(a, b) ((a) > (b) ? a : b)
#define min(a, b) ((a) < (b) ? a : b)
#endif

int8_t BlockFile[20] = "Block_Mode";

/****************************************************************************
 * Refernce Global Variables
 ***************************************************************************/
extern BOOL scaleVoltages;
extern uint32_t timebase;
extern int16_t g_ready;
extern const uint64_t constBufferSize;
/***************************************************************************/

/****************************************************************************
 * BlockDataHandler
 * - Used by all block data routines
 * - acquires data (user sets trigger mode before calling), displays 10 items
 *   and saves all to text file.
 * Inputs :
 * - unit : the unit to use.
 * - noOfPreTriggerSamples : number of samples to capture before trigger.
 * - noOfPostTriggerSamples : number of samples to capture after trigger.
 * - idealTimeInterval : the desired time interval (in seconds) between samples.
 * - nSamples : Set the number of samples per capture - Used by SetDataBuffers()
 * - ratioMode : Set the downsampling mode - Used by SetDataBuffers()
 * - downSampleRatio : Set the downsampling ratio - Used by SetDataBuffers()
 * Returns       none
 ****************************************************************************/
void blockDataHandler(GENERICUNIT *unit,
                      uint64_t noOfPreTriggerSamples,  // Used by RunBlock()
                      uint64_t noOfPostTriggerSamples, // Used by RunBlock()
                      double idealTimeInterval,        // Used by RunBlock()
                      uint64_t nSamples,         // Used by SetDataBuffers()
                      PICO_RATIO_MODE ratioMode, // Used by SetDataBuffers()
                      uint64_t downSampleRatio   // Used by SetDataBuffers()
) {
  int16_t retry;
  int16_t triggerEnabled = 0;
  int16_t pwqEnabled = 0;

  int32_t i;
  double timeIndisposed = 0;

  PICO_STATUS status;
  //--------------------------------------------------------------------------//
  // Capture settings
  // uint64_t nSamples = constBufferSize;	//Set the number of samples per
  // capture

  // Buffers settings (Set DownSampling mode and ratio)
  // Use scope acquisition settings for first data download
  struct tbuffer_settings bufferSettings = {0};
  bufferSettings.startIndex = 0;
  bufferSettings.downSampleRatioMode = ratioMode;
  bufferSettings.downSampleRatio = downSampleRatio;
  bufferSettings.nSamples = nSamples;
  //--------------------------------------------------------------------------//

  // Create Buffers - Min and Max (3D buffer - 1 Segment, Channels, Samples)
  struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
  int16_t ***minBuffers;
  int16_t ***maxBuffers;
  pico_create_multibuffers(unit, bufferSettings, 1, &minBuffers, &maxBuffers,
                           &multiBufferSizes);

  // SetDataBuffers with API
  SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers,
                    &multiBufferSizes, 0, (CAPTURE_MODE)BLOCK, 0);

  // Find nearest timebase
  // Find the analogue channels that are enabled
  PICO_CHANNEL_FLAGS enabledChannelOrPortFlags = (PICO_CHANNEL_FLAGS)0;
  for (int32_t ch = 0; ch < unit->channelCount; ch++) {
    if (unit->channelSettings[ch].enabled) {
      enabledChannelOrPortFlags =
          enabledChannelOrPortFlags | (PICO_CHANNEL_FLAGS)(1 << ch);
    }
  }
  /////////
  if (!unit->hasIntelligentProbes) ////////////////////////////////////////////////
  {
      status = ps4000aNearestSampleIntervalStateless(
          unit->handle, enabledChannelOrPortFlags, idealTimeInterval,
          unit->resolution,
          0, // useEts
          &timebase, &(unit->timeInterval));
      if (status != PICO_OK) {
          printf("BlockDataHandler:ps4000aNearestSampleIntervalStateless ------ "
              "0x%08x \n",
              status);
          return;
      }
  }
  ///////
  printf("\nTimebase: %lu  SampleInterval: %le seconds\n", timebase,
         unit->timeInterval);

  printf("Number of Capture Samples: %llu\n", nSamples);
  if (ratioMode == PICO_RATIO_MODE_RAW)
    printf("DownSampling Mode is set to: None\n");
  if (ratioMode == PICO_RATIO_MODE_AGGREGATE)
    printf("DownSampling Mode is set to: Aggregate (Min. and Max. values)\n");
  if (ratioMode == PICO_RATIO_MODE_DECIMATE)
    printf("DownSampling Mode is set to: Decimate\n");
  if (ratioMode == PICO_RATIO_MODE_AVERAGE)
    printf("DownSampling Mode is set to: Average\n");
  if (ratioMode != PICO_RATIO_MODE_RAW)
    printf("\nDownSampling Ratio is set to: %llu\n", downSampleRatio);

  /* Start it collecting, then wait for completion*/
  g_ready = FALSE;

  do {
    retry = 0;

    status = ps4000aRunBlock(unit->handle, noOfPreTriggerSamples,
                             noOfPostTriggerSamples, timebase, &timeIndisposed,
                             0, callBackBlockReady, NULL);

    if (status != PICO_OK) {
      printf("BlockDataHandler:ps4000aRunBlock ------ 0x%08lx \n", status);
      return;
    }
  } while (retry);

  // status = ps4000aIsTriggerOrPulseWidthQualifierEnabled(unit->handle,
  // &triggerEnabled, &pwqEnabled);

  if (triggerEnabled || pwqEnabled) {
    printf("Waiting for trigger... Press any key to abort\n");
  } else {
    printf("Press any key to abort\n");
  }

  // wait for capture to complete or for user to abort
  while (!g_ready && !_kbhit()) {
    Sleep(1);
  }

  if (g_ready) {
    // Can retrieve data using different ratios and ratio modes from driver
    int16_t overflow = 0;

    status = ps4000aGetValues(unit->handle, 0, (uint64_t *)&nSamples,
                              downSampleRatio, ratioMode, 0, &overflow);

    if (status != PICO_OK) {
      printf("blockDataHandler:ps4000aGetValues ------ 0x%08lx \n", status);
    } else {
      // Get scaling Info for each channel
      struct tPicoProbeScaling enabledChannelsScaling[PS4000A_MAX_CHANNELS] = {0};
      struct tPicoProbeScaling channelRangeInfoTemp;
      for (i = 0; i < unit->channelCount; i++) {
        if (unit->channelSettings[i].enabled) {
          getRangeScaling(unit->channelSettings[PS4000A_CHANNEL_A + i].range,
                          &channelRangeInfoTemp);
          enabledChannelsScaling[i] = channelRangeInfoTemp;
        }
      }
      unit->CapturesComplete = 1; // set to 1 to indicate complete

      // Write to console
      WriteArrayToStdoutGeneric(
          unit, minBuffers, maxBuffers, multiBufferSizes,
          enabledChannelsScaling, (CAPTURE_MODE)BLOCK,
          1,  // Number of buffers to write, 1 for block mode
          10, // Number of samples to write
          0,  // Triggersample
          &overflow);

      // Write one segment to a file as captured
      printf("\nWriting Capture of enabled channels to file.\n");
      WriteArrayToFilesGeneric(unit, minBuffers, maxBuffers, multiBufferSizes,
                               enabledChannelsScaling, BlockFile,
                               0, // Triggersample
                               &overflow, NULL);
    }
  } else {
    printf("Data collection aborted\n");
    _getch();
  }

  clearDataBuffers(unit);
  pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);
}

/****************************************************************************
 * BlockOverlappedDataHandler
 * - acquires data (user sets trigger mode before calling),
 * - saves all data to text file and repeats in a loop.
 * (repeated block captures, used to save calls to the unit
 * (deferred requests for data))
 * Inputs :
 * - unit : the unit to use.
 * - noOfPreTriggerSamples : number of samples to capture before trigger.
 * - noOfPostTriggerSamples : number of samples to capture after trigger.
 * - idealTimeInterval : the desired time interval (in seconds) between samples.
 * - nSamples : Set the number of samples per capture - Used by SetDataBuffers()
 * - ratioMode : Set the downsampling mode - Used by SetDataBuffers()
 * - downSampleRatio : Set the downsampling ratio - Used by SetDataBuffers()
 * Returns       none
 ****************************************************************************/
void blockOverlappedDataHandler(
    GENERICUNIT *unit,
    uint64_t noOfPreTriggerSamples,  // Used by RunBlock()
    uint64_t noOfPostTriggerSamples, // Used by RunBlock()
    double idealTimeInterval,        // Used by RunBlock()
    uint64_t nSamples,               // Used by SetDataBuffers()
    PICO_RATIO_MODE ratioMode,       // Used by SetDataBuffers()
    uint64_t downSampleRatio         // Used by SetDataBuffers()
) {
  int16_t triggerEnabled = 0;
  int16_t pwqEnabled = 0;

  int32_t i;
  double timeIndisposed = 0;

  PICO_STATUS status;
  //--------------------------------------------------------------------------//
  // Capture settings
  // uint64_t nSamples = constBufferSize;	//Set the number of samples per
  // capture

  // Buffers settings (Set DownSampling mode and ratio)
  // Use scope acquisition settings for first data download
  struct tbuffer_settings bufferSettings = {0};
  bufferSettings.startIndex = 0;
  bufferSettings.downSampleRatioMode = ratioMode;
  bufferSettings.downSampleRatio = downSampleRatio;
  bufferSettings.nSamples = nSamples;
  //--------------------------------------------------------------------------//

  // Create Buffers - Min and Max (3D buffer - 1 Segment, Channels, Samples)
  struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
  int16_t ***minBuffers;
  int16_t ***maxBuffers;
  pico_create_multibuffers(unit, bufferSettings, 1, &minBuffers, &maxBuffers,
                           &multiBufferSizes);

  // SetDataBuffers with API
  SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers,
                    &multiBufferSizes, 0, (CAPTURE_MODE)BLOCK, 0);

  // Find nearest timebase
  // Find the analogue channels that are enabled
  PICO_CHANNEL_FLAGS enabledChannelOrPortFlags = (PICO_CHANNEL_FLAGS)0;
  for (int32_t ch = 0; ch < unit->channelCount; ch++) {
    if (unit->channelSettings[ch].enabled) {
      enabledChannelOrPortFlags =
          enabledChannelOrPortFlags | (PICO_CHANNEL_FLAGS)(1 << ch);
    }
  }

  status = ps4000aNearestSampleIntervalStateless(
      unit->handle, enabledChannelOrPortFlags, idealTimeInterval,
      unit->resolution,
      0, // useEts
      &timebase, &(unit->timeInterval));
  if (status != PICO_OK) {
    printf("RapidBlockDataHandler:ps4000aNearestSampleIntervalStateless ------ "
           "0x%08x \n",
           status);
    return;
  }

  printf("\nTimebase: %lu  SampleInterval: %le seconds\n", timebase,
         unit->timeInterval);

  printf("Number of Capture Samples: %llu\n", nSamples);
  if (ratioMode == PICO_RATIO_MODE_RAW)
    printf("DownSampling Mode is set to: None\n");
  if (ratioMode == PICO_RATIO_MODE_AGGREGATE)
    printf("DownSampling Mode is set to: Aggregate (Min. and Max. values)\n");
  if (ratioMode == PICO_RATIO_MODE_DECIMATE)
    printf("DownSampling Mode is set to: Decimate\n");
  if (ratioMode == PICO_RATIO_MODE_AVERAGE)
    printf("DownSampling Mode is set to: Average\n");
  if (ratioMode != PICO_RATIO_MODE_RAW)
    printf("\nDownSampling Ratio is set to: %llu\n", downSampleRatio);

  int16_t overflow = 0;
  // Setup deferred request for data
  // Can retrieve data using different ratios and ratio modes from driver
  status = ps4000aGetValuesOverlapped(unit->handle, 0, (uint64_t *)&nSamples,
                                      downSampleRatio, ratioMode, 0, &overflow);

  // Start capture
  g_ready = FALSE;
  /////////////////////// Loop for overlapped captures ////////////////////
  uint16_t NumOverlapped = 3;
  printf("NumOverlapped captures: %d \n", NumOverlapped);
  for (uint16_t OverlappedtestNo = 0; OverlappedtestNo < NumOverlapped;
       OverlappedtestNo++) {
    status = ps4000aRunBlock(unit->handle, noOfPreTriggerSamples,
                             noOfPostTriggerSamples, timebase, &timeIndisposed,
                             0, callBackBlockReady, NULL);

    if (status != PICO_OK) {
      printf("BlockDataHandler:ps4000aRunBlock ------ 0x%08lx \n", status);
      return;
    }

    printf("Press any key to abort\n");

    // wait for capture to complete or for user to abort
    while (!g_ready && !_kbhit()) {
      Sleep(1);
    }

    if (g_ready) {
      if (status == PICO_OK) {
        // Get scaling Info for each channel
        struct tPicoProbeScaling enabledChannelsScaling[PS4000A_MAX_CHANNELS] =
            {0};
        struct tPicoProbeScaling channelRangeInfoTemp;
        for (i = 0; i < unit->channelCount; i++) {
          if (unit->channelSettings[i].enabled) {
            getRangeScaling(unit->channelSettings[PS4000A_CHANNEL_A + 0].range,
                            &channelRangeInfoTemp);
            enabledChannelsScaling[i] = channelRangeInfoTemp;
          }
        }
        unit->CapturesComplete = 1; // set to 1 to indicate complete

        // Create file name string
        char buf[58 + (3 * sizeof(int))];
        size_t buf_size = sizeof(buf) / sizeof(buf[0]);
        snprintf(buf, buf_size, "%s%d_Segment", BlockFile, OverlappedtestNo);
        printf("\nWriting capture %ld of channels to a file.\n",
               OverlappedtestNo);
        // Write one segment to a file as captured
        printf("\nWriting Capture of enabled channels to file.\n");
        WriteArrayToFilesGeneric(unit, minBuffers, maxBuffers, multiBufferSizes,
                                 enabledChannelsScaling, buf,
                                 0, // Triggersample
                                 &overflow, NULL);
      }
    } else {
      printf("Data collection aborted\n");
      _getch();
    }
  } //////////////////////// End Overlapped loop
    //////////////////////////////////////////

  clearDataBuffers(unit);
  pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);
}
