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
#include <time.h>
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
#include <PicoStatus.h>
#endif



#endif

/****************************************************************************
* Gobal Variables
***************************************************************************/

/***************************************************************************/

/****************************************************************************
* WriteMetaDataToFile
*
* Writes scope data to a file (one file per waveform)
* Writes header info- waveform number, ttrigger sample, Over range flags
* Write sample time vaules and data as ADC counts and voltage
* Inputs:
* - Channel scaling info "enabledChannelsScaling",
* - File name,
* - Triggersample number,
* - Over range flags - "overflow"
* - CAPTURES_RANGE* - pointer to structure defining the range of captures to write, (from, to)
*   can be set to NULL for full range
* 
* Outputs:
* Write a text file to disk of current path
****************************************************************************/

void WriteMetaDataToFile(struct tGenericUnit* unit,
struct tmultiBufferSizes multiBufferSizes,
struct tPicoProbeScaling* enabledChannelsScaling,
char startOfFileName[],
uint64_t Triggersample,
struct tcaptures_range* captures_rangeIp)
{   
    FILE* fp = NULL;
    if(startOfFileName == NULL)
        startOfFileName = "PicoMetaData_";

    uint64_t i;
    struct tcaptures_range captures_range;

	if (captures_rangeIp == NULL) //Set default full range if NULL
    {   
        captures_range.from = 0;
		captures_range.to = multiBufferSizes.numberOfBuffers - 1;
    }
    else
    {
		captures_range = *captures_rangeIp; // Use the provided range
    }

    char buf[58 + (3 * sizeof(int))]= { '\0' }; // null terminate the string
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);

        // Get the current time
        time_t now = time(NULL);
        // Convert to local time
        struct tm* local_time = localtime(&now);
        //printf("Local time: %s", asctime(local_time));
        // Format the time
        char formatted_time[100];
        strftime(formatted_time, sizeof(formatted_time), "%d-%m-%Y %H:%M:%S %Z", local_time);
        //printf("\nFormatted time: %s\n", formatted_time);
        //strftime(formatted_time, sizeof(formatted_time), "%S", local_time);
		// Create file name with number of buffer sets and the seconds from the PC clock.
        snprintf(buf, buf_size, "%s_BufferSets-%d.txt", startOfFileName, (int)multiBufferSizes.numberOfBuffers);
        //snprintf(buf, buf_size, "%s_BufferSets-%d_Sec_%s.txt", startOfFileName, (int)multiBufferSizes.numberOfBuffers, formatted_time);
        fopen_s(&fp, buf, "w");
        if (fp != NULL)
        {
            fprintf(fp, "Formatted time: %s\n", formatted_time);
            //Write header lines
            if (multiBufferSizes.numberOfBuffers != 1)
            {
                fprintf(fp, "Number of Segments: %lld\n",
                    multiBufferSizes.numberOfBuffers);
                fprintf(fp, "From Seg: %lld to Seg: %lld\n",
                    captures_range.from, captures_range.to);
            }

            fprintf(fp, "SampleRate %3.3e SamplesPerBlock %lld Trigger@Sample %lld \n",
                unit->timeInterval, multiBufferSizes.maxBufferSize, Triggersample);

            // Write channel headings - Aggredated Data?
            if (multiBufferSizes.minBufferSize != 0)
            {
                fprintf(fp, "Aggredated Datasample data (Max. Min. values for each channel)");
            }
			// Write channel headings - channel names
            for (i = 0; i < unit->channelCount; i++)
            {
                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "Ch: %C\t\t", 'A' + (int)i);
                }
            }
            fprintf(fp, "\n");

            // Write channel headings - channel units
            for (i = 0; i < unit->channelCount; i++)
            {
                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "Units: %s\t", enabledChannelsScaling[i].Unit_text);
                }
            }
            fprintf(fp, "\n");

            // Write channel headings - channel ProbeEnum range
            for (i = 0; i < unit->channelCount; i++)
            {

                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "ProbeEnum: %d\t", enabledChannelsScaling[i].ProbeEnum);
                }
            }
            fprintf(fp, "\n");

			// Write channel headings - channel MaxScale and MinScale (values corresponding to Scaled max and min range)
            for (i = 0; i < unit->channelCount; i++)
            {

                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "+FS:  %4.2f\t", enabledChannelsScaling[i].MaxScale);
                }
            }
            fprintf(fp, "\n");

            for (i = 0; i < unit->channelCount; i++)
            {

                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "-FS: %4.2f\t", enabledChannelsScaling[i].MinScale);
                }
            }
            fprintf(fp, "\n");

			// Write digital port headings
            for (i = 0; i < unit->digitalPortCount; i++)
            {
                if (unit->digitalChannelSettings[i].enabled)
                {
                    fprintf(fp, "Port%d_Max\t", (int)i);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Port%d_Min\t", (int)i);
                    }
                }
            }
            fprintf(fp, "\n");
            fclose(fp);
        }
    
}

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
* - CAPTURES_RANGE* - pointer to structure defining the range of captures to write, (from, to)
*   can be set to NULL for full range
*
* Outputs:
* Writes text files to disk of current path
****************************************************************************/

void WriteArrayToFilesGeneric(struct tGenericUnit* unit,
    int16_t*** minBuffers,
    int16_t*** maxBuffers,
    struct tmultiBufferSizes multiBufferSizes,
    struct tPicoProbeScaling* enabledChannelsScaling,
    char startOfFileName[],
    uint64_t Triggersample,
    int16_t* overflow,
    struct tcaptures_range* captures_rangeIp)
{
    FILE* fp = NULL;
    if (startOfFileName == NULL)
        startOfFileName = "Pico_BufferCaptureN_";

    uint64_t i;
    uint64_t capture;
    struct tcaptures_range captures_range;

    if (captures_rangeIp == NULL) //Set default full range if NULL
    {
        captures_range.from = 0;
        captures_range.to = multiBufferSizes.numberOfBuffers - 1;
    }
    else
    {
        captures_range = *captures_rangeIp; // Use the provided range
    }

    char buf[58 + (3 * sizeof(int))] = { '\0' }; // null terminate the string
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);

    for (capture = captures_range.from; capture <= captures_range.to; capture++)
    {
        //Goto next file
        snprintf(buf, buf_size, "%s%d.txt", startOfFileName, (int)capture);
        fopen_s(&fp, buf, "w");
        if (fp != NULL)
        {
            //Write 2 header lines (one for Info, one for Channels)
            if (multiBufferSizes.numberOfBuffers != 1)
                fprintf(fp, "Segment: %lld of %lld Segment(s)\n",
                    capture, multiBufferSizes.numberOfBuffers);

            fprintf(fp, "SampleRate %3.3e SamplesPerBlock %lld Trigger@Sample %lld \n",
                unit->timeInterval, multiBufferSizes.maxBufferSize, Triggersample);

            //overrange flags
            if (overflow != NULL)
            {
                fprintf(fp, "OverRange flag: ");
                i = 10; // upto 2 digital ports + 8 analog channels (CHAR_BIT * sizeof integer)
                while (i--)
                {
                    fprintf(fp, "%d", ((uint16_t)overflow[capture] >> i) & 1);
                }
                fprintf(fp, " (LSB ChA)\n");
            }
            // Write time and channel headings
            fprintf(fp, "Time(s) ");

            for (i = 0; i < unit->channelCount; i++)
            {

                if (unit->channelSettings[i].enabled)
                {
                    fprintf(fp, "Ch%C_Max-ADC Max_%s ", 'A' + (int16_t)i, enabledChannelsScaling[i].Unit_text);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Min-ADC Min_V ");//fprintf(fp, "Min-ADC Min_mV ");
                    }
                }
            }
            // Write digital port headings
            for (i = 0; i < unit->digitalPortCount; i++)
            {
                if (unit->digitalChannelSettings[i].enabled)
                {
                    fprintf(fp, "Port%d_Max ", (int)i);  //fprintf(fp, "Ch%C_Max-ADC Max_mV ", 'A' + (int)i);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Port%d_Min ", (int)i);
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
                // Print digital port data
                for (int j = 0; j < unit->digitalPortCount; j++)
                {
                    if (unit->digitalChannelSettings[j].enabled)
                    {
                        fprintf(fp, "0x%02X ", (0x00FF & maxBuffers[capture][unit->channelCount + j][i]));
                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            fprintf(fp, "0x%02X ", (0x00FF & minBuffers[capture][unit->channelCount + j][i]));
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
* WriteArrayToFilesBinary
*
* Writes scope ADC values data to a binary file (one file per waveform)
* Inputs:
* - pointer to double - "scaled" values of 3D arrays ADC counts (Max and Min values if used)
* - Channel scaling info "enabledChannelsScaling",
* - File name,
* - Triggersample number,
* - Over range flags - "overflow"
* - CAPTURES_RANGE* - pointer to structure defining the range of captures to write, (from, to)
*   can be set to NULL for full range
*
* Outputs:
* Writes binary files to disk of current path
****************************************************************************/

void WriteArrayToFilesBinary(struct tGenericUnit* unit,
    int16_t*** minBuffers,
    int16_t*** maxBuffers,
    struct tmultiBufferSizes multiBufferSizes,
    struct tPicoProbeScaling* enabledChannelsScaling,
    char startOfFileName[],
    uint64_t Triggersample,
    int16_t* overflow,
    struct tcaptures_range* captures_rangeIp)
{
    FILE* fp = NULL;
    if (startOfFileName == NULL)
        startOfFileName = "Pico_BufferCaptureBinaryN_";

    uint64_t capture;
    struct tcaptures_range captures_range;

    if (captures_rangeIp == NULL) //Set default full range if NULL
    {
        captures_range.from = 0;
        captures_range.to = multiBufferSizes.numberOfBuffers - 1;
    }
    else
    {
        captures_range = *captures_rangeIp; // Use the provided range
    }

    char buf[58 + (3 * sizeof(int))] = { '\0' }; // null terminate the string
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);
    size_t result = 0;

    for (capture = captures_range.from; capture <= captures_range.to; capture++)
    {
        //Goto next file
        snprintf(buf, buf_size, "%s%d.bin", startOfFileName, (int)capture);
        fopen_s(&fp, buf, "wb");
        if (fp != NULL)
        {
            // Write channel data
			// Add fwrite for overflow flags?
                for (int j = 0; j < unit->channelCount; j++)
                {
                    if (unit->channelSettings[j].enabled)
                    {
                        result = fwrite( (maxBuffers)[capture][j], sizeof(int16_t), multiBufferSizes.maxBufferSize, fp);
                        if (!result)
							break; // Check if fwrite was successful, break if not
                        //printf("maxBuffers fwrite result: %zu\n", result); // Check if fwrite was successful

                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            result = fwrite((minBuffers)[capture][j], sizeof(int16_t), multiBufferSizes.minBufferSize, fp);
                            if (!result)
                                break; // Check if fwrite was successful, break if not
                            //printf("minBuffers fwrite result: %zu\n", result); // Check if fwrite was successful
                        }
                    }
                // digital port data
                for (int j = 0; j < unit->digitalPortCount; j++)
                {
                    if (unit->digitalChannelSettings[j].enabled)
                    {
                        result = fwrite((maxBuffers)[capture][unit->channelCount + j], sizeof(int16_t), multiBufferSizes.maxBufferSize, fp);
                        if (!result)
                            break; // Check if fwrite was successful, break if not
                        //printf("maxBuffers fwrite result: %zu\n", result); // Check if fwrite was successful
                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            result = fwrite((minBuffers)[capture][unit->channelCount + j], sizeof(int16_t), multiBufferSizes.minBufferSize, fp);
                            if (!result)
                                break; // Check if fwrite was successful, break if not
                            //printf("minBuffers fwrite result: %zu\n", result); // Check if fwrite was successful
                        }
                    }
                }
            }
            fclose(fp);
            if (!result)
                printf("fwrite result failed: %zu\n", result);
        }
    }
}

/****************************************************************************
* WriteArrayToStdoutGeneric
*
* Writes scope data to a file (one file per waveform)
* Writes header info- waveform number, ttrigger sample, Over range flags
* Write sample time vaules and data as ADC counts and voltage
* Inputs:
* - pointer to double - "scaled" values of 3D arrays ADC counts (Max and Min values if used)
* - Channel scaling info "enabledChannelsScaling",
* - File name,
* - Triggersample number,
* - CaptureMode - (block, rapid block or streaming),
* - numberOfBuffers to write
* - numberOfSamples to write,
* - Over range flags - "overflow"
* Outputs:
* Writes text to stdout/console
****************************************************************************/

void WriteArrayToStdoutGeneric(struct tGenericUnit* unit,
    int16_t*** minBuffers,
    int16_t*** maxBuffers,
    struct tmultiBufferSizes multiBufferSizes,
    struct tPicoProbeScaling* enabledChannelsScaling,
    enum enCaptureMode CaptureMode,
    int16_t numberOfBuffers,
    uint64_t numberOfSamples,
    uint64_t Triggersample,
    int16_t* overflow)
    {  
        uint64_t i;
        uint64_t capture;

        numberOfBuffers = min(multiBufferSizes.numberOfBuffers, numberOfBuffers);
        numberOfSamples = min(multiBufferSizes.maxBufferSize, numberOfSamples);

        for (capture = 0; capture < numberOfBuffers; capture++)
        {    
            //Write header lines
            printf("Outputting the first: %lld samples...\n",
                numberOfSamples);
            if (CaptureMode != (enum enCaptureMode)BLOCK)
            {
                printf("Capture: %lld of %lld Captures\n",
                    capture, multiBufferSizes.numberOfBuffers);
                printf("Outputting the first: %d Captures\n",
                    numberOfBuffers);
            }
            printf("SampleRate %3.3e SamplesPerBlock %lld Trigger@Sample %lld \n",
                unit->timeInterval, multiBufferSizes.maxBufferSize, Triggersample);
            //overrange flags
            printf("OverRange flags: ");
            i = 10; // upto 2 digital ports + 8 analog channels (CHAR_BIT * sizeof integer)
            while (i--)
            {
                    printf("%d", ((uint16_t)overflow[capture] >> i) & 1);
            }
            printf(" (LSB ChA)\n");
            // Write time and channel headings
            printf("Time(s) \t");

            for (i = 0; i < unit->channelCount; i++)
            {
                printf("Ch:%C Max %s\t", 'A' + (uint16_t)i, (enabledChannelsScaling[i].Unit_text));
            }
            printf("\n");
            // Write time and channel data
            for (i = 0; i < numberOfSamples; i++)
            {
                printf("%3.3e\t", i * unit->timeInterval);
                for (int j = 0; j < unit->channelCount; j++)
                {
                    if (unit->channelSettings[j].enabled)
                    {
                        printf("%+3.3e\t",  //printf("%+5d %+3.3e\t",
                            //maxBuffers[capture][j][i],
                            adc_to_scaled_value((maxBuffers)[capture][j][i], enabledChannelsScaling[PICO_CHANNEL_A + j], unit->maxADCValue)
                        );
                        /*
                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            printf("%+3.3e\t", //printf("%+5d %+3.3e\t",
                                //minBuffers[capture][j][i],
                                adc_to_scaled_value((minBuffers)[capture][j][i], enabledChannelsScaling[PICO_CHANNEL_A + j], unit->maxADCValue)
                            );
                        }*/
                    }
                    else
                    {
                        printf("---     \t");
                    }
                }
                if (unit->channelCount != 0)
                    printf("\n");
            }
            printf("\n");
		    // Print digital port headings and data
            for (i = 0; i < unit->digitalPortCount; i++)
            {
                printf("Port %d:\t\t", (int)i);
            }
            printf("\n");
            for (i = 0; i < numberOfSamples; i++)
            {
                for (int j = 0; j < unit->digitalPortCount; j++)
                {
                    if (unit->digitalChannelSettings[j].enabled)
                    {
                        
                            printf("0x%02X    \t", (0x00FF & maxBuffers[capture][unit->channelCount + j][i]));
                        /*
                        if (multiBufferSizes.minBufferSize != 0)
                        {
                        printf("0x%02X    \t", (0x00FF & minBuffers[capture][unit->channelCount + j][i]));
                        }*/
                    }
                    else
                    {
                        printf("---     \t");
                    }
                }
                if(unit->digitalPortCount != 0)
                    printf("\n");
            } 
        }
    }
