/****************************************************************************
 *
 * Filename:    PicoBuffers.c
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 * This file defines functions for creating buffers to store PicoScope data
 *
 ****************************************************************************/
#include <stdio.h>
#include <math.h>
#include "./PicoBuffers.h"


/* Headers for Windows */
#ifdef _WIN32
#include "PicoDeviceStructs.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef PICO_STATUS
#include <PicoStatus.h>
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

// Buffer related functions //

/****************************************************************************
* data_buffer_sizes
*
* Calulate Buffer sizes for any Down Sample Mode, inclucing none
* Inputs:
* - Down-Sample Mode
* - Down Sample Ratio
* - Number of Samples
* Outputs:
* - Max Buffer Size (via pointer)
* - Min Buffer Size (via pointer)

****************************************************************************/
void data_buffer_sizes(PICO_RATIO_MODE downSampleRatioMode, uint64_t downSampleRatio, uint64_t noOfSamples, uint64_t* maxBufferSize, uint64_t* minBufferSize)
{
    float x, y;
    x = noOfSamples;
    y = downSampleRatio;

    if(downSampleRatioMode != PICO_RATIO_MODE_RAW) 
        if(downSampleRatio == 0)
        {
            printf("\nWarning downSampleRatio must not be zero!\n");
		}

    switch(downSampleRatioMode)
    {
        case 0:
        *maxBufferSize = noOfSamples;
        *minBufferSize = 0;
        break;

        case PICO_RATIO_MODE_RAW:
        *maxBufferSize = noOfSamples;
        *minBufferSize = 0;
        break;

		case PICO_RATIO_MODE_AGGREGATE:
        // Min. buffer size = (Remainder + Quotient (rounded) )
        // of noOfSamples/downSampleRatio
        // 
        //x = noOfSamples;
		//y = downSampleRatio;
        *maxBufferSize = (uint64_t)((x - y * floor(x / y) ) + (round(floor( x / y) )));
        *minBufferSize = *maxBufferSize;
        break;

		default: // For for other modes - PICO_RATIO_MODE_DECIMATE, PICO_RATIO_MODE_AVERAGE
        //do the same as above but set mixBufferSize = 0.
        // 
        //x = noOfSamples;
        //y = downSampleRatio;
        *maxBufferSize = (uint64_t)((x - y * floor(x / y) ) + (round(floor(x / y) )));
        *minBufferSize = 0;
        break;
    }
}

/****************************************************************************
* pico_create_multibuffers
*
* Creates buffers with the correct size for the given settings
* Inputs:
* - GENERICUNIT* unit
* - BUFFER_SETTINGS bufferSettings
* - numberOfBuffers
* Outputs:
* - Max Buffer (3D pointer array)
* - Min Buffer (3D pointer array)
* - MULTIBUFFERSIZES* multiBufferSizes

****************************************************************************/
int16_t pico_create_multibuffers(GENERICUNIT* unit, BUFFER_SETTINGS bufferSettings,
    uint64_t numberOfBuffers, int16_t**** minBuffers, int16_t**** maxBuffers, MULTIBUFFERSIZES* multiBufferSizes)
{

    // Calulate buffer sizes   
    uint64_t maxBufferSize = 0;
    uint64_t minBufferSize = 0;
    data_buffer_sizes(bufferSettings.downSampleRatioMode,
        bufferSettings.downSampleRatio,
        bufferSettings.nSamples,
        &maxBufferSize,
        &minBufferSize);

    // Create buffers 
    *minBuffers = (int16_t***)calloc(numberOfBuffers, sizeof(int16_t*));
    *maxBuffers = (int16_t***)calloc(numberOfBuffers, sizeof(int16_t*));

    for (uint64_t capture = 0; capture < numberOfBuffers; capture++)
    {
        int16_t channel = 0;

        (*minBuffers)[capture] = (int16_t**)calloc(unit->channelCount + unit->digitalPortCount, sizeof(int16_t*));
        (*maxBuffers)[capture] = (int16_t**)calloc(unit->channelCount + unit->digitalPortCount, sizeof(int16_t*));

        for (channel = 0; channel < unit->channelCount; channel++)
        {
            if ((*maxBuffers)[capture] != NULL && (*minBuffers)[capture] != NULL)
            {
                if (unit->channelSettings[channel].enabled)
                {
                        if (minBufferSize)
                            (*minBuffers)[capture][channel] = (int16_t*)calloc(minBufferSize, sizeof(int16_t));
                        else
                            (*minBuffers)[capture][channel] = NULL; // If minBufferSize is 0, set pointer to NULL
                        (*maxBuffers)[capture][channel] = (int16_t*)calloc(maxBufferSize, sizeof(int16_t));
                }
                else // If channel is not enabled, set pointers to NULL
                {
                        (*minBuffers)[capture][channel] = NULL;
                        (*maxBuffers)[capture][channel] = NULL;
                }
            }
            else
            {
				return 1; // Return error if memory allocation failed
            }
        
        }
		//digital channels
        for (channel = 0; channel < unit->digitalPortCount; channel++)
        {
            if ((*maxBuffers)[capture] != NULL && (*minBuffers)[capture] != NULL)
            {
                if (unit->digitalChannelSettings[channel].enabled)
                {
                    if (minBufferSize)
                        (*minBuffers)[capture][(channel + unit->channelCount)] = (int16_t*)calloc(minBufferSize, sizeof(int16_t));
                    else
                        (*minBuffers)[capture][(channel + unit->channelCount)] = NULL; // If minBufferSize is 0, set pointer to NULL
                    (*maxBuffers)[capture][(channel + unit->channelCount)] = (int16_t*)calloc(maxBufferSize, sizeof(int16_t));
                }
                else // If channel is not enabled, set pointers to NULL
                {
                    (*minBuffers)[capture][(channel + unit->channelCount)] = NULL;
                    (*maxBuffers)[capture][(channel + unit->channelCount)] = NULL;
                }
            }
            else
            {
                return 1; // Return error if memory allocation failed
            }
        }    
	}
    // Add sizes of the buffers to MULTIBUFFERSIZES struture
    multiBufferSizes->numberOfBuffers = numberOfBuffers;
    multiBufferSizes->maxBufferSize = maxBufferSize;
    multiBufferSizes->minBufferSize = minBufferSize;
    return 0;
}

/****************************************************************************
* pico_release_multibuffers
*
* releases Pico multibuffers
* Inputs:
* - GENERICUNIT* unit
* - Max Buffer (3D pointer array)
* - Min Buffer (3D pointer array)
* - MULTIBUFFERSIZES* multiBufferSizes
****************************************************************************/
void pico_release_multibuffers(GENERICUNIT* unit,
                                int16_t**** minBuffers, int16_t**** maxBuffers, MULTIBUFFERSIZES* multiBufferSizes)
{
    uint64_t capture = 0;
    int16_t channel = 0;
    for (channel = 0; channel < unit->channelCount; channel++)
    {

        if (unit->channelSettings[channel].enabled)
        {
            for (capture = 0; capture < multiBufferSizes->numberOfBuffers; capture++)
            {
                //if((*maxBuffers)[capture] != NULL)
                    free((*maxBuffers)[capture][channel]);
               // if ((*minBuffers)[capture] != NULL)
                    free((*minBuffers)[capture][channel]);
            }
            //numOfAnalogChs++;
        }
    }
    // Free buffers - digital channels
    for (channel = 0; channel < unit->digitalPortCount; channel++)
    {
        if (unit->digitalChannelSettings[channel].enabled)
        {
            for (capture = 0; capture < multiBufferSizes->numberOfBuffers; capture++)
            {
                //if ((*maxBuffers)[capture] != NULL)
                    free( (*maxBuffers)[capture][(channel + unit->channelCount)] );
               // if ((*minBuffers)[capture] != NULL)
                    free( (*minBuffers)[capture][(channel + unit->channelCount)] );
            }
           
        }
    }
    for (capture = 0; capture < multiBufferSizes->numberOfBuffers; capture++)
    {
           free((*maxBuffers)[capture]);
           free((*minBuffers)[capture]);
    }
    free(*maxBuffers);
    free(*minBuffers);
}
