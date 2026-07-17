/****************************************************************************
 *
 * Filename:    PicoFileFunctions.c
 * Copyright:   Pico Technology Limited 2026
 * Description:
 *
 * This file defines file writing functions for PicoScope data.
 *
 ****************************************************************************/

#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include "./PicoUnit.h"
#include "./PicoFileFunctions.h"
#include "./PicoScaling.h"
#include "./PicoBuffers.h"
#include "./PicoPlotting.h"

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

#define TEXT_FILE_EXTENSION ".csv"
#define SEPARATOR ","

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
* can be set to NULL for full range
* * Outputs:
* Write a text file to disk of current path
****************************************************************************/

void WriteMetaDataToFile(struct tGenericUnit* unit,
    struct tmultiBufferSizes multiBufferSizes,
    struct tPicoProbeScaling* enabledChannelsScaling,
    char startOfFileName[],
    uint64_t Triggersample,
    struct tcaptures_range* captures_rangeIp)
{
    const char* separator = SEPARATOR; // a string defining the delimiter (e.g., "," or "\t")
    FILE* fp = NULL;
    if (startOfFileName == NULL)
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

    char buf[58 + (3 * sizeof(int))] = { '\0' }; // null terminate the string
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);

    // Get the current time
    time_t now = time(NULL);
    // Convert to local time
    struct tm* local_time = localtime(&now);

    // Format the time
    char formatted_time[100];
    strftime(formatted_time, sizeof(formatted_time), "%d-%m-%Y %H:%M:%S %Z", local_time);

    // Create file name with number of buffer sets
    snprintf(buf, buf_size, "%s_BufferSets-%d" TEXT_FILE_EXTENSION, startOfFileName, (int)multiBufferSizes.numberOfBuffers);
    fopen_s(&fp, buf, "w");

    if (fp != NULL)
    {
        // Write time using the separator
        fprintf(fp, "Formatted time:%s%s\n", separator, formatted_time);

        // Write header lines
        if (multiBufferSizes.numberOfBuffers != 1)
        {
            fprintf(fp, "Number of Segments:%s%" PRIu64 "\n", separator, multiBufferSizes.numberOfBuffers);
            fprintf(fp, "From Seg:%s%" PRIu64 "%sto Seg:%s%" PRIu64 "\n",
                separator, captures_range.from, separator, separator, captures_range.to);
        }

        fprintf(fp, "SampleRate%s%3.3e%sSamplesPerBlock%s%" PRIu64 "%sTrigger@Sample%s%" PRIu64 "\n",
            separator, unit->timeInterval, separator, separator, multiBufferSizes.maxBufferSize, separator, separator, Triggersample);

        // Write channel headings - Aggregated Data
        if (multiBufferSizes.minBufferSize != 0)
        {
            fprintf(fp, "Aggregated Datasample data (Max. Min. values for each channel)\n");
        }

        // Write channel headings - channel names
        fprintf(fp, "Ch%s", separator);
        for (i = 0; i < unit->channelCount; i++)
        {
            if (unit->channelSettings[i].enabled)
            {
                fprintf(fp, "%c%s", 'A' + (int)i, separator);
            }
        }
        fprintf(fp, "\n");

        // Write channel headings - channel units
        fprintf(fp, "Units%s", separator);
        for (i = 0; i < unit->channelCount; i++)
        {
            if (unit->channelSettings[i].enabled)
            {
                fprintf(fp, "%s%s", enabledChannelsScaling[i].Unit_text, separator);
            }
        }
        fprintf(fp, "\n");

        // Write channel headings - channel ProbeEnum range
        fprintf(fp, "ProbeEnum%s", separator);
        for (i = 0; i < unit->channelCount; i++)
        {
            if (unit->channelSettings[i].enabled)
            {
                fprintf(fp, "%d%s", enabledChannelsScaling[i].ProbeEnum, separator);
            }
        }
        fprintf(fp, "\n");

        // Write channel headings - channel MaxScale and MinScale 
        fprintf(fp, "'+FS%s", separator);
        for (i = 0; i < unit->channelCount; i++)
        {
            if (unit->channelSettings[i].enabled)
            {
                fprintf(fp, "%4.2f%s", enabledChannelsScaling[i].MaxScale, separator);
            }
        }
        fprintf(fp, "\n");
        fprintf(fp, "'-FS%s", separator);
        for (i = 0; i < unit->channelCount; i++)
        {
            if (unit->channelSettings[i].enabled)
            {
                fprintf(fp, "%4.2f%s", enabledChannelsScaling[i].MinScale, separator);
            }
        }
        fprintf(fp, "\n");

        // Write digital port headings
        for (i = 0; i < unit->digitalPortCount; i++)
        {
            if (unit->digitalChannelSettings[i].enabled)
            {
                fprintf(fp, "Port%d_Max%s", (int)i, separator);
                if (multiBufferSizes.minBufferSize != 0)
                {
                    fprintf(fp, "Port%d_Min%s", (int)i, separator);
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
* can be set to NULL for full range
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
    const char* separator = SEPARATOR; // a string defining the delimiter (e.g., "," or "\t")
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
        snprintf(buf, buf_size, "%s%d" TEXT_FILE_EXTENSION, startOfFileName, (int)capture);
        fopen_s(&fp, buf, "w");
        if (fp != NULL)
        {
            //Write 2 header lines (one for Info, one for Channels)
            if (multiBufferSizes.numberOfBuffers != 1)
                fprintf(fp, "Segment:%s %" PRIu64 "%sof%s%" PRIu64 "%sSegment(s)\n",
                    separator, capture, separator, separator, multiBufferSizes.numberOfBuffers, separator);

            fprintf(fp, "SampleRate%s%3.3e%sSamplesPerBlock%s%" PRIu64 "%sTrigger@Sample%s%" PRIu64 "\n",
                separator, unit->timeInterval, separator, separator, multiBufferSizes.maxBufferSize, separator, separator, Triggersample);

            //overrange flags
            if (overflow != NULL)
            {
                fprintf(fp, "OverRange flag:%s", separator);
                i = 10; // upto 2 digital ports + 8 analog channels (CHAR_BIT * sizeof integer)
                while (i--)
                {
                    fprintf(fp, "%d", ((uint16_t)overflow[capture] >> i) & 1);
                }
                fprintf(fp, "%s(LSB ChA)\n", separator);
            }
            // Write time and channel headings
            fprintf(fp, "Time(s)%s", separator);

            for (i = 0; i < unit->channelCount; i++)
            {
                if (unit->channelSettings[i].enabled)
                {
                    // Note: Changed %C to %c for standard C compliance
                    fprintf(fp, "Ch%c_Max-ADC%sMax_%s%s", 'A' + (int16_t)i, separator, enabledChannelsScaling[i].Unit_text, separator);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Min-ADC%sMin_V%s", separator, separator);
                    }
                }
            }
            // Write digital port headings
            for (i = 0; i < unit->digitalPortCount; i++)
            {
                if (unit->digitalChannelSettings[i].enabled)
                {
                    fprintf(fp, "Port%d_Max%s", (int)i, separator);
                    if (multiBufferSizes.minBufferSize != 0)
                    {
                        fprintf(fp, "Port%d_Min%s", (int)i, separator);
                    }
                }
            }
            fprintf(fp, "\n");

            // Write time and channel data
            for (i = 0; i < multiBufferSizes.maxBufferSize; i++)
            {
                fprintf(fp, "%3.3e%s", i * unit->timeInterval, separator);

                for (int j = 0; j < unit->channelCount; j++)
                {
                    if (unit->channelSettings[j].enabled)
                    {
                        fprintf(fp,
                            "%+5d%s%+3.3e%s",
                            maxBuffers[capture][j][i], separator,
                            adc_to_scaled_value((maxBuffers)[capture][j][i], enabledChannelsScaling[PICO_CHANNEL_A + j], unit->maxADCValue), separator
                        );

                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            fprintf(fp,
                                "%+5d%s%+3.3e%s",
                                minBuffers[capture][j][i], separator,
                                adc_to_scaled_value((minBuffers)[capture][j][i], enabledChannelsScaling[PICO_CHANNEL_A + j], unit->maxADCValue), separator
                            );
                        }
                    }
                }
                // Print digital port data
                for (int j = 0; j < unit->digitalPortCount; j++)
                {
                    if (unit->digitalChannelSettings[j].enabled)
                    {
                        fprintf(fp, "0x%02X%s", (0x00FF & maxBuffers[capture][unit->channelCount + j][i]), separator);
                        if (multiBufferSizes.minBufferSize != 0)
                        {
                            fprintf(fp, "0x%02X%s", (0x00FF & minBuffers[capture][unit->channelCount + j][i]), separator);
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
            printf("Outputting the first: %" PRIu64 " samples...\n",
                numberOfSamples);
            if (CaptureMode != (enum enCaptureMode)BLOCK)
            {
                printf("Capture: %" PRIu64 " of %" PRIu64 " Captures\n",
                    capture, multiBufferSizes.numberOfBuffers);
                printf("Outputting the first: %d Captures\n",
                    numberOfBuffers);
            }
            printf("SampleRate %3.3e SamplesPerBlock %" PRIu64 " Trigger@Sample %" PRIu64 " \n",
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
                printf("Ch:%c Max %s\t", 'A' + (int)i, (enabledChannelsScaling[i].Unit_text));
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

    /****************************************************************************
* WriteArrayToImage
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
* can be set to NULL for full range
*
* Outputs:
* Writes image files to disk of current path
****************************************************************************/

    void WriteArrayToImage(struct tGenericUnit* unit,
        int16_t*** minBuffers,
        int16_t*** maxBuffers,
        struct tmultiBufferSizes multiBufferSizes,
        struct tPicoProbeScaling* enabledChannelsScaling,
        char startOfFileName[],
        uint64_t Triggersample,
        int16_t* overflow,
        uint32_t plotChannelMask,
        struct tcaptures_range* captures_rangeIp)
{
    /* Maximum number of data points passed to the plot renderer.
     * Samples are decimated to this count when the capture is larger,
     * since the output image is 1920 px wide and extra points add render
     * time without adding visible detail. Adjust to match image width. */
    const size_t PLOT_MAX_POINTS = 1920;

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

    int numEnabledChannels = 0;
    for (i = 0; i < unit->channelCount; i++) {
        if (unit->channelSettings[i].enabled &&
            (plotChannelMask == 0 || (plotChannelMask & (1u << i)))) {
            numEnabledChannels++;
        }
    }

    char buf[58 + (3 * sizeof(int))] = { '\0' }; // null terminate the string
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);

    for (capture = captures_range.from; capture <= captures_range.to; capture++)
    {
        //Goto next file
        snprintf(buf, buf_size, "%s%d.png", startOfFileName, (int)capture);

        //if (numEnabledChannels > 0) {
            double** plotDataArray = (double**)malloc(numEnabledChannels * sizeof(double*));
            int* channelIndices   = (int*)malloc(numEnabledChannels * sizeof(int));
            if (plotDataArray && channelIndices) {
                size_t numSamples = multiBufferSizes.maxBufferSize;

                /* Decimate: take every Nth sample so the plot array is no
                 * larger than PLOT_MAX_POINTS. stride=1 means no decimation. */
                size_t stride = (numSamples > PLOT_MAX_POINTS) ? numSamples / PLOT_MAX_POINTS : 1;
                size_t numPlotSamples = (numSamples + stride - 1) / stride; // ceiling division

                int activeIndex = 0;
                for (i = 0; i < unit->channelCount; i++) {
                    if (unit->channelSettings[i].enabled &&
                        (plotChannelMask == 0 || (plotChannelMask & (1u << i)))) {
                        channelIndices[activeIndex] = (int)i;
                        plotDataArray[activeIndex] = (double*)malloc(numPlotSamples * sizeof(double));
                        if (plotDataArray[activeIndex]) {
                            /* Fill decimated array: index p maps to raw sample p*stride */
                            for (size_t p = 0; p < numPlotSamples; p++) {
                                size_t s = p * stride;
                                plotDataArray[activeIndex][p] =
                                    adc_to_scaled_value((maxBuffers)[capture][i][s], enabledChannelsScaling[PICO_CHANNEL_A + i], unit->maxADCValue);
                            }
                        }
                        activeIndex++;
                    }
                }

                // Choose SI prefix for y-axis based on max absolute value across all series
                double maxAbsY = 0.0;
                for (int j = 0; j < numEnabledChannels; j++) {
                    if (plotDataArray[j]) {
                        for (size_t s = 0; s < numPlotSamples; s++) {
                            double v = plotDataArray[j][s];
                            if (v < 0.0) v = -v;
                            if (v > maxAbsY) maxAbsY = v;
                        }
                    }
                }

                double yScale;
                const char *yLabel;
                if (maxAbsY > 0.0 && maxAbsY < 1e-9) {
                    yScale = 1e12; yLabel = "p";
                } else if (maxAbsY > 0.0 && maxAbsY < 1e-6) {
                    yScale = 1e9;  yLabel = "n";
                } else if (maxAbsY > 0.0 && maxAbsY < 1e-3) {
                    yScale = 1e6;  yLabel = "u";
                } else if (maxAbsY > 0.0 && maxAbsY < 1.0) {
                    yScale = 1e3;  yLabel = "m";
                } else {
                    yScale = 1.0;  yLabel = "";
                }

                if (yScale != 1.0) {
                    for (int j = 0; j < numEnabledChannels; j++) {
                        if (plotDataArray[j]) {
                            for (size_t s = 0; s < numPlotSamples; s++) {
                                plotDataArray[j][s] *= yScale;
                            }
                        }
                    }
                }

                /* Choose SI time unit based on full capture duration (use numSamples,
                 * not numPlotSamples, so the axis label reflects the real time span) */
                double maxTime = (numSamples > 0 ? numSamples - 1 : 0) * unit->timeInterval;
                double timeScale;
                const char *xLabel;
                if (maxTime < 1e-9) {
                    timeScale = 1e12; xLabel = "Time (ps)";
                } else if (maxTime < 1e-6) {
                    timeScale = 1e9;  xLabel = "Time (ns)";
                } else if (maxTime < 1e-3) {
                    timeScale = 1e6;  xLabel = "Time (us)";
                } else if (maxTime < 1.0) {
                    timeScale = 1e3;  xLabel = "Time (ms)";
                } else {
                    timeScale = 1.0;  xLabel = "Time (s)";
                }

                double *xTimeData = (double *)malloc(numPlotSamples * sizeof(double));
                if (xTimeData) {
                    for (size_t p = 0; p < numPlotSamples; p++) {
                        size_t s = p * stride;
                        xTimeData[p] = ((double)s - (double)Triggersample) * unit->timeInterval * timeScale;
                    }
                    PlotMultiDataToImage(xTimeData, plotDataArray, channelIndices, numEnabledChannels, numPlotSamples, xLabel, yLabel, buf);
                    free(xTimeData);
                } else {
                    PlotMultiYDataToImage(plotDataArray, numEnabledChannels, numPlotSamples, buf);
                }

                for (int j = 0; j < numEnabledChannels; j++) {
                    free(plotDataArray[j]);
                }
            }
            free(plotDataArray);
            free(channelIndices);
        //}
        
    }

    



}
