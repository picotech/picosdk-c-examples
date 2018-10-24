/**************************************************************************
*
* Filename: plcm3Con.c
*
* Description:
*   This is a console-mode program that demonstrates how to use the
*   plcm3 driver API functions for the PicoLog CM3 Current Data Logger.
*
* Examples:
*    How to set up the channels
*    How to collect data via both USB and ethernet connections
*    How to enable ethernet and set the unit's IP address
*
*	To build this application:-
*
*		If Microsoft Visual Studio (including Express) is being used:
*
*			Select the solution configuration (Debug/Release) and platform (Win32/x64)
*			Ensure that the 32-/64-bit plcm3.lib can be located
*			Ensure that the plcm3Api.h and PicoStatus.h files can be located
*
*		Otherwise:
*
*			Set up a project for a 32-bit/64-bit console mode application
*			Add this file to the project
*			Add plcm3.lib to the project (Microsoft C only)
*			Add plcm3Api.h and PicoStatus.h to the project
*			Then build the project
*
*  Copyright (C) 2011-2018 Pico Technology Ltd. See LICENSE file for terms.
*
*************************************************************************/

#include <stdio.h>
#ifdef WIN32
#include <conio.h>
#include <windows.h>

#include "plcm3Api.h"
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "libplcm3-1.0/PLCM3Api.h"

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

#define NUM_CHANNELS 3

typedef struct 
{
	PLCM3_DATA_TYPES measurementType;
}PLCM3ChannelSettings;

PLCM3ChannelSettings channelSettings[NUM_CHANNELS];
int16_t g_handle;
PICO_STATUS g_status;

// Routine to allow the user to change channel settings
void ChannelSetUp()
{
	int16_t channel;
	int	type;

	printf("0:\tOFF\n");
	printf("1:\t1mV range (1mV/A)\n");
	printf("2:\t10mV range (10mV/A)\n");
	printf("3:\t100mV range (100mV/A)\n");
	printf("4:\tVoltage input\n");
	

	g_status = PICO_OK;

	for (channel = 0; channel < NUM_CHANNELS && g_status == PICO_OK; channel++)
	{
		printf("channel %d\n", channel + 1);

		do
		{
			printf("Enter measurement type: ", channel);
			scanf_s("%d", &type);
		}while (type < 0 || type > 4);

		channelSettings[channel].measurementType = (PLCM3_DATA_TYPES) type;
	}
}

int8_t* MeasurementTypeToString(PLCM3_DATA_TYPES measurementType)
{
	switch(measurementType)
	{
		case PLCM3_OFF:
			return (int8_t *) "OFF";

		case PLCM3_1_MILLIVOLT:
			return (int8_t *) "PLCM3_1MV";

		case PLCM3_10_MILLIVOLTS:
			return (int8_t *) "PLCM3_10MV";

		case PLCM3_100_MILLIVOLTS:
			return (int8_t *) "PLCM3_100MV";

		case PLCM3_VOLTAGE:
			return (int8_t *) "PLCM3_VOLTAGE";

		default:
			return (int8_t *) "ERROR";
	}
}
	
// Convert values depending upon measurement type
// values are returned as uV, so /1000 to give mV reading
double ApplyScaling(int32_t  value, int16_t channel, int8_t *units)
{

	switch(channelSettings[channel].measurementType)
	{
		case PLCM3_OFF:
			strcpy(units, "");
			return 0;

		case PLCM3_1_MILLIVOLT:
			strcpy(units, "A");
			return value / 1000;

		case PLCM3_10_MILLIVOLTS:
			strcpy(units, "A");
			return value / 1000.0;

		case PLCM3_100_MILLIVOLTS:
			strcpy(units, "mA");
			return value;

		case PLCM3_VOLTAGE:
			strcpy(units, "mV");
			return value / 1000.0;

		default:
			return -1;
	}
}

void CollectData()
{
	int8_t	units[NUM_CHANNELS][10];
	int16_t channel;
	int32_t values[NUM_CHANNELS];
	double	scaledValues[NUM_CHANNELS];
	
	g_status = PICO_OK;

	// Display channel settings
	printf("\n");
	printf("Settings:\n\n");
	
	for (channel = 0; channel < NUM_CHANNELS; channel++)
	{
		printf("Channel %d:-\n", channel + 1);
		printf("Measurement Type: %s\n\n", MeasurementTypeToString(channelSettings[channel].measurementType));
	}


	// Set channels
	for (channel = 0; channel < NUM_CHANNELS && g_status == PICO_OK; channel++)
	{
		g_status = PLCM3SetChannel(	g_handle, 
									(PLCM3_CHANNELS) (channel + 1), 
									channelSettings[channel].measurementType);
	}

	if (g_status != PICO_OK)
	{
		printf("\n\nSetChannel: Status = 0x%X\nPress any key", g_status);
		_getch();
		return;
	}

	printf("Press any key to start.\n\n");
	_getch();

	printf("Press any key to stop...\n");

	while (!_kbhit() && (g_status == PICO_OK || g_status == PICO_NO_SAMPLES_AVAILABLE))
	{
		for (channel = 0; channel < NUM_CHANNELS && (g_status == PICO_OK || g_status == PICO_NO_SAMPLES_AVAILABLE); channel++)
		{
			g_status = PLCM3GetValue(g_handle, (PLCM3_CHANNELS) (channel + 1), &values[channel]);
				
			if (g_status == PICO_NO_SAMPLES_AVAILABLE) 
			{
				values[channel] = 0;
			}

			scaledValues[channel] = ApplyScaling(values[channel], channel, units[channel]);
		}

		for (channel = 0; channel < NUM_CHANNELS; channel++)
		{
			printf("%.2f%s\t\t", scaledValues[channel], units[channel]);

			if (channel == NUM_CHANNELS - 1)
			{
				printf("\n");
			}
			else
			{
				printf("");
			}

			Sleep(1000);
		}
	}
		
	if (g_status != PICO_OK && g_status != PICO_NO_SAMPLES_AVAILABLE)
	{
		printf("\n\nGetValue: Status = 0x%X\nPress any key", g_status);
	}

	_getch();
}

void EthernetSettings()
{
	int32_t		tmp;
	int16_t		enabled;
	int8_t		IPAddress[20]; 
	int8_t		ch;
	uint16_t	length;
	uint16_t	port;

	// Display current Ethernet settings
	g_status = PLCM3IpDetails(g_handle, &enabled, IPAddress, &length, &port, PLCM3_IDT_GET);

	if (g_status != PICO_OK)
	{
		printf("IP details: Status = 0x%X", g_status);
		return;
	}

	printf("\nEthernet Settings\n\n");
	printf("Enabled:\t%d\n", enabled);
	printf("IP Address:\t%s\n", IPAddress);
	printf("Port:\t\t%d\n", port);

	// Enter new settings
	printf("\nEdit settings? (Y/N)\n");
	ch = toupper(_getch());

	if (ch == 'Y')
	{
		printf("Enable Ethernet? (Y/N)\n");
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
		
		g_status = PLCM3IpDetails(g_handle, &enabled, IPAddress, &length, &port, PLCM3_IDT_SET);

		if (g_status != PICO_OK)
		{
			printf("IP details: Status = 0x%X", g_status);
			return;
		}
	}
}

// **************************
// Read the units EEPROM data
// **************************
void get_info(int16_t g_handle)
{
	int32_t line;
	int16_t requiredSize;
	int8_t info[80];

	int8_t description[7][25] = { "Driver Version    :", 
									"USB Version       :", 
									"Hardware Version  :", 
									"Variant Info      :", 
									"Batch and Serial  :", 
									"Calibration Date  :", 
									"Kernel Driver Ver.:"
								};

	for (line = 0; line < 7; line++)
	{
		g_status = PLCM3GetUnitInfo(g_handle, info, 80, &requiredSize, (PICO_INFO) line);
		printf("\n%s ", description[line]);
		printf("%s", info);
	}

	// MAC Address

	g_status = PLCM3GetUnitInfo(g_handle, info, 80, &requiredSize, PICO_MAC_ADDRESS);
	printf("\nMAC Address       : %s", info);

	printf("\n");
}

void main()
{
	int8_t IPAddress[20], ch;
	int8_t details[80]; // Ensure the size of the array is large enough to accommodate the information for all devices that are connected.

	int16_t channel;
	int16_t validSelection = 0;
	int16_t USB;

	uint32_t length = 80;

	PICO_STATUS status = PICO_OK;

	printf("Picolog CM3 (plcm3) Driver Example Program\n\n");

	printf("Enumerating devices...\n\n");

	// Enumerate all USB and Ethernet devices
	status = PLCM3Enumerate(details, &length, PLCM3_CT_ALL);

	if(length > 0)
	{	
		printf("PLCM3 devices found: %s\n", details);
	}
	else
	{
		printf("No PLCM3 devices found.");
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
		g_status = PLCM3OpenUnit(&g_handle, NULL);
	}
	else
	{
		// Enter IP address
		printf("Enter IP address of the PLCM3: ");
		scanf("%s", IPAddress);

		g_status = PLCM3OpenUnitViaIp(&g_handle, NULL, IPAddress);
	}
	
	if (g_status != PICO_OK)
	{
		printf("Unable to open device. Status code: 0x%X", g_status);
		_getch();
		return;
	}
	else
	{
		printf("PLCM3 Opened.\n");
	}

	// Set default channel settings
	for(channel = 0; channel < NUM_CHANNELS; channel++)
	{
		channelSettings[channel].measurementType = PLCM3_1_MILLIVOLT;
	}

	// Get unit serial number
	get_info(g_handle);
	
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

	}while (ch != 'X');

	// Close unit
	PLCM3CloseUnit(g_handle);
}
