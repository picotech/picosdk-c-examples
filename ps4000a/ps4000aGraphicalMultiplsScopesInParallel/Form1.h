#pragma once

#include "structImport.h"
#include "ps4000aApi.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <array>
#include <string>
#include <vector>
#include <thread>

#include <sstream>
//#include <msclr/marshal_cppstd.h>
//using namespace System::Runtime::InteropServices;

//#include "windows.h"

namespace CppCLRWinformsProjekt {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;


	

	/// <summary>
	/// Zusammenfassung für Form1
	/// </summary>
	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();
			//
			//TODO: Konstruktorcode hier hinzufügen.
			//
		}

	protected:
		/// <summary>
		/// Verwendete Ressourcen bereinigen.
		/// </summary>
		~Form1()
		{
			if (components)
			{
				delete components;
			}
		}
  private: System::Windows::Forms::Button^ Execute;
  protected:

	protected:


	private: System::Windows::Forms::TextBox^ textBox1;
	private: System::Windows::Forms::Label^ label1;
  private: System::Windows::Forms::Button^ ListAllDevices;
  private: System::Windows::Forms::Button^ SelectDevices;
  private: int32_t count = 0;
  private: int16_t* handle; 
  private: ParallelDevice* parallelDevice;
  private: System::Windows::Forms::DataVisualization::Charting::Chart^ chart1;
  private: System::Windows::Forms::TextBox^ TimebaseInput;
  private: System::Windows::Forms::Label^ TimebaseText;
  private: System::Windows::Forms::Label^ BufferSizeText;
  private: System::Windows::Forms::TextBox^ BufferSizeInput;

  private: System::Windows::Forms::Label^ TrigerTypeText;


  private: System::Windows::Forms::Label^ MinMaxPulseWidth;

  private: System::Windows::Forms::TextBox^ MinPulseWidthInput;
  private: System::Windows::Forms::Label^ MinMaxThresholds;
  private: System::Windows::Forms::TextBox^ minThreshold;
  private: System::Windows::Forms::TextBox^ MaxPulseWidthInput;
  private: System::Windows::Forms::TextBox^ maxThreshold;
  private: System::Windows::Forms::TextBox^ MaxHysteresisInput;


  private: System::Windows::Forms::Label^ MinMaxHysteresis;
  private: System::Windows::Forms::TextBox^ MinHysteresisInput;
  private: System::Windows::Forms::Button^ Stop;
  private: System::Windows::Forms::ComboBox^ TriggerTypeInput;

	private:
		/// <summary>
		/// Erforderliche Designervariable.
		/// </summary>
		System::ComponentModel::Container ^components;

    #pragma region Windows Form Designer generated code
		/// <summary>
		/// Erforderliche Methode für die Designerunterstützung.
		/// Der Inhalt der Methode darf nicht mit dem Code-Editor geändert werden.
		/// </summary>
		void InitializeComponent(void)
		{
      this->Execute = (gcnew System::Windows::Forms::Button());
      this->textBox1 = (gcnew System::Windows::Forms::TextBox());
      this->label1 = (gcnew System::Windows::Forms::Label());
      this->ListAllDevices = (gcnew System::Windows::Forms::Button());
      this->SelectDevices = (gcnew System::Windows::Forms::Button());
      this->TimebaseInput = (gcnew System::Windows::Forms::TextBox());
      this->TimebaseText = (gcnew System::Windows::Forms::Label());
      this->BufferSizeText = (gcnew System::Windows::Forms::Label());
      this->BufferSizeInput = (gcnew System::Windows::Forms::TextBox());
      this->TrigerTypeText = (gcnew System::Windows::Forms::Label());
      this->MinMaxPulseWidth = (gcnew System::Windows::Forms::Label());
      this->MinPulseWidthInput = (gcnew System::Windows::Forms::TextBox());
      this->MinMaxThresholds = (gcnew System::Windows::Forms::Label());
      this->minThreshold = (gcnew System::Windows::Forms::TextBox());
      this->MaxPulseWidthInput = (gcnew System::Windows::Forms::TextBox());
      this->maxThreshold = (gcnew System::Windows::Forms::TextBox());
      this->MaxHysteresisInput = (gcnew System::Windows::Forms::TextBox());
      this->MinMaxHysteresis = (gcnew System::Windows::Forms::Label());
      this->MinHysteresisInput = (gcnew System::Windows::Forms::TextBox());
      this->Stop = (gcnew System::Windows::Forms::Button());
      this->TriggerTypeInput = (gcnew System::Windows::Forms::ComboBox());
      this->SuspendLayout();
      // 
      // Execute
      // 
      this->Execute->Location = System::Drawing::Point(961, 24);
      this->Execute->Name = L"Execute";
      this->Execute->Size = System::Drawing::Size(87, 48);
      this->Execute->TabIndex = 2;
      this->Execute->Text = L"Execute";
      this->Execute->UseVisualStyleBackColor = true;
      this->Execute->Click += gcnew System::EventHandler(this, &Form1::button1_Click);
      // 
      // textBox1
      // 
      this->textBox1->Location = System::Drawing::Point(961, 79);
      this->textBox1->Name = L"textBox1";
      this->textBox1->Size = System::Drawing::Size(100, 26);
      this->textBox1->TabIndex = 3;
      this->textBox1->TextChanged += gcnew System::EventHandler(this, &Form1::textBox1_TextChanged);
      // 
      // label1
      // 
      this->label1->AutoSize = true;
      this->label1->Location = System::Drawing::Point(1066, 41);
      this->label1->Name = L"label1";
      this->label1->Size = System::Drawing::Size(51, 20);
      this->label1->TabIndex = 4;
      this->label1->Text = L"label1";
      this->label1->Click += gcnew System::EventHandler(this, &Form1::label1_Click);
      // 
      // ListAllDevices
      // 
      this->ListAllDevices->Location = System::Drawing::Point(407, 3);
      this->ListAllDevices->Name = L"ListAllDevices";
      this->ListAllDevices->Size = System::Drawing::Size(148, 32);
      this->ListAllDevices->TabIndex = 5;
      this->ListAllDevices->Text = L"List Devices";
      this->ListAllDevices->UseVisualStyleBackColor = true;
      this->ListAllDevices->Click += gcnew System::EventHandler(this, &Form1::FindDevices_Click);
      // 
      // SelectDevices
      // 
      this->SelectDevices->Location = System::Drawing::Point(407, 41);
      this->SelectDevices->Name = L"SelectDevices";
      this->SelectDevices->Size = System::Drawing::Size(148, 31);
      this->SelectDevices->TabIndex = 6;
      this->SelectDevices->Text = L"Select Devices";
      this->SelectDevices->UseVisualStyleBackColor = true;
      this->SelectDevices->Click += gcnew System::EventHandler(this, &Form1::SelectDevices_Click);
      // 
      // TimebaseInput
      // 
      this->TimebaseInput->Location = System::Drawing::Point(750, 14);
      this->TimebaseInput->Name = L"TimebaseInput";
      this->TimebaseInput->Size = System::Drawing::Size(72, 26);
      this->TimebaseInput->TabIndex = 7;
      this->TimebaseInput->Text = L"7";
      this->TimebaseInput->TextChanged += gcnew System::EventHandler(this, &Form1::TimebaseInput_TextChanged);
      // 
      // TimebaseText
      // 
      this->TimebaseText->AutoSize = true;
      this->TimebaseText->Location = System::Drawing::Point(571, 20);
      this->TimebaseText->Name = L"TimebaseText";
      this->TimebaseText->Size = System::Drawing::Size(78, 20);
      this->TimebaseText->TabIndex = 8;
      this->TimebaseText->Text = L"Timebase";
      // 
      // BufferSizeText
      // 
      this->BufferSizeText->AutoSize = true;
      this->BufferSizeText->Location = System::Drawing::Point(571, 52);
      this->BufferSizeText->Name = L"BufferSizeText";
      this->BufferSizeText->Size = System::Drawing::Size(88, 20);
      this->BufferSizeText->TabIndex = 10;
      this->BufferSizeText->Text = L"Buffer Size";
      this->BufferSizeText->Click += gcnew System::EventHandler(this, &Form1::label2_Click);
      // 
      // BufferSizeInput
      // 
      this->BufferSizeInput->Location = System::Drawing::Point(750, 46);
      this->BufferSizeInput->Name = L"BufferSizeInput";
      this->BufferSizeInput->Size = System::Drawing::Size(72, 26);
      this->BufferSizeInput->TabIndex = 9;
      this->BufferSizeInput->Text = L"500";
      this->BufferSizeInput->TextChanged += gcnew System::EventHandler(this, &Form1::textBox2_TextChanged);
      // 
      // TrigerTypeText
      // 
      this->TrigerTypeText->AutoSize = true;
      this->TrigerTypeText->Location = System::Drawing::Point(571, 85);
      this->TrigerTypeText->Name = L"TrigerTypeText";
      this->TrigerTypeText->Size = System::Drawing::Size(96, 20);
      this->TrigerTypeText->TabIndex = 12;
      this->TrigerTypeText->Text = L"Trigger Type";
      this->TrigerTypeText->Click += gcnew System::EventHandler(this, &Form1::label2_Click_1);
      // 
      // MinMaxPulseWidth
      // 
      this->MinMaxPulseWidth->AutoSize = true;
      this->MinMaxPulseWidth->Location = System::Drawing::Point(571, 117);
      this->MinMaxPulseWidth->Name = L"MinMaxPulseWidth";
      this->MinMaxPulseWidth->Size = System::Drawing::Size(163, 20);
      this->MinMaxPulseWidth->TabIndex = 14;
      this->MinMaxPulseWidth->Text = L"Min / Max Pulse Width";
      this->MinMaxPulseWidth->Click += gcnew System::EventHandler(this, &Form1::label2_Click_2);
      // 
      // MinPulseWidthInput
      // 
      this->MinPulseWidthInput->Enabled = false;
      this->MinPulseWidthInput->Location = System::Drawing::Point(750, 111);
      this->MinPulseWidthInput->Name = L"MinPulseWidthInput";
      this->MinPulseWidthInput->Size = System::Drawing::Size(72, 26);
      this->MinPulseWidthInput->TabIndex = 13;
      this->MinPulseWidthInput->Text = L"40";
      this->MinPulseWidthInput->TextChanged += gcnew System::EventHandler(this, &Form1::textBox2_TextChanged_2);
      // 
      // MinMaxThresholds
      // 
      this->MinMaxThresholds->AutoSize = true;
      this->MinMaxThresholds->Location = System::Drawing::Point(571, 151);
      this->MinMaxThresholds->Name = L"MinMaxThresholds";
      this->MinMaxThresholds->Size = System::Drawing::Size(157, 20);
      this->MinMaxThresholds->TabIndex = 16;
      this->MinMaxThresholds->Text = L"Min / Max Thresholds";
      // 
      // minThreshold
      // 
      this->minThreshold->Location = System::Drawing::Point(750, 145);
      this->minThreshold->Name = L"minThreshold";
      this->minThreshold->Size = System::Drawing::Size(72, 26);
      this->minThreshold->TabIndex = 15;
      this->minThreshold->Text = L"10000";
      this->minThreshold->TextChanged += gcnew System::EventHandler(this, &Form1::minThreshold_TextChanged);
      // 
      // MaxPulseWidthInput
      // 
      this->MaxPulseWidthInput->Enabled = false;
      this->MaxPulseWidthInput->Location = System::Drawing::Point(828, 111);
      this->MaxPulseWidthInput->Name = L"MaxPulseWidthInput";
      this->MaxPulseWidthInput->Size = System::Drawing::Size(72, 26);
      this->MaxPulseWidthInput->TabIndex = 17;
      this->MaxPulseWidthInput->Text = L"60";
      // 
      // maxThreshold
      // 
      this->maxThreshold->Location = System::Drawing::Point(828, 143);
      this->maxThreshold->Name = L"maxThreshold";
      this->maxThreshold->Size = System::Drawing::Size(72, 26);
      this->maxThreshold->TabIndex = 18;
      // 
      // MaxHysteresisInput
      // 
      this->MaxHysteresisInput->Location = System::Drawing::Point(828, 176);
      this->MaxHysteresisInput->Name = L"MaxHysteresisInput";
      this->MaxHysteresisInput->Size = System::Drawing::Size(72, 26);
      this->MaxHysteresisInput->TabIndex = 21;
      this->MaxHysteresisInput->Text = L"1";
      this->MaxHysteresisInput->TextChanged += gcnew System::EventHandler(this, &Form1::textBox2_TextChanged_3);
      // 
      // MinMaxHysteresis
      // 
      this->MinMaxHysteresis->AutoSize = true;
      this->MinMaxHysteresis->Location = System::Drawing::Point(571, 184);
      this->MinMaxHysteresis->Name = L"MinMaxHysteresis";
      this->MinMaxHysteresis->Size = System::Drawing::Size(153, 20);
      this->MinMaxHysteresis->TabIndex = 20;
      this->MinMaxHysteresis->Text = L"Min / Max Hysteresis";
      this->MinMaxHysteresis->Click += gcnew System::EventHandler(this, &Form1::label2_Click_3);
      // 
      // MinHysteresisInput
      // 
      this->MinHysteresisInput->Location = System::Drawing::Point(750, 178);
      this->MinHysteresisInput->Name = L"MinHysteresisInput";
      this->MinHysteresisInput->Size = System::Drawing::Size(72, 26);
      this->MinHysteresisInput->TabIndex = 19;
      this->MinHysteresisInput->Text = L"1";
      this->MinHysteresisInput->TextChanged += gcnew System::EventHandler(this, &Form1::textBox3_TextChanged);
      // 
      // Stop
      // 
      this->Stop->Location = System::Drawing::Point(961, 117);
      this->Stop->Name = L"Stop";
      this->Stop->Size = System::Drawing::Size(87, 48);
      this->Stop->TabIndex = 22;
      this->Stop->Text = L"STOP";
      this->Stop->UseVisualStyleBackColor = true;
      this->Stop->Click += gcnew System::EventHandler(this, &Form1::Stop_Click);
      // 
      // TriggerTypeInput
      // 
      this->TriggerTypeInput->FormattingEnabled = true;
      this->TriggerTypeInput->Items->AddRange(gcnew cli::array< System::Object^  >(4) { L"None", L"Simple", L"Pulse Width", L"Drop Out" });
      this->TriggerTypeInput->Location = System::Drawing::Point(750, 77);
      this->TriggerTypeInput->Name = L"TriggerTypeInput";
      this->TriggerTypeInput->Size = System::Drawing::Size(121, 28);
      this->TriggerTypeInput->TabIndex = 23;
      this->TriggerTypeInput->Text = L"Simple";
      this->TriggerTypeInput->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::comboBox1_SelectedIndexChanged);
      // 
      // Form1
      // 
      this->AutoScaleDimensions = System::Drawing::SizeF(9, 20);
      this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
      this->ClientSize = System::Drawing::Size(1144, 857);
      this->Controls->Add(this->TriggerTypeInput);
      this->Controls->Add(this->Stop);
      this->Controls->Add(this->MaxHysteresisInput);
      this->Controls->Add(this->MinMaxHysteresis);
      this->Controls->Add(this->MinHysteresisInput);
      this->Controls->Add(this->maxThreshold);
      this->Controls->Add(this->MaxPulseWidthInput);
      this->Controls->Add(this->MinMaxThresholds);
      this->Controls->Add(this->minThreshold);
      this->Controls->Add(this->MinMaxPulseWidth);
      this->Controls->Add(this->MinPulseWidthInput);
      this->Controls->Add(this->TrigerTypeText);
      this->Controls->Add(this->BufferSizeText);
      this->Controls->Add(this->BufferSizeInput);
      this->Controls->Add(this->TimebaseText);
      this->Controls->Add(this->TimebaseInput);
      this->Controls->Add(this->SelectDevices);
      this->Controls->Add(this->ListAllDevices);
      this->Controls->Add(this->label1);
      this->Controls->Add(this->textBox1);
      this->Controls->Add(this->Execute);
      this->Name = L"Form1";
      this->Text = L"Form1";
      this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
      this->ResumeLayout(false);
      this->PerformLayout();

    }
    #pragma endregion
	  private: System::Void chart1_Click(System::Object^ sender, System::EventArgs^ e) {
	  }
	  private: System::Void dataGridView1_CellContentClick(System::Object^ sender, System::Windows::Forms::DataGridViewCellEventArgs^ e) {
	  }
	  private: System::Void button1_Click(System::Object^ sender, System::EventArgs^ e) {
    
	    //	int16_t handle;
      int32_t noOfDevices = this->count;

      auto timebaseInput = (System::Windows::Forms::TextBox^)this->Controls["TimebaseInput"];
      int32_t timebase = System::Int32::Parse(timebaseInput->Text);

      auto bufferSizeInput = (System::Windows::Forms::TextBox^)this->Controls["BufferSizeInput"];
      int32_t bufferSize = System::Int32::Parse(bufferSizeInput->Text);

      auto triggerTypeInput = (System::Windows::Forms::ComboBox^)this->Controls["TriggerTypeInput"];
      std::string triggerType;
      //  std::vector<char> triggerType; // new int8_t[10];
      for (auto j = 0; j < triggerTypeInput->Text->Length; ++j) {
        triggerType.push_back((char)triggerTypeInput->Text[j]);
      }
      // triggerType.push_back('\0');
    
      {
      
        RAII raii = RAII();
        parallelDevice = new ParallelDevice[noOfDevices];

        for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
          ParallelDevice& dev = parallelDevice[deviceNumber];
          dev.handle = this->handle[deviceNumber];
          if(dev.handle == -2)
            raii.Add(dev.handle);
        }
      
        constexpr auto NUMBER_OF_CHANNELS = 8;

        auto status2 = PICO_OK;

        {
          for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
            status2 = PICO_OK;
            ParallelDevice& dev = parallelDevice[deviceNumber];
            status2 = ps4000aPingUnit(dev.handle);
            if (PICO_OK != status2) {
              //PICO_STATUS status = ps4000aOpenUnit(&this->handle[i], nullptr);// /*res.data()*/);
              //if (PICO_OK != status)
              //  if (PICO_POWER_SUPPLY_NOT_CONNECTED == status || PICO_USB3_0_DEVICE_NON_USB3_0_PORT == status)
              //    status = ps4000aChangePowerSource(this->handle[i], status);
              //if (PICO_OK != status) {
              //  std::cout << "PS" << i << " has an issue on OpenUnit : " << status << std::endl;
              //  //return;
              //}
            }
          }
        }

        // Get Max
        std::cout << "Get Max" << std::endl;
        constexpr auto INIT_MAX_ADC_VALUE = 32000;
        {
          for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
            ParallelDevice& dev = parallelDevice[deviceNumber];
            dev.maxADCValue = INIT_MAX_ADC_VALUE;
            status2 = ps4000aMaximumValue(dev.handle,
              &dev.maxADCValue);
            if (PICO_OK != status2)
              std::cout << "PS" << deviceNumber << " has an issue on Max Value : " << status2 << std::endl;
          }
        }

        // Set Channels
        std::cout << "Set Channels" << std::endl;
        {
          for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
            for (auto ch = 0; ch < NUMBER_OF_CHANNELS; ch++) {
              ParallelDevice& dev = parallelDevice[deviceNumber];
              status2 = ps4000aSetChannel(dev.handle, static_cast<PS4000A_CHANNEL>(ch), 1, PS4000A_DC, PICO_X1_PROBE_1V, 0);
              if (PICO_OK != status2) {
                std::cout << "PS" << deviceNumber << " Set Channel : " << status2 << std::endl;
              //  return;
              }
            }
          }
        }

        // Get Timebase
        std::cout << "Get Timebase" << std::endl;
        const int TEN_MEGA_SAMPLES = bufferSize; //  pow(10, 7);
        const int PRE_TRIGGER = TEN_MEGA_SAMPLES / 2;
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
          for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
            ParallelDevice& dev = parallelDevice[deviceNumber];
            dev.timebase = timebase; // 7
            dev.noSamples = static_cast<int32_t>(TEN_MEGA_SAMPLES);
            status2 = ps4000aGetTimebase2(
              dev.handle,
              dev.timebase,
              dev.noSamples,
              &dev.timeInterval,
              &dev.maxSamples, 0);
            if (PICO_OK != status2) {
              std::cout << "PS" << deviceNumber << " Get Timebase : " << status2 << " Issue." << std::endl;
            //  return;
            }
          }
        }

        // Set Data Buffer
        std::cout << "Set Data Buffer" << std::endl;
        {
          for (auto ch = 0; ch < NUMBER_OF_CHANNELS; ch++) {
            for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
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
              //  return;
              }
            }
          }
        }

        // Set Simple Trigger
        std::cout << "Set Simple Trigger" << std::endl;
        {
          for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
            ParallelDevice& dev = parallelDevice[deviceNumber];

            if(triggerType == "Simple") {

              auto minThresholdsInput = (System::Windows::Forms::TextBox^)this->Controls["minThreshold"];
              int32_t minThresholds = System::Int32::Parse(minThresholdsInput->Text);
              dev.AdcTrigger = minThresholds;

              status2 = ps4000aSetSimpleTrigger(dev.handle, 1, PS4000A_CHANNEL_A, dev.AdcTrigger, PS4000A_RISING, 0, dev.AutoTrigger);
              if (PICO_OK != status2) {
                std::cout << "PS" << deviceNumber << " Trigger set Issue : " << status2 << std::endl;
              //  return;
              }
            }
            else if (triggerType == "Pulse Width") {

              auto minThresholdsInput = (System::Windows::Forms::TextBox^)this->Controls["minThreshold"];
              int32_t minThresholds = System::Int32::Parse(minThresholdsInput->Text);
              dev.AdcTrigger = minThresholds;

              PS4000A_CONDITION tCond[2];
              tCond[0].source = PS4000A_CHANNEL_A;
              tCond[0].condition = PS4000A_CONDITION_TRUE;
              tCond[1].source = PS4000A_PULSE_WIDTH_SOURCE;
              tCond[1].condition = PS4000A_CONDITION_TRUE;
              int input = PS4000A_CLEAR | PS4000A_ADD;
              status2 = ps4000aSetTriggerChannelConditions(dev.handle, &tCond[0], 2, (PS4000A_CONDITIONS_INFO)input);
              if (status2 != PICO_OK) {
                std::cout << "SETUP TRIGGER ERROR 1" << std::endl;
              }

              PS4000A_DIRECTION tDir;
              tDir.direction = PS4000A_FALLING;
              tDir.channel = PS4000A_CHANNEL_A;
              status2 = ps4000aSetTriggerChannelDirections(dev.handle,
                &tDir, 1);
              if (status2 != PICO_OK) {
                std::cout << "SETUP TRIGGER ERROR 2" << std::endl;
              }


              auto minHysteresisInput = (System::Windows::Forms::TextBox^)this->Controls["MinHysteresisInput"];
              int32_t minHysteresis = System::Int32::Parse(minHysteresisInput->Text);
              auto maxHysteresisInput = (System::Windows::Forms::TextBox^)this->Controls["MaxHysteresisInput"];
              int32_t maxHysteresis = System::Int32::Parse(maxHysteresisInput->Text);
            //  dev.AdcTrigger = minHysteresisInput;
              PS4000A_TRIGGER_CHANNEL_PROPERTIES tProp;
              tProp.channel = PS4000A_CHANNEL_A;
              tProp.thresholdMode = PS4000A_LEVEL;
              tProp.thresholdUpper = dev.AdcTrigger; // 1000
              tProp.thresholdUpperHysteresis = maxHysteresis;
              tProp.thresholdLower = dev.AdcTrigger; // -1000
              tProp.thresholdLowerHysteresis = minHysteresis;

              PS4000A_TRIGGER_CHANNEL_PROPERTIES tProp2[1];
              tProp2[0] = tProp;

              status2 = ps4000aSetTriggerChannelProperties(dev.handle, &tProp2[0], 1, 0, 5000);
              if (status2 != PICO_OK) {
                std::cout << "SETUP TRIGGER ERROR 3" << std::endl;
              }

              PS4000A_PULSE_WIDTH_TYPE pulseType;
              int32_t minPulse = 0;
              int32_t maxPulse = 0;
              int pulseFlags = 0;

              auto minPulseInput = (System::Windows::Forms::TextBox^)this->Controls["MinPulseWidthInput"];
              if ("" != minPulseInput->Text) {
                pulseFlags |= 1;
                minPulse = System::Int32::Parse(minPulseInput->Text);
              }
              auto maxPulseInput = (System::Windows::Forms::TextBox^)this->Controls["MaxPulseWidthInput"];
              if ("" != maxPulseInput->Text) {
                pulseFlags |= 2;
                maxPulse = System::Int32::Parse(maxPulseInput->Text);
              }
            //  pulseType = (PS4000A_PULSE_WIDTH_TYPE)pulseFlags;

              uint32_t minPulseWidth; // = uint32_t(minPulse); // 647
              uint32_t maxPulseWidth; // = uint32_t(maxPulse); // 647
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
              PS4000A_CONDITION pwqCond[10];
              pwqCond[0].source = PS4000A_CHANNEL_A;
              pwqCond[0].condition = PS4000A_CONDITION_TRUE;
              for (int i = 1; i < 10; i++) {
                pwqCond[i].source = (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + i);
                pwqCond[i].condition = PS4000A_CONDITION_DONT_CARE;
              }
              std::cout << "Handle : " << dev.handle << std::endl;
              status2 = ps4000aSetPulseWidthQualifierConditions(
                dev.handle,              // device handle
                pwqCond,           // pointer to condition structure
                10,                           // number of structures
                (PS4000A_CONDITIONS_INFO)input                  // N/A for window trigger
              );
              if (status2 != PICO_OK)
              {
                std::cout << "Set pulse width qualifier Conditions failed: err = " << status2 << std::endl;

              }
              status2 = ps4000aSetPulseWidthQualifierProperties(
                dev.handle,              // device handle
                PS4000A_BELOW,
                minPulseWidth,           // pointer to condition structure
                maxPulseWidth,                           // number of structures
                pulseType             // N/A for window trigger
              );
              if (status2 != PICO_OK)
              {
                std::cout << "Set pulse width qualifier Properties failed: err = " << status2 << std::endl;
              }
            }
            else if (triggerType == "Drop Out") {
              auto minThresholdsInput = (System::Windows::Forms::TextBox^)this->Controls["minThreshold"];
              int32_t minThresholds = System::Int32::Parse(minThresholdsInput->Text);
              dev.AdcTrigger = minThresholds;

              PS4000A_CONDITION tCond[1];
              tCond[0].source = PS4000A_PULSE_WIDTH_SOURCE;
              tCond[0].condition = PS4000A_CONDITION_TRUE;
              int input = PS4000A_CLEAR | PS4000A_ADD;
              status2 = ps4000aSetTriggerChannelConditions(dev.handle, &tCond[0], 1, (PS4000A_CONDITIONS_INFO)input);
              if (status2 != PICO_OK) {
                std::cout << "SETUP TRIGGER ERROR 1" << std::endl;
              }

              //PS4000A_DIRECTION tDir;
              //tDir.direction = PS4000A_FALLING;
              //tDir.channel = PS4000A_CHANNEL_A;
              //status2 = ps4000aSetTriggerChannelDirections(dev.handle,
              //  &tDir, 1);
              //if (status2 != PICO_OK) {
              //  std::cout << "SETUP TRIGGER ERROR 2" << std::endl;
              //}


              //auto minHysteresisInput = (System::Windows::Forms::TextBox^)this->Controls["MinHysteresisInput"];
              //int32_t minHysteresis = System::Int32::Parse(minHysteresisInput->Text);
              ////  dev.AdcTrigger = minHysteresisInput;
              //PS4000A_TRIGGER_CHANNEL_PROPERTIES tProp;
              //tProp.channel = PS4000A_CHANNEL_A;
              //tProp.thresholdMode = PS4000A_LEVEL;
              //tProp.thresholdUpper = dev.AdcTrigger; // 1000
              //tProp.thresholdUpperHysteresis = minHysteresis;
              //tProp.thresholdLower = dev.AdcTrigger; // -1000
              //tProp.thresholdLowerHysteresis = minHysteresis;

              //PS4000A_TRIGGER_CHANNEL_PROPERTIES tProp2[1];
              //tProp2[0] = tProp;

              //status2 = ps4000aSetTriggerChannelProperties(dev.handle, &tProp2[0], 1, 0, 5000);
              //if (status2 != PICO_OK) {
              //  std::cout << "SETUP TRIGGER ERROR 3" << std::endl;
              //}

              PS4000A_PULSE_WIDTH_TYPE pulseType;
              int32_t minPulse = 0;
              int32_t maxPulse = 0;
              int pulseFlags = 0;

              auto minPulseInput = (System::Windows::Forms::TextBox^)this->Controls["MinPulseWidthInput"];
              if ("" != minPulseInput->Text) {
                pulseFlags |= 1;
                minPulse = System::Int32::Parse(minPulseInput->Text);
              }
              auto maxPulseInput = (System::Windows::Forms::TextBox^)this->Controls["MaxPulseWidthInput"];
              if ("" != maxPulseInput->Text) {
                pulseFlags |= 2;
                maxPulse = System::Int32::Parse(maxPulseInput->Text);
              }

              uint32_t minPulseWidth; // = uint32_t(minPulse); // 647
              uint32_t maxPulseWidth; // = uint32_t(maxPulse); // 647
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
              PS4000A_CONDITION pwqCond[10];
              pwqCond[0].source = PS4000A_CHANNEL_A;
              pwqCond[0].condition = PS4000A_CONDITION_TRUE;
              for (int i = 1; i < 10; i++) {
                pwqCond[i].source = (PS4000A_CHANNEL)(PS4000A_CHANNEL_A + i);
                pwqCond[i].condition = PS4000A_CONDITION_DONT_CARE;
              }
              std::cout << "Handle : " << dev.handle << std::endl;
              status2 = ps4000aSetPulseWidthQualifierConditions(
                dev.handle,              // device handle
                pwqCond,           // pointer to condition structure
                10,                           // number of structures
                (PS4000A_CONDITIONS_INFO)input                  // N/A for window trigger
              );
              if (status2 != PICO_OK)
              {
                std::cout << "Set pulse width qualifier Conditions failed: err = " << status2 << std::endl;

              }

              status2 = ps4000aSetPulseWidthQualifierProperties(
                dev.handle,              // device handle
                PS4000A_BELOW,
                minPulseWidth,           // pointer to condition structure
                maxPulseWidth,                           // number of structures
                pulseType             // N/A for window trigger
              );
              if (status2 != PICO_OK)
              {
                std::cout << "Set pulse width qualifier Properties failed: err = " << status2 << std::endl;
              }
            }
            else {

            }
          }
        }

        // Run Block
        std::cout << "Run Block" << std::endl;
        {
          for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
            ParallelDevice& dev = parallelDevice[deviceNumber];
            dev.timeIndisposed = new int32_t(NUMBER_OF_CHANNELS);
            status2 = ps4000aRunBlock(dev.handle, PRE_TRIGGER, TEN_MEGA_SAMPLES - PRE_TRIGGER, dev.timebase, dev.timeIndisposed, 0, nullptr, nullptr);
            if (PICO_OK != status2) {
              std::cout << "PS" << deviceNumber << " Run Block : " << status2 << std::endl;
            //  return;
            }
          }


          status2 = PICO_OK;

          for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
            ParallelDevice& dev = parallelDevice[deviceNumber];

            dev.isReady = 0;
            status2 = PICO_OK;
            while (0 == dev.isReady && PICO_OK == status2) {
              status2 = ps4000aIsReady(dev.handle, &dev.isReady);
              std::cout << "PS" << deviceNumber << " IsReady : " << dev.isReady << std::endl;
              if (PICO_OK != status2) {
                std::cout << "PS" << deviceNumber << " IsReady Issue : " << status2 << std::endl;
              //  return;
              }
              // Sleep(1);
            }
          }
        }

        status2 = PICO_OK;
        // Get Values
        std::cout << "Get Values" << std::endl;
        {
          for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
            ParallelDevice& dev = parallelDevice[deviceNumber];

            status2 = ps4000aGetValues(dev.handle, 0, (uint32_t*)&dev.noSamples, 1, PS4000A_RATIO_MODE_NONE, 0, nullptr);
            if (PICO_OK != status2) {
              std::cout << "PS" << deviceNumber << " Get Values Issue : " << status2 << std::endl;
            //  return;
            }
          }
        }

        int32_t handleNumber = 0;
        for (int32_t graphNumber = 0; graphNumber < this->count; graphNumber++) {
          this->Controls->RemoveByKey("chart " + graphNumber);
          if (-2 == this->handle[graphNumber])
            continue;

          auto localChart = (gcnew System::Windows::Forms::DataVisualization::Charting::Chart());
          auto chartArea = (gcnew System::Windows::Forms::DataVisualization::Charting::ChartArea());
          auto legend = (gcnew System::Windows::Forms::DataVisualization::Charting::Legend());
          auto series = (gcnew System::Windows::Forms::DataVisualization::Charting::Series());

          chartArea->Name = L"ChartArea " + graphNumber;

          localChart->ChartAreas->Add(chartArea);
          legend->Name = L"Legend " + graphNumber;
          localChart->Legends->Add(legend);
          localChart->Location = System::Drawing::Point(-14, 156 + 150* handleNumber);
          localChart->Name = L"chart " + graphNumber;
          series->ChartArea = L"ChartArea " + graphNumber;
          series->Legend = L"Legend " + graphNumber;
          series->Name = L"Series " + graphNumber;
          localChart->Series->Clear();
          char strArr[8][2] = {"A", "B", "C", "D", "E", "F", "G", "H"};
          for (int devCount = 0; devCount < NUMBER_OF_CHANNELS; devCount++) {
            System::String^ res = gcnew System::String(strArr[devCount]);
            System::String^ strName = "Channel ";

            strName = strName + res;
           // std::string strName = "Channel ";
          //  strName.push_back(devCount + 'A');
            localChart->Series->Add(strName);
            localChart->Series[strName]->Color = Color::FromArgb((((devCount & 1) == 1) * 200), (((devCount & 2) == 2) * 200), (((devCount & 4) == 4) * 200));
            //	chart1->Series["chart"]->Legend = "chart1";
            //	chart1->Series["chart"]->ChartArea = "ChartArea1";
            localChart->Series[strName]->ChartType = System::Windows::Forms::DataVisualization::Charting::SeriesChartType::Line;
          }

          localChart->Size = System::Drawing::Size(668, 136);
          localChart->TabIndex = 1;

          localChart->Text = L"chart " + graphNumber;
          localChart->Click += gcnew System::EventHandler(this, &Form1::chart1_Click);

          auto button = (System::Windows::Forms::Button^)this->Controls["Button " + graphNumber];

          std::string str;
          for (auto j = 0; j < button->Text->Length; ++j) {
            str.push_back(button->Text[j]);
          }

          localChart->Titles->Add(gcnew String(str.c_str()));

          chartArea->AxisX->IntervalType = System::Windows::Forms::DataVisualization::Charting::DateTimeIntervalType::Number;
          chartArea->AxisX->Minimum = -PRE_TRIGGER;
          chartArea->AxisX->Maximum = TEN_MEGA_SAMPLES - PRE_TRIGGER;

          chartArea->AxisY->IntervalType = System::Windows::Forms::DataVisualization::Charting::DateTimeIntervalType::Number;
          chartArea->AxisY->Minimum = -32999;
          chartArea->AxisY->Maximum = 32999;
          for (char channel = 0; channel < NUMBER_OF_CHANNELS; channel++) {
            ParallelDevice& dev = parallelDevice[graphNumber];
            System::String^ res = gcnew System::String(strArr[graphNumber]);
            System::String^ strName = "Channel ";

            strName = strName + res;

            for (int i = 0; i < TEN_MEGA_SAMPLES ; i++)
              localChart->Series[strName]->Points->AddXY(-PRE_TRIGGER + i , dev.buffer[channel][i]);
          }
          for (double i = -32999; i < 32999; i++)
            localChart->Series["Channel B"]->Points->AddXY(0, i);
          this->Controls->Add(localChart);
          ++handleNumber;
        }

        // Free Buffers
        std::cout << "Free Buffers" << std::endl;
        {
          for (auto ch = 0; ch < NUMBER_OF_CHANNELS; ch++) {
            for (int32_t deviceNumber = 0; deviceNumber < noOfDevices; ++deviceNumber) {
              ParallelDevice& dev = parallelDevice[deviceNumber];
              free(dev.buffer[ch]);
            }
          }
        }
      }
	  }

    private: System::Void textBox1_TextChanged(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void label1_Click(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void FindDevices_Click(System::Object^ sender, System::EventArgs^ e) {

      int16_t count = 0;
      std::array<int8_t,900> serials; // new int8_t[10];
  
      int16_t serialLth = serials.size();
      PICO_STATUS status = ps4000aEnumerateUnits(&count, serials.data(), &serialLth);
      serials[serialLth + 1] = '\0';
      std::string tmpStr(reinterpret_cast<char*>(serials.data()));
      std::string originalStr = tmpStr.substr(0,tmpStr.size());
      auto ptr = tmpStr.begin();
      auto prevPtr = tmpStr.begin();

      std::string delimiter = ",";
      size_t pos = 0;
      std::string token;
      std::vector<std::string> serialsList;
      while ((pos = tmpStr.find(delimiter)) != std::string::npos) {
        token = tmpStr.substr(0, pos);
        serialsList.push_back(token);
        std::cout << token << std::endl;
        tmpStr.erase(0, pos + delimiter.length());
      }
      token = tmpStr.substr(0, pos);
      serialsList.push_back(token);
      std::cout << token << std::endl;
      tmpStr.erase(0, pos + delimiter.length());

      tmpStr = tmpStr.substr(0, originalStr.size());

      for (int i = 0; i < this->count; i++) {
        this->Controls->RemoveByKey("Label " + i);
        this->Controls->RemoveByKey("Check " + i);
        this->Controls->RemoveByKey("Button " + i);
      }
      this->count = count;
      this->handle = new int16_t[this->count];
      this->Controls["ListAllDevices"]->Text = "Device Count : " + count;
      std::string str;

      System::String^ stuff;
      stuff += gcnew System::String(tmpStr.c_str());
      for (int16_t i = 0; i<serialLth; i++) {
        stuff += "o";
      }
      for (int i = 0; i < count; i++) {
        this->handle[i] = 0;
        System::Windows::Forms::Label^ label = (gcnew System::Windows::Forms::Label());
        System::Windows::Forms::CheckBox^ checkBox = (gcnew System::Windows::Forms::CheckBox());
        System::Windows::Forms::Button^ button = (gcnew System::Windows::Forms::Button());

        label->Size = System::Drawing::Size(520, 20);
        label->Location = System::Drawing::Point(120, 5 + 22 * i);
        label->Name = "Label " + i;
        label->Text = gcnew System::String(serialsList[i].c_str());

        checkBox->Location = System::Drawing::Point(100, 20 * i);
        checkBox->CheckState;
        checkBox->Name = "Check " + i;

        button->Location = System::Drawing::Point(10, 20 * i);
        button->Name = "Button " + i;
        button->Text = gcnew System::String(serialsList[i].c_str());
        button->Size = System::Drawing::Size(80, 20);

        this->Controls->Add(label);
        this->Controls->Add(checkBox);
        this->Controls->Add(button);
      }
    }
    private: System::Void SelectDevices_Click(System::Object^ sender, System::EventArgs^ e) {
  
      for (int i = 0; i < this->count; i++) {
        auto checkBox = (System::Windows::Forms::CheckBox^)this->Controls["Check " + i];
        auto button = (System::Windows::Forms::Button^)this->Controls["Button " + i];
        auto label = (System::Windows::Forms::Label^)this->Controls["Label " + i];
        if (false == checkBox->Checked) {
          if (this->handle > 0)
            ps4000aCloseUnit(this->handle[i]);
          this->handle[i] = 0;
          continue;
        }
        if (this->handle[i] > 0)
          continue;

        std::vector<int8_t> res; // new int8_t[10];
        for (auto j = 0; j < button->Text->Length; ++j) {
          res.push_back((int8_t)button->Text[j]);
        }
        res.push_back((int8_t)'\0');

        if (true == checkBox->Checked) {
          PICO_STATUS status = ps4000aOpenUnit(&this->handle[i], res.data());// /*res.data()*/);
          if (PICO_OK != status)
            if (PICO_POWER_SUPPLY_NOT_CONNECTED == status || PICO_USB3_0_DEVICE_NON_USB3_0_PORT == status)
              status = ps4000aChangePowerSource(this->handle[i], status);
          if (PICO_OK != status) {
            std::cout << "PS" << i << " has an issue on OpenUnit : " << status << std::endl;
            //return;
          }
          label->Text = button->Text;
          label->Text += " => handle : " + this->handle[i];
          if (PICO_OK != status)
            label->Text += " => Error : " + status;
        }
      }
  
    }
    private: System::Void label2_Click(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void textBox2_TextChanged(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void label2_Click_1(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void textBox2_TextChanged_1(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void label2_Click_2(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void textBox2_TextChanged_2(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void label2_Click_3(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void textBox2_TextChanged_3(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void textBox3_TextChanged(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void minThreshold_TextChanged(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void Stop_Click(System::Object^ sender, System::EventArgs^ e) {
      // Closing Units
      std::cout << "Closing Units" << std::endl;
      {
        for (int32_t deviceNumber = 0; deviceNumber < this->count; ++deviceNumber) {
          //  ParallelDevice& dev = parallelDevice[deviceNumber];
          PICO_STATUS status2 = ps4000aCloseUnit(this->handle[deviceNumber]);
          if (PICO_OK != status2) {
            std::cout << "PS" << deviceNumber << " has an issue on Closure" << std::endl;
            //  return;
          }
        }
      }
      for (int32_t deviceNumber = 0; deviceNumber < this->count; ++deviceNumber) {
        this->handle[deviceNumber] = 0;
      }
    }
    private: System::Void comboBox1_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e) {

      this->Controls["MinPulseWidthInput"]->Enabled = true;
      this->Controls["MaxPulseWidthInput"]->Enabled = true;

      auto triggerTypeInput = (System::Windows::Forms::ComboBox^)this->Controls["TriggerTypeInput"];
      std::string triggerType;
      //  std::vector<char> triggerType; // new int8_t[10];
      for (auto j = 0; j < triggerTypeInput->Text->Length; ++j) {
        triggerType.push_back((char)triggerTypeInput->Text[j]);
      }
      if (triggerType == "Simple") {
        this->Controls["MinPulseWidthInput"]->Enabled = false;
        this->Controls["MaxPulseWidthInput"]->Enabled = false;
      }
    }
    private: System::Void TimebaseInput_TextChanged(System::Object^ sender, System::EventArgs^ e) {
    }
    private: System::Void Form1_Load(System::Object^ sender, System::EventArgs^ e) {
    }
  };
}
