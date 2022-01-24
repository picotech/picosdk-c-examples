#pragma once

#include "ps4000aApi.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>

const int32_t NUMBER_OF_CHANNELS = 8;

typedef struct PS4000A_PWQ_CONDITIONS4 {
  PS4000A_TRIGGER_STATE channelA;
  PS4000A_TRIGGER_STATE channelB;
  PS4000A_TRIGGER_STATE channelC;
  PS4000A_TRIGGER_STATE channelD;
  PS4000A_TRIGGER_STATE external;
  PS4000A_TRIGGER_STATE aux;
} PS4000A_PWQ_CONDITIONS;

struct ParallelDevice {

  int16_t handle;
  int16_t maxADCValue;
  int32_t noOfChannels = NUMBER_OF_CHANNELS;


  uint32_t timebase;
  int32_t noSamples;
  float timeInterval = 0;
  int32_t maxSamples = 0;


  int16_t* buffer[NUMBER_OF_CHANNELS];

  int32_t AdcTrigger = 500;
  int32_t AutoTrigger = 5000;

  int16_t isReady;

  int32_t* timeIndisposed;
};

class RAII {
  int16_t handleList[8];
  int handleCount = 0;
public:
  void Add(int16_t handle) {
    handleList[handleCount] = handle;
    ++handleCount;
  }
  ~RAII() {
    for (int i = 0; i < handleCount; i++)
      ps4000aCloseUnit(handleList[i]);
  }
};