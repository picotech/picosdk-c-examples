#pragma once

#include "ps4000aApi.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
#include <map>

const int32_t NUMBER_OF_CHANNELS = 8;

typedef struct PS4000A_PWQ_CONDITIONS4 {
  PS4000A_TRIGGER_STATE channelA;
  PS4000A_TRIGGER_STATE channelB;
  PS4000A_TRIGGER_STATE channelC;
  PS4000A_TRIGGER_STATE channelD;
  PS4000A_TRIGGER_STATE external;
  PS4000A_TRIGGER_STATE aux;
} PS4000A_PWQ_CONDITIONS;

class GlobalState {
private:
  /* Here will be the instance stored. */
  static GlobalState* instance;

  /* Private constructor to prevent instancing. */
  GlobalState();

public:
  /* Static access method. */
  bool triggerSet = true;
  static GlobalState* getInstance();
};/* Null, because instance will be initialized on demand. */
GlobalState* GlobalState::instance = 0;

GlobalState* GlobalState::getInstance()
{
  if (instance == 0)
  {
    instance = new GlobalState();
  }

  return instance;
}

GlobalState::GlobalState()
{}

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

class RAII2 {
  int16_t handleList[8];
  int handleCount = 0;
public:
  void Add(int16_t handle) {
    handleList[handleCount] = handle;
    ++handleCount;
  }
  ~RAII2() {
    for (int i = 0; i < handleCount; i++)
      ps4000aCloseUnit(handleList[i]);
  }
};

class Flyweight {
  std::map<std::string , RAII> mp;
};