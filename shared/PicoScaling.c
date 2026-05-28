/****************************************************************************
 *
 * Filename:    PicoScaling.c
 * Copyright:   Pico Technology Limited 2023 - 2025
 * Description:
 *
 * This file defines scaling related to all channel and probe ranges
 * with corresponding units.
 * For example - voltage/current/resistance/pressure/temperature etc.
 *
 ****************************************************************************/

#include "./PicoScaling.h"
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

// Probe and Scaling functions //

/****************************************************************************
* getRangeScaling
*
* Gets the ChannelRangeInfo(Scaling, Units etc) for a given input "ChannelRange" (enum)
* Returns false if not found, with default scale to use.
****************************************************************************/

BOOL getRangeScaling(PICO_CONNECT_PROBE_RANGE ChannelRange, PICO_PROBE_SCALING *ChannelRangeInfo)
{
    uint32_t max_index = (uint32_t) ( sizeof(PicoProbeScaling)/sizeof(PicoProbeScaling[0] ) );

    //Create Unknown_Range and set default to return if not found
    PICO_PROBE_SCALING Unknown_UnitLess = { PICO_X1_PROBE_1V,//ProbeEnum
                                                "Unknown_Range_Normailising_to_+/-1", //Probe_Range_text
                                                -1, //MinScale
                                                1,//MaxScale
                                                 "UnitLess" }; //Unit_text

    *ChannelRangeInfo = Unknown_UnitLess; //ProbeArray[6];//1V x1 range

    // search
    // ChannelRangeInfo = Array.Find(ProbeScaling.Scaling.inputRanges, x => x.ProbeEnum == ChannelRange );
    // returns -1 if not found, else the found index
    int64_t pos = -1;
    for (uint32_t i = 0; i < max_index; i++)
    {
        if (PicoProbeScaling[(uint32_t)i].ProbeEnum == ChannelRange)
        {
            pos = i;
            break;
        }
    }
    if (pos == -1)//range not found
    {
        return false;
    }
    else
    {
        *ChannelRangeInfo = PicoProbeScaling[(uint32_t)pos]; //return pointer to ProbeArray
        return true;
    }
}

/****************************************************************************
* adc_to_scaled_value
*
* Convert an 16-bit ADC count into Scaled data - voltage or Probe units
* Inputs:
* - int - raw ADC value
* - ChannelRangeInfo (Used to scale the raw data)
* - Scopes "maxADCValue" used
****************************************************************************/

double adc_to_scaled_value(int16_t raw, PICO_PROBE_SCALING ChannelRangeInfo, int16_t maxADCValue)
{
    return ((raw * (ChannelRangeInfo.MaxScale)) / (double)maxADCValue);
}

/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into Scaled data - voltage or Probe units
* Inputs:
* - int - raw ADC value
* - ChannelRange
* - Scopes "maxADCValue" used
****************************************************************************/
double adc_to_mv(int16_t raw, PICO_CONNECT_PROBE_RANGE ChannelRange, int16_t maxADCValue)
{

    if ((PICO_X1_PROBE_10MV <= ChannelRange) && (ChannelRange <= PICO_X1_PROBE_200V)) //PICO_X1_PROBE_RANGES
    {
        ChannelRange -= PICO_X1_PROBE_10MV; //to get the index for x1 ranges
        return(double)(((double)raw * (double)inputRanges[ChannelRange]) / (double)maxADCValue);
    }
    else if ((PICO_X10_PROBE_100MV <= ChannelRange) && (ChannelRange <= PICO_X10_PROBE_200V)) //PICO_X10_PROBE_RANGES
    {
		ChannelRange -= PICO_X10_PROBE_100MV; //to get the index for x10 ranges
        return(double)(((double)raw * (double)inputRangesx10[ChannelRange]) / (double)maxADCValue);
    }
    else
    {
        return 0.0f;
    }
}

/****************************************************************************
* mv_to_adc
*
* Convert Scaled value (voltage or Probe units) to ADC value
* Inputs:
* - double - "scaled" value
* - ChannelRangeInfo (Used to scale the raw data)
* - Scopes "maxADCValue" used
****************************************************************************/

int16_t mv_to_adc(double scaled, PICO_CONNECT_PROBE_RANGE ChannelRange, int16_t maxADCValue)
{
    return (int16_t)((scaled / (double)(inputRanges[ChannelRange])) * (double)maxADCValue);
}
