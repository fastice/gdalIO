#include "gdalIO/gdalIO/grimpgdal.h"
#include "mosaicSource/common/common.h"
#include <libgen.h>

#define STR_BUFFER_SIZE 1024
#define STR_BUFF(fmt, ...) ({                                  \
  char *__buf = (char *)calloc(STR_BUFFER_SIZE, sizeof(char)); \
  snprintf(__buf, STR_BUFFER_SIZE, fmt, ##__VA_ARGS__);        \
  __buf;                                                       \
})


void ifNEReturnCode(CPLErr returnCode, int code, char *msg, ...)
{
	if(returnCode == code) return;
	va_list args;
	va_start(args, msg);
	error(msg, args);
	va_end(args);
	exit(1);
}


void *allocData(int data_type, int width, int height)
{
  void *data = NULL;
  void **image;
  int size, i;
  size = GDALGetDataTypeSize(data_type) / 8;
  fprintf(stderr, "Mallocing %i, %i, %i %i\n", width, height, (int)size, width * height * size);
  return CPLMalloc(width * height * size);
}

char *parseNameValue(char *metaBuf, char **value)
{
  char *key;
  key = strtok(metaBuf, "=");
  *value = strtok(NULL, "=");
  return key;
}

int writeDataSetMetaData(GDALDatasetH dataSet, dictNode *metaData)
{
  dictNode *current;
  //
  for (current = metaData; current != NULL; current = current->next)
  {
    GDALSetMetadataItem(dataSet, current->key, current->value, NULL);
  }
}

int readDataSetMetaData(GDALDatasetH dataSet, dictNode **metaDictionary)
{
  int i;
  char *key, *value;
  // GDALRasterBandH hBand = GDALGetRasterBand(dataSet, band);
  char **metadata = GDALGetMetadata(dataSet, NULL);
  // If not meta data return.
  if(metadata == NULL) return 0;
  // Unpack meta data
  for (i = 0; metadata[i] != NULL; i++)
  {
    fprintf(stderr, "%s\n", metadata[i]);
    key = parseNameValue(metadata[i], &value);
    // fprintf(stderr, "key %s value %s\n", key, value);
    insert_node(metaDictionary, key, value);
  }
}

char *extract_filename(char *path)
{
  char *filename = strrchr(path, '/');
  if (filename == NULL)
  {
    filename = strrchr(path, '\\');
  }
  if (filename == NULL)
  {
    filename = path;
  }
  else
  {
    filename++;
  }
  return filename;
}

char *checkForVrt(char *filename, char *vrtBuff)
{
  char *vrtFile;
  vrtFile = appendSuffix(filename, ".vrt", vrtBuff);
  if (access(vrtFile, F_OK) == 0)
  {
    return vrtFile;
  }
  return NULL;
}

void writeSingleVRT(int32_t nR, int32_t nA, dictNode *metaData, char *vrtFile, char *bandFiles[], char *bandNames[],
                    GDALDataType dataTypes[], char *byteSwapOption, double noDataValue, int32_t nBands)
{
  GDALRasterBandH hBand;
  char fileOption[2048];
  char fullPath[2048];
  double geoTransform[6] = {-0.5, 1., 0., -0.5, 0., 1.};
  int i, j;
  fileOption[i] = '\0';
  GDALDriverH vrtDriver = GDALGetDriverByName("VRT");
  GDALDatasetH vrtDataset = GDALCreate(vrtDriver, vrtFile, nR, nA, 0, GDT_Unknown, NULL);
  // Add the meta data
  writeDataSetMetaData(vrtDataset, metaData);
  // Add geo transform
  GDALSetGeoTransform(vrtDataset, geoTransform);
  // Loop through bands
  for (i = 0; i < nBands; i++)
  {
    // Make a copy of the full bath so basename doesn't screw it up.
    strcpy(fullPath, bandFiles[i]);
    // Set SourceFilename (strip of dull path since its relative to VRT)
    sprintf(fileOption, "SourceFilename=%s", basename(fullPath));
    // Setup options
    char *options[] = {fileOption, "relativeToVRT=1", "subclass=VRTRawRasterBand", byteSwapOption, NULL};
    // Add the next band and set its options;
    GDALAddBand(vrtDataset, dataTypes[i], options);
    // Get the band handle
    hBand = GDALGetRasterBand(vrtDataset, i + 1);
    // Add a band description
    GDALSetMetadataItem(hBand, "Description", (const char *)bandNames[i], NULL);
    if(noDataValue != DONOTINCLUDENODATA)
      GDALSetRasterNoDataValue(hBand, noDataValue);
  }
  GDALClose(vrtDataset);
}


int makeVRT(char *vrtFile, int xSize, int ySize, int dataType, char **bandNames, int nBands,
            double *geoTransform, int byteSwap, dictNode *metaData)
{
  // Create the VRT dataset
  char nameBuffer[2048];
  char fileOption[2040], metaBuf[2014];
  int i;
  char *byteSwapOption;
  //

  // Create the driver  and data set
  GDALDriverH vrtDriver = GDALGetDriverByName("VRT");
  GDALDatasetH vrtDataset = GDALCreate(vrtDriver, vrtFile, xSize, ySize, 0, GDT_Unknown, NULL);
  writeDataSetMetaData(vrtDataset, metaData);
  // Add geo transform
  GDALSetGeoTransform(vrtDataset, geoTransform);
  // Set byteswap option, which applies to all bands
  if (byteSwap == TRUE)
    byteSwapOption = "BYTEORDER=MSB";
  else
    byteSwapOption = "BYTEORDER=LSB";
  // Now process bands
  for (i = 0; i < nBands; i++)
  {
    // Define options
    sprintf(fileOption, "SourceFilename=%s", extract_filename(bandNames[i]));
    char *options[] = {fileOption, "relativeToVRT=1", "subclass=VRTRawRasterBand", byteSwapOption, NULL};
    // Add the band
    GDALAddBand(vrtDataset, dataType, options);
    // WRite meta data
  }
  // Close the VRT dataset
  GDALClose(vrtDataset);
}

void *byteSwapData(void *buffer, int dataType, int64_t size)
{
  uint16_t *buffer2Byte = NULL;
  uint32_t *buffer4Byte = NULL;
  uint64_t *buffer8Byte = NULL;
  uint64_t i;
  uint64_t correctedSize = size;

  // Double the size for complex values
  if (dataType == GDT_CInt16 || dataType == GDT_CInt32 ||
      dataType == GDT_CFloat32 || dataType == GDT_CFloat64)
  {
    correctedSize = size * 2;
  }
  // fprintf(stderr, "%li \n", correctedSize);
  //  Now remap the data
  switch (dataType)
  {
  case GDT_Byte:
    break;
  case GDT_UInt16:
  case GDT_Int16:
    // fprintf(stderr, "byteswap 2\n");
    buffer2Byte = (uint16_t *)buffer;
    for (i = 0; i < size; i++)
      CPL_SWAP32PTR(&buffer2Byte[i]);
    break;
  case GDT_Float32:
  case GDT_Int32:
  case GDT_UInt32:
    // fprintf(stderr, "byteswap 4\n");
    buffer4Byte = (uint32_t *)buffer;
    for (i = 0; i < size; i++)
      CPL_SWAP32PTR(&buffer4Byte[i]);
    break;
  case GDT_Float64:
  case GDT_CFloat64:
    // fprintf(stderr, "byteswap 8\n");
    buffer8Byte = (uint64_t *)buffer;
    for (i = 0; i < size; i++)
      CPL_SWAP64PTR(&buffer8Byte[i]);
    break;
  default:
    error("Invalid data type in byte swap %i", dataType);
  }
  return buffer;
}

// double *geoTransform, int byteSwap
int writeRasterAsVRT(void *buffer, char *fileName, int xSize, int ySize, int dataType,
                     int band, double *geoTransform, int byteSwap, dictNode *metaData)
{
  char *vrtFile, buf[2048];
  // Get the driver
  GDALDriverH driver = GDALGetDriverByName("ENVI");
  ifNullError(driver, "writeRasterAsVRT: Error getting driver");          
  // Opent the data set
  GDALDatasetH outputDataset = GDALCreate(driver, fileName, xSize, ySize,
                                        band, dataType, NULL);
  ifNullError(outputDataset, "writeRasterAsVRT: Error creating data set");                                        
  // Get the first raster band
  GDALRasterBandH hBand = GDALGetRasterBand(outputDataset, band);
  // Write the data
  if (byteSwap == TRUE)
    buffer = byteSwapData(buffer, dataType, xSize * ySize);
  CPLErr eErr = GDALRasterIO(hBand, GF_Write, 0, 0, xSize, ySize,
                             buffer, xSize, ySize, dataType, 0, 0);
  ifNEReturnCode(eErr,  CE_None, "writeRasterAsVRT: Failed to write raster data\n");
  //, geoTransform, byteSwap
  fprintf(stderr, "Making vrt...\n");
  vrtFile = appendSuff(fileName, ".vrt", buf);
  makeVRT(vrtFile, xSize, ySize, dataType, &fileName, 1, geoTransform, byteSwap, metaData);
  // Close data set
  GDALClose(outputDataset);
  // Now make a vrt file for data set.
}

void **readRasterVRT(char *fileName, int band, int *xSize, int *ySize, int *dataType, dictNode **metaDictionary)
{
  int nbands, i;
  int dataTypeSize, status;
  void *data;
  // Open Data set and check valid band requested
  fprintf(stderr, "Reading %s\n", fileName);
  GDALDatasetH hDS = GDALOpen(fileName, GDAL_OF_READONLY);
  nbands = GDALGetRasterCount(hDS);
  if (band < 1 || band > nbands)
    error("readRasterVRT: Invalid band %i", band);
  // Get the band metadata.
  fprintf(stderr, "BAND %i\n\n", band);
  GDALRasterBandH hBand = GDALGetRasterBand(hDS, band);
  if (hBand == NULL)
    fprintf(stderr, "readRasterVRT: Could not get raster band\n");
  // Data type
  *dataType = GDALGetRasterDataType(hBand);
  // fprintf(stderr, "type %s %i\n", GDALGetDataTypeName(*dataType), *dataType);
  dataTypeSize = GDALGetDataTypeSize(*dataType) / 8;
  // fprintf(stderr, "type size %i\n", dataTypeSize);
  //  Image size
  *xSize = GDALGetRasterBandXSize(hBand);
  *ySize = GDALGetRasterBandYSize(hBand);
  // fprintf(stderr, "size %i %i\n", *xSize, *ySize);
  //  Malloc data
  data = allocData(*dataType, *xSize, *ySize);
  // Read Data
  status = GDALRasterIO(hBand, GF_Read, 0, 0, *xSize, *ySize, data, *xSize, *ySize, *dataType, 0, 0);
  readDataSetMetaData(hDS, metaDictionary);
  // fprintf(stderr, "read  %10.f %10.f \n", x[5], x[(300 * (*xSize) + 200)]);
  ifNEReturnCode(status,  CE_None, "readRasterVRT: Could not read band data\n");
  return data;
}

/*
append a suffix to a file name, use buff for space.
*/
char *appendSuff(char *file, char *suffix, char *buf)
{
  char *datFile;
  datFile = &(buf[0]);
  buf[0] = '\0';
  datFile = strcat(datFile, file);
  datFile = strcat(datFile, suffix);
  return datFile;
}