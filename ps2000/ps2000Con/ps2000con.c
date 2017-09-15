/*******************************************************************************
 *
 * Filename: ps2000con.c
 *
 * Description:
 *   This is a console-mode program that demonstrates how to use the
 *   PicoScope 2000 Series (ps2000) driver API functions.
 *   
 *	Supported PicoScope models:
 *
 *		PicoScope 2104 & 2105 
 *		PicoScope 2202 & 2203 
 *		PicoScope 2204 & 2204A 
 *		PicoScope 2205 & 2205A
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *    Collect a block of samples using an advanced trigger
 *			- PicoScope 2202, 2204, 2205, 2204A and 2205A only
 *    Collect a block using ETS
 *			PicoScope 2104, 2105, 2203, 2204, 2204A , 2205 &
 *			2205A
 *    Collect a stream of data
 *    Collect a stream of data using an advanced trigger
 *			- PicoScope 2202, 2204, 2204A, 2205, and 2205A only
 *	  Collect a stream of data using an advanced trigger copying
 *	  the data from the callback
 *			- PicoScope 2202, 2204, 2204A, 2205 and 2205A only
 *    Set the signal generator with the built in signals 
 *			- PicoScope 2203, 2204, 2204A, 2205 and 2205A only
 *	  Set the signal generator with the arbitrary signal 
 *			- PicoScope 2203, 2204, 2204A, 2205 and 2205A only
 *	
 * To build this application:-
 *
 *	Windows platforms:
 *
 *		If Microsoft Visual Studio (including Express editions) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps2000.lib can be located
 *			Ensure that the ps2000.h file can be located
 *		
 *		Otherwise:
 *
 *			Set up a project for a 32-/64-bit console mode application
 *			Add this file to the project
 *			Add ps2000.lib to the project (Microsoft C only)
 *			Build the project 
 *
 *  Linux platforms:
 *
 *		Ensure that the libps2000 driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build 
 *		the ps2000con application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2006 - 2017 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>

#ifdef WIN32
#include "windows.h"
#include <conio.h>
#include "ps2000.h"
#define PREF4 _stdcall
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "libps2000-2.1/ps2000.h"
#define PREF4

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)
typedef uint8_t BYTE;
typedef enum enBOOL
{
	FALSE, TRUE
} BOOL;
/* A function to detect a keyboard press on Linux */
int32_t _getch()
{
	struct termios oldt, newt;
	int32_t ch;
	int32_t bytesWaiting;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	do
	{
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
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return bytesWaiting;
}

int32_t fopen_s(FILE ** a, const char * b, const char * c)
{
	FILE * fp = fopen(b, c);
	*a = fp;
	return (fp > 0) ? 0 : -1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

#define BUFFER_SIZE 	1024
#define BUFFER_SIZE_STREAMING 50000		// Overview buffer size
#define NUM_STREAMING_SAMPLES 1000000	// Number of streaming samples to collect
#define MAX_CHANNELS 4
#define SINGLE_CH_SCOPE 1 // Single channel scope
#define DUAL_SCOPE 2      // Dual channel scope

// AWG Parameters - 2203, 2204, 2204A, 2205 & 2205A

#define AWG_MAX_BUFFER_SIZE		4096
#define	AWG_DAC_FREQUENCY		2e6
#define AWG_DDS_FREQUENCY		48e6
#define	AWG_PHASE_ACCUMULATOR	4294967296.0

/*******************************************************
* Data types changed:
*
* int8_t   - int8_t
* int16_t  - 16-bit signed integer (int16_t)
* int32_t  - 32-bit signed integer (int32_t or int32_t)
* uint32_t - 32-bit unsigned integer (int32_t or int32_t)
*******************************************************/

int16_t values_a [BUFFER_SIZE]; // block mode buffer, Channel A
int16_t values_b [BUFFER_SIZE]; // block mode buffer, Channel B

int16_t		overflow;
int32_t		scale_to_mv = 1;

int16_t		channel_mv [PS2000_MAX_CHANNELS];
int16_t		timebase = 8;

int16_t		g_overflow = 0;

// Streaming data parameters
int16_t		g_triggered = 0;
uint32_t	g_triggeredAt = 0;
uint32_t	g_nValues;
uint32_t	g_startIndex;			// Start index in application buffer where data should be written to in streaming mode collection
uint32_t	g_prevStartIndex;		// Keep track of previous index into application buffer in streaming mode collection
int16_t		g_appBufferFull = 0;	// Use this in the callback to indicate if it is going to copy past the end of the buffer

typedef enum {
	MODEL_NONE = 0,
	MODEL_PS2104 = 2104,
	MODEL_PS2105 = 2105,
	MODEL_PS2202 = 2202,
	MODEL_PS2203 = 2203,
	MODEL_PS2204 = 2204,
	MODEL_PS2205 = 2205,
	MODEL_PS2204A = 0xA204,
	MODEL_PS2205A = 0xA205
} MODEL_TYPE;

typedef struct
{
	PS2000_THRESHOLD_DIRECTION	channelA;
	PS2000_THRESHOLD_DIRECTION	channelB;
	PS2000_THRESHOLD_DIRECTION	channelC;
	PS2000_THRESHOLD_DIRECTION	channelD;
	PS2000_THRESHOLD_DIRECTION	ext;
} DIRECTIONS;

typedef struct
{
	PS2000_PWQ_CONDITIONS			*	conditions;
	int16_t							nConditions;
	PS2000_THRESHOLD_DIRECTION		direction;
	uint32_t						lower;
	uint32_t						upper;
	PS2000_PULSE_WIDTH_TYPE			type;
} PULSE_WIDTH_QUALIFIER;


typedef struct
{
	PS2000_CHANNEL channel;
	float threshold;
	int16_t direction;
	float delay;
} SIMPLE;

typedef struct
{
	int16_t hysteresis;
	DIRECTIONS directions;
	int16_t nProperties;
	PS2000_TRIGGER_CONDITIONS * conditions;
	PS2000_TRIGGER_CHANNEL_PROPERTIES * channelProperties;
	PULSE_WIDTH_QUALIFIER pwq;
 	uint32_t totalSamples;
	int16_t autoStop;
	int16_t triggered;
} ADVANCED;


typedef struct 
{
	SIMPLE simple;
	ADVANCED advanced;
} TRIGGER_CHANNEL;

typedef struct {
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
	int16_t values [BUFFER_SIZE];
} CHANNEL_SETTINGS;

typedef struct  {
	int16_t			handle;
	MODEL_TYPE		model;
	PS2000_RANGE	firstRange;
	PS2000_RANGE	lastRange;	
	TRIGGER_CHANNEL trigger;
	int16_t			maxTimebase;
	int16_t			timebases;
	int16_t			noOfChannels;
	CHANNEL_SETTINGS channelSettings[PS2000_MAX_CHANNELS];
	int16_t			hasAdvancedTriggering;
	int16_t			hasFastStreaming;
	int16_t			hasEts;
	int16_t			hasSignalGenerator;
	int16_t			awgBufferSize;
} UNIT_MODEL; 

// Struct to help with retrieving data into 
// application buffers in streaming data capture
typedef struct
{
	UNIT_MODEL unit;
	int16_t *appBuffers[DUAL_SCOPE * 2];
	uint32_t bufferSizes[PS2000_MAX_CHANNELS];
} BUFFER_INFO;

UNIT_MODEL unitOpened;

BUFFER_INFO bufferInfo;

int32_t times[BUFFER_SIZE];

int32_t input_ranges [PS2000_MAX_RANGES] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};

/****************************************************************************
 *
 * Streaming callback
 *
 ****************************************************************************/
void  PREF4 ps2000FastStreamingReady( int16_t **overviewBuffers,
											int16_t overflow,
											uint32_t triggeredAt,
											int16_t triggered,
											int16_t auto_stop,
											uint32_t nValues)
{
	unitOpened.trigger.advanced.totalSamples += nValues;
	unitOpened.trigger.advanced.autoStop = auto_stop;

	g_triggered = triggered;
	g_triggeredAt = triggeredAt;

	g_overflow = overflow;
}

/****************************************************************************
 *
 * Streaming callback
 *
 * This demonstrates how to copy data to application buffers
 *
 ****************************************************************************/
void  PREF4 ps2000FastStreamingReady2( int16_t **overviewBuffers,
											int16_t overflow,
											uint32_t triggeredAt,
											int16_t triggered,
											int16_t auto_stop,
											uint32_t nValues)
{
	int16_t channel = 0;

	unitOpened.trigger.advanced.totalSamples += nValues;
	unitOpened.trigger.advanced.autoStop = auto_stop;

	g_triggered = triggered;
	g_triggeredAt = triggeredAt;

	g_overflow = overflow;

	if(nValues > 0 && g_appBufferFull == 0) 
	{
		for(channel = (int16_t) PS2000_CHANNEL_A; channel < DUAL_SCOPE; channel++)
		{
			if(bufferInfo.unit.channelSettings[channel].enabled)
			{
				if(unitOpened.trigger.advanced.totalSamples <= bufferInfo.bufferSizes[channel * 2] && !g_appBufferFull)
				{
					g_nValues = nValues;
				}
				else if(g_startIndex < bufferInfo.bufferSizes[channel * 2])
				{
					g_nValues = bufferInfo.bufferSizes[channel * 2] - (g_startIndex + 1); // Only copy data into application buffer up to end
					unitOpened.trigger.advanced.totalSamples = bufferInfo.bufferSizes[channel * 2]; // Total samples limited to application buffer
					g_appBufferFull = 1;
				}
				else
				{
					// g_startIndex might be >= buffer length
					g_nValues = 0;
					unitOpened.trigger.advanced.totalSamples = bufferInfo.bufferSizes[channel * 2];
					g_appBufferFull = 1;
				}

				// Copy data...

				// Max buffers
				if(overviewBuffers[channel * 2] && bufferInfo.appBuffers[channel * 2])
				{
					memcpy_s((void *) (bufferInfo.appBuffers[channel * 2] + g_startIndex), g_nValues * sizeof(int16_t), 
									(void *) (overviewBuffers[channel * 2]), g_nValues * sizeof(int16_t));

				}

				// Min buffers
			
				if(overviewBuffers[channel * 2 + 1] && bufferInfo.appBuffers[channel * 2 + 1])
				{
					memcpy_s((void *) (bufferInfo.appBuffers[channel * 2 + 1] + g_startIndex), g_nValues * sizeof(int16_t), 
									(void *) (overviewBuffers[channel * 2 + 1]), g_nValues * sizeof(int16_t));
				}
			}

		}

		g_prevStartIndex = g_startIndex;
		g_startIndex = unitOpened.trigger.advanced.totalSamples;
	}
}


/****************************************************************************
 *
 * adc_units
 *
 ****************************************************************************/
char * adc_units (int16_t time_units)
{
	time_units++;
	//printf ( "time unit:  %d\n", time_units ) ;
	switch ( time_units )
	{
		case 0:
			return "ADC";
		case 1:
			return "fs";
		case 2:
			return "ps";
		case 3:
			return "ns";
		case 4:
			return "us";
		case 5:
			return "ms";
	}
	
	return "Not Known";
}

/****************************************************************************
 * adc_to_mv
 *
 * If the user selects scaling to millivolts,
 * Convert an 12-bit ADC count into millivolts
 ****************************************************************************/
int32_t adc_to_mv (int32_t raw, int32_t ch)
{
	return ( scale_to_mv ) ? ( raw * input_ranges[ch] ) / 32767 : raw;
}

/****************************************************************************
 * mv_to_adc
 *
 * Convert a millivolt value into a 12-bit ADC count
 *
 *  (useful for setting trigger thresholds)
 ****************************************************************************/
int16_t mv_to_adc (int16_t mv, int16_t ch)
{
	return ( ( mv * 32767 ) / input_ranges[ch] );
}

/****************************************************************************
 * set_defaults - restore default settings
 ****************************************************************************/
void set_defaults (void)
{
	int16_t ch = 0;
	ps2000_set_ets ( unitOpened.handle, PS2000_ETS_OFF, 0, 0 );

	for (ch = 0; ch < unitOpened.noOfChannels; ch++)
	{
		ps2000_set_channel ( unitOpened.handle,
			                   ch,
								unitOpened.channelSettings[ch].enabled ,
								unitOpened.channelSettings[ch].DCcoupled ,
								unitOpened.channelSettings[ch].range);
	}
}

void set_trigger_advanced(void)
{
	int16_t ok = 0;
	int16_t auto_trigger_ms = 0;

	// to trigger of more than one channel set this parameter to 2 or more
	// each condition can only have on parameter set to PS2000_CONDITION_TRUE or PS2000_CONDITION_FALSE
	// if more than on condition is set then it will trigger off condition one, or condition two etc.
	unitOpened.trigger.advanced.nProperties = 1;
	// set the trigger channel to channel A by using PS2000_CONDITION_TRUE
	unitOpened.trigger.advanced.conditions = (PS2000_TRIGGER_CONDITIONS*) malloc (sizeof (PS2000_TRIGGER_CONDITIONS) * unitOpened.trigger.advanced.nProperties);
	unitOpened.trigger.advanced.conditions->channelA = PS2000_CONDITION_TRUE;
	unitOpened.trigger.advanced.conditions->channelB = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->channelC = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->channelD = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->external = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->pulseWidthQualifier = PS2000_CONDITION_DONT_CARE;

	// set channel A to rising
	// the remainder will be ignored as only a condition is set for channel A
	unitOpened.trigger.advanced.directions.channelA = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelB = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelC = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelD = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.ext = PS2000_ADV_RISING;


	unitOpened.trigger.advanced.channelProperties = (PS2000_TRIGGER_CHANNEL_PROPERTIES*) malloc (sizeof (PS2000_TRIGGER_CHANNEL_PROPERTIES) * unitOpened.trigger.advanced.nProperties);
	// there is one property for each condition
	// set channel A
	// trigger level 1500 adc counts the trigger point will vary depending on the voltage range
	// hysteresis 4096 adc counts  
	unitOpened.trigger.advanced.channelProperties->channel = (int16_t) PS2000_CHANNEL_A;
	unitOpened.trigger.advanced.channelProperties->thresholdMajor = 1500;
	// not used in level triggering, should be set when in window mode
	unitOpened.trigger.advanced.channelProperties->thresholdMinor = 0;
	// used in level triggering, not used when in window mode
	unitOpened.trigger.advanced.channelProperties->hysteresis = (int16_t) 4096;
	unitOpened.trigger.advanced.channelProperties->thresholdMode = PS2000_LEVEL;

	ok = ps2000SetAdvTriggerChannelConditions (unitOpened.handle, unitOpened.trigger.advanced.conditions, unitOpened.trigger.advanced.nProperties);
	
	ok = ps2000SetAdvTriggerChannelDirections (unitOpened.handle,
												unitOpened.trigger.advanced.directions.channelA,
												unitOpened.trigger.advanced.directions.channelB,
												unitOpened.trigger.advanced.directions.channelC,
												unitOpened.trigger.advanced.directions.channelD,
												unitOpened.trigger.advanced.directions.ext);

	ok = ps2000SetAdvTriggerChannelProperties (unitOpened.handle,
												unitOpened.trigger.advanced.channelProperties,
												unitOpened.trigger.advanced.nProperties,
												auto_trigger_ms);


	// remove comments to try triggering with a pulse width qualifier
	// add a condition for the pulse width eg. in addition to the channel A or as a replacement
	//unitOpened.trigger.advanced.pwq.conditions = malloc (sizeof (PS2000_PWQ_CONDITIONS));
	//unitOpened.trigger.advanced.pwq.conditions->channelA = PS2000_CONDITION_TRUE;
	//unitOpened.trigger.advanced.pwq.conditions->channelB = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->channelC = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->channelD = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->external = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.nConditions = 1;

	//unitOpened.trigger.advanced.pwq.direction = PS2000_RISING;
	//unitOpened.trigger.advanced.pwq.type = PS2000_PW_TYPE_LESS_THAN;
	//// used when type	PS2000_PW_TYPE_IN_RANGE,	PS2000_PW_TYPE_OUT_OF_RANGE
	//unitOpened.trigger.advanced.pwq.lower = 0;
	//unitOpened.trigger.advanced.pwq.upper = 10000;
	//ps2000SetPulseWidthQualifier (unitOpened.handle,
	//															unitOpened.trigger.advanced.pwq.conditions,
	//															unitOpened.trigger.advanced.pwq.nConditions, 
	//															unitOpened.trigger.advanced.pwq.direction,
	//															unitOpened.trigger.advanced.pwq.lower,
	//															unitOpened.trigger.advanced.pwq.upper,
	//															unitOpened.trigger.advanced.pwq.type);

	ok = ps2000SetAdvTriggerDelay (unitOpened.handle, 0, -10); 
}

/****************************************************************************
 * Collect_block_immediate
 *  this function demonstrates how to collect a single block of data
 *  from the unit (start collecting immediately)
 ****************************************************************************/
void collect_block_immediate (void)
{
	int32_t		i;
	int32_t 	time_interval;
	int16_t 	time_units;
	int16_t 	oversample;
	int32_t 	no_of_samples = BUFFER_SIZE;
	FILE *		fp;
	int16_t 	auto_trigger_ms = 0;
	int32_t 	time_indisposed_ms;
	int16_t 	overflow;
	int32_t 	max_samples;
	int16_t		ch = 0;

	printf ( "Collect block immediate...\n" );
	printf ( "Press a key to start\n" );
	_getch ();

	set_defaults ();

	/* Trigger disabled */
	ps2000_set_trigger ( unitOpened.handle, PS2000_NONE, 0, PS2000_RISING, 0, auto_trigger_ms );

	/*  Find the maximum number of samples, the time interval (in time_units),
	*		 the most suitable time units, and the maximum oversample at the current timebase
	*/
	oversample = 1;
	while (!ps2000_get_timebase ( unitOpened.handle,
                        timebase,
  					    no_of_samples,
                        &time_interval,
                        &time_units,
                        oversample,
                        &max_samples))
	timebase++;

	printf ( "timebase: %hd\toversample:%hd\n", timebase, oversample );
	
	/* Start it collecting,
	*  then wait for completion
	*/

	ps2000_run_block ( unitOpened.handle, no_of_samples, timebase, oversample, &time_indisposed_ms );
	
	while ( !ps2000_ready ( unitOpened.handle ) )
	{
		Sleep ( 100 );
	}

	ps2000_stop ( unitOpened.handle );

	/* Should be done now...
	*  get the times (in nanoseconds)
	*   and the values (in ADC counts)
	*/
	ps2000_get_times_and_values ( unitOpened.handle, times,
									unitOpened.channelSettings[PS2000_CHANNEL_A].values, 
									unitOpened.channelSettings[PS2000_CHANNEL_B].values,
									NULL,
									NULL,
									&overflow, time_units, no_of_samples );

	/* Print out the first 10 readings,
	*  converting the readings to mV if required
	*/
	printf ( "First 10 readings\n\n" );
	printf ( "Time(%s) Values\n", adc_units ( time_units ) );

	for ( i = 0; i < 10; i++ )
	{
		printf( "%d\t", times[i] );

		for ( ch = 0; ch < unitOpened.noOfChannels; ch++ )
		{
			if( unitOpened.channelSettings[ch].enabled )
  		{
				printf ( "%d\t", adc_to_mv ( unitOpened.channelSettings[ch].values[i], unitOpened.channelSettings[ch].range ) );
			}
		}
		printf("\n");
	}

	fopen_s (&fp, "data.txt","w" );
	
	for ( i = 0; i < BUFFER_SIZE; i++ )
	{
		fprintf ( fp,"%ld ", times[i] );

		for (ch = 0; ch < unitOpened.noOfChannels; ch++ )
		{
			if ( unitOpened.channelSettings[ch].enabled )
			{
				fprintf ( fp, ", %d, %d", unitOpened.channelSettings[ch].values[i],
											adc_to_mv ( unitOpened.channelSettings[ch].values[i],	unitOpened.channelSettings[ch].range) );
			}
		}

		fprintf(fp, "\n");
	}
	fclose(fp);
}

/****************************************************************************
 * Collect_block_triggered
 *  this function demonstrates how to collect a single block of data from the
 *  unit, when a trigger event occurs.
 ****************************************************************************/

void collect_block_triggered (void)
{
	int32_t		i;
	int32_t		trigger_sample;
	int32_t 	time_interval;
	int16_t 	time_units;
	int16_t 	oversample;
	int32_t 	no_of_samples = BUFFER_SIZE;
	FILE *		fp;
	int16_t 	auto_trigger_ms = 0;
	int32_t 	time_indisposed_ms;
	int16_t 	overflow;
	int32_t 	threshold_mv = 1500;
	int32_t 	max_samples;
	int16_t		ch;

	printf ( "Collect block triggered...\n" );
	printf ( "Collects when value rises past %dmV\n", threshold_mv );
	printf ( "Press a key to start...\n" );
	_getch ();

	set_defaults ();

	/* Trigger enabled
	 * ChannelA - to trigger unsing this channel it needs to be enabled using ps2000_set_channel()
	* Rising edge
	* Threshold = 100mV
	* 10% pre-trigger  (negative is pre-, positive is post-)
	*/
	unitOpened.trigger.simple.channel = PS2000_CHANNEL_A;
	unitOpened.trigger.simple.direction = (int16_t) PS2000_RISING;
	unitOpened.trigger.simple.threshold = 100.f;
	unitOpened.trigger.simple.delay = -10;

	ps2000_set_trigger ( unitOpened.handle,
							(int16_t) unitOpened.trigger.simple.channel,
							mv_to_adc (threshold_mv, unitOpened.channelSettings[(int16_t) unitOpened.trigger.simple.channel].range),
							unitOpened.trigger.simple.direction,
							(int16_t)unitOpened.trigger.simple.delay,
							auto_trigger_ms );


	/*  Find the maximum number of samples, the time interval (in time_units),
	*		 the most suitable time units, and the maximum oversample at the current timebase
	*/
	oversample = 1;
	
	while (!ps2000_get_timebase ( unitOpened.handle,
                        timebase,
  					      	    no_of_samples,
                        &time_interval,
                        &time_units,
                        oversample,
                        &max_samples))
	 timebase++;

	/* Start it collecting,
	*  then wait for completion
	*/
	ps2000_run_block ( unitOpened.handle, BUFFER_SIZE, timebase, oversample, &time_indisposed_ms );

	printf ( "Waiting for trigger..." );
	printf ( "Press a key to abort\n" );

	while (( !ps2000_ready ( unitOpened.handle )) && ( !_kbhit () ))
	{
		Sleep ( 100 );
	}

	if (_kbhit ())
	{
		_getch ();

		printf ( "data collection aborted\n" );
	}
	else
	{
		ps2000_stop ( unitOpened.handle );

		/* Get the times (in units specified by time_units)
		*  and the values (in ADC counts)
		*/
		ps2000_get_times_and_values ( unitOpened.handle,
											times,
											unitOpened.channelSettings[PS2000_CHANNEL_A].values,
											unitOpened.channelSettings[PS2000_CHANNEL_B].values,
											NULL,
											NULL,
											&overflow, time_units, BUFFER_SIZE );

		/* Print out the first 10 readings,
			*  converting the readings to mV if required
			*/
		printf ("Ten readings around trigger\n");
		printf ("Time\tValue\n");
		printf ("(ns)\t(%s)\n", adc_units (time_units));

		/* This calculation is correct for 10% pre-trigger*/
		trigger_sample = BUFFER_SIZE / 10;

		for (i = trigger_sample - 5; i < trigger_sample + 5; i++)
		{
			for (ch = 0; ch < unitOpened.noOfChannels; ch++)
			{
				if (unitOpened.channelSettings[ch].enabled)
				{
					printf ( "%d\t", adc_to_mv ( unitOpened.channelSettings[ch].values[i], unitOpened.channelSettings[ch].range) );
				}
			}
			printf("\n");
		}
 
		fopen_s (&fp, "data.txt","w" );
    
		for ( i = 0; i < BUFFER_SIZE; i++ )
		{
			fprintf ( fp,"%ld ", times[i]);

			for (ch = 0; ch < unitOpened.noOfChannels; ch++)
			{
				if(unitOpened.channelSettings[ch].enabled)
				{
					fprintf ( fp, ", %d, %d", unitOpened.channelSettings[ch].values[i],
									adc_to_mv ( unitOpened.channelSettings[ch].values[i], unitOpened.channelSettings[ch].range) );
				}
			}
		
			fprintf(fp, "\n");
		}

		fclose(fp);

	}
}

void collect_block_advanced_triggered ()
{
	int32_t		i;
	int32_t		trigger_sample;
	int32_t 	time_interval;
	int16_t 	time_units;
	int16_t 	oversample;
	int32_t 	no_of_samples = BUFFER_SIZE;
	FILE *		fp;
	int16_t 	auto_trigger_ms = 0;
	int32_t 	time_indisposed_ms;
	int16_t 	overflow;
	int32_t 	threshold_mv =1500;
	int32_t 	max_samples;
	int16_t		ch;

	printf ( "Collect block triggered...\n" );
	printf ( "Collects when value rises past %dmV\n", threshold_mv );
	printf ( "Press a key to start...\n" );
	_getch ();

	set_defaults ();

	set_trigger_advanced ();


	/*  Find the maximum number of samples, the time interval (in time_units),
	*		 the most suitable time units, and the maximum oversample at the current timebase
	*/

	oversample = 1;
	
	while (!ps2000_get_timebase ( unitOpened.handle,
                        timebase,
  					    no_of_samples,
                        &time_interval,
                        &time_units,
                        oversample,
                        &max_samples))
	  timebase++;

	/* Start it collecting,
	*  then wait for completion
	*/
	ps2000_run_block ( unitOpened.handle, BUFFER_SIZE, timebase, oversample, &time_indisposed_ms );

	printf ( "Waiting for trigger..." );
	printf ( "Press a key to abort\n" );

	while (( !ps2000_ready ( unitOpened.handle )) && ( !_kbhit () ))
	{
		Sleep ( 100 );
	}

	if (_kbhit ())
	{
		_getch ();

		printf ( "data collection aborted\n" );
	}
	else
	{
		ps2000_stop ( unitOpened.handle );

		/* Get the times (in units specified by time_units)
		 *  and the values (in ADC counts)
		 */
		ps2000_get_times_and_values ( unitOpened.handle,
											times,
											unitOpened.channelSettings[PS2000_CHANNEL_A].values,
											unitOpened.channelSettings[PS2000_CHANNEL_B].values,
											NULL,
											NULL,
											&overflow, time_units, BUFFER_SIZE );

		/* Print out the first 10 readings,
		 *  converting the readings to mV if required
		 */
		printf ("Ten readings around trigger\n");
		printf ("Time\tValue\n");
		printf ("(ns)\t(%s)\n", adc_units (time_units));

		/* This calculation is correct for 10% pre-trigger
		 */
		trigger_sample = BUFFER_SIZE / 10;

		for (i = trigger_sample - 5; i < trigger_sample + 5; i++)
		{
				for (ch = 0; ch < unitOpened.noOfChannels; ch++)
				{
					if(unitOpened.channelSettings[ch].enabled)
					{
						printf ( "%d\t", adc_to_mv ( unitOpened.channelSettings[ch].values[i], unitOpened.channelSettings[ch].range) );
					}
				}
				printf("\n");
		}
 
		fopen_s (&fp, "data.txt","w" );

		for ( i = 0; i < BUFFER_SIZE; i++ )
		{
			fprintf ( fp,"%ld ", times[i]);
				
			for (ch = 0; ch < unitOpened.noOfChannels; ch++)
			{
				if (unitOpened.channelSettings[ch].enabled)
				{
					fprintf ( fp, ", %d, %d", unitOpened.channelSettings[ch].values[i],
								adc_to_mv ( unitOpened.channelSettings[ch].values[i], 
									unitOpened.channelSettings[ch].range) );
				}
			}
			
			fprintf(fp, "\n");
		}

		fclose(fp);
	}
}


/****************************************************************************
 * Collect_block_ets
 *  this function demonstrates how to collect a block of
 *  data using equivalent time sampling (ETS).
 ****************************************************************************/

void collect_block_ets (void)
{
	int32_t		i;
	int32_t		trigger_sample;
	FILE *		fp;
	int16_t 	auto_trigger_ms = 0;
	int32_t 	time_indisposed_ms;
	int16_t 	overflow;
	int32_t  	ets_sampletime;
	int16_t ok;
	int16_t ch;

	printf ( "Collect ETS block...\n" );
	printf ( "Collects when value rises past 1500mV\n" );
	printf ( "Press a key to start...\n" );
	_getch ();

	set_defaults ();

	/* Trigger enabled
	* Channel A - to trigger unsing this channel it needs to be enabled using ps2000_set_channel
	* Rising edge
	* Threshold = 1500mV
	* 10% pre-trigger  (negative is pre-, positive is post-)
	*/
	unitOpened.trigger.simple.channel = PS2000_CHANNEL_A;
	unitOpened.trigger.simple.delay = -10.f;
	unitOpened.trigger.simple.direction = PS2000_RISING;
	unitOpened.trigger.simple.threshold = 1500.f;


	ps2000_set_trigger ( unitOpened.handle,
		(int16_t) unitOpened.trigger.simple.channel,
		mv_to_adc (1500, unitOpened.channelSettings[(int16_t) unitOpened.trigger.simple.channel].range),
		unitOpened.trigger.simple.direction ,
		(int16_t) unitOpened.trigger.simple.delay,
		auto_trigger_ms );

	/* Enable ETS in fast mode,
	* the computer will store 60 cycles
	*  but interleave only 4
	*/
	ets_sampletime = ps2000_set_ets ( unitOpened.handle, PS2000_ETS_FAST, 60, 4 );
	printf ( "ETS Sample Time is: %ld\n", ets_sampletime );
	/* Start it collecting,
	*  then wait for completion
	*/
	ok = ps2000_run_block ( unitOpened.handle, BUFFER_SIZE, timebase, 1, &time_indisposed_ms );

	printf ( "Waiting for trigger..." );
	printf ( "Press a key to abort\n" );

	while ( (!ps2000_ready (unitOpened.handle)) && (!_kbhit ()) )
	{
		Sleep (100);
	}

	if ( _kbhit () )
	{
		_getch ();
		printf ( "data collection aborted\n" );
	}
	else
	{
		ps2000_stop ( unitOpened.handle );
		/* Get the times (in microseconds)
		*  and the values (in ADC counts)
		*/
		ok = (int16_t)ps2000_get_times_and_values ( unitOpened.handle,
													times,
													unitOpened.channelSettings[PS2000_CHANNEL_A].values,
													unitOpened.channelSettings[PS2000_CHANNEL_B].values,
													NULL,
													NULL,
													&overflow,
													PS2000_PS,
													BUFFER_SIZE);

		/* Print out the first 10 readings,
		*  converting the readings to mV if required
		*/

		printf ( "Ten readings around trigger\n" );
		printf ( "(ps)\t(mv)\n");

		/* This calculation is correct for 10% pre-trigger
		*/
		trigger_sample = BUFFER_SIZE / 10;

		for ( i = trigger_sample - 5; i < trigger_sample + 5; i++ )
		{
			printf ( "%ld\t", times [i]);
			for (ch = 0; ch < unitOpened.noOfChannels; ch++)
			{
				if (unitOpened.channelSettings[ch].enabled)
				{
					printf ( "%d\t\n", adc_to_mv (unitOpened.channelSettings[ch].values[i], unitOpened.channelSettings[ch].range));
				}
			}
			printf ("\n");
		}

		fopen_s (&fp, "data.txt","w" );

		for ( i = 0; i < BUFFER_SIZE; i++ )
		{
			fprintf ( fp, "%ld,", times[i] );

			for (ch = 0; ch < unitOpened.noOfChannels; ch++)
			{
				if (unitOpened.channelSettings[ch].enabled)
				{
					fprintf ( fp, "%d, %d", times[i], unitOpened.channelSettings[ch].values[i], 
						adc_to_mv (unitOpened.channelSettings[ch].values[i], unitOpened.channelSettings[ch].range) );
				}
			}

			fprintf (fp, "\n");
		}
		fclose( fp );
	}
}

/****************************************************************************
 *
 * Collect_streaming
 *  this function demonstrates how to use streaming.
 *
 * In this mode, you can collect data continuously.
 *
 * This example writes data to disk...
 * don't leave it running too int32_t or it will fill your disk up!
 *
 * Each call to ps2000_get_times_and_values() returns the readings since the
 * last call
 *
 * The time is in microseconds: it will wrap around at 2^32 (approx 2,000 seconds)
 * if you don't need the time, you can just call ps2000_get_values()
 *
 ****************************************************************************/

void collect_streaming (void)
{
	int32_t		i;
	int32_t		block_no;
	FILE *		fp;
	int32_t		no_of_values;
	int16_t		overflow;
	int32_t 	ok;
	int16_t		ch;

	printf ( "Collect streaming...\n" );
	printf ( "Data is written to disk file (test.out)\n" );
	printf ( "Press a key to start\n" );
	_getch ();

	set_defaults ();

	/* You cannot use triggering for the start of the data...
	*/
	ps2000_set_trigger ( unitOpened.handle, PS2000_NONE, 0, 0, 0, 0 );

	/* Collect data at 10ms intervals
	* Max BUFFER_SIZE points on each call
	*  (buffer must be big enough for max time between calls
	*
	*  Start it collecting,
	*  then wait for trigger event
	*  NOTE: The actual sampling interval used by the driver might not be that which is specified below. Use a sampling interval
	*  based on the sampling intervals returned by the ps2000_get_timebase() function to work out the most appropriate sampling interval to use. 
	*/
	ok = ps2000_run_streaming ( unitOpened.handle, 10, 1000, 0 );
	printf ( "OK: %d\n", ok );

	/* From here on, we can get data whenever we want...
	*/
	block_no = 0;
	fopen_s (&fp, "data.txt", "w" );
	
	while ( !_kbhit() )
	{
		no_of_values = ps2000_get_values ( unitOpened.handle, 
			unitOpened.channelSettings[PS2000_CHANNEL_A].values,
			unitOpened.channelSettings[PS2000_CHANNEL_B].values,
			NULL,
			NULL,
			&overflow,
			BUFFER_SIZE );
		printf ( "%d values\n", no_of_values );

		if ( block_no++ > 20 )
		{
			block_no = 0;
			printf ( "Press any key to stop\n" );
		}

		/* Print out the first 10 readings
		*/
		for ( i = 0; i < no_of_values; i++ )
		{
			for (ch = 0; ch < unitOpened.noOfChannels; ch++)
			{
				if (unitOpened.channelSettings[ch].enabled)
				{
					fprintf ( fp, "%d, ", adc_to_mv (unitOpened.channelSettings[ch].values[i], unitOpened.channelSettings[ch].range) );
				}
			}
			fprintf (fp, "\n");
		}

		/* Wait 100ms before asking again
		*/
		Sleep ( 100 );
	}
	fclose ( fp );

	ps2000_stop ( unitOpened.handle );

	_getch ();
}

void collect_fast_streaming (void)
{
	uint32_t	i;
	FILE 	*fp;
	int16_t  overflow;
	int32_t 	ok;
	int16_t ch;
	uint32_t nPreviousValues = 0;
	int16_t values_a[BUFFER_SIZE_STREAMING];
	int16_t values_b[BUFFER_SIZE_STREAMING];
	uint32_t	triggerAt;
	int16_t triggered;
	uint32_t no_of_samples;
	double startTime = 0;


	printf ( "Collect streaming...\n" );
	printf ( "Data is written to disk file (fast_stream.txt)\n" );
	printf ( "Press a key to start\n" );
	_getch ();

	set_defaults ();

	/* You cannot use triggering for the start of the data...
	*/
	ps2000_set_trigger ( unitOpened.handle, PS2000_NONE, 0, 0, 0, 0 );

	unitOpened.trigger.advanced.autoStop = 0;
	unitOpened.trigger.advanced.totalSamples = 0;
	unitOpened.trigger.advanced.triggered = 0;

	/* Collect data at 10us intervals
	* 100000 points with an agregation of 100 : 1
	*	Auto stop after the 100000 samples
	*  Start it collecting,
	* NOTE: The actual sampling interval used by the driver might not be that which is specified below. Use the sampling intervals
	* returned by the ps2000_get_timebase() function to work out the most appropriate sampling interval to use. As these are low memory
	* devices, the fastest sampling intervals may result in lost data.
	*/
	//ok = ps2000_run_streaming_ns ( unitOpened.handle, 10, PS2000_US, BUFFER_SIZE_STREAMING, 1, 100, 30000 );
	ok = ps2000_run_streaming_ns ( unitOpened.handle, 1, PS2000_US, 10000, 0, 100, 50000 );
	
	printf ( "OK: %d\n", ok );

	/* From here on, we can get data whenever we want...
	*/	
	
	//while (!unitOpened.trigger.advanced.autoStop)
	while (!_kbhit())
	{
		ps2000_get_streaming_last_values (unitOpened.handle, ps2000FastStreamingReady);
		
		if (nPreviousValues != unitOpened.trigger.advanced.totalSamples)
		{
			printf ("Values collected: %ld\n", unitOpened.trigger.advanced.totalSamples - nPreviousValues);
			nPreviousValues = 	unitOpened.trigger.advanced.totalSamples;
		}
		Sleep (0);
	}

	ps2000_stop (unitOpened.handle);

	no_of_samples = ps2000_get_streaming_values_no_aggregation (unitOpened.handle,
																	&startTime, // get samples from the beginning
																	values_a, // set buffer for channel A
																	values_b,	// set buffer for channel B
																	NULL,
																	NULL,
																	&overflow,
																	&triggerAt,
																	&triggered,
																	BUFFER_SIZE_STREAMING);

	printf("\nFirst 20 readings:\n");

	// print out the first 20 readings
	for ( i = 0; i < 20; i++ )
	{
		for (ch = 0; ch < unitOpened.noOfChannels; ch++)
		{
			if (unitOpened.channelSettings[ch].enabled)
			{
				printf ("%d, ", adc_to_mv ((!ch ? values_a[i] : values_b[i]), unitOpened.channelSettings[ch].range) );
			}
		}
			
		printf ("\n");
	}

	fopen_s (&fp, "fast_stream.txt", "w" );

	for ( i = 0; i < no_of_samples; i++ )
	{
		for (ch = 0; ch < unitOpened.noOfChannels; ch++)
		{
			if (unitOpened.channelSettings[ch].enabled)
			{
				fprintf ( fp, "%d, ", adc_to_mv ((!ch ? values_a[i] : values_b[i]), unitOpened.channelSettings[ch].range) );
			}
		}
		fprintf (fp, "\n");
	}
	fclose ( fp );

	_getch ();
}

/****************************************************************************
 *
 * collect_fast_streaming_triggered
 *
 * Demonstates how to retrieve data from the device after collecting
 * streaming data. This data is not aggregated.
 * 
 ****************************************************************************/
void collect_fast_streaming_triggered (void)
{
	uint32_t	i;
	FILE *		fp;
	int16_t		overflow;
	int32_t 	ok;
	int16_t		ch;
	uint32_t	nPreviousValues = 0;
	int16_t	*	values_a = (int16_t *) malloc (BUFFER_SIZE_STREAMING * sizeof(int16_t));
	int16_t	*	values_b = (int16_t *) malloc (BUFFER_SIZE_STREAMING * sizeof(int16_t));
	uint32_t	triggerAt;
	int16_t		triggered;
	uint32_t	no_of_samples;
	double		startTime = 0;

	printf ( "Collect streaming...\n" );
	printf ( "Data is written to disk file (fast_stream_trig_data.txt)\n" );
	printf ( "Press a key to start\n" );
	_getch ();

	set_defaults ();

	set_trigger_advanced ();

	unitOpened.trigger.advanced.autoStop = 0;
	unitOpened.trigger.advanced.totalSamples = 0;
	unitOpened.trigger.advanced.triggered = 0;

	/* Collect data at 10us intervals
	* 100000 points with an aggregation of 100 : 1
	*	Auto stop after the 100000 samples
	*  Start it collecting,
	* NOTE: The actual sampling interval used by the driver might not be that which is specified below. Use the sampling intervals
	* returned by the ps2000_get_timebase() function to work out the most appropriate sampling interval to use. As these are low memory
	* devices, the fastest sampling intervals may result in lost data.
	*/
	ok = ps2000_run_streaming_ns ( unitOpened.handle, 10, PS2000_US, BUFFER_SIZE_STREAMING, 1, 100, 30000 );
	//ok = ps2000_run_streaming_ns ( unitOpened.handle, 10, PS2000_US, BUFFER_SIZE_STREAMING, 1, 1, 30000 ); // No aggregation
	printf ( "OK: %d\n", ok );

	/* From here on, we can get data whenever we want...*/	
	
	while (!_kbhit() && !unitOpened.trigger.advanced.autoStop)
	{
		ps2000_get_streaming_last_values (unitOpened.handle, ps2000FastStreamingReady);
		
		if (nPreviousValues != unitOpened.trigger.advanced.totalSamples)
		{
			printf ("Values collected: %ld, Total samples: %ld ", unitOpened.trigger.advanced.totalSamples - nPreviousValues, unitOpened.trigger.advanced.totalSamples);
			nPreviousValues = unitOpened.trigger.advanced.totalSamples;

			if(g_triggered)
			{
				printf("Triggered at: %ld", g_triggeredAt);
			}

			printf("\n");
		}
		Sleep (0);
	}

	ps2000_stop (unitOpened.handle);

	no_of_samples = ps2000_get_streaming_values_no_aggregation (unitOpened.handle,
																	&startTime, // get samples from the beginning
																	values_a, // set buffer for channel A
																	values_b,	// set buffer for channel B
																	NULL,
																	NULL,
																	&overflow,
																	&triggerAt,
																	&triggered,
																	BUFFER_SIZE_STREAMING);


	printf("\n");

	if(triggerAt)
	{
		printf("10 readings either side of the trigger point:\n");
	}
	else
	{
		printf("First 20 readings:\n");
	}

	// if the unit triggered print out ten samples either side of the trigger point
	// otherwise print the first 20 readings
	for ( i = (triggered ? triggerAt - 10 : 0) ; i < ((triggered ? triggerAt - 10 : 0) + 20); i++)
	{
		for (ch = 0; ch < unitOpened.noOfChannels; ch++)
		{
			if (unitOpened.channelSettings[ch].enabled)
			{
				printf ("%d, ", adc_to_mv ((!ch ? values_a[i] : values_b[i]), unitOpened.channelSettings[ch].range) );
			}
		}
		printf ("\n");
	}

	fopen_s (&fp, "fast_stream_trig_data.txt", "w" );

	for ( i = 0; i < no_of_samples; i++ )
	{
		for (ch = 0; ch < unitOpened.noOfChannels; ch++)
		{
			if (unitOpened.channelSettings[ch].enabled)
			{
				fprintf ( fp, "%d, ", adc_to_mv ((!ch ? values_a[i] : values_b[i]), unitOpened.channelSettings[ch].range) );
			}
		}
		fprintf (fp, "\n");
	}
	fclose ( fp );

	_getch ();
}

/****************************************************************************
 *
 * collect_fast_streaming_triggered2
 *
 * Demonstrates how to retrieve data from the device while it is collecting
 * streaming data. This data is not aggregated.
 * 
 * Data is collected into an application buffer specified for a channel. If
 * the maximum size of the application buffer has been reached, the 
 * application will stop collecting data.
 *
 * Ensure minimal processes are running on the PC to reduce risk of lost data
 * values.
 *
 ****************************************************************************/
void collect_fast_streaming_triggered2 (void)
{
	uint32_t	i;
	FILE *		fp;
	int32_t 	ok;
	int16_t		ch;
	uint32_t	nPreviousValues = 0;
	double		startTime = 0.0;
	uint32_t	appBufferSize = (int32_t)(NUM_STREAMING_SAMPLES * 1.5);
	uint32_t	overviewBufferSize = BUFFER_SIZE_STREAMING;
	uint32_t	sample_count;

	printf ( "Collect streaming...\n" );
	printf ( "Data is written to disk file (fast_streaming_trig_data2.txt)\n" );
	printf ( "Press a key to start\n" );
	_getch ();

	set_defaults ();

	// Simple trigger, 500mV, rising
	ok = ps2000_set_trigger(unitOpened.handle, PS2000_CHANNEL_A, 
		mv_to_adc(500, unitOpened.channelSettings[PS2000_CHANNEL_A].range), PS2000_RISING, 0, 0);

	unitOpened.trigger.advanced.autoStop = 0;
	unitOpened.trigger.advanced.totalSamples = 0;
	unitOpened.trigger.advanced.triggered = 0;

	//Reset global values
	g_nValues = 0;
	g_triggered = 0;
	g_triggeredAt = 0;
	g_startIndex = 0;
	g_prevStartIndex = 0;
	g_appBufferFull = 0;

	bufferInfo.unit = unitOpened;

	// Allocate memory for data arrays

	// Max A buffer at index 0, min buffer at index 1
	bufferInfo.appBuffers[PS2000_CHANNEL_A * 2] = (int16_t *) calloc(appBufferSize, sizeof(int16_t));
	bufferInfo.bufferSizes[PS2000_CHANNEL_A * 2] = appBufferSize;

	if(unitOpened.channelSettings[PS2000_CHANNEL_B].enabled)
	{
		// Max B buffer at index 2, min buffer at index 3
		bufferInfo.appBuffers[PS2000_CHANNEL_B * 2] = (int16_t *) calloc(appBufferSize, sizeof(int16_t));
		bufferInfo.bufferSizes[PS2000_CHANNEL_B * 2] = appBufferSize;
	}

	/* Collect data at 10us intervals
	* 100000 points with an aggregation of 100 : 1
	*	Auto stop after the 100000 samples
	*  Start it collecting,
	*/
	//ok = ps2000_run_streaming_ns ( unitOpened.handle, 10, PS2000_US, NUM_STREAMING_SAMPLES, 1, 100, overviewBufferSize );

	/* Collect data at 1us intervals
	* 1000000 points after trigger with 0 aggregation
	* Auto stop after the 1000000 samples
	* Start it collecting,
	* NOTE: The actual sampling interval used by the driver might not be that which is specified below. Use the sampling intervals
	* returned by the ps2000_get_timebase function to work out the most appropriate sampling interval to use. As these are low memory
	* devices, the fastest sampling intervals may result in lost data.
	*/
	ok = ps2000_run_streaming_ns ( unitOpened.handle, 1, PS2000_US, NUM_STREAMING_SAMPLES, 1, 1, overviewBufferSize ); // No aggregation
	
	printf ( "OK: %d\n", ok );

	/* From here on, we can get data whenever we want...*/	
	
	while (!_kbhit() && !unitOpened.trigger.advanced.autoStop && !g_appBufferFull)
	{
		ps2000_get_streaming_last_values (unitOpened.handle, ps2000FastStreamingReady2);
		
		if (nPreviousValues != unitOpened.trigger.advanced.totalSamples)
		{
			sample_count = unitOpened.trigger.advanced.totalSamples - nPreviousValues;
			
			//Printing to console can take up resources
			//printf ("Values collected: %ld, Total samples: %ld ", sample_count, unitOpened.trigger.advanced.totalSamples);
			
			/*if(g_triggered)
			{
				printf("Triggered at index: %lu, overall %lu", g_triggeredAt, nPreviousValues + g_triggeredAt);
			}*/

			nPreviousValues = unitOpened.trigger.advanced.totalSamples;
			//printf("\n");

			if (g_appBufferFull)
			{
				unitOpened.trigger.advanced.totalSamples = appBufferSize;
				printf("\nApplication buffer full - stopping data collection.\n");
			}
			
		}
		
	}

	ps2000_stop (unitOpened.handle);

	printf("\nCollected %lu samples. Writing to file...\n", unitOpened.trigger.advanced.totalSamples);

	fopen_s (&fp, "fast_streaming_trig_data2.txt", "w" );

	fprintf(fp,"For each of the %d Channels, results shown are....\n", unitOpened.noOfChannels);
	fprintf(fp,"Channel ADC Count & mV\n\n");

	for (ch = 0; ch < unitOpened.noOfChannels; ch++) 
	{
		if (unitOpened.channelSettings[ch].enabled) 
		{
			fprintf(fp,"Ch%C   Max ADC    Max mV   ", (char) ('A' + ch));
		}
	}

	fprintf(fp, "\n");

	for (i = 0; i < unitOpened.trigger.advanced.totalSamples; i++)
	{
		if (fp != NULL)
		{
			for (ch = 0; ch < unitOpened.noOfChannels; ch++)
			{
				if (unitOpened.channelSettings[ch].enabled)
				{
					fprintf ( fp, "%4C, %7d, %7d, ",
						'A' + ch,
						bufferInfo.appBuffers[ch * 2][i],
						adc_to_mv (bufferInfo.appBuffers[ch * 2][i], unitOpened.channelSettings[ch].range) );
				}
			}

			fprintf(fp, "\n");
		}
		else
		{
			printf("Cannot open the file fast_streaming_trig_data2.txt for writing.\n");
		}
	}

	printf("Writing to file complete.\n");

	fclose ( fp );

	// Free buffers
	for(ch = 0; ch < unitOpened.noOfChannels; ch++)
	{
		if (unitOpened.channelSettings[ch].enabled)
		{
			free(bufferInfo.appBuffers[ch * 2]);
		}
	}

	if (_kbhit())
	{
		_getch ();
	}

}

/****************************************************************************
* displaySettings 
* Displays information about the user configurable settings in this example
* Parameters 
* - unitOpened        UNIT_MODEL structure
*
* Returns       none
***************************************************************************/
void displaySettings(UNIT_MODEL unitOpened)
{
	int16_t ch;
	int32_t voltage;

	printf("\n\nReadings will be scaled in (%s)\n", (scale_to_mv)? ("mV") : ("ADC counts"));

	for (ch = 0; ch < unitOpened.noOfChannels; ch++)
	{
		if (!(unitOpened.channelSettings[ch].enabled))
			printf("Channel %c Voltage Range = Off\n", 'A' + ch);
		else
		{
			voltage = input_ranges[unitOpened.channelSettings[ch].range];
			printf("Channel %c Voltage Range = ", 'A' + ch);

			if (voltage < 1000)
			{
				printf("%dmV\n", voltage);
			}
			else
			{
				printf("%dV\n", voltage / 1000);
			}
		}
	}
	printf("\n");
}

/****************************************************************************
 *
 *
 ****************************************************************************/
void get_info (void)
{
	int8_t description [8][25]=  {	"Driver Version   ",
									"USB Version      ",
									"Hardware Version ",
									"Variant Info     ",
									"Serial           ", 
									"Cal Date         ", 
									"Error Code       ",
									"Kernel Driver    "
									};
	int16_t 	i;
	int8_t		line [80];
	int32_t		variant;

	if( unitOpened.handle )
	{
		for ( i = 0; i < 8; i++ )
		{
			ps2000_get_unit_info ( unitOpened.handle, line, sizeof (line), i );
		
			if (i == 3)
			{
				variant = atoi((const char*) line);

				if (strlen((const char*) line) == 5) // Identify if 2204A or 2205A
				{
					line[4] = toupper(line[4]);

					if (line[1] == '2' && line[4] == 'A')		// i.e 2204A -> 0xA204
					{
						variant += 0x9968;
					}
				}
			}
		
			if(i != 6) // No need to print error code
			{
				printf ( "%s: %s\n", description[i], line );
			}
		}

		switch (variant)
		{
			case MODEL_PS2104:
				unitOpened.model = MODEL_PS2104;
				unitOpened.firstRange = PS2000_100MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2104_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 1;
				unitOpened.hasAdvancedTriggering = FALSE;
				unitOpened.hasSignalGenerator = FALSE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = FALSE;
			break;

			case MODEL_PS2105:
				unitOpened.model = MODEL_PS2105;
				unitOpened.firstRange = PS2000_100MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2105_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 1; 
				unitOpened.hasAdvancedTriggering = FALSE;
				unitOpened.hasSignalGenerator = FALSE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = FALSE;
			break;
				
			case MODEL_PS2202:
				unitOpened.model = MODEL_PS2202;
				unitOpened.firstRange = PS2000_100MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 2;
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = FALSE;
				unitOpened.hasEts = FALSE;
				unitOpened.hasFastStreaming = TRUE;
			break;

			case MODEL_PS2203:
				unitOpened.model = MODEL_PS2203;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 2; 
				unitOpened.hasAdvancedTriggering = FALSE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
			break;

			case MODEL_PS2204:
				unitOpened.model = MODEL_PS2204;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 2;
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
			break;

			case MODEL_PS2204A:
					unitOpened.model = MODEL_PS2204A;
					unitOpened.firstRange = PS2000_50MV;
					unitOpened.lastRange = PS2000_20V;
					unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
					unitOpened.timebases = unitOpened.maxTimebase;
					unitOpened.noOfChannels = DUAL_SCOPE;
					unitOpened.hasAdvancedTriggering = TRUE;
					unitOpened.hasSignalGenerator = TRUE;
					unitOpened.hasEts = TRUE;
					unitOpened.hasFastStreaming = TRUE;
					unitOpened.awgBufferSize = 4096;
			break;

			case MODEL_PS2205:
				unitOpened.model = MODEL_PS2205;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 2; 
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
			break;

			case MODEL_PS2205A:
				unitOpened.model = MODEL_PS2205A;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = DUAL_SCOPE; 
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
				unitOpened.awgBufferSize = 4096;
			break;

 			default:
				printf("Unit not supported");
		}
		
		unitOpened.channelSettings [PS2000_CHANNEL_A].enabled = 1;
		unitOpened.channelSettings [PS2000_CHANNEL_A].DCcoupled = 1;
		unitOpened.channelSettings [PS2000_CHANNEL_A].range = PS2000_5V;

		if (unitOpened.noOfChannels == DUAL_SCOPE)
		{
			unitOpened.channelSettings [PS2000_CHANNEL_B].enabled = 1;
		}
		else
		{
			unitOpened.channelSettings [PS2000_CHANNEL_B].enabled = 0;
		}

		unitOpened.channelSettings [PS2000_CHANNEL_B].DCcoupled = 1;
		unitOpened.channelSettings [PS2000_CHANNEL_B].range = PS2000_5V;

		set_defaults();
		
	}
	else
	{
		printf ( "Unit Not Opened\n" );
    
		ps2000_get_unit_info ( unitOpened.handle, line, sizeof (line), 5 );
    
		printf ( "%s: %s\n", description[5], line );
		unitOpened.model = MODEL_NONE;
		unitOpened.firstRange = PS2000_100MV;
		unitOpened.lastRange = PS2000_20V;
		unitOpened.timebases = PS2105_MAX_TIMEBASE;
		unitOpened.noOfChannels = SINGLE_CH_SCOPE;	
	}
}

void set_sig_gen ()
{
	int16_t waveform;
	int32_t frequency;
	int16_t waveformSize = 0;

	printf("Enter frequency in Hz: "); // Ask user to enter signal frequency;
	do 
	{
		scanf_s("%lu", &frequency);
	} while (frequency < 1000 || frequency > PS2000_MAX_SIGGEN_FREQ);

	printf("Signal generator On");
	printf("Enter type of waveform (0..9 or 99)\n");
	printf("0:\tSINE\n");
	printf("1:\tSQUARE\n");
	printf("2:\tTRIANGLE\n");
	printf("3:\tRAMP UP\n");
	printf("4:\tRAMP DOWN\n");

	do
	{
		scanf_s("%hd", &waveform);
	} while (waveform < 0 || waveform >= PS2000_DC_VOLTAGE);

	ps2000_set_sig_gen_built_in (unitOpened.handle,
									0,
									1000000, // 1 volt
									(PS2000_WAVE_TYPE) waveform,
									(float)frequency,
									(float)frequency,
									0,
									0,
									PS2000_UPDOWN, 0);
}

void set_sig_gen_arb (void)
{
	int32_t frequency;
	int8_t fileName [128];
	FILE * fp;
	uint8_t arbitraryWaveform [AWG_MAX_BUFFER_SIZE];
	int16_t waveformSize = 0;
	double delta;

	if(unitOpened.hasSignalGenerator)
	{
		memset(&arbitraryWaveform, 0, AWG_MAX_BUFFER_SIZE);

		printf("Enter frequency in Hz: "); // Ask user to enter signal frequency;
		do 
		{
			scanf_s("%lu", &frequency);
		} 
		while (frequency < 1 || frequency > 10000000);

		waveformSize = 0;

		printf("Select a waveform file to load: ");
		scanf_s("%s", fileName);
	
		if (fopen_s(&fp, fileName, "r")) 
		{ // Having opened file, read in data - one number per line (at most 4096 lines), with values in (0..255)
			while (EOF != fscanf_s(fp, "%i", (arbitraryWaveform + waveformSize)) && waveformSize++ < unitOpened.awgBufferSize)
				;
			fclose(fp);
		}
		else
		{
			printf("Invalid filename\n");
			return;
		}


		delta = ((frequency * waveformSize) / unitOpened.awgBufferSize) * AWG_PHASE_ACCUMULATOR * (1/AWG_DDS_FREQUENCY);

		ps2000_set_sig_gen_arbitrary(unitOpened.handle, 0, 2000000, (uint32_t)delta, (uint32_t)delta, 0, 0, arbitraryWaveform, waveformSize, PS2000_UP, 0);

	}
	else
	{
		printf("Signal generator not supported by device\n");
	}
}

/****************************************************************************
 *
 * Select timebase, set oversample to on and time units as nano seconds
 *
 ****************************************************************************/
void set_timebase (void)
{
	int16_t		i;
	int32_t		time_interval = 0;
	int16_t		time_units;
	int16_t		oversample;
	int32_t		max_samples;
	int16_t		status;

	printf ( "Specify timebase\n" );

	/* See what ranges are available...
	*/
	oversample = 1;
	for (i = 0; i <= unitOpened.timebases; i++)
	{
		status = ps2000_get_timebase ( unitOpened.handle, i, BUFFER_SIZE, &time_interval, &time_units, oversample, &max_samples );
		
		// Time units returned are to be used with the ps2000_get_times_and_values function if required
		if ( status == 1 && time_interval > 0 )
		{
			printf ( "%d -> %d %s Time units: %hd (%s)\n", i, time_interval, "ns", time_units, adc_units(time_units) );
		}
	}

	/* Ask the user to select a timebase
	*/
	printf ( "Timebase: " );
	do
	{
		fflush( stdin );
		scanf_s ( "%d", &timebase );
	} while ( (timebase < 0) || (timebase > unitOpened.timebases) );
	
	status = ps2000_get_timebase ( unitOpened.handle, timebase, BUFFER_SIZE, &time_interval, &time_units, oversample, &max_samples );

	printf("Timebase %d - %ld ns\n", timebase, time_interval);
	
}

/****************************************************************************
 * Select input voltage ranges for channels A and B
 ****************************************************************************/
void set_voltages ()
{
	int32_t		i;
	int16_t ch = 0;

	/* See what ranges are available...*/
	for ( i = unitOpened.firstRange; i <= unitOpened.lastRange; i++ )
	{
		printf ( "%d -> %d mV\n", i, input_ranges[i] );
	}

	/* Ask the user to select a range */
	for (ch = 0; ch < unitOpened.noOfChannels; ch++)
	{
		printf ( "Specify voltage range (%d..%d)\n", unitOpened.firstRange, (unitOpened.lastRange) );
		printf ( "99 - switches channel off\n");
		printf ( "\nChannel %c: ", 'A' + ch);
		do
		{
			fflush( stdin );
			scanf_s ( "%d", &unitOpened.channelSettings[ch].range);
		} while ( unitOpened.channelSettings[ch].range != 99 && (unitOpened.channelSettings[ch].range < unitOpened.firstRange || unitOpened.channelSettings[ch].range  > unitOpened.lastRange) );
		if(unitOpened.channelSettings[ch].range != 99)
		{
			printf ( " - %d mV\n", input_ranges[unitOpened.channelSettings[ch].range]);
			unitOpened.channelSettings[ch].enabled = TRUE;
		}
		else
		{
			printf ( "Channel Switched off\n", input_ranges[unitOpened.channelSettings[ch].range]);
			unitOpened.channelSettings[ch].enabled = FALSE;
		}
	}

	set_defaults ();
}	

/****************************************************************************
 *
 *
 ****************************************************************************/

void main (void)
{
	int8_t	ch;

	printf ( "PicoScope 2000 Series (ps2000) Driver Example Program\n" );
	printf ( "Version 1.3\n\n" );

	printf ( "\n\nOpening the device...\n");

	//open unit and show splash screen
	unitOpened.handle = ps2000_open_unit ();
	printf ( "Handler: %d\n", unitOpened.handle );
	if ( !unitOpened.handle )
	{
		printf ( "Unable to open device\n" );
		get_info ();
		while( !_kbhit() );
		exit ( 99 );
	}
	else
	{
		printf ( "Device opened successfully\n\n" );
		get_info ();

		timebase = 0;

		ch = ' ';
		while ( ch != 'X' )
		{
			displaySettings(unitOpened);

			printf ( "\n" );
			printf ( "B - Immediate block                V - Set voltages\n" );
			printf ( "T - Triggered block                I - Set timebase\n" );
			printf ( "Y - Advanced triggered block       A - ADC counts/mV\n" );
			printf ( "E - ETS block\n" );
			printf ( "S - Streaming\n");
			printf ( "F - Fast streaming\n");
			printf ( "D - Fast streaming triggered\n");
			printf ( "C - Fast streaming triggered 2\n");
			printf ( "G - Signal generator\n");
			printf ( "H - Arbitrary signal generator\n");
			printf ( "X - Exit\n" );
			printf ( "Operation:" );

			ch = toupper ( _getch () );

			printf ( "\n\n" );
			switch ( ch )
			{
			case 'B':
				collect_block_immediate ();
				break;

			case 'T':
				collect_block_triggered ();
				break;

			case 'Y':
				if (unitOpened.hasAdvancedTriggering)
				{
					collect_block_advanced_triggered ();
				}
				else
				{
					printf ("Not supported by this model\n\n");
				}
				break;

			case 'S':
				collect_streaming ();
				break;

			case 'F':
				if (unitOpened.hasFastStreaming)
				{
					collect_fast_streaming ();
				}
				else
				{
					printf ("Not supported by this model\n\n");
				}
				break;

			case 'D':
				if (unitOpened.hasFastStreaming && unitOpened.hasAdvancedTriggering)
				{
					collect_fast_streaming_triggered ();
				}
				else
				{
					printf ("Not supported by this model\n\n");
				}
				break;

			case 'C':
				if (unitOpened.hasFastStreaming && unitOpened.hasAdvancedTriggering)
				{
					collect_fast_streaming_triggered2();
				}
				else
				{
					printf ("Not supported by this model\n\n");
				}
				break;

			case 'E':
				if (unitOpened.hasEts)
				{
					collect_block_ets ();
				}
				else
				{
					printf ("Not supported by this model\n\n");
				}
				break;

			case 'G':
				if (unitOpened.hasSignalGenerator)
				{
					set_sig_gen ();
				}
				break;

			case 'H':
				if (unitOpened.hasSignalGenerator)
				{
					set_sig_gen_arb ();
				}
				break;

			case 'V':
				set_voltages ();
				break;

			case 'I':
				set_timebase ();
				break;

			case 'A':
				scale_to_mv = !scale_to_mv;
				if ( scale_to_mv )
				{
					printf ( "Readings will be scaled in mV\n" );
				}
				else
				{
					printf ( "Readings will be scaled in ADC counts\n" );
				}
				break;

			case 'X':
				/* Handled by outer loop */
				break;

			default:
				printf ( "Invalid operation\n" );
				break;
			}
		}

		ps2000_close_unit ( unitOpened.handle );
	}
}
