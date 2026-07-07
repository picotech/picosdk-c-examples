/****************************************************************************
 *
 * Filename:    Libps4000a.h
 * Copyright:   Pico Technology Limited 2026
 * Description:
 *
 * This header defines shared functions and structures for
 * all ps4000a example code.
 *
 ****************************************************************************/

#ifndef __LIBPS4000A_H__
#define __LIBPS4000A_H__
#include "../../shared/PicoUnit.h"
#include <stdint.h>

/* Headers for Windows */
#ifdef _WIN32
#include "ps4000aApi.h"
#include "windows.h"
#include <conio.h>

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

#define OCTA_SCOPE 8
#define QUAD_SCOPE 4
#define DUAL_SCOPE 2

#define MAX_PICO_DEVICES 64

// Max channels for this API/series of models
////////////////////////#define PS4000A_MAX_CHANNELS 8 // analog chs only
#define MSO_MAX_CHANNELS 0     // digital chs only

// Default Enabled Channel defines-
#define ENABLED_CHS_LIMIT                                                      \
  2 // Set to limit the max number channels to enable (for example if set to 2
    // then ChA and ChB will be turned on)
#define TURN_ON_EVERY_N_CH                                                     \
  1 // Set this either 2 or 4 (2 = Every odd Ch is enabled, 4 = Every 4th Ch
    // enabled) Or set to 1 to disable.

typedef struct tBufferInfo
{
    GENERICUNIT* unit;
    int16_t*** driverMaxBuffers;
    int16_t*** driverMinBuffers;
    int16_t*** appMaxBuffers;
    int16_t*** appMinBuffers;

} BUFFER_INFO;

typedef struct tps4000aPwq {
  PS4000A_CONDITION *conditions;
  int16_t nConditions;
  PS4000A_THRESHOLD_DIRECTION *directions;
  int16_t nDirections;
  uint32_t lower;
  uint32_t upper;
  PS4000A_PULSE_WIDTH_TYPE type;
} PS4000APWQ;

// Struct to store Signal generator and AWG unit settings
typedef struct tSigGenSettings {
  int16_t Enabled;
  // General Signal Generator Settings
  PS4000A_WAVE_TYPE WaveType;
  BOOL isArbitrary;
  double PeakVolts;
  double Offset;
  double Frequency;
  double dutyCyclePercent;
  // Signal Generator Sweep Settings
  double FrequencyStop;
  double FrequencyIncrement; // frequencyIncrement
  double DwellTime;          // Seconds
  PS4000A_SWEEP_TYPE SweepType;
  //uint32_t Shots;
  //uint32_t Sweeps;
  // Signal Generator Trigger Settings
  PS4000A_SIGGEN_TRIG_TYPE triggerType; // PICO_SIGGEN_TRIG_TYPE triggerType,
  PS4000A_SIGGEN_TRIG_SOURCE triggerSource; //  PICO_SIGGEN_TRIG_SOURCE triggerSource; // PICO_SIGGEN_TRIG_SOURCE triggerSource,
  uint64_t cycles;
  uint64_t autoTrigPicoSecs; // Signal Generator Waveform Settings
  int16_t *AWGBuffer;
  int32_t AWGBufferSize;
} SIG_GEN_SETTINGS;

// Struct to store intelligent probe information
typedef struct tUserProbeInfo {
  PICO_STATUS status;
  PS4000A_USER_PROBE_INTERACTIONS userProbeInteractions[PS4000A_MAX_CHANNELS]; //PICO_USER_PROBE_INTERACTIONS
  uint32_t numberOfProbes;

} USER_PROBE_INFO;

// Function prototypes

// Callback functions
void PREF4 callBackBlockReady(int16_t handle, PICO_STATUS status,
                              PICO_POINTER pParameter);

void PREF4 callBackDataReady(int16_t handle, PICO_STATUS status,
                             uint64_t noOfSamples, int16_t overflow,
                             PICO_POINTER pParameter);

void PREF4 callBackStreaming(int16_t handle,
                                int32_t noOfSamples,
                                uint32_t	startIndex,
                                int16_t overflow,
                                uint32_t triggerAt,
                                int16_t triggered,
                                int16_t autoStop,
                                void* pParameter);

void PREF4 CallBackProbeInteractions(int16_t handle, PICO_STATUS status,
                                     PS4000A_USER_PROBE_INTERACTIONS *probes,
                                     uint32_t nProbes);

// Request more data function
void GetMoreDataHandler(GENERICUNIT *unit,
						PICO_RATIO_MODE ratioMode,
                        uint64_t downSampleRatio,
						uint64_t nSamples,
						FILE_TYPE filetype,
						BOOL imagefile);

// Unit setup and management functions
void setDefaults(GENERICUNIT *unit);
void ProbestoSettings(GENERICUNIT* unit);
PICO_STATUS ChangePowerSource(int16_t handle, PICO_STATUS status);

void set_info(GENERICUNIT *unit);
int8_t ValidateChannelRange(GENERICUNIT* unit, uint8_t channelIndex, PICO_CONNECT_PROBE_RANGE userRange);
void displaySettings(GENERICUNIT *unit);

// Device connection functions
PICO_STATUS openDevice(GENERICUNIT *unit, int8_t *serial);
void closeDevice(GENERICUNIT *unit);
PICO_STATUS handleDevice(GENERICUNIT *unit, SIG_GEN_SETTINGS *sigGenSettings);

// Timebase and Channel settings functions
void setVoltages(GENERICUNIT *unit);
void setTimebase(GENERICUNIT *unit);

// Resolution functions
void setResolution(GENERICUNIT *unit);
void printResolution(PS4000A_DEVICE_RESOLUTION *resolution);

// Data buffer functions
void SetAllDataBuffers(GENERICUNIT *unit,
                       struct tbuffer_settings *bufferSettings,
                       int16_t ****minBuffers, int16_t ****maxBuffers,
                       struct tmultiBufferSizes *multiBufferSizes,
                       uint64_t StreamBufToSet, enum enCaptureMode CaptureMode,
                       int16_t Reset_action);

PICO_STATUS clearDataBuffers(GENERICUNIT *unit);

// triggering functions
void SetupTrigger(GENERICUNIT *unit);

PICO_STATUS SetTrigger(GENERICUNIT* unit,
    PS4000A_TRIGGER_CHANNEL_PROPERTIES* channelProperties,
    int16_t nChannelProperties,
    int16_t auxOutputMode,
    PS4000A_CONDITION* triggerConditions, int16_t nTriggerConditions,
    PS4000A_DIRECTION* directions, int16_t nDirections,
    struct tps4000aPwq* pwq, uint32_t delay, int32_t autoTrigger_us);

#endif
