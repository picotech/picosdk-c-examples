#pragma once

#include "ps4000aApi.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
#include <map>
#include <vector>

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


  std::vector<std::vector<int16_t>> buffer;

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
//  std::vector<int16_t> vec;
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

enum class encHandleStatus {
  ERROR_OCCURANCE = -1 ,
  NOT_SELECTED = 0
};

void getForwardFFT(
  std::vector<PICO_STATUS>& statusList,
  std::vector<int16_t>& handle_,
  std::vector<ParallelDevice>& parallelDeviceVec,
  System::Windows::Forms::Form^* Form,
  PICO_STATUS& status,
  const int32_t noOfDevices,
  const std::string triggerType) {

}

class IDevice {
public :
  IDevice() {};
  virtual ~IDevice() = default;

  virtual void PowerUp(int16_t* handle, System::String^* serials) = 0;
  // ...
  virtual void SetChannels() = 0;
  virtual void GetTimebase() = 0;
  virtual void SetTrigger() = 0;
  virtual void SetBuffers() = 0;
  virtual void RunBlock() = 0;
  virtual void IsReady() = 0;
  virtual void GetValues() = 0;
  virtual void Render() = 0;
  virtual void Shutdown() = 0;
};

class ps4000aDevice : public IDevice {
public :
  ps4000aDevice() {}
  ~ps4000aDevice() = default;
  void PowerUp(int16_t* handle , System::String^* serials) override {}
  // ...
  void SetChannels() {}
  void GetTimebase() {}
  void SetTrigger() {}
  void SetBuffers() {}
  void RunBlock() {}
  void IsReady() {}
  void GetValues() {}
  void Render() {}
  void Shutdown() {}
};


// Self Contained Trigger
void setTrigger2(
  std::vector<PICO_STATUS>& statusList,
  std::vector<int16_t>& handle_,
  std::vector<ParallelDevice>& parallelDeviceVec,
  System::Windows::Forms::Form^* Form,
  PICO_STATUS& status,
  const int32_t noOfDevices,
  const std::string triggerType,
  IDevice* dev) {
  std::cout << "Set the Trigger" << std::endl;

  if (nullptr == Form) {
    std::cout << "Form is Empty" << std::endl;
    return;
  }
  IDevice* dev2 = new ps4000aDevice();
  {
    for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
      // Check if the device is selected and is not failed
      if (PICO_OK != statusList[deviceNumber] || !handle_[deviceNumber])
        continue;

      ParallelDevice& dev = parallelDeviceVec[deviceNumber];

      if (triggerType == "Simple") {
        auto minThresholdsInput = (System::Windows::Forms::TextBox^)(* Form)->Controls["minThreshold"];
        int32_t minThresholds = System::Int32::Parse(minThresholdsInput->Text);
        dev.AdcTrigger = minThresholds;

        status = ps4000aSetSimpleTrigger(dev.handle, 1, PS4000A_CHANNEL_A, dev.AdcTrigger, PS4000A_RISING, 0, dev.AutoTrigger);
        if (PICO_OK != status) {
          std::cout << "PS" << deviceNumber << " Trigger set Issue : " << status << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => Simple Trigger Error : " + status;
          statusList[deviceNumber] = status;
        }
      }
      else if (triggerType == "Pulse Width") {
        /*
         * HOW THIS TRIGGER WORKS :
         * The trigger is performed on an 'AND' operation of Timing and Trigger Status.
         *  - PULSE_WIDTH functions set the conditions for reseting the timer to '0'
         *  - The timer is incremented by '1' at each sample count.
         *  - At the moment that the Triggering Condition occurs , the timer is checked if it is in the set boundaries
         *    Hence the 'AND' operation ( Is the Trigger condition valid ? Is the Timer within the set boundaries ? If both are right , then perform a trigger. )
         * Result : The trigger would only be performed if a timer reset has has been performed at the latest within the set range AND the triggering conditions have happened.
         *
         * Procedure :
         *  1) Set the Triggering Conditions
         *  2) Set the Timer Reset Conditions
         *
         * Alternative :
         *  - One could customize this trigger to capture when the phase shift between two channels is within a selected range
         */


         // 1) Set the Triggering Conditions
        auto minThresholdsInput = (System::Windows::Forms::TextBox^)(*Form)->Controls["minThreshold"];
        int32_t minThresholds = System::Int32::Parse(minThresholdsInput->Text);
        dev.AdcTrigger = minThresholds;

        PS4000A_CONDITION tCond[2];
        tCond[0].source = PS4000A_CHANNEL_A;
        tCond[0].condition = PS4000A_CONDITION_TRUE;
        tCond[1].source = PS4000A_PULSE_WIDTH_SOURCE;
        tCond[1].condition = PS4000A_CONDITION_TRUE;
        int input = PS4000A_CLEAR | PS4000A_ADD;
        status = ps4000aSetTriggerChannelConditions(dev.handle, &tCond[0], 2, (PS4000A_CONDITIONS_INFO)input);
        if (status != PICO_OK) {
          std::cout << "SETUP TRIGGER ERROR 1" << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => Trigger Condition Error : " + status;
          statusList[deviceNumber] = status;
        }

        PS4000A_DIRECTION tDir;
        tDir.direction = PS4000A_FALLING;
        tDir.channel = PS4000A_CHANNEL_A;
        status = ps4000aSetTriggerChannelDirections(dev.handle,
          &tDir, 1);
        if (status != PICO_OK) {
          std::cout << "SETUP TRIGGER ERROR 2" << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => Trigger Direction Error : " + status;
          statusList[deviceNumber] = status;
        }


        auto minHysteresisInput = (System::Windows::Forms::TextBox^)(*Form)->Controls["MinHysteresisInput"];
        int32_t minHysteresis = System::Int32::Parse(minHysteresisInput->Text);
        auto maxHysteresisInput = (System::Windows::Forms::TextBox^)(*Form)->Controls["MaxHysteresisInput"];
        int32_t maxHysteresis = System::Int32::Parse(maxHysteresisInput->Text);

        PS4000A_TRIGGER_CHANNEL_PROPERTIES tProp;
        tProp.channel = PS4000A_CHANNEL_A;
        tProp.thresholdMode = PS4000A_LEVEL;
        tProp.thresholdUpper = dev.AdcTrigger;
        tProp.thresholdUpperHysteresis = maxHysteresis;
        tProp.thresholdLower = dev.AdcTrigger;
        tProp.thresholdLowerHysteresis = minHysteresis;

        PS4000A_TRIGGER_CHANNEL_PROPERTIES tProp2[1];
        tProp2[0] = tProp;

        status = ps4000aSetTriggerChannelProperties(dev.handle, &tProp2[0], 1, 0, 5000);
        if (status != PICO_OK) {
          std::cout << "SETUP TRIGGER ERROR 3" << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => Trigger Properties Error : " + status;
          statusList[deviceNumber] = status;
        }

        PS4000A_PULSE_WIDTH_TYPE pulseType;
        int32_t minPulse = 0;
        int32_t maxPulse = 0;
        int pulseFlags = 0;

        auto minPulseInput = (System::Windows::Forms::TextBox^)(*Form)->Controls["MinPulseWidthInput"];
        if ("" != minPulseInput->Text) {
          pulseFlags |= 1 << 0;
          minPulse = System::Int32::Parse(minPulseInput->Text);
        }
        auto maxPulseInput = (System::Windows::Forms::TextBox^)(*Form)->Controls["MaxPulseWidthInput"];
        if ("" != maxPulseInput->Text) {
          pulseFlags |= 1 << 1;
          maxPulse = System::Int32::Parse(maxPulseInput->Text);
        }

        uint32_t minPulseWidth;
        uint32_t maxPulseWidth;
        switch (pulseFlags) {
        case 1:
          pulseType = PS4000A_PULSE_WIDTH_TYPE::PS4000A_PW_TYPE_GREATER_THAN;
          minPulseWidth = minPulse;
          break;
        case 2:
          pulseType = PS4000A_PULSE_WIDTH_TYPE::PS4000A_PW_TYPE_LESS_THAN;
          minPulseWidth = maxPulse;
          break;
        case 3:
          pulseType = PS4000A_PULSE_WIDTH_TYPE::PS4000A_PW_TYPE_IN_RANGE;
          minPulseWidth = minPulse;
          maxPulseWidth = maxPulse;
          break;
        }
        if (PS4000A_PULSE_WIDTH_TYPE::PS4000A_PW_TYPE_IN_RANGE == pulseType) {
          minPulseWidth = minPulse;
          maxPulseWidth = maxPulse;
        }

#pragma pack(1)

        PS4000A_PWQ_CONDITIONS  pwqConditions[] =
        {
          {
            PS4000A_CONDITION_TRUE,      // enable pulse width trigger on channel A
            PS4000A_CONDITION_DONT_CARE, // channel B
            PS4000A_CONDITION_DONT_CARE, // channel C
            PS4000A_CONDITION_DONT_CARE, // channel D
            PS4000A_CONDITION_DONT_CARE, // external
            PS4000A_CONDITION_DONT_CARE  // aux
          }
        };

        // 2) Set the Timer Reset Conditions
        PS4000A_CONDITION pwqCond[10];
        pwqCond[0].source = PS4000A_CHANNEL_A;
        pwqCond[0].condition = PS4000A_CONDITION_TRUE;
        for (int i = 1; i < 10; i++) {
          pwqCond[i].source = (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + i);
          pwqCond[i].condition = PS4000A_CONDITION_DONT_CARE;
        }
        std::cout << "Handle : " << dev.handle << std::endl;
        status = ps4000aSetPulseWidthQualifierConditions(
          dev.handle,              // device handle
          pwqCond,                 // pointer to condition structure
          10,                      // number of structures
          (PS4000A_CONDITIONS_INFO)input     // N/A for window trigger
        );
        if (status != PICO_OK)
        {
          std::cout << "Set pulse width qualifier Conditions failed: err = " << status << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => PWQ Condition Error : " + status;
          statusList[deviceNumber] = status;
        }
        status = ps4000aSetPulseWidthQualifierProperties(
          dev.handle,      // device handle
          PS4000A_BELOW,
          minPulseWidth,   // pointer to condition structure
          maxPulseWidth,   // number of structures
          pulseType        // N/A for window trigger
        );
        if (status != PICO_OK)
        {
          std::cout << "Set pulse width qualifier Properties failed: err = " << status << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => PWQ Properties Error : " + status;
          statusList[deviceNumber] = status;
        }
      }
      else if (triggerType == "Drop Out") {
        /*
         * HOW THIS TRIGGER WORKS :
         * The trigger is performed only based on a timer.
         *  - PULSE_WIDTH functions set the conditions for reseting the timer to '0'
         *  - The timer is incremented by '1' and checked at each sample count.
         *  - Whenever the timer is checked within the set boudaries , then perform a trigger.
         * Result : The trigger would only be performed if a timer reset has been performed at the latest within of the set range.
         *
         * Procedure :
         *  1) Set the Triggering Conditions
         *  2) Set the Timer Reset Conditions
         */

         // 1) Set the Triggering Conditions
        auto minThresholdsInput = (System::Windows::Forms::TextBox^)(*Form)->Controls["minThreshold"];
        int32_t minThresholds = System::Int32::Parse(minThresholdsInput->Text);
        dev.AdcTrigger = minThresholds;

        status = ps4000aSetSimpleTrigger(dev.handle, 1, PS4000A_CHANNEL_A, dev.AdcTrigger, PS4000A_RISING, 0, dev.AutoTrigger);
        if (PICO_OK != status) {
          std::cout << "PS" << deviceNumber << " Trigger set Issue : " << status << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => Simple Trigger Error : " + status;
          statusList[deviceNumber] = status;
        }

        PS4000A_CONDITION tCond[1];
        tCond[0].source = PS4000A_PULSE_WIDTH_SOURCE;
        tCond[0].condition = PS4000A_CONDITION_TRUE;
        int input = PS4000A_CLEAR | PS4000A_ADD;
        status = ps4000aSetTriggerChannelConditions(dev.handle, &tCond[0], 1, (PS4000A_CONDITIONS_INFO)input);
        if (status != PICO_OK) {
          std::cout << "SETUP TRIGGER ERROR 1" << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => Trigger Conditions Error : " + status;
          statusList[deviceNumber] = status;
        }

        PS4000A_PULSE_WIDTH_TYPE pulseType;
        int32_t minPulse = 0;
        int32_t maxPulse = 0;
        int pulseFlags = 0;

        auto minPulseInput = (System::Windows::Forms::TextBox^)(*Form)->Controls["MinPulseWidthInput"];
        if ("" != minPulseInput->Text) {
          pulseFlags |= 1 << 0;
          minPulse = System::Int32::Parse(minPulseInput->Text);
        }
        auto maxPulseInput = (System::Windows::Forms::TextBox^)(*Form)->Controls["MaxPulseWidthInput"];
        if ("" != maxPulseInput->Text) {
          pulseFlags |= 1 << 1;
          maxPulse = System::Int32::Parse(maxPulseInput->Text);
        }

        uint32_t minPulseWidth = 0;
        uint32_t maxPulseWidth = 0;
        switch (pulseFlags) {
        case 1:
          pulseType = PS4000A_PULSE_WIDTH_TYPE::PS4000A_PW_TYPE_GREATER_THAN;
          minPulseWidth = minPulse;
          break;
        case 2:
          pulseType = PS4000A_PULSE_WIDTH_TYPE::PS4000A_PW_TYPE_LESS_THAN;
          minPulseWidth = maxPulse;
          break;
        case 3:
          pulseType = PS4000A_PULSE_WIDTH_TYPE::PS4000A_PW_TYPE_IN_RANGE;
          minPulseWidth = minPulse;
          maxPulseWidth = maxPulse;
          break;
        }

#pragma pack(1)

        // 2) Set the Timer Reset Conditions
        PS4000A_CONDITION pwqCond[10];
        pwqCond[0].source = PS4000A_CHANNEL_A;
        pwqCond[0].condition = PS4000A_CONDITION_TRUE;
        for (int i = 1; i < 10; i++) {
          pwqCond[i].source = (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + i);
          pwqCond[i].condition = PS4000A_CONDITION_DONT_CARE;
        }
        std::cout << "Handle : " << dev.handle << std::endl;
        status = ps4000aSetPulseWidthQualifierConditions(
          dev.handle,              // device handle
          pwqCond,                 // pointer to condition structure
          10,                      // number of structures
          (PS4000A_CONDITIONS_INFO)input       // N/A for window trigger
        );
        if (status != PICO_OK)
        {
          std::cout << "Set pulse width qualifier Conditions failed: err = " << status << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => PWQ Conditions Error : " + status;
          statusList[deviceNumber] = status;
        }

        status = ps4000aSetPulseWidthQualifierProperties(
          dev.handle,              // device handle
          PS4000A_BELOW_LOWER,
          minPulseWidth,           // pointer to condition structure
          maxPulseWidth,           // number of structures
          pulseType                // N/A for window trigger
        );
        if (status != PICO_OK)
        {
          std::cout << "Set pulse width qualifier Properties failed: err = " << status << std::endl;
          auto label = (System::Windows::Forms::Label^)(*Form)->Controls["Label " + deviceNumber];
          label->Text += " => PWQ Properties Error : " + status;
          statusList[deviceNumber] = status;
        }
      }
    }
  }
}