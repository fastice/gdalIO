#include "gdal.h"
#include "ogr_srs_api.h"
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include "gdalIO/gdalIO/grimpgdal.h"
#include "mosaicSource/common/common.h"


// Get a gdal data set for a given driver type. 
static GDALDatasetH getDataSetForDriver(char *driverType, const char *filename, void *data,
                                        int32_t width, int32_t height, int dataType, int32_t predictor)
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
            /* Without this GDAL sizes a strip at ~8 KB, which for a typical
               Landsat cull row (1559 float32 = 6.2 KB) is ONE row per strip.
               Every consumer reads these whole and sequentially, so that turns
               one image into ~1500 4 KB reads; on a rotational disk with 30
               mosaics running it is pure seek and the disk saturates on IOPS
               at 13 MB/s. 256 rows cuts the reads per image ~83x (measured
               2403 -> 29 syscalls) at identical file size. */
            "BLOCKYSIZE=256",
            NULL,                /* PREDICTOR=2 slot, used for scaled integer output */
            NULL};
        if (predictor)
        {
            options[3] = "PREDICTOR=2";
        }
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
    saveAsGeotiffScaled(filename, data, width, height, geotransform, epsg_code, metaData, driverType, dataType,
                        noDataValue, 1.0, 0.0);
}

// As saveAsGeotiff, but also records a band scale and offset (value = offset + scale * stored), e.g. for
// integer-packed floats. With scale 1 and offset 0 nothing is written, so the output is as before.
void saveAsGeotiffScaled(const char *filename, void *data, int32_t width, int32_t height, double *geotransform,
                         const char *epsg_code, dictNode *metaData, char *driverType, int32_t dataType,
                         float noDataValue, double scale, double offset)
{
    GDALDatasetH dataset;
    /* Horizontal-differencing predictor only for scaled (integer-packed) output, so every existing
       caller's files are unchanged. It roughly halves Int16 dB*100 mosaics. */
    int32_t scaled = (scale != 1.0 || offset != 0.0);
    //
    // Get the data set
    if (strcmp(driverType, "COG") == 0)
    {
        dataset = getDataSetForDriver("MEM", "", data, width, height, dataType, FALSE);
    }
    else
    {
        dataset = getDataSetForDriver(driverType, filename, data, width, height, dataType, scaled);
    }
    //
    // Set geotransform
    CPLErr returnCode  = GDALSetGeoTransform(dataset, geotransform);
    ifNEReturnCode(returnCode,  CE_None, "GDAL: Failed to set geotransform for filename %s\n", filename);
    // Set projection.  OSRSetFromUserInput accepts "EPSG:nnnnn", a proj string or WKT,
    // so a projection with no EPSG code (e.g. a custom polar stereographic) can be
    // written too.  Callers have always passed a bare code such as "3413", which
    // OSRSetFromUserInput does not accept, so promote all-digit strings to "EPSG:...".
    // The previous version called OSRImportFromEPSG and, on failure, printed
    // "trying wkt" and then set no projection at all -- silently producing a file with
    // no CRS.  That is now a hard error.
    OGRSpatialReferenceH srs = OSRNewSpatialReference(NULL);
    char srsBuf[256];
    const char *srsInput = epsg_code;
    if (epsg_code != NULL && epsg_code[0] != '\0' && strspn(epsg_code, "0123456789") == strlen(epsg_code))
    {
        snprintf(srsBuf, sizeof(srsBuf), "EPSG:%s", epsg_code);
        srsInput = srsBuf;
    }
    if (srsInput == NULL || OSRSetFromUserInput(srs, srsInput) != OGRERR_NONE)
    {
        error("saveAsGeotiff: could not interpret projection \"%s\" for %s",
              (epsg_code == NULL) ? "(null)" : epsg_code, filename);
    }
    char *wkt = NULL;
    OSRExportToWkt(srs, &wkt);
    GDALSetProjection(dataset, wkt);
    CPLFree(wkt);
    OSRDestroySpatialReference(srs);
    //
    // Write data to the raster band
    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    ifNullError(band, "Failed to get raster band.\n");
    // Get set the nod data value
    GDALSetRasterNoDataValue(band, noDataValue);
    if (scale != 1.0 || offset != 0.0)
    {
        GDALSetRasterScale(band, scale);
        GDALSetRasterOffset(band, offset);
    }
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
            NULL,                /* PREDICTOR=YES slot, used for scaled integer output */
            NULL};
        if (scaled)
        {
            options[4] = "PREDICTOR=YES";
        }
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
    /* The format string used to have no arguments, so this message printed stack
       garbage -- which is how a custom polar stereographic (e.g. the Taku
       lat_ts=58/lon_0=-134 grid) reported itself. */
    error("Could not determine epsg from rot=%lf slat=%lf hemisphere=%i", rot, slat, hemisphere);
    return NULL;
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

static void setBandDescriptionAndNoData(GDALDatasetH vrtDataset, const char *filename, const char *bandName,
                                        int bandIndex, float noDataValue)
{
    /* bandName, when given, overrides the filename-derived name -- needed where the tif's
       file suffix and the band name the consumers expect differ in case or spelling
       (e.g. simInSAR's <root>.mask.tif, whose band the raw path names "Mask"). */
    const char *description = (bandName != NULL) ? bandName : getSuffixBeforeTif(filename);
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
    return makeTiffVRTNamed(vrtFile, bands, NULL, nBands, noDataValues, metaData);
}

int makeTiffVRTNamed(char *vrtFile, const char **bands, const char **bandNames, int nBands,
                     float *noDataValues, dictNode *metaData)
{
    // Open each band file to get size/type/geotransform
    GDALDatasetH *pahInputDatasets = (GDALDatasetH *)CPLMalloc(sizeof(GDALDatasetH) * nBands);
    for (int i = 0; i < nBands; i++)
    {
        pahInputDatasets[i] = GDALOpen(bands[i], GA_ReadOnly);
        ifNullError(pahInputDatasets[i], "Failed to open input file: %s\n", bands[i]);
    }
    int width = GDALGetRasterXSize(pahInputDatasets[0]);
    int height = GDALGetRasterYSize(pahInputDatasets[0]);
    GDALDataType dataType = GDALGetRasterDataType(GDALGetRasterBand(pahInputDatasets[0], 1));
    double geoTransform[6];
    GDALGetGeoTransform(pahInputDatasets[0], geoTransform);

    // Build the VRT by hand instead of via GDALBuildVRT: GDALBuildVRT
    // rejects rasters with "positive NS resolution" (GT[5] > 0), which is
    // the pixel-coord convention used for radar-geometry tiffs (e.g.
    // writeFlatTiff's .lat.tif/.lon.tif in simInSAR). The geotransform is
    // copied as-is from the first input band, so this works unchanged for
    // geographic-coord tiffs too (GT[5] < 0, e.g. mosaic3d/geomosaic's
    // saveAsGeotiff outputs) -- this function is convention-agnostic, it
    // just mirrors whatever geotransform the input tiffs already carry.
    // Each band's source is injected via the VRT driver's
    // "new_vrt_sources" metadata domain.
    GDALDriverH vrtDriver = GDALGetDriverByName("VRT");
    GDALDatasetH vrtDataset = GDALCreate(vrtDriver, vrtFile, width, height, 0, GDT_Unknown, NULL);
    ifNullError(vrtDataset, "Failed to create VRT %s\n", vrtFile);
    GDALSetGeoTransform(vrtDataset, geoTransform);
    // Add the meta data
    writeDataSetMetaData(vrtDataset, metaData);

    for (int i = 0; i < nBands; i++)
    {
        GDALAddBand(vrtDataset, dataType, NULL);
        setBandDescriptionAndNoData(vrtDataset, bands[i], (bandNames != NULL) ? bandNames[i] : NULL,
                                    i + 1, noDataValues[i]);
        GDALRasterBandH band = GDALGetRasterBand(vrtDataset, i + 1);
        char pathBuf[2048], sourceXml[2560];
        strncpy(pathBuf, bands[i], sizeof(pathBuf) - 1);
        pathBuf[sizeof(pathBuf) - 1] = '\0';
        snprintf(sourceXml, sizeof(sourceXml),
                 "<SimpleSource>"
                 "<SourceFilename relativeToVRT=\"1\">%s</SourceFilename>"
                 "<SourceBand>1</SourceBand>"
                 "<SrcRect xOff=\"0\" yOff=\"0\" xSize=\"%d\" ySize=\"%d\"/>"
                 "<DstRect xOff=\"0\" yOff=\"0\" xSize=\"%d\" ySize=\"%d\"/>"
                 "</SimpleSource>",
                 basename(pathBuf), width, height, width, height);
        GDALSetMetadataItem(band, "source_0", sourceXml, "new_vrt_sources");
    }
    // Save the VRT dataset to disk
    GDALClose(vrtDataset);
    // Cleanup input datasets
    for (int i = 0; i < nBands; i++)
    {
        GDALClose(pahInputDatasets[i]);
    }
    CPLFree(pahInputDatasets);
}
