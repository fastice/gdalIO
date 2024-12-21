#ifndef GRIMPDAL_H
#define GRIMPDAL_H

#include <gdal.h>
#include <gdal_vrt.h>
#include <cpl_conv.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
// header contents
#define DONOTINCLUDENODATA 1e30
typedef struct dictNode {
    char *key;
    char *value;
    struct dictNode *next;
} dictNode;

dictNode* create_node(char *key, char *value);
void insert_node(dictNode **head, char *key, char *value);
void free_dictionary(dictNode *head);
char* get_value(dictNode *head, char *key);
void printDictionary(dictNode *head);


void *allocData(int data_type, int width, int height);

char *parseNameValue(char *metaBuf, char **value);

int writeDataSetMetaData(GDALDatasetH dataSet, dictNode *metaData);

int readDataSetMetaData(GDALDatasetH dataSet, dictNode **metaDict);
void writeSingleVRT(int32_t nR, int32_t nA, dictNode *metaData, char *vrtFile, char *bandFiles[], char *bandNames[],
                    GDALDataType dataTypes[], char *byteSwapOption, double noDataValue, int32_t nBands);
int makeVRT(char *vrtFile, int xSize, int ySize, int dataType, char **bandNames, int nBands,
            double *geoTransform, int byteSwap, dictNode *metaData);
char *checkForVrt(char *filename, char *vrtBuff);
void *byteSwapData(void *buffer, int dataType, int64_t size);

int writeRasterAsVRT(void *buffer, char *fileName, int xSize, int ySize, int dataType,
                     int band, double *geoTransform, int byteSwap, dictNode *metaData);

void **readRasterVRT(char *fileName, int band, int *xSize, int *ySize, int *dataType, dictNode **metaDictionary);
char *appendSuff(char *file, char *suffix, char *buf);
void saveAsGeotiff(const char *filename, void *data, int32_t width, int32_t height, double *geotransform,
                   const char *epsg_code, char **metadata, char *driverType, int32_t dataType);
char *timeStampMeta();                 
void computeGeoTransform(double geoTransform[6], double x0, double y0, int32_t xSize, int32_t ySize, double deltaX, double deltaY);
const char *getEPSGFromProjectionParams(double rot, double slat, int32_t hemisphere);
#endif