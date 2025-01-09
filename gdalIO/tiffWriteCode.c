#include "gdal.h"
#include "ogr_srs_api.h"
#include <sys/types.h>
#include "gdalIO/gdalIO/grimpgdal.h"
#include "mosaicSource/common/common.h"


// Get a gdal data set for a given driver type. 
static GDALDatasetH getDataSetForDriver(char *driverType, const char *filename, void *data,
                                        int32_t width, int32_t height, int dataType)
{
    // Get the requested triver
    GDALDriverH driver = GDALGetDriverByName(driverType);
    if (driver == NULL)
    {
        error("%s driver not available.\n", driverType);
    }
    // Set up data set for MEM, which will be used for COGs
    if (strcmp(driverType, "MEM") == 0)
    {
        GDALDatasetH dataset = GDALCreate(driver, filename, width, height, 1, dataType, NULL);
        ifNullError(dataset, "GDAL: Failed to create dataset for %s with driver %s\n", filename, driverType);
        return dataset;
    }
    if (strcmp(driverType, "GTiff") == 0)
    {
        const char *options[] = {
            "COMPRESS=DEFLATE",  // Compression for efficient storage
            "BIGTIFF=IF_NEEDED", // Ensure compatibility with large datasets
            NULL};
        GDALDatasetH dataset = GDALCreate(driver, filename, width, height, 1, dataType, (char **)options);
        ifNullError(dataset, "GDAL: Failed to create dataset for %s with driver %s\n", filename, driverType);
        return dataset;
    }
}

static void flip_data_vertically(void *data, int width, int height, GDALDataType dataType) {
    size_t rowSize = GDALGetDataTypeSizeBytes(dataType) * width; // Size of a single row in bytes
    void *tempRow = malloc(rowSize);  // Temporary buffer for swapping rows

    if (tempRow == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        return;
    }

    unsigned char *dataPtr = (unsigned char *)data;
    for (int i = 0; i < height / 2; i++) {
        unsigned char *topRow = dataPtr + i * rowSize;
        unsigned char *bottomRow = dataPtr + (height - i - 1) * rowSize;
        // Swap rows
        memcpy(tempRow, topRow, rowSize);
        memcpy(topRow, bottomRow, rowSize);
        memcpy(bottomRow, tempRow, rowSize);
    }

    free(tempRow);
}
// Write data to a geo tiff. Adapted from an original created with ChatGPT
void saveAsGeotiff(const char *filename, void *data, int32_t width, int32_t height, double *geotransform,
                   const char *epsg_code, dictNode *metaData, char *driverType, int32_t dataType, float noDataValue)
{
    GDALDatasetH dataset;
    //
    // Get the data set
    if (strcmp(driverType, "COG") == 0)
    {
        dataset = getDataSetForDriver("MEM", "", data, width, height, dataType);
    }
    else
    {
        dataset = getDataSetForDriver(driverType, filename, data, width, height, dataType);
    }
    //
    // Set geotransform
    CPLErr returnCode  = GDALSetGeoTransform(dataset, geotransform);
    ifNEReturnCode(returnCode,  CE_None, "GDAL: Failed to set geotransform for filename %s\n", filename);
    // Set projection using EPSG code
    OGRSpatialReferenceH srs = OSRNewSpatialReference(NULL);
    if (OSRImportFromEPSG(srs, atoi(epsg_code)) != OGRERR_NONE)
    {
        fprintf(stderr, "Failed to import EPSG code, trying wkt.\n");
    }
    else
    {
        char *wkt = NULL;
        OSRExportToWkt(srs, &wkt);
        GDALSetProjection(dataset, wkt);
        CPLFree(wkt);
    }
    OSRDestroySpatialReference(srs);
    //
    // Write data to the raster band
    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    ifNullError(band, "Failed to get raster band.\n");
    // Get set the nod data value
    GDALSetRasterNoDataValue(band, noDataValue);
    // flip vertically for tiff output
    flip_data_vertically(data, width, height, dataType);
    // Write the raster bands
    returnCode = GDALRasterIO(band, GF_Write, 0, 0, width, height, data, width, height, dataType, 0, 0);
    // May not be needed in many cases, but flip back to original.
    flip_data_vertically(data, width, height, dataType);
    ifNEReturnCode(returnCode,  CE_None, "Failed to write raster data.\n");
    //
    // Add metadata
    if (metaData != NULL)
    {
        //  Add the meta data
        writeDataSetMetaData(dataset, metaData);
    }
    //
    // Extra stuff to create COGs by copying from memory
    if (strcmp(driverType, "COG") == 0)
    {
        const char *options[] = {
            "COMPRESS=DEFLATE",
            "BIGTIFF=IF_NEEDED",
            "BLOCKSIZE=512",
            "OVERVIEWS=AUTO",
            NULL};
        GDALDriverH cogDriver = GDALGetDriverByName(driverType);
        GDALDatasetH cogDataset = GDALCreateCopy(cogDriver, filename, dataset, FALSE, (char **)options, NULL, NULL);
        ifNullError(cogDataset, "Error: Failed to create COG dataset.\n");
        // Clean up
        GDALClose(cogDataset);
    }
    // Clean up
    GDALClose(dataset);
}

void computeGeoTransform(double geoTransform[6], double x0, double y0, 
                        int32_t xSize, int32_t ySize, double deltaX, double deltaY)
{
    geoTransform[0] = x0 - deltaX * 0.5;
    geoTransform[1] = deltaX;
    geoTransform[2] = 0.0;
    geoTransform[3] = y0 + (ySize - 1) * deltaY + deltaY * 0.5;
    geoTransform[4] = 0.0;
    geoTransform[5] = -deltaY;
}

char *timeStampMeta()
{
    char timestamp[50];
    time_t rawTime;
    struct tm *timeInfo;

    time(&rawTime);
    timeInfo = localtime(&rawTime);

    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeInfo);
    // Dynamically allocate memory for the copy
    char *timeStampCopy = malloc(strlen(timestamp) + 1);
    // Copy the content to the allocated memory
    strcpy(timeStampCopy, timestamp);
    return timeStampCopy;
}

const char *getEPSGFromProjectionParams(double rot, double slat, int32_t hemisphere)
{
    if (rot == 45 && slat == 70 && hemisphere == NORTH)
    {
        return "3413";
    }
    if (rot == 0. && slat == 71. && hemisphere == SOUTH)
    {
        return "3031";
    }
    error("Could not determine epgs from rot=%lf slat=%lf hemisphere=%i");
}

static const char *getFileSuffix(const char *filename)
{
    // Find the last dot in the filename
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
    {
        // No dot found or dot is the first character (no valid suffix)
        return NULL;
    }
    // Return the part of the string after the dot
    return dot + 1;
}

static const char *getSuffixBeforeTif(const char *filename)
{
    // Find the last occurrence of ".tif"
    const char *tif = strstr(filename, ".tif");
    if (!tif)
    {
        // If ".tif" is not found, return NULL
        return NULL;
    }
    // Find the last dot before ".tif"
    const char *lastDot = tif;
    while (lastDot > filename && *(lastDot - 1) != '.')
    {
        lastDot--;
    }
    // If no preceding dot found, return NULL
    if (lastDot == filename)
    {
        return NULL;
    }
    // Allocate a string to hold the suffix
    size_t suffixLength = tif - lastDot;
    static char suffix[256]; // Ensure enough space
    if (suffixLength >= sizeof(suffix))
    {
        return NULL; // Prevent overflow
    }
    strncpy(suffix, lastDot, suffixLength);
    suffix[suffixLength] = '\0'; // Null-terminate the string
    return suffix;
}

static void setBandDescriptionAndNoData(GDALDatasetH vrtDataset, const char *filename, int bandIndex, float noDataValue)
{
    const char *description = getSuffixBeforeTif(filename);
    GDALRasterBandH band = GDALGetRasterBand(vrtDataset, bandIndex);
    ifNullError(band,"Failed to get band %d\n", bandIndex);
    // Set the "Description" metadata for the band
    CPLErr returnCode  = GDALSetMetadataItem(band, "Description", description, NULL);
    ifNEReturnCode(returnCode,  CE_None, "Failed to set Description for band %d\n", bandIndex);
    // Set the no data value for the band
    GDALSetRasterNoDataValue(band, noDataValue);
}

int makeTiffVRT(char *vrtFile, const char **bands, int nBands, float *noDataValues, dictNode *metaData)
{
    // Create GDALBuildVRT options
    char *papszOptions[] = {
        "-separate",
        NULL // Array must be NULL-terminated
    };
    GDALBuildVRTOptions *psOptions = GDALBuildVRTOptionsNew(papszOptions, NULL);
    ifNullError(psOptions, "Failed to create GDALBuildVRTOptions\n");
    // Open each band file
    GDALDatasetH *pahInputDatasets = (GDALDatasetH *)CPLMalloc(sizeof(GDALDatasetH) * nBands);
    
    for (int i = 0; i < nBands; i++)
    {
        pahInputDatasets[i] = GDALOpen(bands[i], GA_ReadOnly);
        ifNullError(pahInputDatasets[i], "Failed to open input file: %s\n", bands[i]);
    }
    // Build VRT
    GDALDatasetH vrtDataset = GDALBuildVRT(vrtFile, nBands, pahInputDatasets, bands, psOptions, NULL);
    
    // Add the suffix of the file name the description
    for (int i = 1; i <= nBands; i++)
    {
        setBandDescriptionAndNoData(vrtDataset, bands[i - 1], i, noDataValues[i - 1]);
    }
    // Add the meta data
    writeDataSetMetaData(vrtDataset, metaData);
    // Check that it worked
    if (vrtDataset == NULL)
    {
        error("GDALBuildVRT failed\n");
    }
    // Save the VRT dataset to disk
    GDALClose(vrtDataset);
    // Cleanup input datasets
    for (int i = 0; i < nBands; i++)
    {
        GDALClose(pahInputDatasets[i]);
    }
    CPLFree(pahInputDatasets);
    // Free options
    GDALBuildVRTOptionsFree(psOptions);
    // Cleanup
    GDALDestroyDriverManager();
}
