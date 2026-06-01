/*******************************************************************************
 *
 * Filename: LibAWGps4000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 4XXX Series (ps4000a) devices.
 *
 * Copyright (C) 2026 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdbool.h>
#include <stdio.h>

#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"
#include "../../shared/PicoScaling.h"
#include "../../shared/PicoUnit.h"
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

/****************************************************************************
 * Gobal Variables
 ***************************************************************************/
BOOL scaleVoltages = TRUE;
uint32_t timebase = 0;
const uint64_t constBufferSize = 131072; // 128kB
int16_t g_ready = FALSE;
// 
// Gobals for streaming CallBack
int32_t      		g_sampleCount = 0;
uint32_t				g_startIndex;
int16_t					g_autoStop;
int16_t					g_trig = 0;
uint32_t				g_trigAt = 0;

int32_t cycles = 0;
int16_t g_probeStateChanged = 0;
USER_PROBE_INFO userProbeInfo;

/****************************************************************************
 * Model Lookup Table and function to return model enum from string
 ***************************************************************************/
typedef struct { char* key; MODEL_TYPE val; } t_symstruct;
t_symstruct lookuptable[] = {
    { "4444", MODEL_4444 },
	{ "4224", MODEL_4224 },
    { "4424", MODEL_4424 },
    { "4824A", MODEL_4824 },
    { "4225A", MODEL_4225A },
    { "4425A", MODEL_4425A },
    { "4825A", MODEL_4825A }
};

#define NKEYS (sizeof(lookuptable)/sizeof(t_symstruct))
MODEL_TYPE keyfromstring(char* key)
{
    int i;
    for (i = 0; i < NKEYS; i++) {
        if (strcmp(lookuptable[i].key, key) == 0)
			return lookuptable[i].val;
    }
    return BADKEY;
}

/****************************************************************************
 * Callback Probe Interaction
 *
 * See ps4000aProbeInteractions (callback)
 *
 * 
 ****************************************************************************/
void PREF4 CallBackProbeInteractions(int16_t handle, PICO_STATUS status,
    PS4000A_USER_PROBE_INTERACTIONS * probes, // PICO_USER_PROBE_INTERACTIONS
    uint32_t nProbes)
{
    uint32_t i = 0;

    userProbeInfo.status = status;
    userProbeInfo.numberOfProbes = nProbes;

    // Store probe interactions in global struct for user access
    // Copy probe interactions to global struct for user access, using channel as index
    for (i = 0; i < nProbes; ++i)
    {
        userProbeInfo.userProbeInteractions[probes[i].channel] = probes[i];
    }
    g_probeStateChanged = 1;

    /*******************************   DEBUG   ************************************/
    /*
    printf("DEBUG - CallBackProbeInteractions()\n");
    uint32_t ch = 0;
    for (ch = 0; ch < 4; ch++)
    {
        printf("Channel: %c\n", (char)('A' + ch));
        printf("\tProbe Enabled : %d\n", userProbeInfo.userProbeInteractions[ch].enabled_);
        printf("\tProbe connected : %d\n", userProbeInfo.userProbeInteractions[ch].connected_);
        printf("\tProbe name  : %d\n", userProbeInfo.userProbeInteractions[ch].probeName_);
        printf("\tProbe Range : %d\n", userProbeInfo.userProbeInteractions[ch].rangeCurrent_);
    } 
    */
    /*******************************   DEBUG   ************************************/
}

/****************************************************************************
 * callBackBlockReady
 * used by ps4000a data block collection calls, on receipt of data.
 * used to set global flags etc checked by user routines
 ****************************************************************************/
void PREF4 callBackBlockReady(int16_t handle, PICO_STATUS status,
                              PICO_POINTER pParameter) {
  if (status != PICO_CANCELLED) {
    g_ready = TRUE;
  }
}

/****************************************************************************
 * callBackDataReady
 * used by ps4000a for Async data collection calls, on receipt of data.
 * used to set global flags etc checked by user routines
 ****************************************************************************/
void PREF4 callBackDataReady(int16_t handle, PICO_STATUS status,
                             uint64_t noOfSamples, int16_t overflow,
                             PICO_POINTER pParameter) {
  if (status != PICO_CANCELLED) {
    g_ready = TRUE;
  }
}

/****************************************************************************
* Streaming Callback
* used by ps4000a data streaming collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackStreaming(int16_t handle,
    int32_t noOfSamples,
    uint32_t	startIndex,
    int16_t overflow,
    uint32_t triggerAt,
    int16_t triggered,
    int16_t autoStop,
    void * pParameter) {

    BUFFER_INFO * bufferInfo = NULL;

    if (pParameter != NULL)
    {
        bufferInfo = (BUFFER_INFO*)pParameter;
    }

    // used for streaming
    g_sampleCount = noOfSamples;
    g_startIndex = startIndex;
    g_autoStop = autoStop;

    // flag to say done reading data
    g_ready = TRUE;

    // flags to show if & where a trigger has occurred
    g_trig = triggered;
    g_trigAt = triggerAt;

    int32_t channel;

    if (bufferInfo != NULL && noOfSamples)
    {
        for (channel = 0; channel < bufferInfo->unit->channelCount; channel++)
        {
            if (bufferInfo->unit->channelSettings[channel].enabled)
            {
                if ((bufferInfo->appMaxBuffers) && (bufferInfo->driverMaxBuffers))
                {
                    // Max buffers
                    if ((bufferInfo->appMaxBuffers)[0][channel] && (bufferInfo->driverMaxBuffers)[0][channel])
                    {
                        memcpy_s(&(bufferInfo->appMaxBuffers)[0][channel][startIndex], noOfSamples * sizeof(int16_t),
                            &(bufferInfo->driverMaxBuffers)[0][channel][startIndex], noOfSamples * sizeof(int16_t));
                    }
                    // Min buffers
                    if ((bufferInfo->appMinBuffers)[0][channel] && (bufferInfo->driverMinBuffers)[0][channel])
                    {
                        memcpy_s(&(bufferInfo->appMinBuffers)[0][channel][startIndex], noOfSamples * sizeof(int16_t),
                            &(bufferInfo->driverMinBuffers)[0][channel][startIndex], noOfSamples * sizeof(int16_t));
                    }
                }
            }
        }
    }
}

/****************************************************************************
 * SetDefaults - restore default settings
 ****************************************************************************/
void setDefaults(GENERICUNIT* unit) {
    PICO_STATUS status;
    int32_t i;

    /*******************************   DEBUG   ************************************/
    /* printf("DEBUG Called - SetDefaults() - Write channel settings (Call SetChannel() )\n");
    int16_t ch = 0;
    for (ch = 0; ch < unit->channelCount; ch++)
    {
        printf("Channel: %c\n", (char)('A' + ch));
        //if (unit->hasIntelligentProbes)
        //{   //Print Probe name and its current range
        printf("\tCh Enabled : %d\n", (int16_t)unit->channelSettings[PS4000A_CHANNEL_A + ch].enabled);
        //printf("\tProbe connected : %d\n", userProbeInfo.userProbeInteractions[ch].probeName_);
        printf("\tSet Range : %ld\n", (uint32_t)unit->channelSettings[PS4000A_CHANNEL_A + ch].range);
    } */
    /*******************************   DEBUG   ************************************/

    for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
    {
        status = ps4000aSetChannel(
            unit->handle, (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + i),
            unit->channelSettings[PS4000A_CHANNEL_A + i].enabled,
            (PS4000A_COUPLING)unit->channelSettings[PS4000A_CHANNEL_A + i].DCcoupled,
            (PICO_CONNECT_PROBE_RANGE)unit->channelSettings[PS4000A_CHANNEL_A + i].range,
            (float)unit->channelSettings[PS4000A_CHANNEL_A + i].analogueOffset);
        printf(status ? "SetDefaults:ps4000aSetChannel------ 0x%08lx \n" : "",
            status);
		// Set bandwidth limiters to most recent settings (if 4444, or if channel enabled and supports bandwidth limit setting)
        if ((unit->channelSettings[PS4000A_CHANNEL_A + i].enabled) == TRUE && (unit->model == MODEL_4444)) {
            if (status == PICO_OK) {
                status = ps4000aSetBandwidthFilter(
                    unit->handle, (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + i),
                    (PS4000A_BANDWIDTH_LIMITER)unit->channelSettings[PS4000A_CHANNEL_A + i]
                    .bandwithLimit);
            }
            printf(status ? "SetDefaults:ps4000aSetBandwidthFilter------ 0x%08lx \n" : "",
                status);
        }
    }
}
/****************************************************************************
 * ProbestoSettings -  Copies some callback probe settings array to
 * unit channel settings for use in setting channels to most probe updates.
 * But does not call setDefaults() to update channels.
 * Modifies the channel settings - Mirrors probe connection status and range if valid.
 * Modify the function as needed for your application.
 * For example if a probe of type "P" is connected, set it to range "X" and coupling "Y" etc.
 * If last Channel and (probe and standard) ranges need to be restored then a 2D array
 * of settings could be stored for each channel and probe combination and copied across as needed.
 ****************************************************************************/
void ProbestoSettings(GENERICUNIT* unit) {
    int16_t i;

    for (i = 0; i < unit->channelCount; i++)
    {
        if (userProbeInfo.userProbeInteractions[i].connected == FALSE)
        {
            if (!(unit->firstRange <= unit->channelSettings[PS4000A_CHANNEL_A + i].range &&
                unit->channelSettings[PS4000A_CHANNEL_A + i].range <= unit->lastRange) )
            {   // If no probe connected, but channel enabled and range outside limits, set to a "default range".
                unit->channelSettings[PS4000A_CHANNEL_A + i].range = PICO_X1_PROBE_1V;
                // printf("DEBUG: CH%c No probe, Out of range Or PICO_CONNECT_PROBE_OFF\n", (char)('A' + i));
            }
        }
        if (userProbeInfo.userProbeInteractions[i].connected == TRUE) // if probe connected
        {
            if (unit->channelSettings[PS4000A_CHANNEL_A + i].enabled == TRUE)
            {
                if (userProbeInfo.userProbeInteractions[i].rangeFirst_ <= unit->channelSettings[PS4000A_CHANNEL_A + i].range &&
                    unit->channelSettings[PS4000A_CHANNEL_A + i].range <= userProbeInfo.userProbeInteractions[i].rangeLast_)
                {
                    //printf("DEBUG: CH%c Probe connected, Ch Enabled, In range", (char)('A' + i));
                    //printf(" %d\n", unit->channelSettings[PS4000A_CHANNEL_A + i].range);
                }
                else
                {
                    unit->channelSettings[PS4000A_CHANNEL_A + i].range = userProbeInfo.userProbeInteractions[i].rangeLast_;
                    //printf("DEBUG: CH%c Probe connected, Ch Enabled, Out of range", (char)('A' + i));
                    //printf(" %d\n", unit->channelSettings[PS4000A_CHANNEL_A + i].range);
				}
            }
            if (unit->channelSettings[PS4000A_CHANNEL_A + i].enabled == FALSE)
            // if probe connected but channel not enabled, set to "probe off" range (if supported) or a default range
            {
                unit->channelSettings[PS4000A_CHANNEL_A + i].range = PICO_CONNECT_PROBE_OFF;
                //printf("DEBUG: CH%c Probe connected, Ch Disabled, Setting to -> PICO_CONNECT_PROBE_OFF", (char)('A' + i));
                //printf(" %d\n", unit->channelSettings[PS4000A_CHANNEL_A + i].range);
            }
        }
    }
}

/****************************************************************************
 * ClearDataBuffers
 *
 * stops GetData writing values to memory that has been released
 ****************************************************************************/
PICO_STATUS clearDataBuffers(GENERICUNIT *unit)
{
    int32_t i;
    PICO_STATUS status = PICO_OK;

    for (i = 0; i < unit->channelCount; i++)
    {
        if ((status = ps4000aSetDataBuffers(unit->handle, PS4000A_CHANNEL_A, NULL, NULL, 0, 0, PS4000A_RATIO_MODE_NONE)) != PICO_OK)
        {
            printf("ClearDataBuffers:ps4000aSetDataBuffers ------ 0x%08lx \n", status);
        }
    }
	if (status == PICO_OK)
    {
            printf("Cleared all DataBuffers\n");
    }
  return status;
}

/****************************************************************************
 * SetTrigger
 *
 * - Used to call all the functions required to set up triggering
 *
 ***************************************************************************/
PICO_STATUS SetTrigger(GENERICUNIT *unit,
                        PS4000A_TRIGGER_CHANNEL_PROPERTIES *channelProperties,
                       int16_t nChannelProperties,
                        int16_t auxOutputMode,
                        PS4000A_CONDITION *triggerConditions, int16_t nTriggerConditions,
                        PS4000A_DIRECTION *directions, int16_t nDirections,
                        struct tps4000aPwq *pwq, uint32_t delay, int32_t autoTrigger_us) {
  PICO_STATUS status;
  PS4000A_CONDITIONS_INFO info = PS4000A_CLEAR;
  PS4000A_CONDITIONS_INFO pwqInfo = PS4000A_CLEAR;

  if ((status = ps4000aSetTriggerChannelProperties(
           unit->handle, channelProperties, nChannelProperties,
           auxOutputMode,
           autoTrigger_us)) != PICO_OK) {
    printf("SetTrigger:ps4000aSetTriggerChannelProperties ------ Ox%08x \n",
           status);
    return status;
  }

  if (nTriggerConditions != 0) {
    info = (PICO_ACTION)(PICO_CLEAR_ALL | PICO_ADD);
    // Clear and add trigger condition specified unless no trigger conditions
    // have been specified
  }

  if ((status = ps4000aSetTriggerChannelConditions(
                    unit->handle, triggerConditions, nTriggerConditions,
                    info) != PICO_OK)) {
    printf("SetTrigger:ps4000aSetTriggerChannelConditions ------ 0x%08x \n",
           status);
    return status;
  }

  if ((status = ps4000aSetTriggerChannelDirections(unit->handle, directions,
                                                   nDirections)) != PICO_OK) {
    printf("SetTrigger:ps4000aSetTriggerChannelDirections ------ 0x%08x \n",
           status);
    return status;
  }

  if ((status = ps4000aSetTriggerDelay(unit->handle, delay)) != PICO_OK) {
    printf("SetTrigger:ps4000aSetTriggerDelay ------ 0x%08x \n", status);
    return status;
  }
  /////////////
  // Clear and add pulse width qualifier condition, clear if no pulse width
  // qualifier has been specified
  if (pwq->nConditions != 0) {
      pwqInfo = (PS4000A_CONDITIONS_INFO)(PS4000A_CLEAR | PS4000A_ADD);
  }
  
  if ((status = ps4000aSetPulseWidthQualifierConditions(
      unit->handle, pwq->conditions, pwq->nConditions, pwqInfo)) !=
      PICO_OK) {
      printf(
          "SetTrigger:ps4000aSetPulseWidthQualifierConditions ------ 0x%08x \n",
          status);
      return status;
  }
  ///
  PS4000A_THRESHOLD_DIRECTION pwqtempdir = PS4000A_ABOVE;
  if(pwq->directions == NULL)
      {
      pwqtempdir = PS4000A_ABOVE;
  }
  else
  {
      pwqtempdir = (PS4000A_THRESHOLD_DIRECTION)(pwq->directions[0]);
  }

  if ( (status = ps4000aSetPulseWidthQualifierProperties(
      unit->handle,
      pwqtempdir,
      pwq->lower, pwq->upper,
      pwq->type ) 
      )
      != PICO_OK
      )
  {
    printf("SetTrigger:ps4000aSetPulseWidthQualifierProperties ------ 0x%08x \n",
        status);
    return status;
  }
  //*/

  /* ps4000aSetPulseWidthQualifierDirections not used/defined in ps4000a API */
  /*
  if ((status = ps4000aSetPulseWidthQualifierDirections(
           unit->handle, pwq->directions, pwq->nDirections)) != PICO_OK) {
    printf(
        "SetTrigger:ps4000aSetPulseWidthQualifierDirections ------ 0x%08x \n",
        status);
    return status;
  }
  */

  
  /* ps4000aSetAuxIoMode not supported/needed in this context or handled via
   * channel properties */
  /*
  if ((status = ps4000aSetAuxIoMode(unit->handle,
          auxOutputMode)) != PICO_OK)
  {
          printf("SetTrigger:ps4000aSetAuxIoMode ------ Ox%08x \n", status);
          return status;
  }
  */
  return status;
}

/****************************************************************************
 * Initialise unit' structure with Variant specific defaults
 ****************************************************************************/
void set_info(GENERICUNIT *unit) {
  int8_t description[11][25] = {"Driver Version",
                                "USB Version",
                                "Hardware Version",
                                "Variant Info",
                                "Serial",
                                "Cal Date",
                                "Kernel Version",
                                "Digital HW Version",
                                "Analogue HW Version",
                                "Firmware 1",
                                "Firmware 2"};

  int16_t i = 0;
  int16_t requiredSize = 0;
  int8_t line[80];
  PICO_STATUS status = PICO_OK;

  // Variables used for arbitrary waveform parameters
  int16_t minArbitraryWaveformValue = 0;
  int16_t maxArbitraryWaveformValue = 0;
  uint32_t minArbitraryWaveformSize = 0;
  uint32_t maxArbitraryWaveformSize = 0;

  // Initialise default unit properties for all models, these may be overwritten by model specific information retrieved from the device
  unit->hasIntelligentProbes = FALSE;
  unit->sigGenfeature = SIGGEN_NONE;
  unit->firstRange = PICO_X1_PROBE_10MV;
  unit->lastRange = PICO_X1_PROBE_50V;
  unit->channelCount = DUAL_SCOPE;
  unit->digitalPortCount = 0;

  if (unit->handle) {
    printf("Device information:-\n\n");

    for (i = 0; i < 11; i++) {
      status = ps4000aGetUnitInfo(unit->handle, line, sizeof(line),
                                  &requiredSize, i);

      // info = 3 - PICO_VARIANT_INFO
      if (i == PICO_VARIANT_INFO) {
        memcpy(unit->modelString, line, sizeof(unit->modelString));
        unit->modelString[sizeof(unit->modelString) - 1] = '\0'; // Ensure null termination
		
        // Extract channel count from the variant info string
        unit->channelCount = (int16_t)line[1];
        unit->channelCount = unit->channelCount - 48; // Subtract ASCII 0 (48)

      } else if (i == PICO_BATCH_AND_SERIAL) // info = 4 - PICO_BATCH_AND_SERIAL
      {
        memcpy(&(unit->serial), line, requiredSize);
      }
      printf("%s: %s\n", description[i], line);
    }
    printf("\n");

    switch (keyfromstring(unit->modelString)) 
    {
    case MODEL_4444: /* 4Ch, Differential Scope */
        printf("Model is 4444\n");
        unit->model = MODEL_4444;
        unit->hasIntelligentProbes = TRUE;
        unit->firstRange = PICO_X1_PROBE_10MV; // PS4000A_10MV;
        unit->lastRange = PICO_X1_PROBE_50V; // PS4000A_50V;
        //unit->sigGen = SIGGEN_NONE;
        break;
    case MODEL_4225A: /* 2Ch, Automotive Scope */
        printf("Model is 4225A\n");
        unit->model = MODEL_4225A;
        unit->hasIntelligentProbes = TRUE;
        unit->firstRange = PICO_X1_PROBE_50MV; // PS4000A_50MV;
        unit->lastRange = PICO_X1_PROBE_200V; // PS4000A_200V;
        //unit->sigGen	= SIGGEN_NONE;
        break;
    case MODEL_4425A: /* 4Ch, Automotive Scope */
        printf("Model is 4225A\n");
        unit->model = MODEL_4425A;
        unit->hasIntelligentProbes = TRUE;
        unit->firstRange = PICO_X1_PROBE_50MV; // PS4000A_50MV;
        unit->lastRange = PICO_X1_PROBE_200V; // PS4000A_200V;
		//unit->sigGen	= SIGGEN_NONE;
        break;
    case MODEL_4824A: /* 12-bit, 8Ch Scope */
        printf("Model is 4824(A)\n");
		unit->model = MODEL_4824A;
		unit->sigGenfeature = SIGGEN_AWG;
		break;
    case MODEL_4424A: /* 12-bit, 4Ch Scope */
        printf("Model is 4424(A)\n");
        unit->model = MODEL_4424A;
        unit->sigGenfeature = SIGGEN_AWG;
        break;
    case MODEL_4224A: /* 12-bit, 2Ch Scope */
        printf("Model is 4224(A)\n");
        unit->model = MODEL_4224A;
        unit->sigGenfeature = SIGGEN_AWG;
        break;
    case BADKEY: /* failed lookup */
        printf("Model not found or not referenced!, using defaults\n");
        break;
    }

    // If device has Arbitrary Waveform Generator, find the maximum AWG buffer size
    //status = ps4000aSigGenArbitraryMinMaxValues(unit->handle, &minArbitraryWaveformValue, &maxArbitraryWaveformValue, &minArbitraryWaveformBufferSize, &maxArbitraryWaveformBufferSize);
    /*
    status = ps4000aSigGenArbitraryMinMaxValues(unit->handle,
    &minArbitraryWaveformValue, &maxArbitraryWaveformValue,
    &minArbitraryWaveformSize, &maxArbitraryWaveformSize); unit->awgBufferSize =
    maxArbitraryWaveformSize;
    */
  }
}

/****************************************************************************
 * Select input voltage ranges for channels
 ****************************************************************************/
void setVoltages(GENERICUNIT *unit) {
  PICO_STATUS status = PICO_OK;
  PICO_DEVICE_RESOLUTION resolution = PICO_DR_12BIT;

  int16_t ch;
  int32_t count = 0;

  bool allChannelsOff = true;
  PICO_CONNECT_PROBE_RANGE _firstRange;
  PICO_CONNECT_PROBE_RANGE _lastRange;

  printf("99 - switches channel off\n");
  do
  {
      for (ch = 0; ch < unit->channelCount; ch++)
      {
          struct tPicoProbeScaling chRangeInfoTempFirst;
          struct tPicoProbeScaling chRangeInfoTempLast;
          uint32_t rangeinput = 99;
          PICO_CONNECT_PROBE_RANGE range = PICO_X1_PROBE_5V;
          do
          {
                getRangeScaling(unit->channelSettings[ch].range, &chRangeInfoTempLast); 
                printf("Channel: %c\n", (char)('A' + ch));

                if (userProbeInfo.userProbeInteractions[ch].connected)
                {   //Print Probe name and its current range
                    printf("Probe connected : %d", userProbeInfo.userProbeInteractions[ch].probeName);
                    printf("\t\tProbe Range : %s\n", chRangeInfoTempLast.Probe_Range_text);
                }
                else if (unit->channelSettings[ch].enabled)
                {
                    printf("Current voltage range %s\n", chRangeInfoTempLast.Probe_Range_text);
                }
                else
                    printf("Channel Off\n\n");

                _firstRange = unit->firstRange;
                _lastRange = unit->lastRange;

                //Print available, standard voltage ranges OR PicoConnectProbes ranges
                if (userProbeInfo.userProbeInteractions[ch].connected)
                {
                    _firstRange = userProbeInfo.userProbeInteractions[ch].rangeFirst_; //copy probe settings to use
                    _lastRange = userProbeInfo.userProbeInteractions[ch].rangeLast_;
                }
                getRangeScaling(_firstRange, &chRangeInfoTempFirst);
                getRangeScaling(_lastRange, &chRangeInfoTempLast);
                printf("Specify Probe range (%s..%s))", chRangeInfoTempFirst.Probe_Range_text, chRangeInfoTempLast.Probe_Range_text);
                printf("        Enter (%d..%d)) ", (int)_firstRange, (int)_lastRange);

                // Keyboard input to range value
                fflush(stdin);
                scanf_s("%d", &rangeinput);
                if (rangeinput == 99) //value to turn channel off
                {
                    range = (PICO_CONNECT_PROBE_RANGE)PICO_CONNECT_PROBE_OFF;
				    unit->channelSettings[PS4000A_CHANNEL_A + ch].enabled = FALSE; //disable channel if off selected
                    printf("Channel Off\n\n");
                }
                else if ((uint32_t)_firstRange <= rangeinput && rangeinput <= (uint32_t)_lastRange)
                {
                    {
                        range = (PICO_CONNECT_PROBE_RANGE)rangeinput;
                        unit->channelSettings[PS4000A_CHANNEL_A + ch].enabled = TRUE;
                    }
                    // Write to chanel settings structure
                    unit->channelSettings[PS4000A_CHANNEL_A + ch].range = range;
                    // Print selected range
                    if (unit->channelSettings[PS4000A_CHANNEL_A + ch].enabled)
                    {
                        if (getRangeScaling(range, &chRangeInfoTempLast))
                            printf("Selected Range: %s\n\n", chRangeInfoTempLast.Probe_Range_text);
                    }
                }

          } while ( (rangeinput != 99) && //While - channel not off & out of range
              (rangeinput < (uint32_t)_firstRange || rangeinput > (uint32_t)_lastRange) );
          allChannelsOff = false;
      }
      printf(allChannelsOff ? "At least one channels must be enabled\n" : "");
  } while (allChannelsOff);
    
  printf("\n");
  ProbestoSettings(unit);
  setDefaults(unit); // Put these changes into effect
}

 /****************************************************************************
 * Validate and set the channel range taking into account probe capabilities.
 * Returns:
 *   0  - success, range set
 *  -1  - invalid channel index
 *   1  - specified range out of bounds for this channel
  ****************************************************************************/
int8_t ValidateChannelRange(GENERICUNIT* unit, uint8_t channelIndex, PICO_CONNECT_PROBE_RANGE userRange)
{
    if (unit == NULL)
    {
        printf("Unit pointer is NULL.\n");
        return -1;
    }

    if (channelIndex < 0 || channelIndex >= 8)
    {
        printf("Invalid channel index: %d\n", channelIndex);
        return -1;
    }
    PICO_CONNECT_PROBE_RANGE _firstRange = unit->firstRange;
    PICO_CONNECT_PROBE_RANGE _lastRange = unit->lastRange;
    struct tPicoProbeScaling chRangeInfoTempFirst;
    struct tPicoProbeScaling chRangeInfoTempLast;

    /* If an intelligent probe is connected for this channel, use its reported range limits */
    if (userProbeInfo.userProbeInteractions[channelIndex].connected == TRUE)
    {
        _firstRange = userProbeInfo.userProbeInteractions[channelIndex].rangeFirst_;
        _lastRange = userProbeInfo.userProbeInteractions[channelIndex].rangeLast_;
    }

    /* Validate requested user range against the resolved bounds */
    if ((uint32_t)userRange < (uint32_t)_firstRange || (uint32_t)userRange >(uint32_t)_lastRange)
    {
        printf("Specified range is out of bounds for channel %c.\n", 'A' + channelIndex); 
        
        getRangeScaling(_firstRange, &chRangeInfoTempFirst);
        getRangeScaling(_lastRange, &chRangeInfoTempLast);
        printf("Use probe range: (%s..%s))", chRangeInfoTempFirst.Probe_Range_text, chRangeInfoTempLast.Probe_Range_text);
        return 1;
    }
    return 0;
}

/****************************************************************************
 * setTimebase
 * Select timebase, set time units asi seconds
 *
 ****************************************************************************/
void setTimebase(GENERICUNIT *unit) {
  PICO_STATUS status = PICO_OK;
  PICO_STATUS powerStatus = PICO_OK;
  double timeInterval = 0.0f; // int32_t
  double *p_timeInterval = &timeInterval;
  // uint64_t maxSamples; //int32_t
  int32_t ch;

  uint32_t shortestTimebase = 0 ;
  double timeIntervalSeconds;

  PICO_CHANNEL_FLAGS enabledChannelOrPortFlags = (PICO_CHANNEL_FLAGS)0;

  int16_t numValidChannels = unit->channelCount;

  // Find the analogue channels that are enabled - if an MSO model is being
  // used, this will need to be modified to add channel flags for enabled
  // digital ports
  for (ch = 0; ch < numValidChannels; ch++) {
    if (unit->channelSettings[ch].enabled) {
      enabledChannelOrPortFlags =
          enabledChannelOrPortFlags | (PICO_CHANNEL_FLAGS)(1 << ch);
    }
  }

  // Find the shortest possible timebase and inform the user.
  /*status = ps4000aGetMinimumTimebaseStateless(
      unit->handle, enabledChannelOrPortFlags, &shortestTimebase,
      &timeIntervalSeconds, unit->resolution);
  if (status != PICO_OK) {
    printf("setTimebase:ps4000aGetMinimumTimebaseStateless ------ 0x%08lx \n",
           status);
    if (status == 0x0000018c)
      printf("The channel combination is not valid for the ADC resolution "
             "(10/12bit)");
    return;
  }
  */
  float testtimeInterval = 0.0f;
  int32_t maxSamples = 0;
  while (ps4000aGetTimebase2(unit->handle, shortestTimebase, (int32_t)constBufferSize, &testtimeInterval, &maxSamples, 0))
  {
      shortestTimebase++;  // Increase timebase if the one specified can't be used. 
  }
  timeIntervalSeconds = (double)(testtimeInterval / 1e9); // Convert to seconds

  printf("Shortest timebase index available %d = %le seconds.\n",
         shortestTimebase, timeIntervalSeconds);

  printf("Specify desired timeInterval (in the format Ne-XX, example 1us -> "
         "1e-06): ");
  fflush(stdin);
  double timeIntervalRequested = 0;
  scanf_s("%le", &timeIntervalRequested);

  testtimeInterval = 0.0f;
  timeInterval = 0.0f;
  shortestTimebase = 0;
  status = PICO_INVALID_TIMEBASE;
  while (TRUE) // Loop until a valid timebase is found that meets the user's requested time interval
  {
      status = ps4000aGetTimebase2(unit->handle, shortestTimebase, (int32_t)constBufferSize, &testtimeInterval, &maxSamples, 0);
      shortestTimebase++;  // Increase timebase if the one specified can't be used. 
      if (testtimeInterval >= (float)(timeIntervalRequested * 1e9) )
          break;
  }
  if (status != PICO_OK)
      printf("setTimebase:ps4000aGetTimebase2 ------ 0x%08lx \n",   status);
  timeInterval = (double)testtimeInterval;
  timeInterval = timeInterval / 1e9; // Convert to seconds

  timebase = shortestTimebase;

  /*timeInterval = timeInterval * 1e9;
  status = ps4000aNearestSampleIntervalStateless(
      unit->handle,
      enabledChannelOrPortFlags, // enabledChannelFlags,
      timeIntervalRequested,     // timeIntervalRequested,
      unit->resolution,          // resolution,
      0,                         // useEts - added for ps4000a
      &timebase,                 //*timebase,
      &timeInterval              //*timeIntervalAvailable
  );


  if (status !=
      PICO_OK) //(status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION)
  {
    printf("NearestSampleIntervalStateless: Error - Invalid number of channels "
           "for resolution.\n");
    return;
  } else {
    // Do nothing
  }
  */

  printf("Timebase used %lu = %le seconds sample interval\n", timebase,
         timeInterval);
  unit->timeInterval = timeInterval;
}

/****************************************************************************
 * printResolution
 *
 * Outputs the resolution in text format to the console window
 ****************************************************************************/
void printResolution(PS4000A_DEVICE_RESOLUTION *resolution) {
  switch (*resolution) {
  case (PS4000A_DEVICE_RESOLUTION)PICO_DR_8BIT:

    printf("8 bits");
    break;

  case (PS4000A_DEVICE_RESOLUTION)PICO_DR_10BIT:

    printf("10 bits");
    break;

  case (PS4000A_DEVICE_RESOLUTION)PICO_DR_12BIT:

    printf("12 bits");
    break;

  case (PS4000A_DEVICE_RESOLUTION)PICO_DR_14BIT:

    printf("14 bits");
    break;

  case (PS4000A_DEVICE_RESOLUTION)PICO_DR_15BIT:

    printf("15 bits");
    break;

  case (PS4000A_DEVICE_RESOLUTION)PICO_DR_16BIT:

    printf("16 bits");
    break;

  default:

    break;
  }

  printf("\n");
}

/****************************************************************************
 * setResolution
 * Set resolution for the device
 *
 ****************************************************************************/
void setResolution(GENERICUNIT *unit) {
  int16_t value = 0;
  int16_t i;
  int16_t numEnabledChannels = 0;
  int16_t retry;
  int32_t resolutionInput = -1;

  PICO_STATUS status;
  PS4000A_DEVICE_RESOLUTION resolution = (PS4000A_DEVICE_RESOLUTION)PICO_DR_12BIT;
  PS4000A_DEVICE_RESOLUTION newResolution = (PS4000A_DEVICE_RESOLUTION)PICO_DR_12BIT;

  // Determine number of channels enabled
  for (i = 0; i < unit->channelCount; i++) {
    if (unit->channelSettings[i].enabled == TRUE) {
      numEnabledChannels++;
    }
  }

  if (numEnabledChannels == 0) {
    printf("setResolution: Please enable channels.\n");
    return;
  }
  
  status = ps4000aGetDeviceResolution(unit->handle, &resolution);
  if (status == PICO_OK) {
    printf("Current resolution: ");
    printResolution(&resolution);
  }
  else
  {
    printf("setResolution:ps4000aGetDeviceResolution ------ 0x%08lx \n", status);
    return;
  }

  printf("\n");
  printf("Select device resolution:\n");
  printf("0: 12 bits\n");
  printf("1: 14 bits\n");

  retry = TRUE;
  do {
    printf("Resolution [0...1]: ");

    fflush(stdin);
    scanf_s("%d", &resolutionInput);
    if (resolutionInput == 0)
      resolutionInput = PICO_DR_12BIT;
    else if (resolutionInput == 1)
      resolutionInput = PICO_DR_14BIT;
    else
        resolutionInput = -1;

    newResolution = (PS4000A_DEVICE_RESOLUTION)resolutionInput;

    // Verify if resolution can be selected for number of channels enabled
    if (resolutionInput == -1)
    {
        printf("Invalid resolution.\n");
    }
    else if (newResolution == ((PS4000A_DEVICE_RESOLUTION)(PICO_DR_14BIT && !MODEL_4444)) )
    {
        printf("setResolution: 14 bit resolution is only supported on the PS4444 model.\n");
    }
    else
    {
        retry = FALSE;
    }
  } while (retry);

  printf("\n");

  status = ps4000aSetDeviceResolution(unit->handle, (PS4000A_DEVICE_RESOLUTION)newResolution);
  if (status != PICO_OK)
  {
      printf("setResolution:ps4000aSetDeviceResolution ------ 0x%08lx \n", status);
  }
  else
  {
    unit->resolution = (PICO_DEVICE_RESOLUTION)newResolution;

    printf("Resolution selected: ");
    printResolution(&newResolution);

    // The maximum ADC value will change if transitioning from 12-bit to 14-bit or vice-versa
    status = ps4000aMaximumValue(unit->handle, &value);
    unit->maxADCValue = value;
  }
  if (status != PICO_OK)
  {
    printf("setResolution:ps4000aMaximumValue ------ 0x%08lx \n", status);
  }
}

/****************************************************************************
 * displaySettings
 * Displays information about the user configurable settings in this example
 * Parameters
 * - unit        pointer to the UNIT structure
 *
 * Returns       none
 ***************************************************************************/
void displaySettings(GENERICUNIT *unit) {
  int32_t ch;
  int32_t voltage;
  PICO_STATUS status = PICO_OK;
  PS4000A_DEVICE_RESOLUTION resolution = (PS4000A_DEVICE_RESOLUTION)PICO_DR_12BIT; //PICO_DEVICE_RESOLUTION
  struct tPicoProbeScaling chRangeInfoTemp;

  printf("\nTrigger values will be scaled in %s\n",
         (scaleVoltages) ? ("Millivolts(mV)") : ("ADC counts"));

  for (ch = 0; ch < unit->channelCount; ch++) {
    if (!(unit->channelSettings[ch].enabled)) {
      printf("Channel %c Range: Off\n", 'A' + ch);
    }
    else {

        printf("Channel %c Range: ", 'A' + ch);

        if (unit->channelSettings[ch].range > PICO_DIFFERENTIAL_200V)
        {
            getRangeScaling(unit->channelSettings[PS4000A_CHANNEL_A + ch].range, &chRangeInfoTemp);
            printf("ProbeRange: %s Probe Units: %s, ", chRangeInfoTemp.Probe_Range_text, chRangeInfoTemp.Unit_text);
        }
        else
        {
            voltage = (int32_t)inputRanges[unit->channelSettings[ch].range];
            if (voltage < 1000) {
                printf("%dmV, ", voltage);
            }
            else {
                printf("%dV, ", voltage / 1000);
            }
        }
      if (unit->channelSettings[ch].DCcoupled == PICO_DC)
        printf("Coupling: DC, ");
      if (unit->channelSettings[ch].DCcoupled == PICO_AC)
        printf("Coupling: AC, ");
      if (unit->channelSettings[ch].bandwithLimit == PICO_BW_FULL)
        printf("bandwithLimit: FULL, ");
      if (unit->channelSettings[ch].bandwithLimit == PS4000A_BW_20KHZ)
        printf("bandwithLimit: 20kHz, ");
      if (unit->channelSettings[ch].bandwithLimit == PS4000A_BW_100KHZ)
          printf("bandwithLimit: 100kHz, ");
      if (unit->channelSettings[ch].bandwithLimit == PS4000A_BW_1MHZ)
          printf("bandwithLimit: 1MHz, ");
      
      printf("analogueOffset: %g\n", unit->channelSettings[ch].analogueOffset);
    }
  }
  printf("\n");

  status = ps4000aGetDeviceResolution(unit->handle, &resolution);

  printf("Device Resolution: ");
  printResolution(&resolution);
}

/****************************************************************************
 * openDevice
 * Parameters
 * - unit        pointer to the UNIT structure, where the handle will be stored
 * - serial		pointer to the int8_t array containing serial number
 *
 * Returns
 * - PICO_STATUS to indicate success, or if an error occurred
 ***************************************************************************/
PICO_STATUS openDevice(GENERICUNIT *unit, int8_t *serial) {
  PICO_STATUS status;
  unit->resolution = PICO_DR_12BIT;

  if (serial == NULL) {
    status =
        ps4000aOpenUnitWithResolution(&unit->handle, NULL, unit->resolution);
  } else {
    status =
        ps4000aOpenUnitWithResolution(&unit->handle, serial, unit->resolution);
  }

  unit->openStatus = (int16_t)status;
  unit->complete = 1;

  return status;
}

/****************************************************************************
 * handleDevice
 * Parameters
 * - unit        pointer to the UNIT structure, where the handle will be stored
 *
 * Returns
 * - PICO_STATUS to indicate success, or if an error occurred
 ***************************************************************************/
PICO_STATUS handleDevice(GENERICUNIT *unit, SIG_GEN_SETTINGS *sigGenSettings) {
  int16_t value = 0;
  int32_t i;
  PICO_STATUS status;
  PICO_STATUS currentPowerStatus;

  if (unit->openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || unit->openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
  {
      unit->openStatus = (uint32_t)ChangePowerSource(unit->handle, unit->openStatus);

      currentPowerStatus = ps4000aCurrentPowerSource(unit->handle);

      if (currentPowerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
      {
          printf("USB Powered");
      }
      else
      {
          printf("5 V Power Supply Connected");
      }

      printf("\n");
  }

  printf("Handle: %d\n", unit->handle);

  if (unit->openStatus != PICO_OK && unit->openStatus != PICO_POWER_SUPPLY_NOT_CONNECTED)
  {
      printf("Unable to open device\n");
      printf("Error code : 0x%08x\n", (uint32_t)unit->openStatus);
      while (!_kbhit());
      exit(99); // exit program
  }

  printf("Device opened successfully, cycle %d\n\n", ++cycles);

  // Setup device info - unless it's set already
  if (unit->model == MODEL_NONE) {
    set_info(unit);
  }

  // Register probe interaction callback 
  if (unit->hasIntelligentProbes)
  {
      status = ps4000aSetProbeInteractionCallback(unit->handle, CallBackProbeInteractions);
      // Wait for information to populate (callback will be called twice initially)
      do
      {
        Sleep(2000);
      } while (g_probeStateChanged == 0);
  }

  // Set default channel settings, turn on channels according to TURN_ON_EVERY_N_CH definition
  int16_t enabled_chs_limit = unit->channelCount;
  if (unit->channelCount > ENABLED_CHS_LIMIT) {
      enabled_chs_limit = ENABLED_CHS_LIMIT;
      // printf("Limiting enabled channels to %d! (Starting at ChA)\n",
      // enabled_chs_limit);
  }
  for (i = 0; i < unit->channelCount; i++) {
      // define "TURN_ON_EVERY_N_CH" to either 2 or 4 (2 = Every odd Ch is
      // enabled, 4 = Every 4th Ch enabled), set 1 to disable.

      unit->channelSettings[i].DCcoupled = PICO_DC; // PICO_AC, PICO_DC, PICO_DC_50OHM
      unit->channelSettings[i].analogueOffset = 0.0f;
      unit->channelSettings[i].bandwithLimit =
          PICO_BW_FULL; // PICO_BW_FULL, PICO_BW_20MHZ, PICO_BW_200MHZ

      if (i % TURN_ON_EVERY_N_CH == 0 && i < enabled_chs_limit)
      {
          unit->channelSettings[i].enabled = TRUE;
          if (userProbeInfo.userProbeInteractions[i].connected && (userProbeInfo.userProbeInteractions[i].rangeLast_ > PICO_X1_PROBE_200V) )
              unit->channelSettings[i].range = userProbeInfo.userProbeInteractions[i].rangeLast_;
          else
              unit->channelSettings[i].range = PICO_X1_PROBE_1V;
      }
      else
      {
          unit->channelSettings[i].enabled = FALSE;
          unit->channelSettings[i].range = PICO_CONNECT_PROBE_OFF;
      }
  }

  float temp_timeIntervalns = 0.0f;
  do {
    status = ps4000aGetTimebase2(unit->handle, timebase, (int32_t)(constBufferSize), &temp_timeIntervalns, NULL, 0);

    if (status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION) {
      printf(
          "SetTimebase: Error - Invalid number of channels for resolution.\n");
      return status;
    } else if (status == PICO_OK) {
      // Do nothing
    } else {
      timebase++; // Increase timebase if the one specified can't be used.
    }

  } while (status != PICO_OK);

  unit->timeInterval = (double)(temp_timeIntervalns * 1e-9);

  status = ps4000aMaximumValue(unit->handle, &value);
  unit->maxADCValue = value;

  unit->CapturesComplete = 0; // used by GetMoreDataHandler()

  if (sigGenSettings != NULL) {
    // Set default Signal Generator settings /AWG settings
    ///////////
    sigGenSettings->Enabled = 0;
    //
    sigGenSettings->PeakVolts = 2.0f;
    sigGenSettings->Offset = 0.0f;
    sigGenSettings->Frequency = 1000.0f; // 1.0e3;
    // Sweep settings
    sigGenSettings->FrequencyStop = 2000.0f;
    sigGenSettings->FrequencyIncrement =
        100.0f;                       // double* frequencyIncrement(Hz),

    sigGenSettings->DwellTime = 0.01f; // double* dwellTime (s)
    sigGenSettings->SweepType = PS4000A_UP;
    // Waveform settings
    sigGenSettings->isArbitrary = FALSE;
    sigGenSettings->AWGBufferSize = 0;
    // sigGenSettings->AWGBuffer = (int16_t*)calloc(maxAwgBufferLeght,
    // sizeof(int16_t));
    sigGenSettings->AWGBuffer = NULL;
    // Trigger settings
    sigGenSettings->triggerSource = PS4000A_SIGGEN_NONE;
    sigGenSettings->triggerType = PS4000A_SIGGEN_RISING;
    sigGenSettings->cycles = 1; // Number of cycles to output
    sigGenSettings->autoTrigPicoSecs =
        0; // Auto trigger in pico seconds (0 = no auto trigger)
  }
  ProbestoSettings(unit);
  g_probeStateChanged = 0;
  setDefaults(unit);

  /* Trigger disabled	*/
  status = ps4000aSetSimpleTrigger(unit->handle, 0, PS4000A_CHANNEL_A, 0,
                                   PICO_RISING, 0, 0);

  return unit->openStatus;
}

/******************************************************************************
* ChangePowerSource -
* 	function to handle switches between USB 3.0 and non-USB 3.0 connections
*******************************************************************************/
PICO_STATUS ChangePowerSource(int16_t handle, PICO_STATUS status)
{
    int8_t ch = 'Y';

    switch (status)
    {

    case PICO_POWER_SUPPLY_NOT_CONNECTED:

        do
        {
            printf("\n5 V power supply not connected.");
            printf("\nDo you want to run using USB only Y/N?\n");

            ch = toupper(_getch());

            if (ch == 'Y')
            {
                printf("\nPower OK\n");
                status = ps4000aChangePowerSource(handle, PICO_POWER_SUPPLY_NOT_CONNECTED);		// Tell the driver that's ok
            }

        } while (ch != 'Y' && ch != 'N');

        printf(ch == 'N' ? "Please set correct USB connection setting for this device\n" : "");
        break;

    case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:			// User must acknowledge they want to power via USB

        do
        {
            printf("\nUSB 3.0 device on non-USB 3.0 port.\n");
            status = ps4000aChangePowerSource(handle, PICO_USB3_0_DEVICE_NON_USB3_0_PORT);		// Tell the driver that's ok

        } while (ch != 'Y' && ch != 'N');

        printf(ch == 'N' ? "Please set correct USB connection setting for this device\n" : "");
        break;
    }
    return status;
}

/****************************************************************************
 * closeDevice
 ****************************************************************************/
void closeDevice(GENERICUNIT *unit) { ps4000aCloseUnit(unit->handle); }

/****************************************************************************
 * GetMoreDataHandler
 * - Used by all data routines
 * - acquires data, displays 10 items
 *   and saves all data to a file.
 * Input :
 * - unit : the unit to use.
 * - noOfPreTriggerSamples : number of samples to capture before trigger.
 * - autostop : 1 to stop when trigger condition is met, 0 to continue until
 * user stops.
 ****************************************************************************/
void GetMoreDataHandler(GENERICUNIT *unit, PICO_RATIO_MODE ratioMode,
                        uint64_t downSampleRatio,
                        uint64_t nSamples,
                        FILE_TYPE filetype) // Set the number of raw samples
{
  int32_t index = 0;
  int16_t channel = 0;
  PICO_STATUS status = PICO_OK;
  // Set the number buffers from previous Rapid block capture.
  if (unit->CapturesComplete == 0) {
    printf("No Captures done - Exiting MoreDataHandler()\n");
    return;
  }
  uint64_t nCaptures = unit->CapturesComplete;

  // Define acquisition Settings

  // Buffers settings
  // Use scope acquisition settings for data download
  struct tbuffer_settings bufferSettings = {0};
  bufferSettings.startIndex = 0;
  bufferSettings.downSampleRatioMode = ratioMode;
  bufferSettings.downSampleRatio = downSampleRatio;
  bufferSettings.nSamples = nSamples;

  // Create Buffers - Min and Max (3D buffers - Captures, Channels, Samples)
  struct tmultiBufferSizes multiBufferSizes; // to store buffer sizes
  int16_t ***minBuffersStopped;
  int16_t ***maxBuffersStopped;
  pico_create_multibuffers(unit, bufferSettings, nCaptures, &minBuffersStopped,
                           &maxBuffersStopped, &multiBufferSizes);

  printf("\nRequesting More Data.");
  // printf("\nNumber of PreTriggerSamples: %lld", noOfPreTriggerSamples);

  // Save and print Sample Internal set (in seconds)
  // unit->timeInterval = (idealTimeInterval * (pow(10, 3 *
  // sampleIntervalTimeUnits) / 1E+15));
  printf("\nsample Internal: %g seconds\n", unit->timeInterval);
  // print number of Samples
  printf("%llu Samples\n", nSamples);
  uint64_t printTriggerSample = 0;

  // SetDataBuffers with API
  if (nCaptures == 1) // only 1 segment, for block and streaming download
    SetAllDataBuffers(unit, &bufferSettings, &minBuffersStopped,
                      &maxBuffersStopped, &multiBufferSizes, 0,
                      (CAPTURE_MODE)BLOCK, 0);
  else // > 1 segment, Rapid download only
    SetAllDataBuffers(unit, &bufferSettings, &minBuffersStopped,
                      &maxBuffersStopped, &multiBufferSizes, 0,
                      (CAPTURE_MODE)RAPID_BLOCK, 0);

  printf("\nPress any key to abort.");
  printf("\nWaiting for Data ");

  g_ready = FALSE; // reset flag

  if (unit->CapturesComplete == 1) {
    status = ps4000aGetValuesAsync(
        unit->handle,
        0, // startIndex
        bufferSettings.nSamples, downSampleRatio, ratioMode,
        0,                 // segmentIndex
        callBackDataReady, // pointer to Data callback
        NULL);             // pParameter

    if (status != PICO_OK) {
      printf(status ? "blockDataHandler:ps4000aGetValuesAsync ------ 0x%08lx \n"
                    : "",
             status);
      return;
    }
  } else // > 1 segment
  {
    status = ps4000aGetValuesBulk(unit->handle,
                                  (uint32_t *)&(bufferSettings.nSamples), // noOfSamples
                                  0,                          // From Segment
                                  (uint32_t)nCaptures - 1,              // To Segment
                                  downSampleRatio, ratioMode,
                                  NULL); // overflow (int16_t*)

    // Since this is synchronous, we manually call callback or set ready
    (void)callBackDataReady;
    g_ready = TRUE;

    if (status != PICO_OK) {
      printf(
          status
              ? "blockDataHandler:ps4000aGetValuesBulkAsync ------ 0x%08lx \n"
              : "",
          status);
      return;
    }
  }
  // wait for capture to complete or for user to abort
  while (!g_ready && !_kbhit()) {
    Sleep(500);
    printf(". ");
  }

  printf("\nFinished Data download");
  // Write one segment to a file as captured
  printf("\nWriting Buffer Set of channels to a file.\n");

  // Create file name string
  char startOfFileName2[] = "MoreDataStopped";
  char buf[58 + (3 * sizeof(int))];
  size_t buf_size = sizeof(buf) / sizeof(buf[0]);
  // snprintf(buf, buf_size, "%s%d.txt", startOfFileName, (int)capture);
  snprintf(buf, buf_size, "%s", startOfFileName2);

  // Get scaling Info for each channel
  struct tPicoProbeScaling enabledChannelsScaling[PS4000A_MAX_CHANNELS] = {0};
  struct tPicoProbeScaling channelRangeInfoTemp;
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      getRangeScaling(unit->channelSettings[PS4000A_CHANNEL_A + 0].range,
                      &channelRangeInfoTemp);
      enabledChannelsScaling[channel] = channelRangeInfoTemp;
    }
  }

  WriteArrayToFilesGeneric(
      unit, minBuffersStopped, maxBuffersStopped, multiBufferSizes,
      enabledChannelsScaling, buf,
      0,     // streamingDataTriggerInfoTemp.triggerAt_, // Triggersample
      NULL,  // No overflow flags
      NULL); // Set default full range if NULL

  // Release Buffer memory from API
  clearDataBuffers(unit);

  // Free buffers
  pico_release_multibuffers(unit, &minBuffersStopped, &maxBuffersStopped,
                            &multiBufferSizes);
}

/****************************************************************************
 * SetupTrigger
 * This function sets up an advanced trigger on Channel A, rising, at +50% of
 * the channel range. Inputs :
 * - unit : the unit to use.
 * Returns       none
 ****************************************************************************/
void SetupTrigger(GENERICUNIT *unit) {
  PICO_STATUS status = PICO_OK;

  // Set triggerLevelADC to +50% of set channel voltage range
  int16_t triggerLevelADC = mv_to_adc(
      (double)inputRanges[unit->channelSettings[PS4000A_CHANNEL_A].range] / 2,
      unit->channelSettings[PS4000A_CHANNEL_A].range, unit->maxADCValue);

  struct tPS4000ATriggerChannelProperties sourceDetails = {
      triggerLevelADC, // thresholdUpper
      (uint16_t)256 * 16,        // thresholdUpperHysteresis
      triggerLevelADC, // thresholdLower
      (uint16_t)256 * 16,        // thresholdLowerHysteresis
      PS4000A_CHANNEL_A,  // channel - PS4000A_CHANNEL
      PS4000A_LEVEL
  };

  struct tPS4000ACondition conditions = {
      sourceDetails.channel, // PS4000A_CHANNEL
      PS4000A_CONDITION_TRUE    // PICO_TRIGGER_STATE - true/false/Don't care
  };

  struct tPS4000ADirection directions = {directions.channel = conditions.source,
                                      directions.direction = PS4000A_RISING,
                                      };

  // Create Pulse Width Qualifier structure with settings
  struct tps4000aPwq pulseWidth = {0}; // zero out pulseWidth

  printf("Trigger Channel is %c\n", 'A' + sourceDetails.channel);
  printf("Collects when value rises past %d",
         scaleVoltages
             ? (int16_t)adc_to_mv(
                   sourceDetails.thresholdUpper,
                   unit->channelSettings[sourceDetails.channel].range,
                   unit->maxADCValue) // If scaleVoltages, print mV value
             : sourceDetails.thresholdUpper); // else print ADC Count

  printf(scaleVoltages ? " mV\n" : " ADC Counts\n");

  printf("Press a key to start...\n");
  _getch();
  ProbestoSettings(unit);
  setDefaults(unit);

  status = SetTrigger(unit, &sourceDetails, 1, // channelProperties //nChannelProperties
                      PICO_AUXIO_INPUT, // auxIoMode
                      &conditions, 1,   // conditions		//nConditions
                      &directions, 1,   // directions		//nDirections
                      &pulseWidth,      // PWQ
                      0, 0);            // TrigDelay //AutoTrigger_us
}

/****************************************************************************
 * SetAllDataBuffers
 * This function Setups all the data buffers for enabled channels and segments.
 * Inputs :
 * - unit : the unit to use.
 * - bufferSettings : structure containing buffer settings
 * - minBuffers : 3D array of pointers to int16_t buffers for minimum ADC values
 * - maxBuffers : 3D array of pointers to int16_t buffers for maximum ADC values
 * - multiBufferSizes : structure containing buffer sizes
 * - StreamBufToSet : in streaming mode, the buffer number to set (0 to
 * (multiBufferSizes->numberOfBuffers -1) )
 * - CaptureMode : enum to indicate if BLOCK, RAPID_BLOCK or STREAMING mode
 * - Reset_action : if 0, first buffer set uses CLEAR_ALL | ADD, if not 0, first
 * buffer set uses ADD only (used in streaming mode) Returns       none
 ****************************************************************************/
void SetAllDataBuffers(GENERICUNIT *unit,
                       struct tbuffer_settings *bufferSettings,
                       int16_t ****minBuffers, int16_t ****maxBuffers,
                       struct tmultiBufferSizes *multiBufferSizes,
                       uint64_t StreamBufToSet, enum enCaptureMode CaptureMode,
                       int16_t Reset_action) {
  uint64_t waveform;
  uint64_t nCaptures;
  int16_t channel;
  uint64_t capture;
  PICO_STATUS status = PICO_OK;

  if (CaptureMode != (enum enCaptureMode)STREAMING) {
    nCaptures = multiBufferSizes->numberOfBuffers;
  } else // Streaming mode - only set one buffer at a time
  {
    waveform = 0;
    // force "for loop" to only use one buffer set
    nCaptures = StreamBufToSet + 1;
  }
  //  SetDataBuffers with API
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      if (CaptureMode != (enum enCaptureMode)STREAMING) {
        capture = 0;
      } else // Streaming mode - only set one buffer at a time
      {
        capture = StreamBufToSet; // force "for loop" to only use one buffer set
      }

      for (capture; capture < nCaptures; capture++) {
        if (CaptureMode != (enum enCaptureMode)STREAMING)
          waveform = capture;
        status = ps4000aSetDataBuffers(
            unit->handle, (PS4000A_CHANNEL)channel,
            (*maxBuffers)[capture][channel], (*minBuffers)[capture][channel],
            multiBufferSizes->maxBufferSize, waveform,
            bufferSettings->downSampleRatioMode);
        if (status != PICO_OK) {
          printf("SetAllDataBuffers:ps4000aSetDataBuffers ------ 0x%08x, for "
                 "channel %c \n",
              status, 'A' + channel);
          return;
        }
      }
    }
  }
}
