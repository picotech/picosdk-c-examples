/******************************************************************************
*
* Filename: usbtc08gui.c
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
* Copyright (C) 2007 - 2017 Pico Technology Ltd. See LICENSE file for terms.
*
******************************************************************************/

#include <windows.h>
#include <stdio.h>
#include "usbtc08.h"


#define TC_TYPE_K 'K'

#define ID_TIMER 1

// allow upto 4 units to connect, but currently set up for only first unit found
short	hTC08 [4];
int tc08_found;


/****************************************************************************
*  Set channels 
****************************************************************************/
void set_channels (void)
{
	short channel;
	short ok;

	for (channel = 0; channel <= USBTC08_MAX_CHANNELS; channel++)
	{
		ok = usb_tc08_set_channel (hTC08 [0], channel, TC_TYPE_K);

	}
}


/****************************************************************************
*
*
****************************************************************************/
long PASCAL WndProc (HWND hwnd, UINT message, UINT wParam,
										 LONG lParam)
{
	int   c, i;
	BOOL  ok;
	static BOOL in_timer;
	static BOOL opened = FALSE;
	char  line [80];
	float temp_buffer [9];
	short overflow;

	static FARPROC lpfnPortDlgProc;
	static FARPROC lpfnChannelDlgProc;
	static HANDLE hInstance;

	switch (message)
	{
	case WM_TIMER:
		if (!in_timer)
		{
			in_timer = TRUE;

			ok = usb_tc08_get_single (hTC08[0], temp_buffer, &overflow, USBTC08_UNITS_CENTIGRADE);
			for (c = 0; c <= USBTC08_MAX_CHANNELS; c++)
			{       
				if(ok)
				{
					sprintf (line, "%f", temp_buffer[c]);
					SetDlgItemText (hwnd, 100 + c, line);
				}
			}
			in_timer = FALSE;
		}

		return 0;

	case WM_DESTROY:

		for (i = 0; i < tc08_found; i++)
		{
			usb_tc08_stop (hTC08 [i]);
			usb_tc08_close_unit (hTC08 [i]);
		}

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

int PASCAL WinMain(HINSTANCE hInst,
									 HINSTANCE hPrevInstance,
									 LPSTR lpszCmdLine,
									 int nCmdShow)
{
	static char szAppName [] = "usb_tc08";
	HWND        hwnd ;
	MSG         msg;
	WNDCLASS    wndclass ;
	char        buffer [10];
	char        key_text[10];
	char        ch_buffer[8];
	char			  str [20];
	int         c;
	short      hTemp;
	long				time_interval;

	if (!hPrevInstance)
	{
		wndclass.style          = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc    = WndProc;
		wndclass.cbClsExtra     = 0 ;
		wndclass.cbWndExtra     = DLGWINDOWEXTRA ;
		wndclass.hInstance      = hInst;
		wndclass.hIcon          = LoadIcon (hInst, szAppName) ;
		wndclass.hCursor        = LoadCursor (NULL, IDC_ARROW) ;
		wndclass.hbrBackground  = COLOR_WINDOW;
		wndclass.lpszMenuName   = NULL;
		wndclass.lpszClassName  = szAppName ;

		RegisterClass (&wndclass) ;
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
			wsprintf (str, "Error Code: %d", usb_tc08_get_last_error (0));
			MessageBox (NULL, str, "Error", MB_ICONEXCLAMATION);
			exit (99);
		}

	} while (hTemp && tc08_found < 4);

	if (!tc08_found)
	{
		MessageBox (NULL, "No USB TC-08's Found", "TC-08 Report", MB_ICONEXCLAMATION);
		exit (99);
	}

	set_channels ();

	for (c = 0; c < tc08_found; c++)
	{
		usb_tc08_set_mains (hTC08[c], TRUE);
	}

	hwnd = CreateDialog (hInst, szAppName, 0, NULL) ;

	SetTimer (hwnd, ID_TIMER, 1000, NULL);

	ShowWindow (hwnd, nCmdShow) ;

	while (GetMessage (&msg, NULL, 0, 0))
	{
		DispatchMessage (&msg) ;
	}

	return msg.wParam ;
}

