/****************************************************************************
 *
 * Filename:    PicoFileFunctions.c
 * Copyright:   Pico Technology Limited 2023 - 2025
 * Description:
 *
 * This header defines scaling related to 
 *
 ****************************************************************************/

#include <stdio.h>
#include "./PicoUnit.h"
#include "./PicoFileFunctions.h"
#include "./PicoScaling.h"
#include "./PicoBuffers.h"
////#include "../ps6000a-NEW/shared/Libps60000a.h" //TEMPORARY for UNIT struct

/* Headers for Windows */
#ifdef _WIN32
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef PICO_STATUS
#include <libps6000a-1.0/PicoStatus.h>
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
* Convert Scaled value (voltage or Probe units) to ADC value
* Inputs:
* /////////////////////////////////////////////////////////////////////////////////- double - "scaled" value
*

****************************************************************************/

void WriteArrayToFilesGeneric(GENERICUNIT* unit,
int16_t*** minBuffers,
int16_t*** maxBuffers,
MULTIBUFFERSIZES multiBufferSizes,
PICO_PROBE_SCALING enabledChannelsScaling[10], ////////////////////////////////////////////////////////////////////////////////////////
double actualTimeInterval,// = 1,
char startOfFileName[],// = "Output",
int16_t Triggersample, // = 0,//int16_t maxADCValue) // =0
int16_t* overflow)
{   
    FILE* fp = NULL;
    if(startOfFileName == NULL)
        startOfFileName = "Pico_BufferCaptureN_";
    //size_t size_startOfFileName = sizeof(startOfFileName) / sizeof(startOfFileName[0]);

    uint64_t i;
    uint64_t capture;

    //For scaling Info for each channel
	////////////PICO_PROBE_SCALING enabledChannelsScaling[PS6000A_MAX_CHANNELS]; //[unit->channelCount]; //Move to global/golobal struture

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
            fprintf(fp, "Segment: %lld of %d Segment(s)\n",
                capture, multiBufferSizes.numberOfBuffers);

            fprintf(fp, "SampleRate %3.3e SamplesPerBlock %d Trigger@Sample %d \n",
                actualTimeInterval, multiBufferSizes.maxBufferSize, Triggersample);
            fprintf(fp, "OverRange flag: %d\n", overflow[capture]);
            //fprintf(fp, "value ADC Count & mV\n\n");
            fprintf(fp, "Time(s) ");

            for (i = 0; i < unit->channelCount; i++)
            {
                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "Ch%C_Max-ADC Max_mV ", 'A' + (int)i);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Min-ADC Min_mV ");
                    }
                }
            }
            fprintf(fp, "\n");

            for (i = 0; i < multiBufferSizes.maxBufferSize; i++)
            {
                fprintf(fp, "%3.3e ", i * actualTimeInterval);

                for (int j = 0; j < unit->channelCount; j++)
                {
                    if (unit->channelSettings[j].enabled)
                    {
                        fprintf(fp,
                            "%+5d %+3.3e ",
                            maxBuffers[j][capture][i],
                            (double)adc_to_mv((maxBuffers)[j][capture][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue)
                        );

                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            fprintf(fp,
                                "%+5d %+3.3e ",
                                minBuffers[j][capture][i],
                                (double)adc_to_mv((minBuffers)[j][capture][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue)
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
* WriteArrayToFilesGeneric
*
* Convert Scaled value (voltage or Probe units) to ADC value
* Inputs:
* /////////////////////////////////////////////////////////////////////////////////- double - "scaled" value
*

****************************************************************************/

void WriteArrayToFileGeneric(GENERICUNIT* unit,
    int16_t** minBuffers,
    int16_t** maxBuffers,
    MULTIBUFFERSIZES multiBufferSizes,
    PICO_PROBE_SCALING enabledChannelsScaling[10], ////////////////////////////////////////////////////////////////////////////////////////
    double actualTimeInterval,// = 1,
    char startOfFileName[],// = "Output",
    int16_t Triggersample, // = 0,//int16_t maxADCValue) // =0
    int16_t* overflow)
{
    FILE* fp = NULL;
    if (startOfFileName == NULL)
        startOfFileName = "Pico_BufferCaptureN_";
    //size_t size_startOfFileName = sizeof(startOfFileName) / sizeof(startOfFileName[0]);

    uint64_t i;
    uint64_t capture = 0;

    //For scaling Info for each channel
    ////////////PICO_PROBE_SCALING enabledChannelsScaling[PS6000A_MAX_CHANNELS]; //[unit->channelCount]; //Move to global/golobal struture

    char buf[58 + (3 * sizeof(int))] = { '\0' }; // null terminate the string
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);

    int16_t test = -1;

    //for (capture = 0; capture < multiBufferSizes.numberOfBuffers; capture++)
    //{
        //Goto next file
        snprintf(buf, buf_size, "%s%d.txt", startOfFileName, 0);
        fopen_s(&fp, buf, "w");
        if (fp != NULL)
        {
            //Write 2 header lines (one for Info, one for Channels)

            fprintf(fp, "SampleRate %3.3e SamplesPerBlock %d Trigger@Sample %d \n",
                actualTimeInterval, multiBufferSizes.maxBufferSize, Triggersample);
            fprintf(fp, "OverRange flag: %d\n", overflow[0]);
            //fprintf(fp, "value ADC Count & mV\n\n");
            fprintf(fp, "Time(s) ");

            for (i = 0; i < unit->channelCount; i++)
            {
                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "Ch%C_Max-ADC Max_mV ", 'A' + (int)i);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Min-ADC Min_mV ");
                    }
                }
            }
            fprintf(fp, "\n");

            for (i = 0; i < multiBufferSizes.maxBufferSize; i++)
            {
                fprintf(fp, "%3.3e ", i * actualTimeInterval);

                for (int j = 0; j < unit->channelCount; j++)
                {
                    if (unit->channelSettings[j].enabled)
                    {
                        test = maxBuffers[i][j];
                        //fprintf(fp,
                        //    "%+5d ",
                         //   maxBuffers[j][i]
                            //(double)adc_to_mv((maxBuffers)[j][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue)
                        //);

                        //if (multiBufferSizes.minBufferSize != 0)
                       // {
                        //    fprintf(fp,
                        //        "%+5d %+3.3e ",
                        //        minBuffers[j][i],
                        //        (double)adc_to_mv((minBuffers)[j][i], unit->channelSettings[PICO_CHANNEL_A + j].range, unit->maxADCValue)
                        //    );
                        //}
                    }
                }
                fprintf(fp, "\n");
            }
        fclose(fp);
        }
        

    //}


}