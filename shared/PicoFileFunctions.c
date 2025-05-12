/****************************************************************************
 *
 * Filename:    PicoFileFunctions.c
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 * This file defines file writing functions for PicoScope data.
 *
 ****************************************************************************/

#include <stdio.h>
#include "./PicoUnit.h"
#include "./PicoFileFunctions.h"
#include "./PicoScaling.h"
#include "./PicoBuffers.h"

/* Headers for Windows */
#ifdef _WIN32
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef PICO_STATUS
#include <libps6000a/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL{FALSE,TRUE} BOOL;

/* A function to detect a keyboard press on Linux */
int32_t _getch()
{
        struct termios oldt, newt;
        int32_t ch;
        int32_t bytesWaiting;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~( ICANON | ECHO );
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        setbuf(stdin, NULL);
        do {
                ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
                if (bytesWaiting)
                        getchar();
        } while (bytesWaiting);

        ch = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
}

int32_t _kbhit()
{
        struct termios oldt, newt;
        int32_t bytesWaiting;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~( ICANON | ECHO );
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        setbuf(stdin, NULL);
        ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return bytesWaiting;
}

int32_t fopen_s(FILE ** a, const char * b, const char * c)
{
FILE * fp = fopen(b,c);
*a = fp;
return (fp>0)?0:-1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

/****************************************************************************
* Gobal Variables
***************************************************************************/

/***************************************************************************/

/****************************************************************************
* WriteArrayToFilesGeneric
*
* Writes scope data to a file (one file per waveform)
* Writes header info- waveform number, ttrigger sample, Over range flags
* Write sample time vaules and data as ADC counts and voltage
* Inputs:
* - pointer to double - "scaled" values of 3D arrays ADC counts (Max and Min values if used)
* - Channel scaling info "enabledChannelsScaling",
* - File name,
* - Triggersample number,
* - Over range flags - "overflow"
* Outputs:
* Writes files to disk of current path
****************************************************************************/

void WriteArrayToFilesGeneric(GENERICUNIT* unit,
int16_t*** minBuffers,
int16_t*** maxBuffers,
MULTIBUFFERSIZES multiBufferSizes,
PICO_PROBE_SCALING* enabledChannelsScaling,
char startOfFileName[],// = "Output",
int16_t Triggersample,
int16_t* overflow)
{   
    FILE* fp = NULL;
    if(startOfFileName == NULL)
        startOfFileName = "Pico_BufferCaptureN_";
    //size_t size_startOfFileName = sizeof(startOfFileName) / sizeof(startOfFileName[0]);

    uint64_t i;
    uint64_t capture;

    char buf[58 + (3 * sizeof(int))]= { '\0' }; // null terminate the string
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);
        
    for (capture = 0; capture < multiBufferSizes.numberOfBuffers; capture++)
    {
        //Goto next file
        snprintf(buf, buf_size, "%s%d.txt", startOfFileName, (int)capture);
        fopen_s(&fp, buf, "w");
        if (fp != NULL)
        {
            //Write 2 header lines (one for Info, one for Channels)
            fprintf(fp, "Segment: %lld of %lld Segment(s)\n",
                capture, multiBufferSizes.numberOfBuffers);

            fprintf(fp, "SampleRate %3.3e SamplesPerBlock %lld Trigger@Sample %d \n",
                unit->timeInterval, multiBufferSizes.maxBufferSize, Triggersample);

			//overrange flags
            fprintf(fp, "OverRange flag: ");
            i = 10; // upto 2 digital ports + 8 analog channels (CHAR_BIT * sizeof integer)
            while (i--)
            {
                fprintf(fp, "%d", ((uint16_t)overflow[capture] >> i) & 1);
            }
            fprintf(fp, " (LSB ChA)\n");

            // Write time and channel headings
            fprintf(fp, "Time(s) ");

            for (i = 0; i < unit->channelCount; i++)
            {
                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "Ch%C_Max-ADC Max_V ", 'A' + (int)i);  //fprintf(fp, "Ch%C_Max-ADC Max_mV ", 'A' + (int)i);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Min-ADC Min_V ");//fprintf(fp, "Min-ADC Min_mV ");
                    }
                }
            }
            fprintf(fp, "\n");

            // Write time and channel data
            for (i = 0; i < multiBufferSizes.maxBufferSize; i++)
            {
                fprintf(fp, "%3.3e ", i * unit->timeInterval);

                for (int j = 0; j < unit->channelCount; j++)
                {
                    if (unit->channelSettings[j].enabled)
                    {
                        fprintf(fp,
                            "%+5d %+3.3e ",
                            maxBuffers[capture][j][i],
                            //(double)adc_to_mv((maxBuffers)[capture][j][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue)
                            adc_to_scaled_value((maxBuffers)[capture][j][i], enabledChannelsScaling[PICO_CHANNEL_A + j], unit->maxADCValue)
                        );

                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            fprintf(fp,
                                "%+5d %+3.3e ",
                                minBuffers[capture][j][i],
                                //(double)adc_to_mv((minBuffers)[capture][j][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue)
                                adc_to_scaled_value((minBuffers)[capture][j][i], enabledChannelsScaling[PICO_CHANNEL_A + j], unit->maxADCValue)
                            );
                        }
                    }
                }
                fprintf(fp, "\n");
            }
            fclose(fp);
        }
    }
}

/****************************************************************************
* WriteArrayToFileGeneric
*
* Write scope data to one file.
* Writes header info- waveform number, ttrigger sample, Over range flags
* Write sample time vaules and data as ADC counts and voltage
* Inputs:
* - pointer to double - "scaled" values of 3D arrays ADC counts (Max and Min values if used)
* - Channel scaling info "enabledChannelsScaling",
* - File name,
* - Triggersample number,
* - Over range flags - "overflow"
* Outputs:
* Writes files to disk of current path
****************************************************************************/

void WriteArrayToFileGeneric(GENERICUNIT* unit,
    int16_t** minBuffers,
    int16_t** maxBuffers,
    MULTIBUFFERSIZES multiBufferSizes,
    PICO_PROBE_SCALING* enabledChannelsScaling,
    //double actualTimeInterval,
    char startOfFileName[],
    int16_t Triggersample,
    int16_t* overflow)
{
    FILE* fp = NULL;
    if (startOfFileName == NULL)
        startOfFileName = "Pico_BufferCapture";

    uint64_t i;
    uint64_t capture = 0;

        //Goto next file
        fopen_s(&fp, startOfFileName, "w");
        if (fp != NULL)
        {
            //Write 2 header lines (one for Info, one for Channels)

            fprintf(fp, "SampleRate %3.3e SamplesPerBlock %lld Trigger@Sample %d \n",
                unit->timeInterval, multiBufferSizes.maxBufferSize, Triggersample);

            //overrange flags
            fprintf(fp, "OverRange flag: ");
            i = 10; // upto 2 digital ports + 8 analog channels (CHAR_BIT * sizeof integer)
            while (i--)
            {
                fprintf(fp, "%d", ((uint16_t)overflow[0] >> i) & 1 );
            }
            fprintf(fp, " (LSB ChA)\n");

            // Write time and channel headings
            fprintf(fp, "Time(s) ");

            for (i = 0; i < unit->channelCount; i++)
            {
                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "Ch%C_Max-ADC Max_V ", 'A' + (int)i);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Min-ADC Min_V ");
                    }
                }
            }
            fprintf(fp, "\n");

			// Write time and channel data
            for (i = 0; i < multiBufferSizes.maxBufferSize; i++)
            {
                fprintf(fp, "%3.3e ", i * unit->timeInterval);

                for (int j = 0; j < unit->channelCount; j++)
                {
                    if (unit->channelSettings[j].enabled)
                    {
                        fprintf(fp,
                            "%+5d %+3.3e ",
                            (maxBuffers)[j][i],              
                            //(double)adc_to_mv(maxBuffers[j][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue)
                            adc_to_scaled_value(maxBuffers[j][i], enabledChannelsScaling[PICO_CHANNEL_A + j], unit->maxADCValue)
                        );

                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            fprintf(fp,
                                "%+5d %+3.3e ",
                                minBuffers[j][i],
                                //(double)adc_to_mv(minBuffers[j][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue)
                                adc_to_scaled_value(minBuffers[j][i], enabledChannelsScaling[PICO_CHANNEL_A + j], unit->maxADCValue)
                            );
                        }
                    }
                }
                fprintf(fp, "\n");
            }
        fclose(fp);
        }
}