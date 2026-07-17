#include "PicoPlotting.h"
#include "pbPlots.h"
#include "supportLib.h"
#include <stdio.h>
#include <stdlib.h>

int PlotDataToImage(double* xData, double* yData, size_t dataLength, const char* outputFilename) {
    if (dataLength == 0 || !xData || !yData || !outputFilename) {
        return 0;
    }

    ScatterPlotSeries *series = GetDefaultScatterPlotSeriesSettings();
    series->xs = xData;
    series->xsLength = dataLength;
    series->ys = yData;
    series->ysLength = dataLength;
    series->linearInterpolation = true;
    series->pointType = L"dots";
    series->pointTypeLength = wcslen(series->pointType);
    series->color = CreateRGBColor(0, 0, 1); // Blue

    ScatterPlotSettings *settings = GetDefaultScatterPlotSettings();
    settings->width = 800;
    settings->height = 600;
    settings->autoBoundaries = true;
    settings->autoPadding = true;
    settings->xAxisAuto = false;
    settings->xAxisBottom = true;
    settings->xAxisTop = false;
    settings->yAxisAuto = false;
    settings->yAxisLeft = true;
    settings->yAxisRight = false;
    
    // Convert generic title
    settings->title = L"PicoScope Data";
    settings->titleLength = wcslen(settings->title);
    
    settings->xLabel = L"X Axis";
    settings->xLabelLength = wcslen(settings->xLabel);
    
    settings->yLabel = L"Volts or Probe Units";
    settings->yLabelLength = wcslen(settings->yLabel);

    ScatterPlotSeries *s [] = {series};
    settings->scatterPlotSeries = s;
    settings->scatterPlotSeriesLength = 1;

    RGBABitmapImageReference *canvasReference = CreateRGBABitmapImageReference();
    StringReference *errorMessage = CreateStringReference(L"", 0);
    
    int success = DrawScatterPlotFromSettings(canvasReference, settings, errorMessage);

    if (success) {
        ByteArray *pngData = ConvertToPNG(canvasReference->image);
        WriteToFile(pngData, outputFilename);
        FreeByteArray(pngData);
        DeleteImage(canvasReference->image);
    } else {
        wprintf(L"Error drawing scatter plot: %ls\n", errorMessage->string);
    }

    FreeAllocations(); // supportLib utility to free its internal allocations
    return success;
}

int PlotYDataToImage(double* yData, size_t dataLength, const char* outputFilename) {
    if (dataLength == 0 || !yData || !outputFilename) {
        return 0;
    }

    double *xData = (double *)malloc(dataLength * sizeof(double));
    if (!xData) return 0;

    for (size_t i = 0; i < dataLength; ++i) {
        xData[i] = (double)i;
    }

    int result = PlotDataToImage(xData, yData, dataLength, outputFilename);
    free(xData);
    return result;
}

int PlotMultiDataToImage(double* xData, double** yDataArray, const int* channelIndices, int numSeries, size_t dataLength, const char* xLabel, const char* yLabel, const char* outputFilename) {
    if (dataLength == 0 || numSeries <= 0 || !xData || !yDataArray || !outputFilename) {
        return 0;
    }

    ScatterPlotSeries **seriesArray = (ScatterPlotSeries **)malloc(numSeries * sizeof(ScatterPlotSeries *));
    if (!seriesArray) return 0;

	// PicoScope channel colours: ChA=Blue, ChB=Red, ChC=Green, ChD=Yellow, etc. (up to 8 channels)
    RGBA* colors[] = {
        CreateRGBColor(0, 0, 1),            // Blue    - Channel A
        CreateRGBColor(1, 0, 0),            // Red     - Channel B
        CreateRGBColor(0, 1, 0),            // Green   - Channel C
        CreateRGBColor(1, 1, 0),            // Yellow  - Channel D
		CreateRGBColor(0.5, 0, 0.5),        // Purple  - Channel E
		CreateRGBColor(0.5, 0.5, 0.5),      // Gray    - Channel F
		CreateRGBColor(0, 1, 1),            // Cyan    - Channel G
		CreateRGBColor(1, 0, 1)             // Magenta - Channel H
    };
    int numColors = sizeof(colors) / sizeof(colors[0]);

    for (int i = 0; i < numSeries; i++) {
        int colorIndex = channelIndices ? channelIndices[i] : i;
        seriesArray[i] = GetDefaultScatterPlotSeriesSettings();
        seriesArray[i]->xs = xData;
        seriesArray[i]->xsLength = dataLength;
        seriesArray[i]->ys = yDataArray[i];
        seriesArray[i]->ysLength = dataLength;
        seriesArray[i]->linearInterpolation = true;
        seriesArray[i]->pointType = L"dots";
        seriesArray[i]->pointTypeLength = wcslen(seriesArray[i]->pointType);
        seriesArray[i]->color = colors[colorIndex % numColors];
    }

    ScatterPlotSettings *settings = GetDefaultScatterPlotSettings();
    settings->width = 1920; // 800;
    settings->height = 1080; // 600;
    settings->autoBoundaries = true;
    settings->autoPadding = true;
    settings->xAxisAuto = false;
    settings->xAxisBottom = true;
    settings->xAxisTop = false;
    settings->yAxisAuto = false;
    settings->yAxisLeft = true;
    settings->yAxisRight = false;
    
    settings->title = L"PicoScope Data";
    settings->titleLength = wcslen(settings->title);

    wchar_t wXLabel[32] = L"Time (s)";
    if (xLabel != NULL) {
        mbstowcs(wXLabel, xLabel, (sizeof(wXLabel) / sizeof(wXLabel[0])) - 1);
        wXLabel[(sizeof(wXLabel) / sizeof(wXLabel[0])) - 1] = L'\0';
    }
    settings->xLabel = wXLabel;
    settings->xLabelLength = wcslen(wXLabel);

    wchar_t wYLabel[40] = L"";
    wchar_t wYPrefix[4] = L"";
    if (yLabel != NULL && yLabel[0] != '\0') {
        mbstowcs(wYPrefix, yLabel, (sizeof(wYPrefix) / sizeof(wYPrefix[0])) - 1);
        wYPrefix[(sizeof(wYPrefix) / sizeof(wYPrefix[0])) - 1] = L'\0';
    }
    wcscpy(wYLabel, wYPrefix);
    wcscat(wYLabel, L"(Volts or Probe units)");
    settings->yLabel = wYLabel;
    settings->yLabelLength = wcslen(wYLabel);

    settings->scatterPlotSeries = seriesArray;
    settings->scatterPlotSeriesLength = numSeries;

    RGBABitmapImageReference *canvasReference = CreateRGBABitmapImageReference();
    StringReference *errorMessage = CreateStringReference(L"", 0);
    
    int success = DrawScatterPlotFromSettings(canvasReference, settings, errorMessage);

    if (success) {
        ByteArray *pngData = ConvertToPNG(canvasReference->image);
        WriteToFile(pngData, (char*)outputFilename);
        FreeByteArray(pngData);
        DeleteImage(canvasReference->image);
    } else {
        wprintf(L"Error drawing scatter plot: %ls\n", errorMessage->string);
    }

    FreeAllocations(); // supportLib utility to free its internal allocations
    free(seriesArray);
    return success;
}

int PlotMultiYDataToImage(double** yDataArray, int numSeries, size_t dataLength, const char* outputFilename) {
    if (dataLength == 0 || numSeries <= 0 || !yDataArray || !outputFilename) {
        return 0;
    }

    double *xData = (double *)malloc(dataLength * sizeof(double));
    if (!xData) return 0;

    for (size_t i = 0; i < dataLength; ++i) {
        xData[i] = (double)i;
    }

    int result = PlotMultiDataToImage(xData, yDataArray, NULL, numSeries, dataLength, NULL, NULL, outputFilename);
    free(xData);
    return result;
}