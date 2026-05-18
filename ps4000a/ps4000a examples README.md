# picosdk-c-examples ps4000a API examples

Project files have been created to demo and test acquisition modes and the signal generate.  
Projects are;  
**ps4000aAWG**  
**ps4000aBlock**  
**ps4000aRapidBlock**  
**ps4000aStreaming**  
These have a basic console menu to change related settings.

And with the addition **ps4000aTemplate** project for quick user setup for applications.

## Using the Template project

All that is required is to edit the code in the file-  
UserSetup.c, in the userSetup() function.

The UserSetup.c is currently setup as a fast streaming example.

### To add/change configuration

Add/remove C files in shared folder to include different functionality.

Change the #include lines at the top of UserSetup.c and add the related .c file(s) to the visual studio project or build tool.

### Channel Setup  
- Override default channel settings
  - Turn Off all channels first
  - Enable required Channel(s), and define the following for each;
  - unit->channelSettings[0] is ChA, to unit->channelSettings[7] is ChH
- See related enum for available options
  - unit->channelSettings[0].enabled
  - unit->channelSettings[0].DCcoupled
  - unit->channelSettings[0].range
  - unit->channelSettings[0].analogueOffset - Voltage (double datatype). The allowable analog offset for a given input voltage range can be read using psospaGetAnalogueOffsetLimits().
  - unit->channelSettings[0].bandwithLimit

### Trigger Setup
- Set a trigger if required.
  - Call `ps4000aSetupTrigger(GENERICUNIT* unit);`       // unit structure
  - The default is Channel A, rising edge, at +50% of the channel range, using advanced trigger functions.
  - You will need to edit the function to change this or use the `ps4000aSetSimpleTrigger()` function.

### Acquisition modes

**Block mode**

Call the function with following parameters;  

`void blockDataHandler(GENERICUNIT* unit,`      // unit structure  
						`uint64_t noOfPreTriggerSamples,`		// on Device, (Used by RunBlock())  
						`uint64_t noOfPostTriggerSamples,`	// on Device, (Used by RunBlock())  
						`double idealTimeInterval,`			    // in seconds use 0 to find the max. sample rate, (Used by RunBlock())  
						`uint64_t nSamples,`					      // PC buffer size (Used by SetDataBuffers())  
						`PICO_RATIO_MODE ratioMode,`			  // See enum for downsampling modes (Used by SetDataBuffers())  
						`uint64_t downSampleRatio`			    // (Used by SetDataBuffers())  
						`FILE_BIN)`					// Save data as Binary / txt OR off (see enum FILE_TYPE)	

Data is then to scaled to voltage and some written the to console and all data is written to a text file.  
Using the functions `WriteArrayToStdoutGeneric()` to write to the console and  
`WriteArrayToFilesGeneric()` to write to text file.

#### Overlapped version
For repeated block captures, that use deferred requests for data (to save calls to the unit)  
A "Overlapped" version of the function exists with same parameters;  
`blockOverlappedDataHandler()`
The number of loop irrationals can be changed inside the function.

**Rapid Block mode**

Call the function with following parameters;  

`void rapidblockDataHandler(GENERICUNIT* unit,`   // unit structure  
							`uint64_t noOfPreTriggerSamples,`		// on Device, (Used by RunBlock())  
							`uint64_t noOfPostTriggerSamples,`	// on Device, (Used by RunBlock())  
							`double idealTimeInterval,`			    // in seconds use 0 to find the max. sample rate, (Used by RunBlock())  
							`uint64_t nSamples,`					      // PC buffer size (Used by SetDataBuffers())  
							`uint64_t nCaptures,`               // Number of Captures on device and to download  
							`PICO_RATIO_MODE ratioMode,`			  // see enum (Used by SetDataBuffers())  
							`uint64_t downSampleRatio`          // (Used by SetDataBuffers())  
							`FILE_BIN)`					// Save data as Binary / txt OR off (see enum FILE_TYPE)	

Data is then to scaled to voltage and some written the to console and all data is written to a text file.  
Using the functions `WriteArrayToStdoutGeneric()` to write to the console and  
`WriteArrayToFilesGeneric()` to write to text file.

#### Overlapped version
For repeated rapid block captures, that use deferred requests for data (to save calls to the unit)  
A "Overlapped" version of the function exists with same parameters;  
`blockOverlappedDataHandler()`
The number of loop irrationals can be changed inside the function.

**Streaming**

Call the function with following parameters;  

`void streamDataHandler(GENERICUNIT* unit,`     // unit structure  
						`uint64_t noOfPreTriggerSamples,`	  // on Device, (Used by RunStreaming())  
						`uint64_t noOfPostTriggerSamples,`	// on Device, (Used by RunStreaming())  
						`double idealTimeInterval,`			    // in sampleIntervalTimeUnits, (Used by RunStreaming())  
						`uint32_t sampleIntervalTimeUnits,`	// time units enum, (Used by RunStreaming())  
						`uint64_t nSamples,`					      // Set the number of samples per capture - Used by SetDataBuffers()  
						`PICO_RATIO_MODE ratioMode,`			  // // See enum for downsampling modes (Used by SetDataBuffers())  
						`uint64_t downSampleRatio,`			    // (Used by SetDataBuffers())  
						`int16_t autostop`                 // autostop - 0: Off OR 1: Stop after trigger event  
						`FILE_BIN)`					// Save data as Binary / txt OR off (see enum FILE_TYPE)	 

Data is then to scaled to voltage and some written the to console.  
Data is only written to a text file when sample rate is greater than 0.9us ( less than 1.1MS/s). This is to not slow down the streaming code processing data at high data rates, which will result in data lost.
This uses the functions `WriteArrayToStdoutGeneric()` to write to the console and  
`WriteArrayToFilesGeneric()` to write to text file.

Note: It is recommended that the buffer sizes be set to a min. of 1MiB (2^20) especially for fast sample rates.  
This to avoid buffer overflow, and lost data.  
The size is set by the value of `nSamples`.

#### Request more data

With all data modes you can request more data after data capture has stopped using the function;

`void GetMoreDataHandler(GENERICUNIT* unit,`  
						`PICO_RATIO_MODE ratioMode,`    // See enum for downsampling modes  
						`uint64_t downSampleRatio,`     // Ratio, is inorged if RAW mode  
						`uint64_t nSamples`            // Set the number of raw samples  
						`FILE_BIN)`					// Save data as Binary / txt OR off (see enum FILE_TYPE)	

### Signal Generator and AWG

See `\picosdk-c-examples\psospaNewTeleplate\shared\LibAWGps4000a.c` for example function calls.  
All example functions pass a pointer to a `SIG_GEN_SETTINGS` structure and call, `SigGenAWG()`, to write down changes to the unit.

The function `AWGLoadFile()` is hard coded to load the file; PicoScope7AWG_Demo.csv but can be changed to use any file saved from the PicoScope 7 AWG editor.