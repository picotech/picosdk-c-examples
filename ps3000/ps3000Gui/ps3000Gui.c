/**************************************************************************
*
* Filename: ps3000gui.c
*
* Description:
*   This is a GUI program that demonstrates how to use the
*   PicoScope 3000 Series (ps3000) driver API functions.
*
*	Supported PicoScope models:
*
*		PicoScope 3204, 3205 & 3206
*		PicoScope 3223 & 3224
*		PicoScope 3423 & 3224
*
* Examples:
*    Collect a block of samples immediately
*    Collect a block of samples when a trigger event occurs
*	 Set the signal generator (PicoScope 3204, 3205 & 3206)
*
*	To build this application:-
*
*		If Microsoft Visual Studio (including Express) is being used:
*
*			Select the solution configuration (Debug/Release) and platform (Win32/x64)
*			Ensure that the 32-/64-bit ps3000.lib can be located
*			Ensure that the ps3000.h file can be located
*
*		Otherwise:
*
*			 Set up a project for a 32-/64-bit console mode application
*			 Add this file to the project
*			 Add ps3000.lib to the project (Microsoft C only)
*			 Add ps3000.h to the project
*			 Add ps3000Gui.rc and ps3000Gui.rch to the project
*			 Build the project
*
* Copyright (C) 2009 - 2017 Pico Technology Ltd. See LICENSE file for terms.
*
**************************************************************************/

#define FALSE           0

#include <windows.h>

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <assert.h>
#include "ps3000Gui.rch"
#include "ps3000.h"
#include "math.h"

#define BUFFER_SIZE 200000
#define WIDTH 450
#define HEIGHT 340
#define NUMBER_VOLT_USED 10
#define ID_TIMER 1

#define QUAD_SCOPE 4
#define DUAL_SCOPE 2

#define MAX_CHANNELS 4

#define PS_CHANNELS 0x8000

typedef enum {
	MODEL_NONE = 0,
	MODEL_PS3204 = 3204,
	MODEL_PS3205 = 3205,
	MODEL_PS3206 = 3206,
	MODEL_PS3224 = 3224,
	MODEL_PS3424 = 3424,
} MODEL_TYPE;

typedef struct {
	short DCcoupled;
	short range;
	short enabled;
} CHANNEL_SETTINGS;

typedef struct {
  int point [WIDTH];
  short values [BUFFER_SIZE];
	unsigned long lineColour;
} GRAPH_DETAILS;

typedef struct  {
	short handle;
	MODEL_TYPE model;
	PS3000_RANGE firstRange;
	PS3000_RANGE lastRange;
	BYTE signalGenerator;
	BYTE external;
	short timebases;
	short noOfChannels;
	CHANNEL_SETTINGS channelSettings [MAX_CHANNELS];
	GRAPH_DETAILS channels [MAX_CHANNELS];
	PS3000_RANGE triggerRange;
} UNIT_MODEL; 

UNIT_MODEL unitOpened;

long times[BUFFER_SIZE];


short input_ranges [PS3000_MAX_RANGES] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};
int running;


static HANDLE   hInstance;
long CALLBACK WndProc (HWND, UINT, UINT, LONG) ;


HWND    hwnd;

/****************************************************************************
 *
 * adc_to_mv
 *
 * If the user selects scaling to millivolts,
 * Convert an 12-bit ADC count into millivolts
 *
 ****************************************************************************/
int adc_to_mv (short int raw, int ch)
  {
  return ((raw * (input_ranges[ch])) / (PS3000_MAX_VALUE-1));
  }

/****************************************************************************
 *  Change millivolts to ADC counts, used for trigger levels
 ****************************************************************************/
short mv_to_adc (short mv, short ch)
  {
  return (short)((mv * 32767) / input_ranges[ch]);
  }

char * adc_units (short time_units)
  {
   switch (time_units)
     {
     case PS3000_FS:
       return "fs";
     case PS3000_PS:
       return "ps";
     case PS3000_NS:
       return "ns";
     case PS3000_US:
       return "us";
     case PS3000_MS:
       return "ms";
     case PS3000_S:
       return "s";
     }
     return "Not Known";
  }

/****************************************************************************
 *
 *
 ****************************************************************************/

long CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
  static int 	set_channels = 0;
  HDC 			hdc;
  static 		BOOL sig_gen = 0;
  PAINTSTRUCT 	ps;
  char 			str [80], str2[80];
  char 			*volt_range [PS3000_MAX_RANGES] = {"±10mV", "±20mV", "±50mV", "±100 mV", "±200 mV", "±500 mV", "±1V", "±2V", "±5V", "±10V", "±20V", "±50V"};
  short			i, j;
  short 			splashscreen = 1;
  long 			time_interval;
  short 			time_units;
  short 			oversample;
  long 			no_of_samples = 450;
  short 			timebase;
  long 			sig_gen_frequency;
  long 			sig_gen_finish;
  short 			increment;
  long 			time_indisposed_ms;
  short 			dwell_time;
  short 			repeat;
  short 			dual_slope;
  short 			trig_channel;
  short 			trig_volts, trig_direction, trig_delay;
  short 			auto_trigger_ms =0;
  short 			overflow;
  HPEN 			hpen, oldPen;
  char 			action [2][6] = {"Start", "Stop"};
  RECT  			rect;
  char 			description [6][25]=  {"Driver Version ","USB Version ","Hardware Version ",
                                   "Variant Info ","Serial ", "Error Code "};
  long 			max_samples;
  	

  switch ( message )
    {
    case WM_CREATE:
      
		unitOpened.handle = ps3000_open_unit ();

			if (unitOpened.handle)
			{
				ps3000_get_unit_info(unitOpened.handle, str, sizeof(str), 3);
				i = atoi(str);
				
				switch (i)
				{
					case MODEL_PS3206:
						unitOpened.model = MODEL_PS3206;
						unitOpened.external = TRUE;
						unitOpened.signalGenerator = TRUE;
						unitOpened.firstRange = PS3000_100MV;
						unitOpened.lastRange = PS3000_20V;
						unitOpened.timebases = PS3206_MAX_TIMEBASE;
						unitOpened.noOfChannels = DUAL_SCOPE;
					break;

					case MODEL_PS3205:
						unitOpened.model = MODEL_PS3205;
						unitOpened.external = TRUE;
						unitOpened.signalGenerator = TRUE;
						unitOpened.firstRange = PS3000_100MV;
						unitOpened.lastRange = PS3000_20V;
						unitOpened.timebases = PS3205_MAX_TIMEBASE;
						unitOpened.noOfChannels = DUAL_SCOPE;
					break;
				
					case MODEL_PS3204:
						unitOpened.model = MODEL_PS3204;
						unitOpened.external = TRUE;
						unitOpened.signalGenerator = TRUE;
						unitOpened.firstRange = PS3000_100MV;
						unitOpened.lastRange = PS3000_20V;
						unitOpened.timebases = PS3204_MAX_TIMEBASE;
						unitOpened.noOfChannels = DUAL_SCOPE;
					break;
				
					case MODEL_PS3224:
						unitOpened.model = MODEL_PS3224;
						unitOpened.external = FALSE;
						unitOpened.signalGenerator = FALSE;
						unitOpened.firstRange = PS3000_20MV;
						unitOpened.lastRange = PS3000_20V;
						unitOpened.timebases = PS3224_MAX_TIMEBASE;
						unitOpened.noOfChannels = DUAL_SCOPE;
					break;

					case MODEL_PS3424:
						unitOpened.model = MODEL_PS3424;
						unitOpened.external = FALSE;
						unitOpened.signalGenerator = FALSE;
						unitOpened.firstRange = PS3000_20MV;
						unitOpened.lastRange = PS3000_20V;
						unitOpened.timebases = PS3424_MAX_TIMEBASE;
						unitOpened.noOfChannels = QUAD_SCOPE;					
					break;

 					default:
						MessageBox(hwnd, "Unit not supported", "Variant Error", MB_OK);
						SendMessage(hwnd, WM_DESTROY, 0, 0);
				}
			}
			else
			{
					unitOpened.model = MODEL_NONE;
					unitOpened.external = TRUE;
					unitOpened.signalGenerator = TRUE;
					unitOpened.firstRange = PS3000_10MV;
					unitOpened.lastRange = PS3000_20V;
					unitOpened.timebases = PS3206_MAX_TIMEBASE;
					unitOpened.noOfChannels = QUAD_SCOPE;	
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
          unitOpened.channels[j].point[i] = HEIGHT/2;
				}
    	}

      running = FALSE;
      PostMessage ( hwnd, WM_PAINT, 0, 0 );
      SetTimer ( hwnd, ID_TIMER, 100, NULL );

      rect.right = 425 + WIDTH;
      rect.left = 425;
	    rect.bottom = HEIGHT;
      rect.top = 0;
			PostMessage (hwnd, WM_COMMAND, PS_CHANNELS, 0);
    break;

    case WM_PAINT:
      hdc = BeginPaint ( hwnd, &ps );

      Rectangle(hdc, 425, 0, 875, 340);

      hpen = CreatePen ( PS_DOT, 0, 0x000000 );
      oldPen = SelectObject ( hdc, hpen );
      MoveToEx (hdc, 425, HEIGHT/2, (LPPOINT) NULL );
      LineTo ( hdc, 425+WIDTH, HEIGHT/2 );

      for( i=0 ; i < 9 ; i++)
        {
        MoveToEx ( hdc, 425+(45*(1+i)), 0, (LPPOINT) NULL );
        LineTo ( hdc, 425+(45*(i+1)), HEIGHT );
        }

      DeleteObject ( SelectObject ( hdc, oldPen ) );

			for (j = 0; j < unitOpened.noOfChannels; j++)
			{
        if ( IsDlgButtonChecked( hwnd,IDC_CHA + j ) == BST_CHECKED )
        {
          hpen = CreatePen ( PS_SOLID, 0, (COLORREF)unitOpened.channels[j].lineColour);
          oldPen = SelectObject ( hdc, hpen );
          for ( i = 0; i < ( WIDTH-1 ); i++)
          {
					  MoveToEx ( hdc, 425+i, unitOpened.channels[PS3000_CHANNEL_A + j].point[i], (LPPOINT) NULL );
            LineTo ( hdc, 425+i+1, unitOpened.channels[PS3000_CHANNEL_A + j].point[i + 1]);
          }
          DeleteObject(SelectObject(hdc, oldPen));
        }
			}
 
      BitBlt(hdc, 0, 0, (int)425 + WIDTH, (int)HEIGHT, hdc, 0, 0, SRCCOPY);

      EndPaint(hwnd, &ps);

      if( !set_channels )
        {
        for ( i = 0; i < ( unitOpened.handle ? 5:2 ); i++)
          {
          ps3000_get_unit_info ( unitOpened.handle, str, sizeof (str), i );
          sprintf ( str2,"%s%s", description[i], str );
          SetDlgItemText ( hwnd, IDC_INFO1+i, str2 );
          }

        SendDlgItemMessage ( hwnd, IDC_COMBOBOX, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "None" );
        SendDlgItemMessage ( hwnd, IDC_COMBOBOX, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "Channel A" );
        SendDlgItemMessage ( hwnd, IDC_COMBOBOX, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "Channel B" );
				if (unitOpened.noOfChannels == QUAD_SCOPE)
				{
	        SendDlgItemMessage ( hwnd, IDC_COMBOBOX, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "Channel C" );
          SendDlgItemMessage ( hwnd, IDC_COMBOBOX, CB_ADDSTRING, 0, (LPARAM) (LPCSTR) "Channel D" );
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
          SendDlgItemMessage ( hwnd, IDC_VOLTAGE + j, CB_SETCURSEL, 0, 0 );
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

        if ((unitOpened.noOfChannels == DUAL_SCOPE             && 
					   IsDlgButtonChecked( hwnd,IDC_CHA ) != BST_CHECKED &&
             IsDlgButtonChecked( hwnd,IDC_CHB ) != BST_CHECKED)||
             (unitOpened.noOfChannels == QUAD_SCOPE            &&
						 IsDlgButtonChecked( hwnd,IDC_CHA ) != BST_CHECKED &&
             IsDlgButtonChecked( hwnd,IDC_CHB ) != BST_CHECKED &&
						 IsDlgButtonChecked( hwnd,IDC_CHC ) != BST_CHECKED &&
						 IsDlgButtonChecked( hwnd,IDC_CHD ) != BST_CHECKED)) 
          return 0;

				for (i = 0; i < unitOpened.noOfChannels; i++)
				{
          if ( unitOpened.channelSettings[PS3000_CHANNEL_A + i].enabled = (IsDlgButtonChecked ( hwnd,IDC_CHA + i) == BST_CHECKED) )
          {
					  unitOpened.channelSettings[PS3000_CHANNEL_A + i].range = (short) SendDlgItemMessage (hwnd, IDC_VOLTAGE + i, CB_GETCURSEL, 0, 0) + unitOpened.firstRange;
            GetDlgItemText ( hwnd, IDC_COUPLING + i, str, 10 );
            unitOpened.channelSettings[PS3000_CHANNEL_A + i].DCcoupled = strcmp( str, "DC" )== 0;
				  }

					ps3000_set_channel(unitOpened.handle,
						                 PS3000_CHANNEL_A + i,
						                 unitOpened.channelSettings[PS3000_CHANNEL_A + i].enabled,
														 unitOpened.channelSettings[PS3000_CHANNEL_A + i].DCcoupled,
														 unitOpened.channelSettings[PS3000_CHANNEL_A + i].range);
				}

        // get triggering info if checkbox true otherwise set to trigger to false and
        // use default values.
        // if trigger set but variables not entered use 0 volts on channel A.
        if ( IsDlgButtonChecked( hwnd,IDC_TRIGGER ) == BST_CHECKED )
        {
          GetDlgItemText ( hwnd, IDC_COMBOBOX, str, 10 );

          if ( strcmp( str, "Channel A") == 0 )
					{
						unitOpened.triggerRange = unitOpened.channelSettings[PS3000_CHANNEL_A].range;
            trig_channel = PS3000_CHANNEL_A;
					}
          else if ( strcmp ( str, "Channel B" ) == 0 )
					{
            trig_channel = PS3000_CHANNEL_B;
						unitOpened.triggerRange = unitOpened.channelSettings[PS3000_CHANNEL_B].range;
					}
          else if ( strcmp ( str, "Channel C" ) == 0 )
					{
            trig_channel = PS3000_CHANNEL_C;
						unitOpened.triggerRange = unitOpened.channelSettings[PS3000_CHANNEL_C].range;
					}
					else if( strcmp ( str, "Channel D" ) == 0 )
					{
            trig_channel = PS3000_CHANNEL_D;
						unitOpened.triggerRange = unitOpened.channelSettings[PS3000_CHANNEL_D].range;
					}
					else
					{
            trig_channel = PS3000_NONE;
						unitOpened.triggerRange =  unitOpened.lastRange;
					}

          trig_volts = (short) GetDlgItemInt( hwnd, IDC_TRG6, NULL, FALSE );
          GetDlgItemText ( hwnd, IDC_TRG7, str, 9 );
          if ( strcmp ( str, "Rising" ) == 0 )
            trig_direction = 0;
          else
            trig_direction = 1;

          trig_delay = (short)GetDlgItemInt ( hwnd, IDC_TRG8, NULL, TRUE );
        }
				else
				{
          trig_channel = PS3000_NONE;
					trig_direction= trig_delay= auto_trigger_ms = trig_volts = 0;
					unitOpened.triggerRange =  unitOpened.lastRange;
				}

        ps3000_set_trigger ( unitOpened.handle, trig_channel, mv_to_adc ( trig_volts, (short)unitOpened.triggerRange), trig_direction, trig_delay, auto_trigger_ms );
        // end setting trigger

				if (unitOpened.model != MODEL_PS3224 && unitOpened.model != MODEL_PS3424)
				{
				  // switch ets off
          ps3000_set_ets ( unitOpened.handle, PS3000_ETS_OFF, 0, 0 );
				}

				// Get the required timebase
        GetDlgItemText ( hwnd, IDC_TIMEBASE, str, 3 );
        timebase = atoi ( str );
				 
				// check that the timebase is valid
				oversample = 1;
				if(!ps3000_get_timebase ( unitOpened.handle, timebase, no_of_samples, &time_interval, &time_units, oversample, &max_samples))
				{
					return 0;
				}

        /* Start it collecting,
   	   *  then wait for completion
         */
  		  ps3000_run_block ( unitOpened.handle, no_of_samples, timebase, oversample, &time_indisposed_ms );

        while ( ( !ps3000_ready ( unitOpened.handle ) ) && !kbhit() );

  		  ps3000_stop ( unitOpened.handle );

  		  /* Should be done now...
   	   *  get the times (in nanoseconds)
   	   *   and the values (in ADC counts)
   	   */
        ps3000_get_times_and_values ( unitOpened.handle,
					                            times, 
																			unitOpened.channels[PS3000_CHANNEL_A].values,
																			unitOpened.channels[PS3000_CHANNEL_B].values,
																			unitOpened.channels[PS3000_CHANNEL_C].values,
																			unitOpened.channels[PS3000_CHANNEL_D].values,
																			&overflow, time_units, no_of_samples );
       for (j = 0; j < unitOpened.noOfChannels; j++)
			 {
         if(IsDlgButtonChecked ( hwnd,IDC_CHA + j ) == BST_CHECKED )
         {
					//SendDlgItemMessage ( hwnd, IDC_VALUES, LB_RESETCONTENT, 0, 0 );
					//  wsprintf ( str, "%s    mV", adc_units ( time_units ) );
					//  SendDlgItemMessage ( hwnd, IDC_VALUES, LB_ADDSTRING, 0, (LPARAM) (LPCSTR) str );
					//  for ( i = 0; i < no_of_samples; i++ )
					// {
					//   wsprintf ( str, "%ld %d", times[i], adc_to_mv ( unitOpened.channels[PS3000_CHANNEL_A].values[i],unitOpened.channelSettings[PS3000_CHANNEL_A].range ) );
					//   SendDlgItemMessage ( hwnd, IDC_VALUES, LB_ADDSTRING, 0, (LPARAM) (LPCSTR) str );
					// }

           for ( i = 0; i < WIDTH; i++)
           {
             if ( adc_to_mv ( unitOpened.channels[PS3000_CHANNEL_A + j].values[i],unitOpened.channelSettings[PS3000_CHANNEL_A + j].range ) >= 0 )
               unitOpened.channels[PS3000_CHANNEL_A + j].point[i] = (int)(170.f - (170.0f/(float)input_ranges[unitOpened.channelSettings[PS3000_CHANNEL_A + j].range])*(float)( adc_to_mv ( unitOpened.channels[PS3000_CHANNEL_A + j].values[i], unitOpened.channelSettings[PS3000_CHANNEL_A + j].range) ));
             else
               unitOpened.channels[PS3000_CHANNEL_A + j].point[i] = (int)(170.f + (170.0f/(float)input_ranges[unitOpened.channelSettings[PS3000_CHANNEL_A + j].range])*(float)( abs( adc_to_mv(unitOpened.channels[PS3000_CHANNEL_A + j].values[i], unitOpened.channelSettings[PS3000_CHANNEL_A + j].range) ) ));
           }
         }
			 }
       rect.left = 425;
       rect.right = 425 + WIDTH;
       rect.top = 0;
       rect.bottom = HEIGHT;
       InvalidateRect ( hwnd, &rect, TRUE );
     }
    break;

    case WM_COMMAND:
    switch ( wParam )
      {
		  case PS_CHANNELS:
			  // disable channels C and D if only two channels on the device
			  if(unitOpened.noOfChannels == DUAL_SCOPE)
			  {
				  for (i = 0; i < 2; i++)
				  {
				    EnableWindow(GetDlgItem(hwnd, IDC_VOLTAGE_C + i), FALSE);
				    EnableWindow(GetDlgItem(hwnd, IDC_CHC + i), FALSE);
				    EnableWindow(GetDlgItem(hwnd, IDC_COUPLING_C + i), FALSE);
			  	}
			  }

				if(unitOpened.model == MODEL_PS3224 || unitOpened.model == MODEL_PS3424)
			  {
          EnableWindow(GetDlgItem(hwnd, IDC_EDIT1), FALSE);
          EnableWindow(GetDlgItem(hwnd, IDC_SWEEP), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_SIGGEN), FALSE);
				}
			break;

      case IDC_OK:
         if ( !unitOpened.handle )
	        {
           MessageBox ( NULL, "Unit Not Open", "Error", MB_OK );
           return 0;
           }

         running = !running;
         SetDlgItemText ( hwnd, IDC_OK, action[running] );
      break;

      case IDC_SWEEP:
        if ( unitOpened.handle )
          {
          if( IsDlgButtonChecked ( hwnd,IDC_SWEEP ) == BST_CHECKED )
            {
            EnableWindow( GetDlgItem ( hwnd,IDC_EDIT2 ), TRUE );
            EnableWindow( GetDlgItem ( hwnd,IDC_EDIT3 ), TRUE );
            EnableWindow( GetDlgItem ( hwnd,IDC_EDIT4 ), TRUE );
            EnableWindow( GetDlgItem ( hwnd,IDC_EDIT5 ), TRUE );
            EnableWindow( GetDlgItem ( hwnd,IDC_EDIT9 ), TRUE );
            }
          else
            {
            EnableWindow ( GetDlgItem ( hwnd,IDC_EDIT2 ), FALSE );
            EnableWindow ( GetDlgItem ( hwnd,IDC_EDIT3 ), FALSE );
            EnableWindow ( GetDlgItem ( hwnd,IDC_EDIT4 ), FALSE );
            EnableWindow ( GetDlgItem ( hwnd,IDC_EDIT5 ), FALSE );
            EnableWindow ( GetDlgItem ( hwnd,IDC_EDIT9 ), FALSE );
            }
          InvalidateRect(hwnd, NULL, TRUE);
          }
      break;

      case IDC_SIGGEN:
        if ( unitOpened.handle )
          {
          if ( sig_gen )
            {
            sig_gen_frequency = sig_gen_finish = 0;
            increment = 0;
            dual_slope = 0;
            repeat = 0;
            dwell_time = 0;
            SetDlgItemText ( hwnd, IDC_SIGGEN, "Off" );
            }
          else
            {
            SetDlgItemText ( hwnd, IDC_SIGGEN, "On" );
            if ( IsDlgButtonChecked(hwnd,IDC_SWEEP ) == BST_CHECKED )
            {
                sig_gen_frequency = GetDlgItemInt( hwnd, IDC_EDIT1, NULL, FALSE );
                sig_gen_finish = GetDlgItemInt ( hwnd, IDC_EDIT2, NULL, FALSE );
                if ( sig_gen_frequency == sig_gen_finish )
                  sig_gen_finish = sig_gen_frequency + 10000;
                else if ( sig_gen_finish == 0 )
                  sig_gen_finish = 1;
                sprintf ( str, "%ld", sig_gen_finish );
                SetDlgItemText ( hwnd, IDC_EDIT2, str );
                sprintf ( str, "%ld", sig_gen_frequency );
                SetDlgItemText ( hwnd, IDC_EDIT1, str );
                dwell_time = (short) GetDlgItemInt ( hwnd, IDC_EDIT3, NULL, FALSE );
                if( dwell_time == 0 )
                {
                  dwell_time = 100;
                  SetDlgItemText ( hwnd, IDC_EDIT3, "100" );
                }
                repeat = (short) GetDlgItemInt( hwnd, IDC_EDIT4, NULL, FALSE );
                if( repeat == 0 )
                {
                  SetDlgItemText ( hwnd, IDC_EDIT4, "0" );
                }
                dual_slope = (short) GetDlgItemInt( hwnd, IDC_EDIT5, NULL, FALSE );
                if( dual_slope == 0 )
                {
                  SetDlgItemText ( hwnd, IDC_EDIT5, "0" );
                }
                increment = (short) GetDlgItemInt( hwnd, IDC_EDIT9, NULL, FALSE );
                if ( increment == 0 )
                {
                  increment = 10;
                  SetDlgItemText ( hwnd, IDC_EDIT9, "10" );
                }
            }
            else
            {
               sig_gen_frequency = sig_gen_finish = GetDlgItemInt ( hwnd, IDC_EDIT1, NULL, FALSE );
               if ( sig_gen_frequency == 0 )
               {
                  sig_gen_frequency = sig_gen_finish = 1000;
                  SetDlgItemText ( hwnd, IDC_EDIT1, "1000" );
               }
               increment = 0;
               dual_slope = 0;
               repeat = 0;
               dwell_time = 0;
            }
         }
         sig_gen = !sig_gen;
         ps3000_set_siggen ( unitOpened.handle, PS3000_SINE, sig_gen_frequency, sig_gen_finish, increment,
                            dwell_time, repeat, dual_slope );
      }
      break;
    }
    break;
    case WM_DESTROY:
      KillTimer(hwnd, ID_TIMER);
      ps3000_close_unit (unitOpened.handle);
      PostQuitMessage (0);
    return 0;

  }
  return DefWindowProc ( hwnd, message, wParam, lParam );
}

/****************************************************************************
 *
 *
 ****************************************************************************/

int CALLBACK WinMain (
  HINSTANCE hInst,
  HINSTANCE hPrevInstance,
  LPSTR lpszCmdLine,
  int32_t nCmdShow )

  {
  static int8_t 	szClass [] = "ps3000";
  MSG         	msg;
  WNDCLASS    	wndclass;

  hInstance = hInst;

  if ( !hPrevInstance )
    {
    wndclass.style          = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc    = WndProc;
    wndclass.cbClsExtra     = 0;
    wndclass.cbWndExtra     = DLGWINDOWEXTRA;
    wndclass.hInstance      = hInstance;
    wndclass.hIcon          = NULL;
    wndclass.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wndclass.hbrBackground  = (HBRUSH) (COLOR_WINDOW);
    wndclass.lpszMenuName   = NULL;
    wndclass.lpszClassName  = (LPSTR) szClass;
    RegisterClass (&wndclass);
   }

  hwnd = CreateDialog ( hInstance, MAKEINTRESOURCE ( IDD_MAIN ), 0, NULL ) ;

  ShowWindow (hwnd, nCmdShow);
  UpdateWindow(hwnd);

  while ( GetMessage ( &msg, NULL, 0, 0 ) )
    {
    TranslateMessage ( &msg );
    DispatchMessage ( &msg );
    }

  return msg.wParam ;
  }

