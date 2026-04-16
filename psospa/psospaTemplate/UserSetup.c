#include <stdio.h>
#include <math.h>

#include "../shared/Libpsospa.h"
#include "../shared/LibStreamingpsospa.h"
#include "../../shared/PicoScaling.h"
#include "./UserSetup.h"

/****************************************************************************
* userSetup
* Controls and sets up the selected unit
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void userSetup(GENERICUNIT* unit)
{
	// Add/remove C files in shared folder to include different functionality
	// 
	// Override default channel settings
	// Turn Off all channels first
	unit->channelSettings[0].enabled = FALSE;	// ChA
	unit->channelSettings[1].enabled = FALSE;	// ChB
	unit->channelSettings[2].enabled = FALSE;	// ChC
	unit->channelSettings[3].enabled = FALSE;	// ChD
	// Enable Channel A only
	unit->channelSettings[0].enabled = TRUE;									// ChA
	unit->channelSettings[0].range = PICO_X1_PROBE_2V;							// Set range
	unit->channelSettings[0].DCcoupled = PICO_DC;								// AC, DC or 50 Ohm coupling
	unit->channelSettings[0].rangeMax = inputRanges[PICO_X1_PROBE_2V] * 1000000;// convert mV to nV
	unit->channelSettings[0].rangeMin = inputRanges[PICO_X1_PROBE_2V] * -1000000;
	unit->channelSettings[0].rangeType = PICO_X1_PROBE_NV;						// x1 probe
	unit->channelSettings[0].analogueOffset = 0.0f;								// Set analogue offset voltage
	unit->channelSettings[0].bandwithLimit = PICO_BW_FULL; // Options: PICO_BW_20MHZ, PICO_BW_200MHZ

	// Apply default settings and Channels Settings above
	setDefaults(unit);
	displaySettings(unit);						// Display the settings
	printf("\nTemplate Demo - Fast Streaming Example\n");
	SetupTrigger(unit);							// Set up a basic trigger on Channel A

	///////////////////// Add data Aquisition functions here /////////////////////
	// 
	// Streaming example
	// recommended buffer size for max/fast streaming rates (streaming mode only)
	const uint64_t BufferSizeFast = pow(2, 21); // 2^21 -> 2MB (MiB)

	// Collect data in streaming mode
	streamDataHandler(unit,
		1024,					// noOfPreTriggerSamples - Used by RunStreaming() // trigger point at 1k samples
		BufferSizeFast - 1024,	// noOfPostTriggerSamples - Used by RunStreaming()
		3400,					// idealTimeInterval - Used by RunStreaming() // (300MS/s = 3400ps)
		PICO_PS,				// sampleIntervalTimeUnits - Used by RunStreaming()
		BufferSizeFast,			// nSamples - Set the number of samples per capture - Used by SetDataBuffers()
		PICO_RATIO_MODE_RAW,	// ratioMode - Used by SetDataBuffers()
		1,						// downSampleRatio - Used by SetDataBuffers()
		0,						// autostop - 0: Off or 1: Stop after trigger event
		FILE_BIN);				// Save data as Binary file			

	// Device stopped, Now get more data
	// Pull Downsampled max. and min. data from the device
	GetMoreDataHandler(unit, PICO_RATIO_MODE_DECIMATE, 64, BufferSizeFast, FILE_TXT);
	// You can ask for more samples (nSamples) if they are available
}