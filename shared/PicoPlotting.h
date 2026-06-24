#ifndef PICO_PLOTTING_H
#define PICO_PLOTTING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Helper function to plot Y data against X data and save to a PNG file.
// Returns 1 on success, 0 on failure.
int PlotDataToImage(double* xData, double* yData, size_t dataLength, const char* outputFilename);

// Helper function to plot Y data only (X axis will be array indices) and save to a PNG file.
int PlotYDataToImage(double* yData, size_t dataLength, const char* outputFilename);

// Helper function to plot multiple Y data series against shared X data and save to a PNG file.
// channelIndices maps each series to its physical channel number (0=A, 1=B, ...) for correct
// PicoScope colours (A=Blue, B=Red, C=Green, D=Yellow). Pass NULL to use sequential indexing.
// xLabel is the x-axis label e.g. "Time (ns)". Pass NULL to default to "Time (s)".
// yLabel is the SI prefix for the y-axis e.g. "m", "u", "n", "p", or "" for no prefix.
// Pass NULL to show no y-axis label.
int PlotMultiDataToImage(double* xData, double** yDataArray, const int* channelIndices, int numSeries, size_t dataLength, const char* xLabel, const char* yLabel, const char* outputFilename);

// Helper function to plot multiple Y data series (X axis will be array indices) and save to a PNG file.
int PlotMultiYDataToImage(double** yDataArray, int numSeries, size_t dataLength, const char* outputFilename);

#ifdef __cplusplus
}
#endif

#endif // PICO_PLOTTING_H
