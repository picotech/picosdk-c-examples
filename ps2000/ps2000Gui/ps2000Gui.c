/*******************************************************************************
 *
 * Filename: ps2000Gui.c
 *
 * Description:
 *   This is a GUI program that demonstrates how to use the
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
 *			Set up a project for a 32-bit/64-bit Windows application
 *			Add this file to the project
 *			Add resource.h to the project
 *			Add ps2000Gui.rc to the project
 *			Add ps2000.lib to the project (Microsoft C only)
 *			Build the project
 *
 * Copyright (C) 2006-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#define FALSE   0

#include <windows.h>

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <assert.h>
#include "resource.h"
#include "ps2000.h"
#include "math.h"

#define WM_REFRESH_CHANNEL_B WM_USER + 1

#define BUFFER_SIZE 24000
#define WIDTH 450
#define HEIGHT 340
#define NUMBER_VOLT_USED 10
#define ID_TIMER 1

#define POINTX_REF 225
#define POINTY_REF 10

#define PEN_SCOPE   1
#define DUAL_SCOPE  2

#define MAX_CHANNELS 2

#define PS_CHANNELS 0x8000

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

typedef struct {
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
} CHANNEL_SETTINGS;

typedef struct {
	int32_t point [WIDTH];
	int16_t values [BUFFER_SIZE];
	uint32_t lineColour;
} GRAPH_DETAILS;

typedef struct  {
	int16_t handle;
	MODEL_TYPE model;
	PS2000_RANGE firstRange;
	PS2000_RANGE lastRange;
	BYTE signalGenerator;
	BYTE external;
	int16_t timebases;
	int16_t noOfChannels;
	CHANNEL_SETTINGS channelSettings [MAX_CHANNELS];
	GRAPH_DETAILS channels [MAX_CHANNELS];
	PS2000_RANGE triggerRange;
	int16_t			maxTimebase;
	int16_t			hasAdvancedTriggering;
	int16_t			hasFastStreaming;
	int16_t			hasEts;
	int16_t			hasSignalGenerator;
	int16_t			awgBufferSize;
} UNIT_MODEL; 

UNIT_MODEL unitOpened;

int32_t times[BUFFER_SIZE];

int16_t input_ranges [PS2000_MAX_RANGES] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};
int32_t running;

static HANDLE   hInstance;
int32_t CALLBACK WndProc (HWND, UINT, UINT, LONG) ;


HWND    hwnd;

/****************************************************************************
 *
 * adc_to_mv
 *
 * If the user selects scaling to millivolts,
 * Convert an 12-bit ADC count into millivolts
 *
 ****************************************************************************/
int32_t adc_to_mv (int32_t raw, int32_t ch)
{
	return ((raw * (input_ranges[ch])) / (PS2000_MAX_VALUE-1));
}

/****************************************************************************
 *  Change millivolts to ADC counts, used for trigger levels
 ****************************************************************************/
int16_t mv_to_adc (int16_t mv, int16_t ch)
{
	return (int16_t)((mv * 32767) / input_ranges[ch]);
}

int8_t * adc_units (int16_t time_units)
{
	switch (time_units)
    {
		case PS2000_FS:
			return "fs";
		case PS2000_PS:
			return "ps";
		case PS2000_NS:
			return "ns";
		case PS2000_US:
			return "us";
		case PS2000_MS:
			return "ms";
		case PS2000_S:
			return "s";
    }
    return "Not Known";
}

void initialize_logfont(LOGFONT *lf)
{
	// initisase logfont
	lf->lfCharSet = 0;
	lf->lfHeight = 0 ;
	lf->lfWidth = 0;
	lf->lfEscapement = 0;
	lf->lfOrientation = 0;
	lf->lfItalic = 0;
	lf->lfUnderline = 0;
	lf->lfStrikeOut = 0;
	lf->lfOutPrecision = 3;
	lf->lfClipPrecision = 2;
	lf->lfQuality = 1;
	lf->lfPitchAndFamily = 0;
	strcpy(lf->lfFaceName, "MS Sans Serif");
	lf->lfWeight = FW_NORMAL;
	lf->lfHeight = 8;
}


/****************************************************************************
 * VoltageRectangle
 *
 ****************************************************************************/
void VoltageRectangle(HDC hdc, RECT * rect)
{
	int32_t i;
	SIZE size;
	int8_t szVoltage[] = "XXXXXXX";
	LOGFONT lf;
	HFONT h;
	int32_t voltageInterval;

	initialize_logfont(&lf);
	h = CreateFontIndirect(&lf);
	GetTextExtentPoint32(hdc, szVoltage, 7, &size);

	rect->left = POINTX_REF - (size.cx + 5);
	rect->right = POINTX_REF - 5;
	rect->bottom = POINTY_REF + size.cy/2;
	rect->top = POINTY_REF - size.cy/2;

	sprintf(szVoltage, "%d",input_ranges[unitOpened.channelSettings[PS2000_CHANNEL_A].range]);
	MoveToEx(hdc, rect->left, rect->top, (LPPOINT)NULL);
	DrawText(hdc, szVoltage, (int32_t)strlen(szVoltage), rect, DT_RIGHT);
	voltageInterval = input_ranges[unitOpened.channelSettings[PS2000_CHANNEL_A].range]/ 5;
	for( i=1 ; i < 6 ; i++)
	{
		MoveToEx (hdc, POINTX_REF, POINTY_REF + (i * (HEIGHT/10)), (LPPOINT) NULL );
		LineTo ( hdc, POINTX_REF+WIDTH, POINTY_REF + (i * (HEIGHT/10)));
		
		// insert the voltage level
		sprintf(szVoltage, "%d", (voltageInterval * (5 - i)) );
		MoveToEx(hdc, rect->left, POINTY_REF + (i * (HEIGHT/10)) - (size.cy/2), (LPPOINT)NULL);
			rect->top = POINTY_REF + (i * (HEIGHT/10)) - (size.cy/2);
			rect->bottom = POINTY_REF + (i * (HEIGHT/10)) + (size.cy/2);
			DrawText(hdc, szVoltage, (int32_t)strlen(szVoltage), rect, DT_RIGHT);
	}

	voltageInterval = -voltageInterval;
	
	for( i=1 ; i < 5 ; i++)
	{
		MoveToEx (hdc, POINTX_REF, POINTY_REF + ((5 + i) * (HEIGHT/10)), (LPPOINT) NULL );
		LineTo ( hdc, POINTX_REF+WIDTH, POINTY_REF + ((5 + i) * (HEIGHT/10)));
		
		// insert the voltage level
		sprintf(szVoltage, "%d", (voltageInterval * i) );
		MoveToEx(hdc, rect->left, POINTY_REF + ((5 + i) * (HEIGHT/10)) - (size.cy/2), (LPPOINT)NULL);
		rect->top = POINTY_REF + ((5 + i) * (HEIGHT/10)) - (size.cy/2);
		rect->bottom = POINTY_REF + ((5 + i) * (HEIGHT/10)) + (size.cy/2);
		DrawText(hdc, szVoltage, (int32_t)strlen(szVoltage), rect, DT_RIGHT);
	}

	sprintf(szVoltage, "%d",-input_ranges[unitOpened.channelSettings[PS2000_CHANNEL_A].range]);
	MoveToEx(hdc, rect->left, POINTY_REF + HEIGHT - (size.cy/2), (LPPOINT)NULL);
	rect->top = POINTY_REF + HEIGHT - (size.cy/2);
	rect->bottom = POINTY_REF + HEIGHT + (size.cy/2);
	DrawText(hdc, szVoltage, (int32_t)strlen(szVoltage), rect, DT_RIGHT);

 	rect->left = POINTX_REF - (size.cx + 5);
	rect->right = POINTX_REF - 5;
	rect->bottom = POINTY_REF + HEIGHT + (size.cx/2) ;
	rect->top = POINTY_REF - size.cy/2;
}


void TimeAxis(HDC hdc)
{
	int32_t i;

	for(i = 0; i < 9; i++)
	{
		MoveToEx ( hdc, POINTX_REF+(45*(1+i)), POINTY_REF, (LPPOINT) NULL );
		LineTo ( hdc, POINTX_REF+(45*(i+1)), POINTY_REF + HEIGHT );
	}
}

/****************************************************************************
 *
 *
 ****************************************************************************/

int32_t CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int32_t 	set_channels = 0;
	HDC 			hdc;
	static 		BOOL sig_gen = 0;
	PAINTSTRUCT 	ps;
	int8_t 			str [80], str2[80];
	int8_t 			*volt_range [PS2000_MAX_RANGES] = {"±10mV", "±20mV", "±50mV", "±100 mV", "±200 mV", "±500 mV", "±1V", "±2V", "±5V", "±10V", "±20V", "±50V"};
	int16_t			i, j;
	int16_t 			splashscreen = 1;
	int32_t 			time_interval;
	int16_t 			time_units;
	int16_t 			oversample;
	int32_t 			no_of_samples = 450;
	int16_t 			timebase;
	int32_t 			time_indisposed_ms;
	int16_t 			trig_channel;
	int16_t 			trig_volts, trig_direction, trig_delay;
	int16_t 			auto_trigger_ms =0;
	int16_t 			overflow;
	HPEN 			hpen, oldPen;
	int8_t 			action [2][6] = {"Start", "Stop"};
	RECT  			rect;
	int8_t 			description [6][25]=  {"Driver Version ","USB Version ","Hardware Version ",
									"Variant Info ","Serial ", "Error Code "};
	int32_t 			max_samples;
	static RECT      voltageRect;

	switch ( message )
    {
		case WM_INITDIALOG:
			unitOpened.handle = ps2000_open_unit ();

			if(unitOpened.handle)
			{
				ps2000_get_unit_info(unitOpened.handle, str, sizeof(str), 3);
				
				i = atoi(str);
				
				switch (i)
				{
					case MODEL_PS2105:
						unitOpened.model = MODEL_PS2105;
						unitOpened.external = FALSE;
						unitOpened.signalGenerator = FALSE;
						unitOpened.firstRange = PS2000_100MV;
						unitOpened.lastRange = PS2000_20V;
						unitOpened.timebases = PS2105_MAX_TIMEBASE;
						unitOpened.noOfChannels = PEN_SCOPE;
						unitOpened.channelSettings[PS2000_CHANNEL_A].range = PS2000_100MV;
					break;

					case MODEL_PS2104:
						unitOpened.model = MODEL_PS2104;
						unitOpened.external = FALSE;
						unitOpened.signalGenerator = FALSE;
						unitOpened.firstRange = PS2000_100MV;
						unitOpened.lastRange = PS2000_20V;
						unitOpened.timebases = PS2105_MAX_TIMEBASE;
						unitOpened.noOfChannels = PEN_SCOPE;
						unitOpened.channelSettings[PS2000_CHANNEL_A].range = PS2000_100MV;
					break;

					case MODEL_PS2202:
						unitOpened.model = MODEL_PS2202;
						unitOpened.external = FALSE;
						unitOpened.signalGenerator = FALSE;
						unitOpened.firstRange = PS2000_50MV;
						unitOpened.lastRange = PS2000_20V;
						unitOpened.timebases = PS2000_MAX_TIMEBASE;
						unitOpened.noOfChannels = DUAL_SCOPE;
						unitOpened.channelSettings[PS2000_CHANNEL_A].range = PS2000_50MV;
						unitOpened.channelSettings[PS2000_CHANNEL_B].range = PS2000_50MV;
					break;

					case MODEL_PS2203:
						unitOpened.model = MODEL_PS2203;
						unitOpened.firstRange = PS2000_50MV;
						unitOpened.lastRange = PS2000_20V;
						unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
						unitOpened.timebases = unitOpened.maxTimebase;
						unitOpened.noOfChannels = 2; 
						unitOpened.hasAdvancedTriggering = TRUE;
						unitOpened.hasSignalGenerator = TRUE;
						unitOpened.hasEts = TRUE;
						unitOpened.hasFastStreaming = TRUE;
					break;

					case MODEL_PS2204:
						unitOpened.model = MODEL_PS2204;
						unitOpened.firstRange = PS2000_50MV;
						unitOpened.lastRange = PS2000_20V;
						unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
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
							unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
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
						unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
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
						unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
						unitOpened.timebases = unitOpened.maxTimebase;
						unitOpened.noOfChannels = DUAL_SCOPE; 
						unitOpened.hasAdvancedTriggering = TRUE;
						unitOpened.hasSignalGenerator = TRUE;
						unitOpened.hasEts = TRUE;
						unitOpened.hasFastStreaming = TRUE;
						unitOpened.awgBufferSize = 4096;
					break;
				
 					default:
						MessageBox(hwnd, "Unit not supported", "Variant Error", MB_OK);
						SendMessage(hwnd, WM_DESTROY, 0, 0);
				}
			}
			else
			{
				unitOpened.model = MODEL_NONE;
				unitOpened.external = FALSE;
				unitOpened.signalGenerator = TRUE;
				unitOpened.firstRange = PS2000_100MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.timebases = PS2105_MAX_TIMEBASE;
				unitOpened.noOfChannels = PEN_SCOPE;	
				unitOpened.channelSettings[PS2000_CHANNEL_A].range = 0;
			}

  			// set graph line colours
			for (i = 0; i < MAX_CHANNELS; i++)
			{
				unitOpened.channels[i].lineColour = 0xFF0000 >> (4 * i);
			}

			for (i = 0; i < MAX_CHANNELS; i++)
			{
				unitOpened.channelSettings[i].enabled = FALSE;
			}

			for(i = 0; i < WIDTH; i++)
			{
				for (j = 0; j < unitOpened.noOfChannels; j++)
				{
					unitOpened.channels[j].point[i] = POINTY_REF + HEIGHT/2;
				}
    		}

			running = FALSE;
			PostMessage ( hwnd, WM_PAINT, 0, 0 );
			SetTimer ( hwnd, ID_TIMER, 100, NULL );

			rect.right = POINTX_REF + WIDTH;
			rect.left = POINTX_REF;
			rect.bottom = HEIGHT + POINTY_REF;
			rect.top = POINTY_REF;
			PostMessage (hwnd, WM_COMMAND, PS_CHANNELS, 0);
			PostMessage(hwnd, WM_REFRESH_CHANNEL_B, 0, 0);

		break;

		case WM_REFRESH_CHANNEL_B:

			if(unitOpened.noOfChannels == DUAL_SCOPE)
			{
				EnableWindow(GetDlgItem(hwnd, IDC_VOLTAGE_B), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_CHB), TRUE);
				EnableWindow(GetDlgItem(hwnd, IDC_COUPLING_B), TRUE);
			}

		break;

		case WM_PAINT:
      
			hdc = BeginPaint ( hwnd, &ps );

			Rectangle(hdc, POINTX_REF, POINTY_REF, POINTX_REF + WIDTH, 340 + POINTY_REF);

			hpen = CreatePen ( PS_DOT, 0, 0x000000 );
			oldPen = SelectObject ( hdc, hpen );
			VoltageRectangle(hdc, &voltageRect);
			TimeAxis(hdc);
			DeleteObject ( SelectObject ( hdc, oldPen ) );

			for (j = 0; j < unitOpened.noOfChannels; j++)
			{
				if ( IsDlgButtonChecked( hwnd,IDC_CHA + j ) == BST_CHECKED )
				{
					hpen = CreatePen ( PS_SOLID, 0, (COLORREF)unitOpened.channels[j].lineColour);
					oldPen = SelectObject ( hdc, hpen );
					
					for ( i = 0; i < ( WIDTH-1 ); i++)
					{
						MoveToEx ( hdc, POINTX_REF+i, unitOpened.channels[PS2000_CHANNEL_A + j].point[i], (LPPOINT) NULL );
						LineTo ( hdc, POINTX_REF+i+1, unitOpened.channels[PS2000_CHANNEL_A + j].point[i + 1]);
					}
					
					DeleteObject(SelectObject(hdc, oldPen));
				}
			}

			EndPaint(hwnd, &ps);

			if( !set_channels )
			{
				for ( i = 0; i < ( unitOpened.handle ? 5:2 ); i++)
				{
					ps2000_get_unit_info ( unitOpened.handle, str, sizeof (str), i );
					sprintf ( str2,"%s%s", description[i], str );
					SetDlgItemText ( hwnd, IDC_INFO1+i, str2 );
				}

				SendDlgItemMessage ( hwnd, IDC_COMBOBOX, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "None" );
				
				for(i = 0; i < ( unitOpened.noOfChannels); i++)
				{
					sprintf(str, "Channel %c", 'A' + i); 
					SendDlgItemMessage ( hwnd, IDC_COMBOBOX, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) str);
				}

				SendDlgItemMessage ( hwnd, IDC_COMBOBOX, CB_SETCURSEL, 0, 0 );
				set_channels = 1;

				for (j = 0; j < MAX_CHANNELS; j++)
				{
					SendDlgItemMessage ( hwnd, IDC_COUPLING + j, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "AC" );
					SendDlgItemMessage ( hwnd, IDC_COUPLING + j, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "DC" );
					SendDlgItemMessage ( hwnd, IDC_COUPLING + j, CB_SETCURSEL, 0, 0 );
				}

					SendDlgItemMessage ( hwnd, IDC_TRG7, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "Rising" );
					SendDlgItemMessage ( hwnd, IDC_TRG7, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "Falling" );
					SendDlgItemMessage ( hwnd, IDC_TRG7, CB_SETCURSEL, 0, 0 );

				for (j = 0; j < MAX_CHANNELS; j++)
				{
					for ( i = unitOpened.firstRange; i <= unitOpened.lastRange; i++ )
					{
						SendDlgItemMessage ( hwnd, IDC_VOLTAGE + j, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) volt_range[i] );
					}
					
					SendDlgItemMessage ( hwnd, IDC_VOLTAGE + j, CB_SETCURSEL, 0,	unitOpened.channelSettings[PS2000_CHANNEL_A].range);
				}

				for (i = 0; i<= unitOpened.timebases; i++)
				{
					sprintf(str, "%d", i);
					SendDlgItemMessage (hwnd, IDC_TIMEBASE, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) str);
				}
					SendDlgItemMessage (hwnd, IDC_TIMEBASE, CB_SETCURSEL, 0, 0);
			}
			
		break;

		case WM_TIMER:

			if( running )
			{

				if (IsDlgButtonChecked( hwnd,IDC_CHA ) != BST_CHECKED)
				{
					return 0;
				}

				for (i = 0; i < unitOpened.noOfChannels; i++)
				{
					if ( unitOpened.channelSettings[PS2000_CHANNEL_A + i].enabled = (IsDlgButtonChecked ( hwnd,IDC_CHA + i) == BST_CHECKED) )
					{
						GetDlgItemText ( hwnd, IDC_COUPLING + i, str, 10 );
						unitOpened.channelSettings[PS2000_CHANNEL_A + i].DCcoupled = strcmp( str, "DC" )== 0;
					}

					ps2000_set_channel(unitOpened.handle,
						                 PS2000_CHANNEL_A + i,
						                 unitOpened.channelSettings[PS2000_CHANNEL_A + i].enabled,
														 unitOpened.channelSettings[PS2000_CHANNEL_A + i].DCcoupled,
														 unitOpened.channelSettings[PS2000_CHANNEL_A + i].range);
				}

				// get triggering info if checkbox true otherwise set to trigger to false and
				// use default values.
				// if trigger set but variables not entered use 0 volts on channel A.
				if ( IsDlgButtonChecked( hwnd,IDC_TRIGGER ) == BST_CHECKED )
				{
					GetDlgItemText ( hwnd, IDC_COMBOBOX, str, 10 );

					if ( strcmp( str, "Channel A") == 0 )
					{
						unitOpened.triggerRange = unitOpened.channelSettings[PS2000_CHANNEL_A].range;
						trig_channel = PS2000_CHANNEL_A;
					}
					else if ( strcmp ( str, "Channel B" ) == 0 )
					{
						trig_channel = PS2000_CHANNEL_B;
						unitOpened.triggerRange = unitOpened.channelSettings[PS2000_CHANNEL_B].range;
					}
					else if ( strcmp ( str, "Channel C" ) == 0 )
					{
						trig_channel = PS2000_CHANNEL_C;
						unitOpened.triggerRange = unitOpened.channelSettings[PS2000_CHANNEL_C].range;
					}
					else if( strcmp ( str, "Channel D" ) == 0 )
					{
						trig_channel = PS2000_CHANNEL_D;
						unitOpened.triggerRange = unitOpened.channelSettings[PS2000_CHANNEL_D].range;
					}
					else
					{
						trig_channel = PS2000_NONE;
						unitOpened.triggerRange =  unitOpened.lastRange;
					}

					trig_volts = (int16_t) GetDlgItemInt( hwnd, IDC_TRG6, NULL, FALSE );
					GetDlgItemText ( hwnd, IDC_TRG7, str, 9 );

					if ( strcmp ( str, "Rising" ) == 0 )
					{
						trig_direction = 0;
					}
					else
					{
						trig_direction = 1;
					}
					
					trig_delay = (int16_t)GetDlgItemInt ( hwnd, IDC_TRG8, NULL, TRUE );
				}
				else
				{
					trig_channel = PS2000_NONE;
					trig_direction= trig_delay= auto_trigger_ms = trig_volts = 0;
					unitOpened.triggerRange =  unitOpened.lastRange;
				}

				ps2000_set_trigger ( unitOpened.handle, trig_channel, mv_to_adc ( trig_volts, (int16_t)unitOpened.triggerRange), trig_direction, trig_delay, auto_trigger_ms );
				// end setting trigger

				// switch ets off
				ps2000_set_ets ( unitOpened.handle, PS2000_ETS_OFF, 0, 0 );

				// Get the required timebase
				GetDlgItemText ( hwnd, IDC_TIMEBASE, str, 3 );
				timebase = atoi ( str );
				 
				// check that the timebase is valid
				oversample = 1;
				if(!ps2000_get_timebase ( unitOpened.handle, timebase, no_of_samples, &time_interval, &time_units, oversample, &max_samples))
				{
					return 0;
				}

				/* Start it collecting,
   				*  then wait for completion
				*/
  				ps2000_run_block ( unitOpened.handle, no_of_samples, timebase, oversample, &time_indisposed_ms );

				while ( ( !ps2000_ready ( unitOpened.handle ) ) && !kbhit() );
				running = FALSE;

  				ps2000_stop ( unitOpened.handle );

  				/* Should be done now...
   				*  get the times (in nanoseconds)
   				*   and the values (in ADC counts)
   				*/
				ps2000_get_times_and_values ( unitOpened.handle,
					                            times, 
												unitOpened.channels[PS2000_CHANNEL_A].values,
												unitOpened.channels[PS2000_CHANNEL_B].values,
												unitOpened.channels[PS2000_CHANNEL_C].values,
												unitOpened.channels[PS2000_CHANNEL_D].values,
												&overflow, time_units, no_of_samples );

				for (j = 0; j < unitOpened.noOfChannels; j++)
				{
					if(IsDlgButtonChecked ( hwnd,IDC_CHA + j ) == BST_CHECKED )
					{
						for ( i = 0; i < WIDTH; i++)
						{
							if ( adc_to_mv ( unitOpened.channels[PS2000_CHANNEL_A + j].values[i],unitOpened.channelSettings[PS2000_CHANNEL_A + j].range ) >= 0 )
							{
								unitOpened.channels[PS2000_CHANNEL_A + j].point[i] = POINTY_REF + (int32_t)(170.f - (170.0f/(float)input_ranges[unitOpened.channelSettings[PS2000_CHANNEL_A + j].range])*(float)( adc_to_mv ( unitOpened.channels[PS2000_CHANNEL_A + j].values[i], unitOpened.channelSettings[PS2000_CHANNEL_A + j].range) ));
							}
							else
							{
								unitOpened.channels[PS2000_CHANNEL_A + j].point[i] =  POINTY_REF  + (int32_t)(170.f + (170.0f/(float)input_ranges[unitOpened.channelSettings[PS2000_CHANNEL_A + j].range])*(float)( abs( adc_to_mv(unitOpened.channels[PS2000_CHANNEL_A + j].values[i], unitOpened.channelSettings[PS2000_CHANNEL_A + j].range) ) ));
							}
						}
					}
				}

				rect.left = POINTX_REF;
				rect.right = POINTX_REF + WIDTH;
						rect.top = POINTY_REF;
				rect.bottom = POINTY_REF + HEIGHT;
				InvalidateRect ( hwnd, &rect, TRUE );
			}

		break;

		case WM_COMMAND:
    
			switch (LOWORD(wParam))
			{
				case IDC_OK:
				
					if ( !unitOpened.handle )
					{
						MessageBox ( NULL, "Unit Not Open", "Error", MB_OK );
						return 0;
					}

					running = !running;
					SetDlgItemText ( hwnd, IDC_OK, action[running] );

				break;
    
				case IDC_VOLTAGE:
			  
					if (HIWORD(wParam) == CBN_SELENDOK)
					{
						unitOpened.channelSettings[PS2000_CHANNEL_A].range = (int16_t) SendDlgItemMessage (hwnd, IDC_VOLTAGE, CB_GETCURSEL, 0, 0) + unitOpened.firstRange;
						InvalidateRect(hwnd, &voltageRect, TRUE);
					}

				case IDC_VOLTAGE_B:
			  
					if (HIWORD(wParam) == CBN_SELENDOK)
					{
						unitOpened.channelSettings[PS2000_CHANNEL_B].range = (int16_t) SendDlgItemMessage (hwnd, IDC_VOLTAGE_B, CB_GETCURSEL, 0, 0) + unitOpened.firstRange;
						InvalidateRect(hwnd, &voltageRect, TRUE);
					}
					break;
			}

		break;

		case WM_DESTROY:
		
			KillTimer(hwnd, ID_TIMER);
			ps2000_close_unit (unitOpened.handle);
			PostQuitMessage (0);
			return 0;

	}
  
	return (int32_t)DefWindowProc ( hwnd, message, wParam, lParam );
}

/****************************************************************************
 * CALLBACK
 *
 ****************************************************************************/

int32_t CALLBACK WinMain (
  HINSTANCE hInst,
  HINSTANCE hPrevInstance,
  LPSTR lpszCmdLine,
  int32_t nCmdShow )
{
	static int8_t 	szClass [] = "PS2000";
	MSG         	msg;
	WNDCLASS    	wndclass;
	DWORD			lastError;

	ZeroMemory (&wndclass, sizeof (WNDCLASS));
	hInstance = hInst;

	if ( !hPrevInstance )
	{
		wndclass.style          = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc    = (WNDPROC)WndProc;
		wndclass.cbClsExtra     = 0;
		wndclass.cbWndExtra     = DLGWINDOWEXTRA;
		wndclass.hInstance      = hInstance;
		wndclass.hIcon          = NULL;
		wndclass.hCursor        = LoadCursor (NULL, IDC_ARROW);
		wndclass.hbrBackground  = (HBRUSH) (COLOR_WINDOW);
		wndclass.lpszMenuName   = NULL;
		wndclass.lpszClassName  = (LPSTR)szClass;
		RegisterClass (&wndclass);
	}

	hwnd = CreateDialog ( hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, (DLGPROC)WndProc) ;
	lastError = GetLastError();

	ShowWindow (hwnd, nCmdShow);
	UpdateWindow(hwnd);

	while ( GetMessage ( &msg, NULL, 0, 0 ) )
	{
		TranslateMessage ( &msg );
		DispatchMessage ( &msg );
	}

	return (int32_t) msg.wParam ;
}