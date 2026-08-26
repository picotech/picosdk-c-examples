/******************************************************************************
*
* Filename: usbtc08Gui.c
*
* Description:
*   This is a Windows application that demonstrates how to setup and
*	collect data from a USB TC-08 Thermocouple Data Logger using the
*	usbtc08 driver functions.
*
* To build this application:-
*
*		If Microsoft Visual Studio (including Express) is being used:
*
*			Select the solution configuration (Debug/Release) and platform (x86/x64)
*			Ensure that the 32-/64-bit usbtc08.lib can be located
*			Ensure that the usbtc08.h file can be located
*
*		Otherwise:
*
*			 Set up a project for a 32-/64-bit console mode application
*			 Add this file to the project
*			 Add usbtc08.lib to the project (Microsoft C only)
*			 Add usbtc08.h to the project
*			 Build the project
*
* Copyright (C) 2007-2018 Pico Technology Ltd. See LICENSE file for terms.
*
******************************************************************************/

#include <windows.h>
#include <stdio.h>
#include "usbtc08.h"


/* Thermocouple type applied to channels 1 to 8. */
#define TC_TYPE_K 'K'

/* The cold junction (channel 0) takes this dedicated type, not a
 * thermocouple type. */
#define TC_TYPE_CJC 'C'

#define ID_TIMER 1

/* Number of channels reported by the driver: the cold junction (channel 0)
 * plus the eight thermocouple inputs. */
#define NUM_TC08_CHANNELS (USBTC08_MAX_CHANNELS + 1)

/* Maximum number of units this example will open. Deliberately not named
 * MAX_TC08_UNITS: usbtc08.h already defines that for the legacy API. */
#define MAX_OPEN_UNITS 4

/* Mains rejection frequency passed to usb_tc08_set_mains(). The driver's
 * argument is named "sixty_hertz", so 0 selects 50 Hz rejection and 1 selects
 * 60 Hz. Change this to MAINS_REJECT_50HZ on a 50 Hz supply. */
#define MAINS_REJECT_50HZ 0
#define MAINS_REJECT_60HZ 1

// allow upto 4 units to connect, but currently set up for only first unit found
int16_t	hTC08 [MAX_OPEN_UNITS];
int tc08_found;


/****************************************************************************
*  Close every unit this example has opened
****************************************************************************/
void close_all_units (void)
{
	int i;

	for (i = 0; i < tc08_found; i++)
	{
		usb_tc08_stop (hTC08 [i]);
		usb_tc08_close_unit (hTC08 [i]);
	}

	tc08_found = 0;
}


/****************************************************************************
*  Set channels
*
*  Returns zero if any channel could not be set up.
****************************************************************************/
int16_t set_channels (void)
{
	int16_t channel;
	int16_t ok;

	/* Channel 0 is the cold junction and takes the dedicated 'C' type. */
	ok = usb_tc08_set_channel (hTC08 [0], USBTC08_CHANNEL_CJC, TC_TYPE_CJC);

	for (channel = USBTC08_CHANNEL_1; channel < NUM_TC08_CHANNELS && ok; channel++)
	{
		/* Each result is tested in turn: the driver documents "non-zero means
		 * success", so the individual codes must not be combined. */
		ok = usb_tc08_set_channel (hTC08 [0], channel, TC_TYPE_K);
	}

	return ok;
}


/****************************************************************************
*
*
****************************************************************************/
LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam,
											 LPARAM lParam)
{
	int   c;
	int16_t ok;
	static BOOL in_timer;
	char  line [80];
	float temp_buffer [NUM_TC08_CHANNELS];
	int16_t overflow [NUM_TC08_CHANNELS];

	switch (message)
	{
	case WM_TIMER:
		if (!in_timer)
		{
			in_timer = TRUE;

			/* An overflow array is passed rather than NULL so the driver always
			 * has somewhere to record over-range channels. */
			ok = usb_tc08_get_single (hTC08[0], temp_buffer, overflow, USBTC08_UNITS_CENTIGRADE);

			if (ok)
			{
				for (c = USBTC08_CHANNEL_CJC; c < NUM_TC08_CHANNELS; c++)
				{
					/* snprintf() bounds the write; sprintf() could not. */
					snprintf (line, sizeof(line), "%f", temp_buffer[c]);
					SetDlgItemText (hwnd, 100 + c, line);
				}
			}

			in_timer = FALSE;
		}

		return 0;

	case WM_DESTROY:

		close_all_units ();

		KillTimer (hwnd, ID_TIMER);
		PostQuitMessage (0) ;
		return 0 ;
	}

	return DefWindowProc (hwnd, message, wParam, lParam) ;
}


/****************************************************************************
*
*
****************************************************************************/

int WINAPI WinMain(HINSTANCE hInst,
									 HINSTANCE hPrevInstance,
									 LPSTR lpszCmdLine,
									 int nCmdShow)
{
	static char szAppName [] = "usb_tc08";
	HWND        hwnd ;
	MSG         msg;
	WNDCLASS    wndclass ;
	char			  str [64];
	int         c;
	int16_t     hTemp;
	BOOL        result;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpszCmdLine);

	wndclass.style          = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc    = WndProc;
	wndclass.cbClsExtra     = 0 ;
	wndclass.cbWndExtra     = DLGWINDOWEXTRA ;
	wndclass.hInstance      = hInst;
	wndclass.hIcon          = LoadIcon (hInst, szAppName) ;
	wndclass.hCursor        = LoadCursor (NULL, IDC_ARROW) ;
	/* COLOR_WINDOW is a system colour index, so it has to be turned into a
	 * brush handle: the +1 form is what Windows expects here. */
	wndclass.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
	wndclass.lpszMenuName   = NULL;
	wndclass.lpszClassName  = szAppName ;

	if (!RegisterClass (&wndclass))
	{
		MessageBox (NULL, "Unable to register the window class", "USB TC-08 Report", MB_ICONEXCLAMATION);
		return 1;
	}

	tc08_found = 0;

	do
	{
		hTemp = usb_tc08_open_unit();

		if (hTemp > 0)
		{
			hTC08 [tc08_found] = hTemp;
			tc08_found++;
		}
		else if (hTemp < 0)
		{
			snprintf (str, sizeof(str), "Error Code: %d", usb_tc08_get_last_error (0));
			MessageBox (NULL, str, "Error", MB_ICONEXCLAMATION);
			/* Release anything already opened before leaving. */
			close_all_units ();
			return 1;
		}

	} while (hTemp && tc08_found < MAX_OPEN_UNITS);

	if (!tc08_found)
	{
		MessageBox (NULL, "No USB TC-08 devices found", "USB TC-08 Report", MB_ICONEXCLAMATION);
		return 1;
	}

	if (!set_channels ())
	{
		snprintf (str, sizeof(str), "Error setting up channels: %d", usb_tc08_get_last_error (hTC08[0]));
		MessageBox (NULL, str, "Error", MB_ICONEXCLAMATION);
		close_all_units ();
		return 1;
	}

	for (c = 0; c < tc08_found; c++)
	{
		usb_tc08_set_mains (hTC08[c], MAINS_REJECT_60HZ);
	}

	hwnd = CreateDialog (hInst, szAppName, 0, NULL) ;

	if (hwnd == NULL)
	{
		MessageBox (NULL, "Unable to create the dialog", "USB TC-08 Report", MB_ICONEXCLAMATION);
		close_all_units ();
		return 1;
	}

	if (!SetTimer (hwnd, ID_TIMER, 1000, NULL))
	{
		MessageBox (NULL, "Unable to create the update timer", "USB TC-08 Report", MB_ICONEXCLAMATION);
		DestroyWindow (hwnd);
		close_all_units ();
		return 1;
	}

	ShowWindow (hwnd, nCmdShow) ;

	/* GetMessage() returns -1 on error, which is truthy: the result has to be
	 * tested against zero rather than used as a plain condition, or an error
	 * would spin this loop forever. */
	while ((result = GetMessage (&msg, NULL, 0, 0)) != 0)
	{
		if (result == -1)
		{
			break;
		}

		DispatchMessage (&msg) ;
	}

	return (int) msg.wParam ;
}

