/*****************************************************************************
 * A simple streaming application to capture data on a single channel
 *
 * Copyright (C) 2023-2024 Pico Technology Ltd. See LICENSE file for terms.
 ****************************************************************************/
#include <stdio.h>
#include "windows.h"
#include <conio.h>
#include "ps6000aApi.h"



int main(void)
{
	PICO_STATUS status;
	int16_t handle;
	uint32_t sampleCount = 100000000;
	double sampleInterval = 1;
	PICO_STREAMING_DATA_INFO streamData;
	PICO_STREAMING_DATA_TRIGGER_INFO streamTrigger;
	int i;
	int16_t* streamBuffers[8];
	FILE* fp;
	int8_t streamFile[20] = "stream.csv";

	status = ps6000aOpenUnit(&handle, NULL, PICO_DR_8BIT);

	for (i = 0; i < 8; i++)
		status = ps6000aSetChannelOff(handle, i);

	for (i = 0; i < 4; i++)
		ps6000aSetDigitalPortOff(handle, i);

	status = ps6000aSetChannelOn(handle, PICO_CHANNEL_A, PICO_DC_50OHM, PICO_X1_PROBE_100MV, 0, PICO_BW_FULL);

	for (i = 0; i < 8; i++) {
		streamBuffers[i] = (int16_t*)calloc(sampleCount, sizeof(int16_t));
	}

	// Set the first data buffer for use
	status = ps6000aSetDataBuffer(handle, PICO_CHANNEL_A, streamBuffers[0], sampleCount, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, PICO_ADD);
	
	status = ps6000aRunStreaming(handle, &sampleInterval, PICO_NS, 0, 1000000, 0, 1, PICO_RATIO_MODE_RAW);	// Start continuous streaming

	streamData.bufferIndex_ = 0;
	streamData.channel_ = PICO_CHANNEL_A;
	streamData.mode_ = PICO_RATIO_MODE_RAW;
	streamData.noOfSamples_ = 0;
	streamData.overflow_ = 0;
	streamData.startIndex_ = 0;
	streamData.type_ = PICO_INT16_T;

	if (status == PICO_OK) {
		i = 0;

		while (i<8) {
			Sleep(10);
			status = ps6000aGetStreamingLatestValues(handle, &streamData, 1, &streamTrigger);	// Get the latest values
			printf("Status %d Samples %d StartIndex %d \r", status, streamData.noOfSamples_, streamData.startIndex_);

			if (status != PICO_OK) {
				printf("\nBuffer %d ready to process\n", i);
				
			// If buffers full move to next buffer
				status = ps6000aSetDataBuffer(handle, PICO_CHANNEL_A, streamBuffers[i], sampleCount, PICO_INT16_T, 0, PICO_RATIO_MODE_RAW, PICO_ADD);
				i++;
			}
		}
	}

	status = ps6000aStop(handle);

	fopen_s(&fp, streamFile, "w");

	for (i = 0; i < 100; i++) {
		fprintf(fp, "%5d, %5d, %5d, %5d,", streamBuffers[0][i], streamBuffers[1][i], streamBuffers[1][i], streamBuffers[3][i]);
		fprintf(fp, "%5d, %5d, %5d, %5d\n", streamBuffers[4][i], streamBuffers[5][i], streamBuffers[6][i], streamBuffers[7][i]);
	}

	status = ps6000aCloseUnit(handle);

	for (i = 0; i < 8; i++)
	{
		free(streamBuffers[i]);
	}

}


