/****************************************************************************
 *
 * Filename:    PicoBuffers.c
 * Copyright:   Pico Technology Limited 2025
 * Description:
 *
 * This header defines 
 *
 ****************************************************************************/
#include <stdio.h>
#include "./PicoBuffers.h"
#include "./PicoUnit.h"
//#include "../ps6000a-NEW/shared/Libps60000a.h" //TEMPORARY for UNIT struct

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
//#include "ps6000aApi.h"
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
void data_buffer_sizes(PICO_RATIO_MODE downSampleRatioMode, uint64_t downSampleRatio, uint64_t noOfSamples, int32_t* maxBufferSize, int32_t* minBufferSize)
{
    float x, y;
    x = noOfSamples;
    y = downSampleRatio;

             switch(downSampleRatioMode)
            {
                case 0:
                *maxBufferSize = (int32_t)noOfSamples;
                *minBufferSize = 0;
                break;

                case PICO_RATIO_MODE_RAW:
                *maxBufferSize = (int32_t)noOfSamples;
                *minBufferSize = 0;
                break;

				case PICO_RATIO_MODE_AGGREGATE:
                // Min. buffer size = (Remainder + Quotient (rounded) )
                // of noOfSamples/downSampleRatio
                // 
                //x = noOfSamples;
				//y = downSampleRatio;
                *maxBufferSize = (int32_t)((x - y * floor(x / y) ) + (round(floor( x / y) )));
                *minBufferSize = *maxBufferSize;
                break;

				default: // For for other modes - PICO_RATIO_MODE_DECIMATE, PICO_RATIO_MODE_AVERAGE
                //do the same as above but set mixBufferSize = 0.
                // 
                //x = noOfSamples;
                //y = downSampleRatio;
                *maxBufferSize = (int32_t)((x - y * floor(x / y) ) + (round(floor(x / y) )));
                *minBufferSize = 0;
                break;
            }
}

/****************************************************************************
* pico_create_multibuffers
*
* Creates buffers with the correct size for the given settings
* Inputs:
* - UNIT* unit
* - BUFFER_SETTINGS bufferSettings
* - numberOfBuffers
* Outputs:
* - Max Buffer (3D pointer array)
* - Min Buffer (3D pointer array)

****************************************************************************/

void pico_create_multibuffers(GENERICUNIT* unit, BUFFER_SETTINGS bufferSettings,
    int32_t numberOfBuffers, int16_t**** minBuffers, int16_t**** maxBuffers, MULTIBUFFERSIZES* multiBufferSizes)
{
    
	// Calulate buffer sizes   
    int32_t maxBufferSize=0;
    int32_t minBufferSize=0;
    data_buffer_sizes(bufferSettings.downSampleRatioMode,
                        bufferSettings.downSampleRatio,
                        bufferSettings.nSamples,
                        &maxBufferSize,
                        &minBufferSize);
    
    // Create buffers 
    *minBuffers = (int16_t***)calloc(unit->channelCount, sizeof(int16_t*));
	*maxBuffers = (int16_t***)calloc(unit->channelCount, sizeof(int16_t*));

    for (int16_t channel = 0; channel < unit->channelCount; channel++)
    {
        if (unit->channelSettings[channel].enabled)
        {
            //if ( (*minBuffers)[channel] != NULL )
                (*minBuffers)[channel] = (int16_t**)calloc(numberOfBuffers, sizeof(int16_t*));
            //if ( (*maxBuffers)[channel] != NULL)
			    (*maxBuffers)[channel] = (int16_t**)calloc(numberOfBuffers, sizeof(int16_t*));

            for (int32_t capture = 0; capture < numberOfBuffers; capture++)
            {
                   
                if( (*minBuffers)[channel] != NULL )
                    (*minBuffers)[channel][capture] = (int16_t*)calloc(minBufferSize, sizeof(int16_t));
                if( (*maxBuffers)[channel] != NULL)
                    (*maxBuffers)[channel][capture] = (int16_t*)calloc(maxBufferSize, sizeof(int16_t));
            }
        }
    }
	// Add sizes of the buffers to MULTIBUFFERSIZES struture
	multiBufferSizes->numberOfBuffers = numberOfBuffers;
	multiBufferSizes->maxBufferSize = maxBufferSize;
	multiBufferSizes->minBufferSize = minBufferSize;
}