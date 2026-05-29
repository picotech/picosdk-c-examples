# picosdk-c-examples

*picosdk-c-examples* is a set of C/C++ examples for PicoScope<sup>®</sup> oscilloscopes and PicoLog<sup>®</sup> data loggers.

Examples are provided as Microsoft Visual Studio Express 2015 for Windows Desktop solutions and are grouped by driver name. Each driver folder has a `linux-build-files` sub-folder for building applications on Linux and macOS platforms from a terminal window.

## Getting started

### Prerequisites

#### Microsoft Windows

* Microsoft Visual Studio 2015 (including Express and Community editions) or later. 

#### Linux

* A suitable IDE such as [Eclipse](https://www.eclipse.org/downloads/?)
* Alternatively, ensure that the `autoconf` and `libtool` packages are installed for your operating system if building applications from the terminal. (for example "sudo apt-get install `build-essential autoconf automake make libtool`")

#### macOS

* A suitable IDE such as [XCode](https://developer.apple.com/xcode/)

### Installing drivers

Drivers are available for the following platforms. Refer to the subsections below for further information.

#### Microsoft Windows

* Download the PicoSDK (32-bit or 64-bit) driver package installer from our [Downloads page](https://www.picotech.com/downloads).

#### Linux

* Follow the instructions from our [Linux Software & Drivers for Oscilloscopes and Data Loggers page](https://www.picotech.com/downloads/linux) to install the required driver packages for your product.

#### macOS

* Download the PicoSDK driver package installer (for your MacOS system x86-64 or ARM64) from our [Downloads page](https://www.picotech.com/downloads).

### Programmer's Guides

You can download Programmer's Guides providing a description of the API functions for the relevant PicoScope or PicoLog driver from our [Documentation page](https://www.picotech.com/library/documentation).

### Building example applications

#### Microsoft Windows

* Open the solution .sln file for the required driver in Visual Studio
* Select the required configuration and solution platform
* Build the solution

#### Linux and macOS

<ins>For older console examples located in the format /psXXXXCon/psXXXXCon.c</ins>

(For example /ps3000aCon/ps3000aCon.c)
* Copy the required source code C file (e.g. ps3000aCon.c) into the corresponding `linux-build-files` sub-folder for the driver or copy the files to another folder.  
* Copy the PicoSDK header file `PicoStatus.h` into the same folder from-  
`/opt/picoscope/include/libps3000a/`.  
Note other Pico header files maybe required (`PicoDeviceEnums`, `PicoDeviceStructs.h`, `PicoConnectProbes.h`)

* Open a terminal window from the example folder, for example `/ps3000a`:
* Run the following commands:
  * `./autogen.sh`
  * `make`
* Run the executable, for example; `./ps3000aCon`

<ins>For console examples that have a CMakeLists.txt</ins>

(For example /psospa/CMakeLists.txt)
* Open a terminal window from the example folder, for example `/psospa`:  
* Run the following commands:
  * `mkdir build`
  * `cd build`
  * `cmake ..`
  * `make`  
  
* Run one of the executables, for example; `./psospaBlock`, `./psospaRapidBlock`, `./psospaStreaming`
* See the `psXXXX README.md` in the example folder for more information and setting up the template example in `./XXXXXTemplate/`.  
(psospa template example- `./psospa/psospaTemplate/`)

## Obtaining support

Please visit our [Support page](https://www.picotech.com/tech-support) to contact us directly or visit our [Test and Measurement Forum](https://www.picotech.com/support/forum19.html) to post questions.

## Contributing

Contributions are welcome. Please refer to our [guidelines for contributing](.github/CONTRIBUTING.md) for further information.

## Copyright and licensing

See [LICENSE.md](LICENSE.md) for license terms. 

*PicoScope* and *PicoLog* are registered trademarks of Pico Technology Ltd. 

*Windows* and *Visual Studio* are registered trademarks of Microsoft Corporation. 

*macOS* is a registered trademark of Apple Inc. 

*Linux* is the registered trademark of Linus Torvalds in the U.S. and other countries.

Copyright © 2004-2026 Pico Technology Ltd. All rights reserved.

