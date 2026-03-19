/*******************************************************************************
 *
 * Filename: Libps4000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 4XXXE Series (ps4000a) devices,
 *   for Streaming captures.
 *
 * Copyright (C) 2013-2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"
#include "../../shared/PicoScaling.h"
#include "./Libps4000a.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
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

char startOfFileName[] = "StreamingCaptureNoS_";

/****************************************************************************
 * Refernce Global Variables
 ***************************************************************************/
extern BOOL scaleVoltages;
extern uint32_t timebase;
extern int16_t g_ready;
extern const uint64_t constBufferSize;

typedef struct {
  struct tPicoStreamingDataInfo *dataStreamInfo;
  struct tPicoStreamingDataTriggerInfo *triggerInfo;
  int16_t numEnabled;
} StreamingContext;

void PREF4 CallStreamingReady(int16_t handle, int32_t noOfSamples,
                              uint32_t startIndex, int16_t overflow,
                              uint32_t triggerAt, int16_t triggered,
                              int16_t autoStop, void *pParameter) {
  StreamingContext *context = (StreamingContext *)pParameter;
  // Update trigger info
  if (context->triggerInfo) {
    context->triggerInfo->triggered_ = triggered;
    context->triggerInfo->triggerAt_ = triggerAt;
    context->triggerInfo->autoStop_ = autoStop;
  }

  // Update dataStreamInfo for all enabled channels
  if (context->dataStreamInfo) {
    for (int i = 0; i < context->numEnabled; i++) {
      context->dataStreamInfo[i].startIndex_ = startIndex;
      context->dataStreamInfo[i].noOfSamples_ = noOfSamples;
      // ps4000a implies overflow bitmask or global. Assigning global for now as
      // safety
      context->dataStreamInfo[i].overflow_ = (overflow == 0) ? 0 : 1;
    }
  }
}
/***************************************************************************/

/****************************************************************************
 * streamDataHandler
 * - Used by all streaming data routines
 * - acquires data (user sets trigger mode before calling), displays 10 items
 *   and saves all data to a file.
 * Input :
 * - unit : the unit to use.
 * - noOfPreTriggerSamples : number of samples to capture before trigger.
 * - noOfPostTriggerSamples : number of samples to capture after trigger.
 * - idealTimeInterval : ideal time interval between samples (in seconds).
 * - sampleIntervalTimeUnits : time units for idealTimeInterval (0 = fs, 1 = ps,
 * 2 = ns, 3 = us, 4 = ms, 5 = s).
 * - nSamples : Set the number of samples per capture - Used by SetDataBuffers()
 * - ratioMode : Set the downsampling mode - Used by SetDataBuffers()
 * - downSampleRatio : Set the downsampling ratio - Used by SetDataBuffers()
 * - autostop : 1 to stop when trigger condition is met, 0 to continue until
 * user stops.
 ****************************************************************************/
void streamDataHandler(
    GENERICUNIT *unit,
    uint64_t noOfPreTriggerSamples,   // Used by RunStreaming()
    uint64_t noOfPostTriggerSamples,  // Used by RunStreaming()
    double idealTimeInterval,         // Used by RunStreaming()
    uint32_t sampleIntervalTimeUnits, // Used by RunStreaming()
    uint64_t nSamples, // Set the number of samples per capture - Used by
                       // SetDataBuffers()
    PICO_RATIO_MODE ratioMode, // Used by SetDataBuffers()
    uint64_t downSampleRatio,  // Used by SetDataBuffers()
    int16_t autostop)
{
  uint16_t Triggered = 0;
  uint64_t triggeredAt = 0;
  int32_t TriggeredBufNo =
      0; // to store the buffer number where the trigger occured

  int16_t channel = 0;
  uint64_t capture = 0;
  int16_t NoEnabledchannels = 0;
  int32_t counter = 0;        // counter for number of waveform captures
  unit->CapturesComplete = 0; // clear number of captures done

  // Set the number buffers needed (2 or greater) for this code.
  const uint64_t nCaptures = 2; // Set the number of buffer sets to create

  // Define acquisition Settings

  // Buffers settings (Set DownSampling mode and ratio)
  // Use scope acquisition settings for first data download
  struct tbuffer_settings bufferSettings = {0};
  bufferSettings.startIndex = 0;
  bufferSettings.downSampleRatioMode = ratioMode;
  bufferSettings.downSampleRatio = downSampleRatio;
  bufferSettings.nSamples = nSamples;

  // Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
  struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
  int16_t ***minBuffers;
  int16_t ***maxBuffers;
  if (pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffers,
                               &maxBuffers, &multiBufferSizes))
    printf("\nCreated Buffers");

  printf("\nNumber of PreTriggerSamples: %lld", noOfPreTriggerSamples);

  // Get scaling Info for each channel
  struct tPicoProbeScaling enabledChannelsScaling[ps4000A_MAX_CHANNELS];
  struct tPicoProbeScaling channelRangeInfoTemp;
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      getRangeScaling(unit->channelSettings[PS4000A_CHANNEL_A + 0].range,
                      &channelRangeInfoTemp);
      enabledChannelsScaling[channel] = channelRangeInfoTemp;
      NoEnabledchannels++;
    }
  }

  // Save and print Sample Internal set (in seconds)
  unit->timeInterval =
      (idealTimeInterval * (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15));
  printf("\nRunStreaming sample Internal: %g seconds\n", unit->timeInterval);
  // print number of Samples
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

  printf("\nAutostop: %d", autostop);
  printf("\nPress a key to Abort\n");

  // Create Overflow Array Buffers
  int16_t *FileOverflow;
  FileOverflow = (int16_t *)calloc(nCaptures, sizeof(int16_t));

    // delay millseconds for driver to fill channel buffer(s)
    // (timeInternal x SI units x samples x 1000) x 0.3 delay in ms to fill
    // buffer 30% (Recommended delay is 30-50%)
    double timedelay_ms =
        (double)((idealTimeInterval *
                  (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15)) *
                 nSamples * 0.333 * 1000);

    capture = 0;
    uint64_t printTriggerSample = 0;

    // Set
    SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers,
        &multiBufferSizes, capture, (CAPTURE_MODE)STREAMING,
        (int16_t)counter);

    //
            // Start continuous streaming
    printf("\nStarting Data Capture...");
    status = ps4000aRunStreaming(
        unit->handle, &idealTimeInterval, sampleIntervalTimeUnits,
        noOfPreTriggerSamples, noOfPostTriggerSamples, autostop,
        downSampleRatio, ratioMode,
        (uint32_t)constBufferSize); // overviewBufferSize

    if (status != PICO_OK) {
        printf(
            "\nError from function RunStreaming with status: ------ 0x%08lx",
            status);
        return;
    }
    //

    while (!_kbhit()) // loop for each buffer set created, exit if a key is pressed
    {

      if (timedelay_ms > 20)
        Sleep((int)timedelay_ms);

      // Call GetStreamingLatestValues() - passing buffer status data in and out
      status = ps4000aGetStreamingLatestValues(unit->handle, CallStreamingReady,
                                               NULL);



      // DEBUG CODE
      // if(dataStreamInfo[0].noOfSamples_ != 0)
      //{
      // printf("\nPolling GetStreamingLatestValues status = 0x%08lx -
      // noOfSamples: %08ld StartIndex: %08ld", 	status,
      // dataStreamInfo[0].noOfSamples_, dataStreamInfo[0].startIndex_);
      //}

      //if (streamingDataTriggerInfoTemp.triggered_ ==
      //    1) // Latch Triggered flag and sample
      //{
      //  Triggered = 1;
      //  triggeredAt = streamingDataTriggerInfoTemp.triggerAt_;
      //  TriggeredBufNo = counter; // Store buffer number where trigger occured
      //}
      // If buffers full move to next bufferSet, or continue if autoStop
      // triggered
      //if ((status == PICO_WAITING_FOR_DATA_BUFFERS) |
       //   (streamingDataTriggerInfoTemp.autoStop_ == 1)) {
        // OFFLOAD DATA HERE FOR PROCESSING - "maxBuffers[i] and minBuffers[i]"
        if ((unit->timeInterval) >
            0.9e-06) // Only write to file if sample interval is > 0.9us
                     // (1.1MS/s) for demo purposes
        {
          // FOR HIGH SPEED SAMPLING WRITE TO BINARY FILE OR COPY TO ANOTHER
          // BUFFER
          //if (streamingDataTriggerInfoArray &&
           //   FileOverflow) // Check for dereferencing null pointers
          //{
            // Create file name string
            char buf[58 + (3 * sizeof(int))];
            size_t buf_size = sizeof(buf) / sizeof(buf[0]);
            snprintf(buf, buf_size, "%s%d_SubSet", startOfFileName, counter);
            printf("\nWriting capture %ld (Buffer Set %lld) of channels to a "
                   "file.\n",
                   counter, capture);
            struct tcaptures_range captures_range = {
                capture, capture}; // Set range to current capture only
            WriteArrayToFilesGeneric(
                unit, minBuffers, maxBuffers, multiBufferSizes,
                enabledChannelsScaling, buf,
                0, //streamingDataTriggerInfoTemp.triggerAt_, // Triggersample
                (int16_t *)(FileOverflow), &captures_range);
          //}
        }

        //if (streamingDataTriggerInfoTemp.autoStop_ == 1) {
        //  printTriggerSample = streamingDataTriggerInfoTemp.triggerAt_;
        //  printf("\nAutoStop Triggered!\n");
         // break; // exit loop on Autostop
        //}

        capture++; // index next bufferSet and set flag
        counter++; // counter for buffers used and file name (loop irritations)
        if (capture == nCaptures) // Create circular buffer
        {
          capture = 0;
        }
        SetDataBufferFlag = TRUE; // Set flag to move to next bufferSet
      } else {
        if (status != PICO_OK) {
          printf("\nError from function GetStreamingLatestValues with status: "
                 "------ 0x%08lx",
                 status);
          break;
        }
      }
    }
    //if (Triggered)
    //  printf("\nTriggered in Buffer No: %d, At Sample: %lld", TriggeredBufNo,
     //        triggeredAt);

    // OR WAIT UNTIL ALL BUFFER SEGMENTS ARE CAPTURED AND PROCESS DATA IN -
    // "maxBuffers and minBuffers"
    //  Write to console
    printf("\n");
    WriteArrayToStdoutGeneric(
        unit, minBuffers, maxBuffers, multiBufferSizes, enabledChannelsScaling,
        (enum enCaptureMode)STREAMING,
        3,                  // Number of buffers to write
        10,                 // Number of samples to write
        printTriggerSample, // passes Triggersample regardless of which buffer
                            // triggered,(0 if no trigger)
        FileOverflow);
  }

  printf("Stopping Streaming... ");
  // Stop
  status = ps4000aStop(unit->handle);
  if (status != PICO_OK) {
    printf("\nError from function Stop with status: ------ 0x%08lx", status);
  } else
    printf("Stopped capture\n");

  unit->CapturesComplete = 1; // set to 1 to indicate complete
  // Release Buffer memory from API
  clearDataBuffers(unit);

  // Free buffers
  pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);

}
