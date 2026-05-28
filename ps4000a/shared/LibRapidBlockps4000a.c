/*******************************************************************************
 *
 * Filename: LibRapidBlockps4000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 4XXXE Series (ps4000a) devices,
 *   for RapidBlock captures.
 *
 * Copyright (C) 2026 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"
#include "../../shared/PicoScaling.h"
#include <stdbool.h>
#include <stdio.h>


#include "./Libps4000a.h"

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


#include <ps4000aApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif

#endif

int8_t RapidBlockFile[] = "rapidBlock_Segment";
int8_t RapidBlockOverlappedFile[] = "rapidBlockOverlapped";
FILE *fp = NULL;

/****************************************************************************
 * Refernce Global Variables
 ***************************************************************************/
extern BOOL scaleVoltages;
extern uint32_t timebase;
extern int16_t g_ready;
extern const uint64_t constBufferSize;
/***************************************************************************/

/****************************************************************************
 * rapidblockDataHandler
 *  This function demonstrates how to collect a set of captures using
 *  rapid block mode.
 * Inputs :
 * - unit : the unit to use.
 * - noOfPreTriggerSamples : number of samples to capture before trigger.
 * - noOfPostTriggerSamples : number of samples to capture after trigger.
 * - idealTimeInterval : the desired time interval (in seconds) between samples.
 * - nSamples : Set the number of samples per capture - Used by SetDataBuffers()
 * - nCaptures : Set the number of captures - Used by SetNoOfCaptures()
 * - ratioMode : Set the downsampling mode - Used by SetDataBuffers()
 * - downSampleRatio : Set the downsampling ratio - Used by SetDataBuffers()
 * Returns       none
 ****************************************************************************/
void rapidblockDataHandler(
    GENERICUNIT *unit,
    uint64_t noOfPreTriggerSamples,  // Used by RunBlock()
    uint64_t noOfPostTriggerSamples, // Used by RunBlock()
    double idealTimeInterval,        // Used by RunBlock()
    uint64_t nSamples,               // Used by SetDataBuffers()
    uint64_t nCaptures,
    PICO_RATIO_MODE ratioMode, // Used by SetDataBuffers()
    uint64_t downSampleRatio,   // Used by SetDataBuffers()
    FILE_TYPE filetype) {
  PICO_STATUS status = 0;
  int16_t i;

  int32_t nMaxSamples = 0;
  int32_t timeIndisposed = 0;

  int16_t ***minBuffers;
  int16_t ***maxBuffers;

  uint32_t nCompletedCaptures = 0;
  PICO_ACTION action_flag =
      (PICO_CLEAR_ALL |
       PICO_ADD); // bitwise OR flags for first buffer that is set

  // Capture settings
  // Buffers settings (Set DownSampling mode and ratio)
  struct tbuffer_settings bufferSettings = {0};
  bufferSettings.startIndex = 0;
  bufferSettings.downSampleRatioMode = ratioMode;
  bufferSettings.downSampleRatio = downSampleRatio;
  bufferSettings.nSamples = (uint32_t)nSamples;

  // printf(scaleVoltages ? "Volts\n" : "ADC Counts\n");
  printf("Press any key to abort\n");

  setDefaults(unit);

  // Segment the memory
  status = ps4000aMemorySegments(unit->handle, nCaptures, &nMaxSamples);

  // Set the number of captures
  status = ps4000aSetNoOfCaptures(unit->handle, nCaptures);

  // Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
  struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
  pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffers,
                           &maxBuffers, &multiBufferSizes);

  // Create Overflow Array Buffers
  int16_t *overflowArray;
  overflowArray = (int16_t *)calloc(nCaptures, sizeof(int16_t));

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


  printf("\nTimebase: %lu  SampleInterval: %le seconds\n", timebase,
         unit->timeInterval);
  printf("%llu Captures each with %llu ADC Samples\n", nCaptures, nSamples);
  if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_RAW)
    printf("DownSampling Mode is set to: None\n");
  if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_AGGREGATE)
    printf("DownSampling Mode is set to: Aggregate (Min. and Max. values)\n");
  if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_DECIMATE)
    printf("DownSampling Mode is set to: Decimate\n");
  if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_AVERAGE)
    printf("DownSampling Mode is set to: Average\n");
  if (bufferSettings.downSampleRatioMode != PICO_RATIO_MODE_RAW)
    printf("DownSampling Ratio is set to: %llu\n",
           bufferSettings.downSampleRatio);

  // Start acquisition
  status = ps4000aRunBlock(unit->handle, noOfPreTriggerSamples,
                           noOfPostTriggerSamples, timebase, &timeIndisposed, 0,
                           callBackBlockReady, NULL);

  if (status != PICO_OK) {
    printf("BlockDataHandler:ps4000aRunBlock ------ 0x%08x \n", status);
  }

  // Wait until data ready
  g_ready = FALSE;

  while (!g_ready && !_kbhit()) {
    Sleep(1);
  }

  if (!g_ready) // If user aborted stop the acquisition
  {
    _getch();
    printf("Rapid capture aborted. ");
    status = ps4000aStop(unit->handle);
  }
  // Get the number of captures that were completed
  status = ps4000aGetNoOfCaptures(unit->handle, &nCompletedCaptures);
  printf("%llu complete blocks were captured\n", nCompletedCaptures);
  printf("\nPress any key...\n\n");
  _getch();

  if (nCompletedCaptures == 0) {
    return; // Exit if no captures were made
  }

  // Only use the blocks that were captured
  nCaptures = nCompletedCaptures;
  unit->CapturesComplete = nCompletedCaptures;

  // SetDataBuffers with API
  SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers,
                    &multiBufferSizes, 0, (CAPTURE_MODE)RAPID_BLOCK, 0);
;
  // Get data from device
  status = ps4000aGetValuesBulk(
      unit->handle,
      (uint32_t *)&nSamples,                // Number of samples for each segment
      0,                                  // From Segment
      nCaptures - 1,                      // To Segment
      bufferSettings.downSampleRatio,     // Down Sample Ratio
      bufferSettings.downSampleRatioMode, // Down Sample Ratio mode
      overflowArray);                     // Array of Channel overrage flags
  
  if (status == PICO_OK) {
    // Get scaling Info for each channel
    struct tPicoProbeScaling enabledChannelsScaling[PS4000A_MAX_CHANNELS] = {0};
    struct tPicoProbeScaling channelRangeInfoTemp;
    for (i = 0; i < unit->channelCount; i++)
    {
      if (unit->channelSettings[i].enabled)
      {
        getRangeScaling(unit->channelSettings[PS4000A_CHANNEL_A + i].range,
                        &channelRangeInfoTemp);
        //if (channelRangeInfoTemp.ProbeEnum >
        //    PICO_X10_PROBE_RANGES) // Print nonstandard ranges info
        //{
          //printf("Channel %c:\tEnum range:%d text range:%s MinS:%f MaxS:%f "
          //       "UnitText:%s\n",
          //       'A' + i, channelRangeInfoTemp.ProbeEnum,
          //       channelRangeInfoTemp.Probe_Range_text,
          //       channelRangeInfoTemp.MinScale, channelRangeInfoTemp.MaxScale,
          //       channelRangeInfoTemp.Unit_text);
        //}
        enabledChannelsScaling[i] = channelRangeInfoTemp;
      }
    }
    // Write to console
    WriteArrayToStdoutGeneric(unit, minBuffers, maxBuffers, multiBufferSizes,
                              enabledChannelsScaling,
                              (enum enCaptureMode)RAPID_BLOCK,
                              3,  // Number of buffers to write
                              10, // Number of samples to write
                              noOfPreTriggerSamples, // Triggersample
                              overflowArray);
    // Print each segment capture to a file
    printf("\nWriting each of: %lld channel buffer sets to a file.\n",
           multiBufferSizes.numberOfBuffers);
    if (filetype == FILE_TXT)
    {
        WriteArrayToFilesGeneric(unit, minBuffers, maxBuffers, multiBufferSizes,
            enabledChannelsScaling, RapidBlockFile,
            noOfPreTriggerSamples, // Triggersample
            overflowArray, NULL);
    }
    if (filetype == FILE_BIN)
    {
        WriteMetaDataToFile(unit, multiBufferSizes,
            enabledChannelsScaling, "PicoMetaData_RapidBlock",
            noOfPreTriggerSamples, // Triggersample
            NULL);
        WriteArrayToFilesBinary(unit, minBuffers, maxBuffers, multiBufferSizes,
            enabledChannelsScaling, RapidBlockFile,
            noOfPreTriggerSamples, // Triggersample
            overflowArray, NULL);
    }
    printf("\n");
  }
  // Stop device
  status = ps4000aStop(unit->handle);

  // Release buffers from API
  clearDataBuffers(unit);
  // free buffers
  pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);
  free(overflowArray);
}

/****************************************************************************
 * rapidblockOverlappedDataHandler
 *  This function demonstrates how to collect a set of captures using
 *  rapid block Overlapped mode in a loop.
 * (repeated rapid blocks, used to save calls to the unit
 * (deferred requests for data))
 * Inputs :
 * - unit : the unit to use.
 * - noOfPreTriggerSamples : number of samples to capture before trigger.
 * - noOfPostTriggerSamples : number of samples to capture after trigger.
 * - idealTimeInterval : the desired time interval (in seconds) between samples.
 * - nSamples : Set the number of samples per capture - Used by SetDataBuffers()
 * - nCaptures : Set the number of captures - Used by SetNoOfCaptures()
 * - ratioMode : Set the downsampling mode - Used by SetDataBuffers()
 * - downSampleRatio : Set the downsampling ratio - Used by SetDataBuffers()
 * Returns       none
 ****************************************************************************/
void rapidblockOverlappedDataHandler(
    GENERICUNIT *unit,
    uint64_t noOfPreTriggerSamples,  // Used by RunBlock()
    uint64_t noOfPostTriggerSamples, // Used by RunBlock()
    double idealTimeInterval,        // Used by RunBlock()
    uint64_t nSamples,               // Used by SetDataBuffers()
    uint64_t nCaptures,
    PICO_RATIO_MODE ratioMode, // Used by SetDataBuffers()
    uint64_t downSampleRatio   // Used by SetDataBuffers()
) {
  PICO_STATUS status = 0;
  int16_t i;

  int32_t nMaxSamples = 0;
  int32_t timeIndisposed = 0;

  int16_t ***minBuffers;
  int16_t ***maxBuffers;

  uint32_t nCompletedCaptures = 0;
  PICO_ACTION action_flag =
      (PICO_CLEAR_ALL |
       PICO_ADD); // bitwise OR flags for first buffer that is set

  // Capture settings
  // Buffers settings (Set DownSampling mode and ratio)
  struct tbuffer_settings bufferSettings = {0};
  bufferSettings.startIndex = 0;
  bufferSettings.downSampleRatioMode = ratioMode;
  bufferSettings.downSampleRatio = downSampleRatio;
  bufferSettings.nSamples = nSamples;

  printf("Rapidblock Overlapped capture looping...\n");
  printf("Press any key to abort\n");

  setDefaults(unit);

  // Segment the memory
  status = ps4000aMemorySegments(unit->handle, nCaptures, &nMaxSamples);

  // Set the number of captures
  status = ps4000aSetNoOfCaptures(unit->handle, nCaptures);

  // Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
  struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
  pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffers,
                           &maxBuffers, &multiBufferSizes);

  // SetDataBuffers with API
  SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers,
                    &multiBufferSizes, 0, (CAPTURE_MODE)RAPID_BLOCK, 0);

  // Create Overflow Array Buffers
  int16_t *overflowArray;
  overflowArray = (int16_t *)calloc(nCaptures, sizeof(int16_t));

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
  printf("%llu Captures each with %llu ADC Samples\n", nCaptures, nSamples);
  if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_RAW)
    printf("DownSampling Mode is set to: None\n");
  if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_AGGREGATE)
    printf("DownSampling Mode is set to: Aggregate (Min. and Max. values)\n");
  if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_DECIMATE)
    printf("DownSampling Mode is set to: Decimate\n");
  if (bufferSettings.downSampleRatioMode == PICO_RATIO_MODE_AVERAGE)
    printf("DownSampling Mode is set to: Average\n");
  if (bufferSettings.downSampleRatioMode != PICO_RATIO_MODE_RAW)
    printf("DownSampling Ratio is set to: %llu\n",
           bufferSettings.downSampleRatio);
  printf("\n");

  // Setup deferred request for data
  status = ps4000aGetValuesOverlappedBulk(
      unit->handle,
      0,                                  // Start Index for each segment
      (uint32_t *)&nSamples,              // Number of samples for each segment
      bufferSettings.downSampleRatio,     // Down Sample Ratio
      bufferSettings.downSampleRatioMode, // Down Sample Ratio mode
      0,                                  // From Segment
      nCaptures - 1,                      // To Segment
      overflowArray);

  /////////////////////// Loop for overlapped captures ////////////////////
  uint16_t NumOverlapped = 4;
  for (uint16_t OverlappedtestNo = 0; OverlappedtestNo < NumOverlapped;
       OverlappedtestNo++) {
    g_ready = FALSE;
    printf("Loop: #%d of %d Rapid Block Overlapped captures\n",
           OverlappedtestNo + 1, NumOverlapped);

    // Start acquisition
    status = ps4000aRunBlock(unit->handle, noOfPreTriggerSamples,
                             noOfPostTriggerSamples, timebase, &timeIndisposed,
                             0, callBackBlockReady, NULL);

    if (status != PICO_OK) {
      printf("BlockDataHandler:ps4000aRunBlock ------ 0x%08x \n", status);
    }

    // Wait for capture to complete or for user to abort
    printf("Press any key to abort\n");
    while (!g_ready && !_kbhit()) {
      Sleep(1);
    }

    if (!g_ready) // If user aborted stop the acquisition
    {
      _getch();
      printf("Rapid capture aborted.\n");
      status = ps4000aStop(unit->handle);
    }
    // Get the number of captures that were completed
    status = ps4000aGetNoOfCaptures(unit->handle, &nCompletedCaptures);
    printf("%llu complete blocks were captured\n", nCompletedCaptures);

    if (nCompletedCaptures == 0) {
      return; // Exit if no captures were made
    }

    // Only use the blocks that were captured
    nCaptures = nCompletedCaptures;
    unit->CapturesComplete = nCompletedCaptures;

    if (status == PICO_OK) {
      // Get scaling Info for each channel
      struct tPicoProbeScaling enabledChannelsScaling[PS4000A_MAX_CHANNELS] = {
          0};
      struct tPicoProbeScaling channelRangeInfoTemp;
      for (i = 0; i < unit->channelCount; i++) {
        if (unit->channelSettings[i].enabled) {
          getRangeScaling(unit->channelSettings[PS4000A_CHANNEL_A + 0].range,
                          &channelRangeInfoTemp);
          if (channelRangeInfoTemp.ProbeEnum >
              PICO_X10_PROBE_RANGES) // Print nonstandard ranges info
          {
            printf("Channel %c:\tEnum range:%d text range:%s MinS:%f MaxS:%f "
                   "UnitText:%s\n",
                   'A' + i, channelRangeInfoTemp.ProbeEnum,
                   channelRangeInfoTemp.Probe_Range_text,
                   channelRangeInfoTemp.MinScale, channelRangeInfoTemp.MaxScale,
                   channelRangeInfoTemp.Unit_text);
          }
          enabledChannelsScaling[i] = channelRangeInfoTemp;
        }
      }

      // Print each segment capture to a file
      printf("Writing each of: %lld channel buffer sets to a file.\n",
             multiBufferSizes.numberOfBuffers);
      // Create file name string
      char buf[58 + (3 * sizeof(int))];
      size_t buf_size = sizeof(buf) / sizeof(buf[0]);
      snprintf(buf, buf_size, "%s%d_Segment", RapidBlockOverlappedFile,
               OverlappedtestNo);
      printf("\nWriting capture %ld of channels to a file.\n",
             OverlappedtestNo);
      WriteArrayToFilesGeneric(unit, minBuffers, maxBuffers, multiBufferSizes,
                               enabledChannelsScaling, buf,
                               noOfPreTriggerSamples, // Triggersample
                               overflowArray, NULL);

      printf("\n");
    }
  } //////////////////////// End Overlapped loop
    //////////////////////////////////////////
  // Stop device
  status = ps4000aStop(unit->handle);

  // Release buffers from API
  clearDataBuffers(unit);
  // free buffers
  pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);
  free(overflowArray);
}
