/****************************************************************************
 *
 * Filename:    UserSetup.h
 * Copyright:   Pico Technology Limited 2026
 * Description:
 *
 * This header defines shared functions and structures for
  * UserSetup function in psospa example code.
 *
 ****************************************************************************/

#ifndef __USERSETUP_H__
#define __USERSETUP_H__

#include "../../shared/PicoUnit.h"

 /* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>

#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <psospaApi.h>
#ifndef PICO_STATUS
#include <PicoStatus.h>
#endif

#endif

// Function prototypes

void userSetup(GENERICUNIT* unit);

#endif
