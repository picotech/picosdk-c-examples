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

* Open a terminal window from the linux example folder. For example `/ps3000a/linux-build-files/`:
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

## Working with Examples

Note the below features only apply for newer examples like; `psospaBlock`, `psospaRapidBlock`, `psospaStreaming`. But could be added to other example code.

### Data text files with WriteArrayToFilesGeneric and WriteMetaDataToFile

Data written by `WriteArrayToFilesGeneric()` and `WriteMetaDataToFile()` (defined in `shared/PicoFileFunctions.c`) 
are written to csv file for format, these can be changed in the C file, by changing the #define values-  
`#define TEXT_FILE_EXTENSION ".csv"`  
`#define SEPARATOR ","` 

### Plotting selected channels with WriteArrayToImage

`WriteArrayToImage()` (defined in `shared/PicoFileFunctions.c`) writes captured scope data to a PNG image file using the [pbPlots](https://github.com/InductiveComputerScience/pbPlots) library. The `plotChannelMask` parameter controls which channels are included in the plot.

Each bit in the mask corresponds to a channel, where bit 0 = Channel A, bit 1 = Channel B, and so on. Pass `0` to plot all enabled channels (default behaviour).


**Examples:**

```c
// Plot all enabled channels
WriteArrayToImage(unit, minBuffers, maxBuffers, multiBufferSizes,
    enabledChannelsScaling, filename, triggerSample, &overflow,
    0,      // plotChannelMask: 0 = all enabled channels
    NULL);

// Plot Channel B only
WriteArrayToImage(unit, minBuffers, maxBuffers, multiBufferSizes,
    enabledChannelsScaling, filename, triggerSample, &overflow,
    1u << PS4000A_CHANNEL_B,
    NULL);

// Plot Channels A and C only
WriteArrayToImage(unit, minBuffers, maxBuffers, multiBufferSizes,
    enabledChannelsScaling, filename, triggerSample, &overflow,
    (1u << PS4000A_CHANNEL_A) | (1u << PS4000A_CHANNEL_C),
    NULL);
```

Requesting a channel that is not enabled is silently ignored. The x-axis is scaled to the most readable SI time unit (ps, ns, us, ms, or s) based on the total capture duration, with the trigger sample positioned at time zero. The y-axis is similarly scaled using SI prefixes (p, n, u, m) based on the peak signal amplitude.

**Decimation:** when the capture contains more samples than the image is wide, the function automatically decimates the data before plotting — taking every Nth sample so that the number of plotted points does not exceed `PLOT_MAX_POINTS` (default 1920, matching the output image width). This keeps render time proportional to the image size rather than the capture depth. The x-axis labels always reflect the full capture time span regardless of decimation.

**Changing the image size and decimation limit:** the output image dimensions are set in `PlotMultiDataToImage()` in `shared/PicoPlotting.c`:

```c
settings->width  = 1920;
settings->height = 1080;
```

If you change the image width here, update `PLOT_MAX_POINTS` to match. It is defined as a `const size_t` at the top of `WriteArrayToImage()` in `shared/PicoFileFunctions.c`:

```c
const size_t PLOT_MAX_POINTS = 1920;
```

## Obtaining support

Please visit our [Support page](https://www.picotech.com/tech-support) to contact us directly or visit our [Test and Measurement Forum](https://www.picotech.com/support/forum19.html) to post questions.

## Contributing

Contributions are welcome. Please refer to our [guidelines for contributing](.github/CONTRIBUTING.md) for further information.

## Third-party libraries

### pbPlots

PNG image plotting is provided by [pbPlots](https://github.com/InductiveComputerScience/pbPlots), a portable plotting library developed using [progsbase](https://repo.progsbase.com).

pbPlots is distributed under the MIT License. Copyright © InductiveComputerScience. The source files `shared/pbPlots.c`, `shared/pbPlots.h`, `shared/supportLib.c`, and `shared/supportLib.h` are reproduced here under the terms of that licence.

## Copyright and licensing

See [LICENSE.md](LICENSE.md) for license terms. 

*PicoScope* and *PicoLog* are registered trademarks of Pico Technology Ltd. 

*Windows* and *Visual Studio* are registered trademarks of Microsoft Corporation. 

*macOS* is a registered trademark of Apple Inc. 

*Linux* is the registered trademark of Linus Torvalds in the U.S. and other countries.

Copyright © 2004-2026 Pico Technology Ltd. All rights reserved.

