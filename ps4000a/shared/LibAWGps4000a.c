/*******************************************************************************
 *
 * Filename: LibAWGps4000a.c
 *
 * Description:
 *   This is a C Library file to use with the
 *   PicoScope 4XXX Series (ps4000a) devices,
 *   for Signal Generator (AWG) functionality.
 *
 * Copyright (C) 2026 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#include "../../shared/PicoScaling.h"
#include "../../shared/PicoBuffers.h"
#include "../../shared/PicoFileFunctions.h"
#include "./Libps4000a.h"

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

/****************************************************************************
* Refernce Global Variables
***************************************************************************/
extern BOOL		scaleVoltages;
extern uint32_t	timebase;

/****************************************************************************
* Global Variables
***************************************************************************/
int16_t 	    DutyCycle = FALSE;      // Default to no duty cycle
int16_t 	    Sweep = FALSE;          // Default to no sweep
int16_t 	    SigGenTrigger = FALSE;  // Default to no trigger
#define MAX_AWG_BUFFER_SIZE PS4000A_MAX_SIG_GEN_BUFFER_SIZE
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
    uint64_t deltastart, deltastop, deltaInc;
    PICO_STATUS status = PICO_OK;
    double TempFrequencyStop = 0e0;
	PS4000A_SIGGEN_TRIG_SOURCE TempTriggerSource;
    uint64_t TempShots, TempSweeps;

    // Set Stop frequecy and Mask Sweep
    if ((sigGenSettings->FrequencyIncrement * sigGenSettings->DwellTime) * Sweep)
        TempFrequencyStop = sigGenSettings->FrequencyStop;
    else
        TempFrequencyStop = sigGenSettings->Frequency;

	// Mask trigger source and switch cycles to shots or sweeps depending on sweep mode
    if (SigGenTrigger) 
    {
        TempTriggerSource = sigGenSettings->triggerSource;
        if (!Sweep)
        {
            TempShots = sigGenSettings->cycles;
            TempSweeps = 0;
        }
        else
        {
            TempSweeps = sigGenSettings->cycles;
            TempShots = 0;
        }
    }
    else
    {
        TempTriggerSource = PS4000A_SIGGEN_NONE;
        TempShots = 0;
        TempSweeps = 0;
    }

	if (sigGenSettings->isArbitrary) //if waveform is arbitrary, use ps4000aSetSigGenArbitrary(), else use ps4000aSetSigGenBuiltIn()
    {
        // Find the delta phase corresponding to the frequency
        status = ps4000aSigGenFrequencyToPhaseV2(unit->handle, sigGenSettings->Frequency, PS4000A_SINGLE, (uint32_t)sigGenSettings->AWGBufferSize, &deltastart);
        status = ps4000aSigGenFrequencyToPhaseV2(unit->handle, TempFrequencyStop, PS4000A_SINGLE, (uint32_t)sigGenSettings->AWGBufferSize, &deltastop);
        status = ps4000aSigGenFrequencyToPhaseV2(unit->handle, sigGenSettings->FrequencyIncrement, PS4000A_SINGLE, (uint32_t)sigGenSettings->AWGBufferSize, &deltaInc);

        status = ps4000aSetSigGenArbitraryV2(unit->handle,
            (int32_t)sigGenSettings->Offset * 1000000,				// offset voltage
            (uint32_t)sigGenSettings->PeakVolts * 1000000,			// PkToPk in microvolts. Max = 4000000 uV  +2v to -2V
            deltastart,			// start delta
            deltastop,			// stop delta
            deltaInc,              // delta increment, not used when sweep type is none
			(uint64_t)(sigGenSettings->DwellTime / 12.5e-09f),
            // dwell time in seconds, converted to number of samples by dividing by the sample time of 12.5ns, not used when sweep type is none
			sigGenSettings->AWGBuffer,     // buffer is NULL to use default settings
            sigGenSettings->AWGBufferSize,  // bufferLenght
            sigGenSettings->SweepType,
            (PS4000A_EXTRA_OPERATIONS)0,
            (PS4000A_INDEX_MODE)0,
            TempShots,
            TempSweeps,
            sigGenSettings->triggerType,
            TempTriggerSource,
            (int16_t)0 );

        printf(status ? "\nps4000aSetSigGenArbitrary: Status Error 0x%x \n" : "", status);	// If status != 0, show the error
    }
    else
    {      
        status = ps4000aSetSigGenBuiltInV2(unit->handle,
            (int32_t)(sigGenSettings->Offset * 1000000),
            (uint32_t)(sigGenSettings->PeakVolts * 1000000),
            sigGenSettings->WaveType,
            sigGenSettings->Frequency,
            TempFrequencyStop, //sigGenSettings->FrequencyStop,
            sigGenSettings->FrequencyIncrement,
            sigGenSettings->DwellTime,
            sigGenSettings->SweepType,
            (PS4000A_EXTRA_OPERATIONS)0,
            TempShots, //shots and sweeps are not used when sweep type is none
            TempSweeps,
            sigGenSettings->triggerType,
            TempTriggerSource,
            (int16_t)0 );

        printf(status ? "\nps4000aSetSigGenBuiltIn: Status Error 0x%x \n" : "", status);		// If status != 0, show the error
    }

    if (status != PICO_OK)
    {
        printf(status ? "SigGenAWG:ps4000aSigGenWaveform ------ 0x%08lx \n" : "", status);
    }
    
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
     printf("Enter the desired Frequency (in the format Ne-XX, example 1kHz -> 1e03 ): ");
     fflush(stdin);
     scanf_s("%le", &sigGenSettings->Frequency);

     SigGenAWG(unit, sigGenSettings);
}

void AWGSetFrequencyStop(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Enter the desired Stop Frequency (in the format Ne-XX, example 1kHz -> 1e03 ): ");
    fflush(stdin);
    scanf_s("%le", &sigGenSettings->FrequencyStop);

    SigGenAWG(unit, sigGenSettings);
}

void AWGSetFrequencyInc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Enter the desired Sweep Increment Frequency (in the format Ne-XX, example 1kHz -> 1e03 ): ");
    fflush(stdin);
    scanf_s("%le", &sigGenSettings->FrequencyIncrement);

    SigGenAWG(unit, sigGenSettings);
}

void AWGSetSweepTimeInc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Enter the desired Sweep Time Increment (in the format Ne-XX, example 10ms -> 10e-3 ): ");
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
    sigGenSettings->triggerSource = PS4000A_SIGGEN_SOFT_TRIG;
     sigGenSettings->triggerType = PS4000A_SIGGEN_RISING;
     SigGenAWG(unit, sigGenSettings); // Write down changes to the unit
	// Then trigger the software trigger
	 ps4000aSigGenSoftwareControl(unit->handle, sigGenSettings->triggerType);
}

void SigGenTriggerExt(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    printf("Ext triggering NOT supported!\n");
     //sigGenSettings->triggerSource = PICO_SIGGEN_AUX_IN;
     //sigGenSettings->triggerType = PICO_SIGGEN_RISING;
     //SigGenAWG(unit, sigGenSettings); // Write down changes to the unit
}

void SineWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("Sine wave immediate...\n");
    sigGenSettings->isArbitrary = FALSE;
    sigGenSettings->WaveType = PS4000A_SINE;
    SigGenAWG(unit, sigGenSettings);
}

void SquareWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("Square wave immediate...\n");
    sigGenSettings->isArbitrary = FALSE;
    sigGenSettings->WaveType = PS4000A_SQUARE;
    SigGenAWG(unit, sigGenSettings);
}

void TriangleWave(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("Triangle wave immediate...\n");
    sigGenSettings->isArbitrary = FALSE;
    sigGenSettings->WaveType = PS4000A_TRIANGLE;
    SigGenAWG(unit, sigGenSettings);
}

void dc(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("DC immediate...\n");
    sigGenSettings->isArbitrary = FALSE;
    sigGenSettings->WaveType = PS4000A_DC_VOLTAGE;
    SigGenAWG(unit, sigGenSettings);
}

void AWG(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("AWG wave immediate...\n");
    sigGenSettings->isArbitrary = TRUE;
	//Set the AWG buffer to a test waveform
    int16_t myAWGwaveformtest[] =
    { -32768, -32768, 0, 0, 1024, 1024, 0, 0, 2048, 2048, 0, 0, 4096, 4096, 0, 0, 8192, 8192, 0, 0, 16384, 16384, 0, 0, 32767, 32767 };

	//Copy myAWGwaveformtest waveform into myAWGwaveform buffer
    memcpy_s(&myAWGwaveform[0], sizeof(myAWGwaveform),
        &myAWGwaveformtest[0], sizeof(myAWGwaveformtest));
    sigGenSettings->AWGBuffer = &myAWGwaveform[0];
	sigGenSettings->AWGBufferSize = (int32_t)( (sizeof(myAWGwaveformtest)) / (sizeof(myAWGwaveformtest[0]) ) );
    SigGenAWG(unit, sigGenSettings);
}

int8_t AWGLoadFile(GENERICUNIT* unit, SIG_GEN_SETTINGS* sigGenSettings)
{
    setDefaults(unit);
    printf("AWG wave immediate...\n");
    sigGenSettings->isArbitrary = TRUE;
	sigGenSettings->AWGBufferSize = 0; // Initialize the buffer size to 0

    char* filename = "../../PicoScope7AWG_Demo.csv";
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
    
    sigGenSettings->AWGBufferSize = (int32_t)i; // Set the actual size of the buffer based on how many values were read
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
        case PS4000A_SINE:
            printf("Sine wave    ");
            break;

        case PS4000A_SQUARE:
            printf("Square wave  ");
            break;

        case PS4000A_TRIANGLE:
            printf("Triangle wave");
            break;

        case PS4000A_DC_VOLTAGE:
            printf("DC           ");
            break;

        //case PICO_ARBITRARY:
        //    printf("AWG          ");
        //    break;

        default:
            if(sigGenSettings->isArbitrary)
                printf("AWG          ");
            else
                printf("Unknown enum:%ld", sigGenSettings->WaveType);
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
        case PS4000A_UP:
            printf("Up     ");
            break;

        case PS4000A_DOWN:
            printf("Down   ");
            break;

        case  PS4000A_UPDOWN:
            printf("Up Down");
            break;

        case PS4000A_DOWNUP:
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
        case PS4000A_SIGGEN_NONE:
            printf("None      ");
            break;

        case PS4000A_SIGGEN_SCOPE_TRIG:
            printf("Scope     ");
            break;

        case  PS4000A_SIGGEN_SOFT_TRIG:
            printf("Software  ");
            break;

        case PS4000A_SIGGEN_AUX_IN:
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




