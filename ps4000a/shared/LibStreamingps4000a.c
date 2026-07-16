/*******************************************************************************
 *
 * Filename: Libps4000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 4XXX Series (ps4000a) devices,
 *   for Streaming captures.
 *
 * Copyright (C) 2026 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include "../../shared/PicoScaling.h"
#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"
#include "./Libps4000a.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

/* Headers for Windows */
#ifdef _WIN32
#include "ps4000aApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <ps4000aApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif

#endif

char startOfFileName[] = "StreamingCaptureNoS_";

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase;
extern int16_t  g_ready;
extern int16_t  g_autoStop;
extern int16_t	g_trig;
extern uint32_t	g_trigAt;
extern int32_t  g_sampleCount;
extern uint32_t	g_startIndex;
extern int16_t  g_probeStateChanged;
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
void streamDataHandler(GENERICUNIT *unit,
    uint64_t noOfPreTriggerSamples,   // Used by RunStreaming()
    uint64_t noOfPostTriggerSamples,  // Used by RunStreaming()
    uint32_t idealTimeInterval,         // Used by RunStreaming()
    uint32_t sampleIntervalTimeUnits, // Used by RunStreaming()
    uint64_t nSamples, // Set the number of samples per capture - Used by
                       // SetDataBuffers()
    PICO_RATIO_MODE ratioMode, // Used by SetDataBuffers()
    uint64_t downSampleRatio,  // Used by SetDataBuffers()
    int16_t autostop,
    FILE_TYPE filetype,
    BOOL imagefile)
{
  uint16_t Triggered = 0;
  uint64_t triggeredAt = 0;
  int32_t TriggeredBufNo = 0; // to store the buffer number where the trigger occured
  uint64_t totalSamples = 0;

  int16_t channel = 0;
  uint64_t capture = 0;
  int16_t NoEnabledchannels = 0;
  int32_t counter = 0;        // counter for number of waveform captures
  unit->CapturesComplete = 0; // clear number of captures done

  // Set the number buffers needed (2 or greater) for this code.
  const uint64_t nCaptures = 1; // Set the number of buffer sets to create
  PICO_STATUS status;
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
    printf("\nCreated API Buffers");

  printf("\nNumber of PreTriggerSamples: %" PRIu64 "", noOfPreTriggerSamples);

  int16_t*** AppminBuffers;
  int16_t*** AppmaxBuffers;
  if (pico_create_multibuffers(unit, bufferSettings, nCaptures, &AppminBuffers,
                               &AppmaxBuffers, &multiBufferSizes))
      printf("\nCreated App. Buffers");

  // Get scaling Info for each channel
  struct tPicoProbeScaling enabledChannelsScaling[PS4000A_MAX_CHANNELS] = {0};
  struct tPicoProbeScaling channelRangeInfoTemp;
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      getRangeScaling(unit->channelSettings[PS4000A_CHANNEL_A + channel].range,
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
  printf("Requested number of Samples: %" PRIu64 "\n", nSamples);
  if (bufferSettings.downSampleRatioMode == PS4000A_RATIO_MODE_NONE)
    printf("DownSampling Mode is set to: None\n");
  if (bufferSettings.downSampleRatioMode == PS4000A_RATIO_MODE_AGGREGATE)
    printf("DownSampling Mode is set to: Aggregate (Min. and Max. values)\n");
  if (bufferSettings.downSampleRatioMode == PS4000A_RATIO_MODE_DECIMATE)
    printf("DownSampling Mode is set to: Decimate\n");
  if (bufferSettings.downSampleRatioMode == PS4000A_RATIO_MODE_AVERAGE)
    printf("DownSampling Mode is set to: Average\n");
  if (bufferSettings.downSampleRatioMode != PS4000A_RATIO_MODE_NONE)
    printf("DownSampling Ratio is set to: %" PRIu64 "\n",
           bufferSettings.downSampleRatio);

  printf("\nAutostop: %d", autostop);
  //printf("\nPress a key to Abort\n");

  // Create Overflow Array Buffers
  int16_t *FileOverflow;
  FileOverflow = (int16_t *)calloc(nCaptures, sizeof(int16_t));

    // delay millseconds for driver to fill channel buffer(s)
    // (timeInternal x SI units x samples x 1000) x 0.3 delay in ms to fill
    // buffer 30% (Recommended delay is 30-50%)
    double timedelay_ms =
        (double)((idealTimeInterval *
                  (pow(10, 3 * sampleIntervalTimeUnits) / 1E+15)) *
                 nSamples * 0.1 * 1000);

    capture = 1;
    uint64_t printTriggerSample = 0;

	// Set API Buffers for streaming capture
    SetAllDataBuffers(unit, &bufferSettings, &minBuffers, &maxBuffers,
        &multiBufferSizes, 0, (CAPTURE_MODE)BLOCK, 0);

	// Setup "bufferInfo" for data buffers to be used in the callback function.
    BUFFER_INFO bufferInfo;
	bufferInfo.unit = unit; // need access to channel settings
    bufferInfo.driverMaxBuffers = maxBuffers;
    bufferInfo.driverMinBuffers = minBuffers;
	bufferInfo.appMaxBuffers = AppmaxBuffers;
	bufferInfo.appMinBuffers = AppminBuffers;

    if (autostop)
    {
        printf("\nStreaming Data for %" PRIu64 " samples", noOfPostTriggerSamples / downSampleRatio);

        if (noOfPreTriggerSamples)							// we pass 0 for preTrigger if we're not setting up a trigger
        {
            printf(" after the trigger occurs\nNote: %" PRIu64 " Pre Trigger samples before Trigger arms\n\n",
                noOfPreTriggerSamples / downSampleRatio);
        }
        else
        {
            printf("\n\n");
        }
    }
    else
    {
        printf("\nStreaming Data continually...\n\n");
    }
    g_autoStop = FALSE;

    // Write Metadata to file
    if (filetype == FILE_BIN)
    {
        WriteMetaDataToFile(
            unit,
            multiBufferSizes,
            enabledChannelsScaling,
            "PicoMetaData_Streaming",
            noOfPreTriggerSamples, // set to 0 if not using trigger or pre-trigger samples
            NULL); // captures_range set to NULL to write full range
    }

     // Start continuous streaming
     printf("\nStarting Data Capture...");
    status = ps4000aRunStreaming(
        unit->handle, &idealTimeInterval, sampleIntervalTimeUnits,
        noOfPreTriggerSamples, noOfPostTriggerSamples, autostop,
        downSampleRatio, ratioMode,
        multiBufferSizes.maxBufferSize); // overviewBufferSize
    if (status != PICO_OK) {
        printf(
            "\nError from function RunStreaming with status: ------ 0x%08x",
            status);
        return;
    }
    printf("\nStreaming data...Press a key to abort.\n");

	// Setup filename for streaming capture -
    // we will append the buffer number as each buffer set is written to file in the streaming loop below
    char buf[58 + 20] = { '\0' }; // 20 chars is enough for the largest uint64_t buffer-set number
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);

    uint64_t processedBuffer = 0;
    totalSamples = 0;
    BOOL processFlag = FALSE;
	// Loop to wait for data and write to file.
    while (!_kbhit() && !g_autoStop)
    {
        g_ready = FALSE;

        if (timedelay_ms > 10)
            Sleep((int)timedelay_ms); // delay for driver to part fill buffers
        
        if (g_probeStateChanged == 1)
        {
            printf("\nProbe state changed, Stopping streaming...Manual restart streaming required!\n");
		    g_probeStateChanged = 0;
            break;
		}
		// Get data from driver -
        // this will call the callback function repeatedly until all data in the driver buffers has been retrieved,
        // which will set "g_ready" to TRUE
        status = ps4000aGetStreamingLatestValues(unit->handle, callBackStreaming, &bufferInfo);
        if (status != PICO_OK && status != PICO_BUSY) {
            printf("\nError from function GetStreamingLatestValues with status: "
                "------ 0x%08x",
                status);
            break;
        }
        if (g_ready && g_sampleCount > 0) /* can be ready and have no data, if autoStop has fired */
        {
            if (g_trig)
            {
                triggeredAt = totalSamples + g_trigAt;		// calculate where the trigger occurred in the total samples collected
            }

            totalSamples += g_sampleCount;
            //printf("\nCollected %3i samples, index = %6u, Total: %llu samples ", g_sampleCount, g_startIndex, totalSamples);
            //printf("\t\t Buffer Set: %llu", totalSamples / nSamples);
            printf(".");
            if (g_trig)
            {
                printf("Trig. at index %llu", triggeredAt);	// show where trigger occurred
            }
        }
		// if buffer filled, process data, write to file etc
        if(g_sampleCount + g_startIndex == nSamples)
		{
            if (processedBuffer < (totalSamples / nSamples))
            {
                processFlag = TRUE;
            }
            //printf("\nDEBUG - processedBuffer =  %llu to BufferSet: %llu", processedBuffer, totalSamples / nSamples);
            if (processFlag)
            {
                processedBuffer++;
                processFlag = FALSE; 
                snprintf(buf, buf_size, "%s%" PRIu64 "_ss", startOfFileName, totalSamples / nSamples);

                if (filetype != FILE_NONE)
                {
                    // Only write to binary file if sample interval is < 0.9us (1.1MS/s)
                    if (((unit->timeInterval) < 0.9e-06) && (filetype == FILE_BIN))
                    {
                        WriteArrayToFilesBinary(
                            unit,
                            AppminBuffers,
                            AppmaxBuffers,
                            multiBufferSizes,
                            enabledChannelsScaling,
                            buf,
                            triggeredAt, // Should be >= noOfPreTriggerSamples,
                            FileOverflow,
                            NULL);
                    }
                    else // For slower sampling rates write to text file (csv) and/or image file, if requested
                    {
                        WriteArrayToFilesGeneric(
                            unit,
                            AppminBuffers,
                            AppmaxBuffers,
                            multiBufferSizes,
                            enabledChannelsScaling,
                            buf,
                            triggeredAt, // Should be >= noOfPreTriggerSamples,
                            FileOverflow,
                            NULL);
                    }
                }
                if (((unit->timeInterval) > 0.9e-06) && (imagefile == TRUE))
                {
                    printf("\nSaved plot to %s", buf);
                    WriteArrayToImage(
                        unit,
                        AppminBuffers,
                        AppmaxBuffers,
                        multiBufferSizes,
                        enabledChannelsScaling,
                        buf,
                        triggeredAt, // Should be >= noOfPreTriggerSamples,
                        FileOverflow,
                        0,      // plotChannelMask: 0 = all enabled channels
                        NULL);
                }
            }
        }
    }
  // Stop the streaming capture
  printf("\nStopping Streaming... ");
  status = ps4000aStop(unit->handle);
  if (status != PICO_OK) {
    printf("\nError from function Stop with status: ------ 0x%08x", status);
  }

    if (!g_autoStop)
    {
        printf("\nData collection aborted.\n");
        _getch();
    }
    else
    {
        printf("\nData collection complete.\n\n");
    }
    
    WriteArrayToStdoutGeneric(
        unit,
        AppminBuffers,
        AppmaxBuffers,
        multiBufferSizes,
        enabledChannelsScaling,
        (enum enCaptureMode)BLOCK,  // For older Streaming APIs use BLOCK,
        1,						    // Number of buffers to write
        10,						    // Number of samples to write
        triggeredAt,		        // Should be >= noOfPreTriggerSamples,
        FileOverflow
    );

  unit->CapturesComplete = 1; // set to 1 to indicate complete
  // Release Buffer memory from API
  clearDataBuffers(unit);

  // Free buffers
  pico_release_multibuffers(unit, &minBuffers, &maxBuffers, &multiBufferSizes);
  pico_release_multibuffers(unit, &AppminBuffers, &AppmaxBuffers, &multiBufferSizes);
}
