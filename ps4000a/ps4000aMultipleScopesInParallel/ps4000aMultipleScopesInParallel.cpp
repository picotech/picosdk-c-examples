
#include "ps4000aApi.h"
#include "iostream"
#include "fstream"

#include "windows.h"

const int32_t NUMBER_OF_CHANNELS = 8;

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
  int32_t AutoTrigger = 30000;

  int16_t isReady;

  int32_t* timeIndisposed;
};

int main() {
  constexpr int32_t numberOfDevices = 1;
  ParallelDevice parallelDevice[numberOfDevices];

  constexpr auto NUMBER_OF_CHANNELS = 8;

  auto status2 = PICO_OK;

  // Openning
  std::cout << "Openning" << std::endl;
  {
    for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
      ParallelDevice& dev = parallelDevice[deviceNumber];
      status2 = ps4000aOpenUnit(&dev.handle, NULL);
      if (PICO_OK != status2)
        if (PICO_POWER_SUPPLY_NOT_CONNECTED == status2 || PICO_USB3_0_DEVICE_NON_USB3_0_PORT == status2)
          status2 = ps4000aChangePowerSource(dev.handle, status2);
      if (PICO_OK != status2) {
        std::cout << "PS" << deviceNumber << " has an issue on OpenUnit : " << status2 << std::endl;
        return -1;
      }
    }
  }

  // Get Max
  std::cout << "Get Max" << std::endl;
  constexpr auto INIT_MAX_ADC_VALUE = 32000;
  int16_t maxADCValue1;
  int16_t maxADCValue2;
  int16_t maxADCValue3;
  {
    for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
      maxADCValue1 = INIT_MAX_ADC_VALUE;
      ParallelDevice& dev = parallelDevice[deviceNumber];
      status2 = ps4000aMaximumValue(dev.handle,
        &dev.maxADCValue);
      if (PICO_OK != status2)
        std::cout << "PS" << deviceNumber << " has an issue on Max Value : " << status2 << std::endl;
    }
  }

  // Set Channels
  std::cout << "Set Channels" << std::endl;
  {
    for (auto ch = 0; ch < NUMBER_OF_CHANNELS; ch++) {
      for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
        ParallelDevice& dev = parallelDevice[deviceNumber];
        status2 = ps4000aSetChannel(dev.handle, static_cast<PS4000A_CHANNEL>(ch), 1, PS4000A_DC, PICO_X1_PROBE_10V, 0);
        if (PICO_OK != status2) {
          std::cout << "PS" << deviceNumber << " Set Channel : " << status2 << std::endl;
          return -1;
        }
      }
    }
  }

  // Get Timebase
  std::cout << "Get Timebase" << std::endl;
  const auto TEN_MEGA_SAMPLES = pow(10, 7);
  auto numOfSamples = TEN_MEGA_SAMPLES;
  // 12.5 ns × (n+1)
  // Sampling Frequency = 80MHz / ( n + 1 )

  // PicoScope 4824 and 4000A Series
  // Timebase(n) Sampling interval(tS) = 12.5 ns ×(n + 1)
  // Sampling frequency(fS) = 80 MHz / (n + 1)
  // 0    12.5 ns       80 MHz
  // 1    25 ns         40 MHz
  // ... ... ...
  // 2    32–1 ~54 s    ~18.6 mHz
  {
    for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
      ParallelDevice& dev = parallelDevice[deviceNumber];
      dev.timebase = 7;
      dev.noSamples = static_cast<int32_t>(TEN_MEGA_SAMPLES);
      status2 = ps4000aGetTimebase2(
        dev.handle,
        dev.timebase,
        dev.noSamples,
        &dev.timeInterval,
        &dev.maxSamples, 0);
      if (PICO_OK != status2) {
        std::cout << "PS" << deviceNumber << " Get Timebase : " << status2 << " Issue." << std::endl;
        return -1;
      }
    }
  }

  // Set Data Buffer
  std::cout << "Set Data Buffer" << std::endl;
  {
    for (auto ch = 0; ch < NUMBER_OF_CHANNELS; ch++) {
      for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
        ParallelDevice& dev = parallelDevice[deviceNumber];
        dev.buffer[ch] = (int16_t*)calloc(dev.noSamples, sizeof(int16_t));
        status2 = ps4000aSetDataBuffer(
          dev.handle,
          static_cast<PS4000A_CHANNEL>(ch),
          dev.buffer[ch],
          dev.noSamples,
          0,
          PS4000A_RATIO_MODE_NONE);
        if (PICO_OK != status2) {
          std::cout << "PS" << deviceNumber << " Set Data Buffer : " << status2 << std::endl;
          return -1;
        }
      }
    }
  }

  // Set Simple Trigger
  std::cout << "Set Simple Trigger" << std::endl;
  {
    for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
      ParallelDevice& dev = parallelDevice[deviceNumber];
      status2 = ps4000aSetSimpleTrigger(dev.handle, 1, PS4000A_CHANNEL_A, dev.AdcTrigger, PS4000A_RISING, 0, dev.AutoTrigger);
      if (PICO_OK != status2) {
        std::cout << "PS" << deviceNumber << " Trigger set Issue : " << status2 << std::endl;
        return -1;
      }
    }
  }

  // Run Block
  std::cout << "Run Block" << std::endl;
  {
    for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
      ParallelDevice& dev = parallelDevice[deviceNumber];
      dev.timeIndisposed = new int32_t(NUMBER_OF_CHANNELS);
      status2 = ps4000aRunBlock(dev.handle, 100, TEN_MEGA_SAMPLES - 100, dev.timebase, dev.timeIndisposed, 0, nullptr, nullptr);
      if (PICO_OK != status2) {
        std::cout << "PS" << deviceNumber << " Run Block : " << status2 << std::endl;
        return -1;
      }
    }


    status2 = PICO_OK;

    for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
      ParallelDevice& dev = parallelDevice[deviceNumber];

      dev.isReady = 0;
      status2 = PICO_OK;
      while (0 == dev.isReady && PICO_OK == status2) {
        status2 = ps4000aIsReady(dev.handle, &dev.isReady);
        std::cout << "PS" << deviceNumber << " IsReady : " << dev.isReady << std::endl;
        if (PICO_OK != status2) {
          std::cout << "PS" << deviceNumber << " IsReady Issue : " << status2 << std::endl;
          return -1;
        }
        Sleep(1);
      }
    }
  }

  // Get Values
  std::cout << "Get Values" << std::endl;
  {
    for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
      ParallelDevice& dev = parallelDevice[deviceNumber];

      status2 = ps4000aGetValues(dev.handle, 0, (uint32_t*)&dev.noSamples, 1, PS4000A_RATIO_MODE_NONE, 0, nullptr);
      if (PICO_OK != status2) {
        std::cout << "PS" << deviceNumber << " Get Values Issue : " << status2 << std::endl;
        return -1;
      }
    }
  }

  // Printing Values
  std::cout << "Printing Values" << std::endl;
  std::ofstream outputFile;
  outputFile.open("outputFile.txt");
  enum class encIncrementStep {
    OneUnitIncrementStep = 1,
    TenThousandIncrementStep = 10000
  };
  constexpr auto incrementStep = encIncrementStep::TenThousandIncrementStep;
  enum class encPrintStyle {
    TriggerChannelOnly = 1,
    EveryChannel = 2
  };
  constexpr auto channelPrintStyle = encPrintStyle::EveryChannel;

  constexpr auto PRINT_ONLY_EVERY_10000_SAMPLES = 10000;
  for (auto s = 0; s < 1000; s++) {
    switch (channelPrintStyle) {
    case encPrintStyle::EveryChannel: {
      for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
        ParallelDevice& dev = parallelDevice[deviceNumber];
        if (0 == deviceNumber) {
          outputFile << s << " ; " << dev.buffer[0][s];
          for (auto ch = 1; ch < NUMBER_OF_CHANNELS; ++ch)
            outputFile << " ; " << dev.buffer[ch][s];
        }
        else if (numberOfDevices - 1 == deviceNumber) {
          outputFile << "\t || \t" << dev.buffer[0][s];
          for (auto ch = 1; ch < NUMBER_OF_CHANNELS; ++ch)
            outputFile << " ; " << dev.buffer[ch][s];
          outputFile << std::endl;
        }
        else {
          outputFile << "\t || \t" << dev.buffer[0][s];
          for (auto ch = 1; ch < NUMBER_OF_CHANNELS; ++ch)
            outputFile << " ; " << dev.buffer[ch][s];
        }
      }
      break;
    }
    case encPrintStyle::TriggerChannelOnly: {
      constexpr int CHANNEL_A = 0;
      for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
        ParallelDevice& dev = parallelDevice[deviceNumber];
        if (0 == deviceNumber)
          outputFile << s << " ; " << dev.buffer[CHANNEL_A][s];
        else if (numberOfDevices - 1 == deviceNumber)
          outputFile << "\t || \t" << dev.buffer[CHANNEL_A][s] <<
          std::endl;
        else
          outputFile << "\t || \t" << dev.buffer[CHANNEL_A][s];
      }
      break;
    }
    }
  }
  for (auto s = 0; s < numOfSamples; s += static_cast<int>(incrementStep)) {
    if (0 == (s % PRINT_ONLY_EVERY_10000_SAMPLES))
      std::cout << s << std::endl;

    switch (channelPrintStyle) {
    case encPrintStyle::EveryChannel: {
      for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
        ParallelDevice& dev = parallelDevice[deviceNumber];
        if (0 == deviceNumber) {
          outputFile << s << " ; " << dev.buffer[0][s];
          for (auto ch = 1; ch < NUMBER_OF_CHANNELS; ++ch)
            outputFile << " ; " << dev.buffer[ch][s];
        }
        else if (numberOfDevices - 1 == deviceNumber) {
          outputFile << "\t || \t" << dev.buffer[0][s];
          for (auto ch = 1; ch < NUMBER_OF_CHANNELS; ++ch)
            outputFile << " ; " << dev.buffer[ch][s];
          outputFile << std::endl;
        }
        else {
          outputFile << "\t || \t" << dev.buffer[0][s];
          for (auto ch = 1; ch < NUMBER_OF_CHANNELS; ++ch)
            outputFile << " ; " << dev.buffer[ch][s];
        }
      }
      break;
    }
    case encPrintStyle::TriggerChannelOnly: {
      constexpr int CHANNEL_A = 0;
      for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
        ParallelDevice& dev = parallelDevice[deviceNumber];
        if (0 == deviceNumber)
          outputFile << s << " ; " << dev.buffer[CHANNEL_A][s];
        else if (numberOfDevices - 1 == deviceNumber)
          outputFile << "\t || \t" << dev.buffer[CHANNEL_A][s] <<
          std::endl;
        else
          outputFile << "\t || \t" << dev.buffer[CHANNEL_A][s];
      }
      break;
    }
    }
  }

  // Free Buffers
  std::cout << "Free Buffers" << std::endl;
  {
    for (auto ch = 0; ch < NUMBER_OF_CHANNELS; ch++) {
      for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
        ParallelDevice& dev = parallelDevice[deviceNumber];
        free(dev.buffer[ch]);
      }
    }
  }

  // Closing Units
  std::cout << "Closing Units" << std::endl;
  {
    for (int32_t deviceNumber = 0; deviceNumber < numberOfDevices; ++deviceNumber) {
      ParallelDevice& dev = parallelDevice[deviceNumber];
      status2 = ps4000aCloseUnit(dev.handle);
      if (PICO_OK != status2) {
        std::cout << "PS" << deviceNumber << " has an issue on Closure" << std::endl;
        return -1;
      }
    }
  }
	return 0;
}