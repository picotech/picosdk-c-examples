#include <stdio.h>
#include <math.h>

#include "../shared/Libps4000a.h"
#include "../shared/LibStreamingps4000a.h"
#include "../../shared/PicoScaling.h"
#include "./UserSetup.h"

/****************************************************************************
* Refernce Global Variables
***************************************************************************/

extern int16_t			g_probeStateChanged;
extern USER_PROBE_INFO userProbeInfo;
/***************************************************************************/

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
	// if you are using PicoConnect Probes, check the probe state change flag and update settings accordingly
	if (g_probeStateChanged == 1)
	{
		printf("\nProbe state change detected.\n");
		g_probeStateChanged = 0;
		// ProbestoSettings() -
		// Looks at the callback probe updated array and updates the unit channel settings accordingly
		// Modifies the channel settings - Modify the function as needed for your application.
		ProbestoSettings(unit);
		setDefaults(unit); // Put these changes into effect
	}
	// Override default channel settings
	// Turn Off all channels first
	unit->channelSettings[0].enabled = FALSE;	// ChA
	unit->channelSettings[1].enabled = FALSE;	// ChB
	unit->channelSettings[2].enabled = FALSE;	// ChC
	unit->channelSettings[3].enabled = FALSE;	// ChD
	unit->channelSettings[4].enabled = FALSE;	// ChE
	unit->channelSettings[5].enabled = FALSE;	// ChF
	unit->channelSettings[6].enabled = FALSE;	// ChG
	unit->channelSettings[7].enabled = FALSE;	// ChH

	for (uint8_t i = 0; i < 2; i++) // Set up first 2 channels with custom settings
	{
		PICO_CONNECT_PROBE_RANGE userRange1 = PICO_X1_PROBE_1V;

		// Uncomment and modify the below code to set range based on probe connection status and /or user input
		// Otherwise, the channel range will be overridden by ProbestoSettings() function.
		/*
		if (ValidateChannelRange(unit, (uint8_t)i, userRange1) == 0)
			unit->channelSettings[i].range = userRange1;
		else
			return; // Range not valid for channel, return to menu or exit depending on your application needs
		*/

		unit->channelSettings[i].enabled = TRUE;									// Channel enabled
		unit->channelSettings[i].DCcoupled = PICO_DC;								// AC, DC coupling
		unit->channelSettings[i].analogueOffset = 0.0f;								// Set analogue offset voltage
		unit->channelSettings[i].bandwithLimit = PS4000A_BW_FULL; // Options: PS4000A_BW_20MHZ, PS4000A_BW_200MHZ
	}

	// Apply default settings and Channels Settings above (for non-intelligent probe models)
	if(unit->hasIntelligentProbes == FALSE)
		setDefaults(unit);

	displaySettings(unit);
	printf("\nTemplate Demo - Fast Streaming Example\n");
	
	///////////////////// Add data Aquisition functions here /////////////////////
	// Set buffer size for streaming mode - adjust as needed, consider sample rate used (which affects how fast the buffer fills up).
	const uint64_t BufferSizeFast = pow(2, 20); // 2^20 -> 1MB (MiB), 2^21 -> 2MB (MiB)

	// Trigger disabled
	PICO_STATUS status = ps4000aSetSimpleTrigger(unit->handle, 0, PS4000A_CHANNEL_A, 0, PICO_RISING, 0, 0);
	//SetupTrigger(unit); // Set up a basic trigger on Channel A

	// Collect data in streaming mode
	// (160MS/s max. shared between Chs) (80MS/s max.(12.5ns) for one chanel
	streamDataHandler(unit,
		1024,					// noOfPreTriggerSamples - Used by RunStreaming() // trigger point at 1k samples
		BufferSizeFast - 1024,	// noOfPostTriggerSamples - Used by RunStreaming()
		12500,					// idealTimeInterval - Used by RunStreaming() // 12500
		PICO_PS,				// sampleIntervalTimeUnits - Used by RunStreaming() // PS
		BufferSizeFast,			// nSamples - Set the number of samples per capture - Used by SetDataBuffers()
		PS4000A_RATIO_MODE_NONE,// ratioMode - Used by SetDataBuffers()
		1,						// downSampleRatio - Used by SetDataBuffers()
		1,						// autostop - 0: Off or 1: Stop after trigger event
		FILE_BIN);				// Save data as Binary file	

	// ps4000a API -
	// NOTE: If downsampled data is requested in streaming mode, the driver will also pull raw data as well.

	// Device stopped, Now get more data
	// Pull Downsampled max. and min. data from the device
	printf("\nTemplate Demo - Device stopped, Now get more data\n");
	GetMoreDataHandler(unit, PS4000A_RATIO_MODE_AGGREGATE, 64, BufferSizeFast, FILE_TXT);
	// You can ask for more samples (nSamples) if they are available
}