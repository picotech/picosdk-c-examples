/*******************************************************************************
 *
 * Filename: Libpsospa.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 3XXXE Series (psospa) devices,
 *   for Signal Generator (AWG) functionality.
 *
 * Copyright (C) 2025 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include "../../shared/PicoScaling.h"
#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"
#include "./Libpsospa.h"

/* Headers for Windows */
#ifdef _WIN32
#include "psospaApi.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <psospaApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif


#endif

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase;
//extern const uint64_t constBufferSize;

/****************************************************************************
* Global Variables
***************************************************************************/
//int16_t   		g_ready = FALSE;
int16_t 	    DutyCycle = FALSE;      // Default to no duty cycle
int16_t 	    Sweep = FALSE;          // Default to no sweep
int16_t 	    SigGenTrigger = FALSE;  // Default to no trigger
#define MAX_AWG_BUFFER_SIZE 32768
int16_t myAWGwaveform[MAX_AWG_BUFFER_SIZE]; // Buffer for the AWG waveform, set to max size
/****************************************************************************
* SigGenAWG
* - Used to set the signal generator or AWG settings
* - acquires settings from the SIG_GEN_SETTINGS structure set by other user called functions
* Input :
* - unit : the unit to use.
* -sigGenSettings : the settings to apply to the signal generator.
****************************************************************************/
void SigGenAWG(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{ 
    PICO_STATUS status = PICO_OK; 
    // SigGenWaveform
    psospaSigGenWaveform(unit->handle,
                        sigGenSettings->WaveType,       //waveType,
                         sigGenSettings->AWGBuffer,     //buffer is NULL to use default settings
                         sigGenSettings->AWGBufferSize  //bufferLenght
                        );
    if (status != PICO_OK)
    {
        printf(status ? "SigGenAWG:psospaSigGenWaveform ------ 0x%08lx \n" : "", status);
    }
    //printf("SigGenWaveform\n");

    // SigGenRange
    psospaSigGenRange(unit->handle,
         sigGenSettings->PeakVolts,
         sigGenSettings->Offset);
    if (status != PICO_OK)
    {
        printf(status ? "SigGenAWG:psospaSigGenRange ------ 0x%08lx \n" : "", status);
    }
    //printf("SigGenRange\n");

    // psospaSigGenWaveformDutyCycle
    if (DutyCycle)
    {
        psospaSigGenWaveformDutyCycle(unit->handle,
             sigGenSettings->dutyCyclePercent);          //double dutyCyclePercent (0.0 to 100.0)
        if (status != PICO_OK)
        {
            printf(status ? "SigGenAWG:psospaSigGenWaveformDutyCycle ------ 0x%08lx \n" : "", status);
        }
        //printf("SigGenWaveformDutyCycle\n");
    }

    // SigGenFrequency
    psospaSigGenFrequency(unit->handle,
         sigGenSettings->Frequency);
    if (status != PICO_OK)
    {
        printf(status ? "SigGenAWG:psospaSigGenFrequency ------ 0x%08lx \n" : "", status);
    }
    //printf("SigGenFrequency\n");

    // psospaSigGenFrequencySweep
    if (Sweep)
    {
        psospaSigGenFrequencySweep(unit->handle,
             sigGenSettings->FrequencyStop,         //double stopFrequencyHz,
             sigGenSettings->FrequencyIncrement,    //double frequencyIncrement (Hz),
             sigGenSettings->DwellTime,             //double dwellTimeSeconds
             sigGenSettings->SweepType);            //sweepType
        if (status != PICO_OK)
        {
            printf(status ? "SigGenAWG:psospaSigGenFrequencySweep ------ 0x%08lx \n" : "", status);
        }
        //printf("SigGenFrequencySweep\n");
    }

    if (SigGenTrigger)
    {
        // psospaSigGenTrigger
        psospaSigGenTrigger(unit->handle,
             sigGenSettings->triggerType,        // PICO__TRIG_TYPE triggerType,
             sigGenSettings->triggerSource,      // PICO__TRIG_SOURCE triggerSource,
             sigGenSettings->cycles,
             sigGenSettings->autoTrigPicoSecs
        );
        if (status != PICO_OK)
        {
            printf(status ? "SigGenAWG:psospaSigGenTrigger ------ 0x%08lx \n" : "", status);
        }
        //printf("SigGenTrigger\n");
    }
    double tempFrequency =  sigGenSettings->Frequency;                     //double* frequency,
    double tempStopFrequency =  sigGenSettings->FrequencyStop;             //double* stopFrequency,
    double tempFrequencyIncrement =  sigGenSettings->FrequencyIncrement;   //double* frequencyIncrement(Hz),
    double tempDwellTime =  sigGenSettings->DwellTime;                        //double* dwellTime (s)
    // SigGenApply
    psospaSigGenApply(unit->handle,
         sigGenSettings->Enabled, 			//int16_t sigGenEnabled,
        (int16_t)((Sweep) ? 1 : 0), 		//int16_t sweepEnabled,
        (int16_t)((SigGenTrigger) ? 1 : 0), //int16_t triggerEnabled,
        &tempFrequency,             		//double* frequency,
        &tempStopFrequency,             	//double* stopFrequency,
        &tempFrequencyIncrement,    		//double* frequencyIncrement(Hz),
        &tempDwellTime             			//double* dwellTime (s)
    );
    if (status != PICO_OK)
    {
        printf(status ? "SigGenAWG:psospaSigGenApply ------ 0x%08lx \n" : "", status);
    }
    //printf("SigGenApply\n");
}

void AWGSetPeaktoPVoltage(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Enter the desired Peak to peak voltage (in the format Ne-XX, example 1V -> 1e0 ): ");
    fflush(stdin);
    scanf_s("%le", &sigGenSettings->PeakVolts);

    SigGenAWG(unit, sigGenSettings);
}

void AWGSetOffsetVoltage(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
     printf("Enter the desired voltage offset (in the format Ne-XX, example 1V -> 1e0 ): ");
     fflush(stdin);
     scanf_s("%le", &sigGenSettings->Offset);
     SigGenAWG(unit, sigGenSettings);
}

void AWGSetFrequency(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
     printf("Enter the desired Frequency (in the format Ne-XX, example 1kHz -> 1e06 ): ");
     fflush(stdin);
     scanf_s("%le", &sigGenSettings->Frequency);

     SigGenAWG(unit, sigGenSettings);
}

void AWGSetFrequencyStop(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Enter the desired Stop Frequency (in the format Ne-XX, example 1kHz -> 1e06 ): ");
    fflush(stdin);
    scanf_s("%le", &sigGenSettings->FrequencyStop);

    SigGenAWG(unit, sigGenSettings);
}

void AWGSetFrequencyInc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Enter the desired Sweep Increment Frequency (in the format Ne-XX, example 1kHz -> 1e06 ): ");
    fflush(stdin);
    scanf_s("%le", &sigGenSettings->FrequencyIncrement);

    SigGenAWG(unit, sigGenSettings);
}

void AWGSetSweepTimeInc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Enter the desired Sweep Time Increment (in the format Ne-XX, example 10ms -> 10e03 ): ");
    fflush(stdin);
    scanf_s("%le", &sigGenSettings->DwellTime);

    SigGenAWG(unit, sigGenSettings);
}

void SweepOnOff(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    Sweep = !Sweep;
    if(Sweep)
        printf("Sweep ON...\n");
    else
        printf("Sweep OFF...\n");
    SigGenAWG(unit, sigGenSettings);
}

void SigGenTriggerOnOff(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Enter Peak to peak voltage...\n");
    SigGenTrigger = !SigGenTrigger;
    if (SigGenTrigger)
    {
        printf("Trigger ON...\n");
        printf("Select trigger mode.\n");
    }
    else
        printf("Trigger OFF...\n");
    SigGenAWG(unit, sigGenSettings);
}

void SigGenTriggerNow(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Triggering Now!\n");
     sigGenSettings->triggerSource = PICO_SIGGEN_SOFT_TRIG;
     sigGenSettings->triggerType = PICO_SIGGEN_RISING;
     SigGenAWG(unit, sigGenSettings); // Write down changes to the unit
	// Then trigger the software trigger
    psospaSigGenSoftwareTriggerControl(unit->handle,  sigGenSettings->triggerType);
}

void SigGenTriggerExt(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Trigger set to AUX in...\n");
     sigGenSettings->triggerSource = PICO_SIGGEN_AUX_IN;
     sigGenSettings->triggerType = PICO_SIGGEN_RISING;
     SigGenAWG(unit, sigGenSettings); // Write down changes to the unit
}

void SineWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("Sine wave immediate...\n");

    sigGenSettings->WaveType = PICO_SINE;
    SigGenAWG(unit, sigGenSettings);
}

void SquareWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("Square wave immediate...\n");

    sigGenSettings->WaveType = PICO_SQUARE;
    SigGenAWG(unit, sigGenSettings);
}

void TriangleWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("Triangle wave immediate...\n");

    sigGenSettings->WaveType = PICO_TRIANGLE;
    SigGenAWG(unit, sigGenSettings);
}

void dc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("DC immediate...\n");

    sigGenSettings->WaveType = PICO_DC_VOLTAGE;
    SigGenAWG(unit, sigGenSettings);
}

void AWG(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("AWG wave immediate...\n");
    sigGenSettings->WaveType = PICO_ARBITRARY;
	//Set the AWG buffer to a test waveform
    int16_t myAWGwaveformtest[] =
    { -32768, -32768, 0, 0, 1024, 1024, 0, 0, 2048, 2048, 0, 0, 4096, 4096, 0, 0, 8192, 8192, 0, 0, 16384, 16384, 0, 0, 32767, 32767 };

    //Copy myAWGwaveformtest waveform into myAWGwaveform buffer
    memcpy_s(&myAWGwaveform[0], sizeof(myAWGwaveform),
        &myAWGwaveformtest[0], sizeof(myAWGwaveformtest));
    sigGenSettings->AWGBuffer = &myAWGwaveform[0];
    sigGenSettings->AWGBufferSize = (int32_t)((sizeof(myAWGwaveformtest)) / (sizeof(myAWGwaveformtest[0])));
    SigGenAWG(unit, sigGenSettings);
}

int8_t AWGLoadFile(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("AWG wave immediate...\n");
    sigGenSettings->WaveType = PICO_ARBITRARY;
	sigGenSettings->AWGBufferSize = 0; // Initialize the buffer size to 0

    char* filename = "./PicoScope7AWG_Demo.csv";
    FILE* fp = fopen(filename, "r");

    if (fp == NULL)
    {
        printf("Error: could not open file %s", filename);
        return 1;
    }
    else
    {
        printf("Opening file: %s", filename);
    }
    // reading line by line, max 16 bytes (chars)
    #define MAX_LINE_LENGTH 16
    char buffer[MAX_LINE_LENGTH];

	uint16_t i = 0; // Index for the waveform array
    char* eptr;
    while ( fgets(buffer, MAX_LINE_LENGTH, fp) )
    {
		// Convert the string to an integer and store it in the array
        double value = strtod(buffer, &eptr);
        // Check if the value is within the range of -1 to 1
        if (value < -1.0f || value > 1.0f)
        {
            printf("Error: value out of range: %f\n", value);
            fclose(fp);
            return 1;
        }
		// Append the value to the array
        if(i < MAX_AWG_BUFFER_SIZE)
        {
            myAWGwaveform[i++] = (int16_t)(value * (int16_t)32767);
        }
        else
        {
            printf("Error: Buffer out of range at Index:%d Vaule:%f\n", i-1, value);
            fclose(fp);
            return 1;
        }
        // printf("String:%s, int16_t: %d\n", buffer, myAWGwaveform[i-1]); // DEBUG     
    }
	fclose(fp); // close the file
    
    sigGenSettings->AWGBufferSize = (uint64_t)i; // Set the actual size of the buffer based on how many values were read
    sigGenSettings->AWGBuffer = &myAWGwaveform[0];
    SigGenAWG(unit, sigGenSettings);
    return 0;
}

/****************************************************************************
* printsigGenSettings
*  this function prints the current settings of the signal generator
****************************************************************************/
void printsigGenSettings(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    //printf("|         Signal Type:\t\t\t|   Sweep type:\t|\n");
    printf("|         Signal Type: ");
    if (sigGenSettings->Enabled)
    {
        switch (sigGenSettings->WaveType)
        {
        case PICO_SINE:
            printf("Sine wave    ");
            break;

        case PICO_SQUARE:
            printf("Square wave  ");
            break;

        case PICO_TRIANGLE:
            printf("Triangle wave");
            break;

        case PICO_DC_VOLTAGE:
            printf("DC           ");
            break;

        case PICO_ARBITRARY:
            printf("AWG          ");
            break;

        default:
            printf("Unknown enum:%ld", sigGenSettings->WaveType);
            //printf("Unknown/Invalid signal type enum: %ld", sigGenSettings->WaveType);
            break;
        }
    }
    else
        printf("OFF          ");

    printf("\t|");

    printf("   Sweep type: ");
    if (Sweep)
    {
        switch (sigGenSettings->SweepType)
        {
        case PICO_UP:
            printf("Up     ");
            break;

        case PICO_DOWN:
            printf("Down   ");
            break;

        case  PICO_UPDOWN:
            printf("Up Down");
            break;

        case PICO_DOWNUP:
            printf("Down Up");
            break;

        default:
            printf("Unknown enum:%ld", sigGenSettings->SweepType);
            //printf("Unknown/Invalid Sweep enum: %ld", sigGenSettings->SweepType);
            break;
        }
    }
    else
        printf("OFF    ");

    printf("\t\t\t|\n");
    printf("|           Frequency:  %3.3e\t|   Sweep Stop Frequency: %3.3e\t|\n", sigGenSettings->Frequency, sigGenSettings->FrequencyStop);
    printf("|      P-to-P Voltage: %+3.3e\t|   Sweep Inc. Frequency: %3.3e\t|\n", sigGenSettings->PeakVolts, sigGenSettings->FrequencyIncrement);
    printf("|  Offset(DC) Voltage: %+3.3e\t|\t\t\t\t\t|\n", sigGenSettings->Offset);
    printf("|\t\t\t\t\t|   Trigger Source: ");
    if (SigGenTrigger)
    {
        switch (sigGenSettings->triggerSource)
        {
        case PICO_SIGGEN_NONE:
            printf("None      ");
            break;

        case PICO_SIGGEN_SCOPE_TRIG:
            printf("Scope     ");
            break;

        case  PICO_SIGGEN_SOFT_TRIG:
            printf("Software  ");
            break;

        case PICO_SIGGEN_AUX_IN:
            printf("Aux In    ");
            break;

        default:
            printf("Unknown enum:%ld", sigGenSettings->triggerSource);
            break;
        }
    }
    else
        printf("OFF       ");

    printf("\t\t| \n");
    printf("|\t\t\t\t\t|   Cycles per Trigger: %lld\t\t|\n", sigGenSettings->cycles);
}
