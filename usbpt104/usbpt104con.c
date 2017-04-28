/*******************************************************************************
 * 
 * Filename: usbpt104con.c
 *
 * Description:
 *   This is a console-mode program that demonstrates how to use the
 *   usbpt104 driver API functions for the USB PT-104 Platinum Resistance
 *	 Data Logger.
 *
 * Examples:
 *    How to set up the channels
 *    How to collect data via both USB and ethernet connections
 *    How to enable ethernet and set the unit's IP address and port
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (Win32/x64)
 *			Ensure that the 32-/64-bit usbpt104.lib can be located
 *			Ensure that the usbpt104Api.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			Set up a project for a 32-bit/64-bit console mode application
 *			Add this file to the project
 *			Add usbpt104.lib to the project (Microsoft C only)
 *			Add usbpt104Api.h and PicoStatus.h to the project
 *			Then build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libusbpt104 driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the usbpt104con application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 *	Copyright (C) 2010 - 2017 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
#ifdef WIN32
#include <conio.h>
#include <windows.h>
#include "usbPT104Api.h"

#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "libusbpt104-1.0/UsbPT104Api.h"

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL{FALSE,TRUE} BOOL;

/* A function to detect a keyboard press on Linux */
int32_t _getch()
{
        struct termios oldt, newt;
        int32_t ch;
        int32_t bytesWaiting;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~( ICANON | ECHO );
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        setbuf(stdin, NULL);
        do {
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
        newt.c_lflag &= ~( ICANON | ECHO );
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        setbuf(stdin, NULL);
        ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return bytesWaiting;
}

int32_t fopen_s(FILE ** a, const char * b, const char * c)
{
FILE * fp = fopen(b,c);
*a = fp;
return (fp>0)?0:-1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

#define NUM_CHANNELS 4

typedef struct 
{
	USBPT104_DATA_TYPES		measurementType;
	int16_t					noWires;
}PT104ChannelSettings;

int16_t g_handle;
PICO_STATUS g_status;
PT104ChannelSettings channelSettings[NUM_CHANNELS];


// Routine to allow the user to change channel settings
void ChannelSetUp()
{
	int16_t channel;
	int32_t	wires;
	int32_t	type;

	printf("Measurement type options:\n\n");

	printf("0:\tOFF\n");
	printf("1:\tPT100\n");
	printf("2:\tPT1000\n");
	printf("3:\tResistance to 375 Ohms\n");
	printf("4:\tResistance to 10 kOhms\n");
	printf("5:\tDifferential voltage to 115 mV\n");
	printf("6:\tDifferential voltage to 2500 mV\n");
	printf("7:\tSingle-ended voltage to 115 mV\n");
	printf("8:\tSingle-ended voltage to 2500 mV\n\n");

	g_status = PICO_OK;

	for(channel = 0; channel < NUM_CHANNELS && g_status == PICO_OK; channel++)
	{
		printf("\nChannel %d:-\n", channel + 1);

		do
		{
			printf("Enter measurement type: ", channel);
			scanf_s("%d", &type);

		}while(type < 0 || type > 8);

		channelSettings[channel].measurementType = (USBPT104_DATA_TYPES) type;

		// If channel is OFF, we don't need to set the number of wires
		if(type == 0) continue;

		do
		{
			printf("Enter number of wires: ", channel);
			scanf_s("%d", &wires);
		}while(wires < 2 || wires > 4);

		channelSettings[channel].noWires = wires;
	}
}

int8_t* MeasurementTypeToString(USBPT104_DATA_TYPES measurementType)
{
	switch(measurementType)
	{
		case USBPT104_OFF:
			return (int8_t*) "OFF";

		case USBPT104_PT100:
			return (int8_t*) "PT100";

		case USBPT104_PT1000:
			return (int8_t*) "PT1000";

		case USBPT104_RESISTANCE_TO_375R:
			return (int8_t*) "Resistance to 375 Ohms";

		case USBPT104_RESISTANCE_TO_10K:
			return (int8_t*) "Resistance to 10 kOhms";

		case USBPT104_DIFFERENTIAL_TO_115MV:
			return (int8_t*) "Differential voltage to 115 mV";

		case USBPT104_DIFFERENTIAL_TO_2500MV:
			return (int8_t*) "Differential voltage to 2500 mV";

		case USBPT104_SINGLE_ENDED_TO_115MV:
			return (int8_t*) "Single-ended voltage to 115 mV";

		case USBPT104_SINGLE_ENDED_TO_2500MV:
			return (int8_t*) "Single-ended voltage to 2500 mV";

		default:
			return (int8_t*) "ERROR";
	}
}

// Convert values to degrees C, Ohms or millivolts
double ApplyScaling(int32_t value, int16_t channel)
{
	switch(channelSettings[channel].measurementType)
	{
		case USBPT104_OFF:
			return 0;

		case USBPT104_PT100:
			return value / 1000.0;

		case USBPT104_PT1000:
			return value / 1000.0;

		case USBPT104_RESISTANCE_TO_375R:
			return value / 1000000.0;

		case USBPT104_RESISTANCE_TO_10K:
			return value / 1000.0;

		case USBPT104_DIFFERENTIAL_TO_115MV:
			return value / 1000000.0;

		case USBPT104_DIFFERENTIAL_TO_2500MV:
			return value / 100000.0;

		case USBPT104_SINGLE_ENDED_TO_115MV:
			return value / 1000000.0;

		case USBPT104_SINGLE_ENDED_TO_2500MV:
			return value / 100000.0;

		default:
			return -1;
	}
}

void CollectData()
{
	int16_t channel;
	int32_t values[NUM_CHANNELS];
	double scaledValues[NUM_CHANNELS];

	g_status = PICO_OK;

	// Display channel settings
	printf("\n");
	printf("Settings:\n\n");
	
	for(channel = 0; channel < NUM_CHANNELS; channel++)
	{
		printf("Channel %d\n", channel + 1);
		printf("Measurement Type: %s\n", MeasurementTypeToString(channelSettings[channel].measurementType));
		printf("Number of Wires: %d\n\n", channelSettings[channel].noWires);
	}


	// Set channels
	for(channel = 0; channel < NUM_CHANNELS && g_status == PICO_OK; channel++)
	{
		g_status = UsbPt104SetChannel(	g_handle, 
										(USBPT104_CHANNELS) (channel + 1), 
										channelSettings[channel].measurementType, 
										channelSettings[channel].noWires);
	}

	if(g_status != PICO_OK)
	{
		printf("\n\nSetChannel: Status = 0x%X\nPress any key", g_status);
		_getch();
		return;
	}

	printf("Readings will be in degrees C, Ohms or millivolts depending on the channel settings.\n\n");
	printf("Press any key to start.\n\n");
	_getch();

	printf("Press any key to stop data collection...\n\n");

	for(channel = 0; channel < NUM_CHANNELS; channel++)
	{
		printf("Ch %d:\t\t", channel);
	}

	printf("\n\n");

	// Allow time for collection of data from all 4 channels
	Sleep(2880);

	while(!_kbhit() && (g_status == PICO_OK || g_status == PICO_NO_SAMPLES_AVAILABLE || g_status == PICO_WARNING_REPEAT_VALUE))
	{
		for(channel = 0; channel < NUM_CHANNELS && (g_status == PICO_OK || g_status == PICO_NO_SAMPLES_AVAILABLE); channel++)
		{
			g_status = UsbPt104GetValue(g_handle, (USBPT104_CHANNELS) (channel + 1), &values[channel], 0);
				
			if(g_status == PICO_NO_SAMPLES_AVAILABLE) 
			{
				values[channel] = 0;
			}

			scaledValues[channel] = ApplyScaling(values[channel], channel);
		}

		printf("%.4f\t\t%.4f\t\t%.4f\t\t%.4f\n", scaledValues[0], scaledValues[1], scaledValues[2], scaledValues[3]);
		Sleep(2280); // Allow time for collection of data from all 4 channels
	}
		
	if(g_status != PICO_OK && g_status != PICO_NO_SAMPLES_AVAILABLE)
	{
		printf("\n\nGetValue: Status = 0x%X\nPress any key", g_status);
	}

	_getch();
}

void EthernetSettings()
{
	int32_t tmp;
	int16_t enabled;
	int8_t IPAddress[20], ch;
	uint16_t length, port;

	//Display current ethernet settings
	g_status = UsbPt104IpDetails(g_handle, &enabled, IPAddress, &length, &port, IDT_GET);

	if(g_status != PICO_OK)
	{
		printf("IP details: Status = 0x%X", g_status);
		return;
	}

	printf("\nEthernet Settings\n\n");
	printf("Enabled:\t%d\n", enabled);
	printf("IP Address:\t%s\n", IPAddress);
	printf("Port:\t\t%d\n", port);

	//Enter new settings
	printf("\nEdit settings? (Y/N)\n");
	ch = toupper(_getch());

	if (ch == 'Y')
	{
		printf("Enable ethernet? (Y/N)\n");
		ch = toupper(_getch());
		
		if (ch == 'N')
		{
			enabled = 0;
		}
		else
		{
			enabled = 1;
			
			fflush(stdin);
			printf("Enter IP address: ");
			scanf("%s", IPAddress);
			length = sizeof(IPAddress);

			printf("Enter port: ");
			scanf_s("%d", &tmp);
			port = (uint16_t)tmp;
		}
		
		g_status = UsbPt104IpDetails(g_handle, &enabled, IPAddress, &length, &port, IDT_SET);

		if (g_status != PICO_OK)
		{
			printf("IP details: Status = 0x%X", g_status);
			return;
		}
	}
}

void main()
{
	
	int8_t IPAddress[20], ch, info[40];
	int8_t details[120]; // Ensure size of the details array is large enough to accomodate the information for all devices detected
	int16_t infoLength = 40;

	int8_t description [7][25]= {   "Driver Version  ",
									"USB Version     ",
									"Hardware Version",
									"Variant Info    ",
									"Serial          ",
									"Cal Date        ",
									"Kernel Version  "};

	int16_t channel, requiredSize;
	int16_t validSelection = 0;
	int16_t USB;
	int16_t i;

	uint32_t detailsLength = 120; // Size of the details array
	
	PICO_STATUS status = PICO_OK;

	printf("USB PT-104 (usbpt104) Driver Example Program\n\n");

	printf("Enumerating devices...\n\n");

	// Enumerate all USB and Ethernet devices
	status = UsbPt104Enumerate(details, &detailsLength, CT_ALL);

	if(detailsLength > 0)
	{
		printf("USB PT-104 devices found: %s\n", details);
	}
	else
	{
		printf("No USB PT-104 devices found.\n");
	}

	// User must select USB or Ethernet before the device is opened
	do
	{
		printf("\n\n");
		printf("Select connection:\n");
		printf("U:\tUSB\n");
		printf("E:\tEthernet\n\n");

		ch = toupper(_getch());

		switch(ch)
		{
		case 'U':
			USB = 1;
			validSelection = 1;
			break;

		case 'E':
			USB = 0;
			validSelection = 1;
			break;

		default:
			printf("Invalid input.\n");
		}

	}while(!validSelection);


	// Open unit
	if (USB)
	{
		g_status = UsbPt104OpenUnit(&g_handle, NULL);
	}
	else
	{
		// Get IP address
		printf("Enter IP address of the USB PT-104 in the format IPAddress:port \nand press Enter: ");
		scanf("%s", IPAddress);

		g_status = UsbPt104OpenUnitViaIp(&g_handle, NULL, IPAddress);
	}
	
	if (g_status != PICO_OK)
	{
		printf("Unable to open device. Status code: 0x%X", g_status);
		_getch();
		return;
	}
	else
	{
		printf("\n");
		printf("USB PT-104 opened:-\n");
		printf("-------------------\n\n");

	}

	// Set default channel settings
	for (channel = 0; channel < NUM_CHANNELS; channel++)
	{
		channelSettings[channel].measurementType	= USBPT104_PT100;
		channelSettings[channel].noWires			= 4;
	}

	// Get unit information
	for (i = 0; i < 7; i++)
	{
		g_status = UsbPt104GetUnitInfo(g_handle, info, infoLength, &requiredSize, (PICO_INFO) i);
		printf("%s: %s\n", description[i], info);
	}

	// Display MAC Address

	g_status = UsbPt104GetUnitInfo(g_handle, info, infoLength, &requiredSize, PICO_MAC_ADDRESS);
	printf("MAC Address     : %s\n", info);
	printf("\n");

	if (g_status != PICO_OK)
	{
		printf("Error. Status code: 0x%X", g_status);
		_getch();
		return;
	}

	do
	{
		printf("\n\n");
		printf("S:\tStart Aquisition\n");
		printf("C:\tChannel Settings\n");
		printf("E:\tEthernet Settings\n");
		printf("X:\tExit\n\n");

		ch = toupper(_getch());

		switch(ch)
		{
			case 'S':
				CollectData();
				break;

			case 'C':
				ChannelSetUp();
				break;

			case 'E':
				if(!USB)
				{
					printf("Connect via USB to set up ethernet.");
					break;
				}
				EthernetSettings();
				break;

			case 'X':
				break;

			default:
				printf("Invalid input.\n");
		}

	}while(ch != 'X');

	// Close unit
	UsbPt104CloseUnit(g_handle);
}