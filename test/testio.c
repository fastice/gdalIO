#include <gdal.h>
#include <gdal_vrt.h>
#include <cpl_conv.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include "gdalIO/gdalIO/grimpgdal.h"

char *appendSuffix(char *file, char *suffix, char *buf)
{
	char *datFile;
	datFile = &(buf[0]);
	buf[0] = '\0';
	strcpy(datFile, file);
	strcat(datFile, suffix);
	return datFile;
}

int main(int argc, char **argv)
{
  dictNode *metaOut = NULL;
  dictNode *metaIn = NULL;
  int xSize, ySize, dataType, status;
  // float *data, *data1;
  float_t *data, *data1;
  int nbands, i, n;
  int outputDataType;
  double geoTransform[6] = {0., 1., 0., 0., 0., 1.};
GDALAllRegister();
  data1 = (float_t *)readRasterVRT("RSLCbinary.4x6.HH.vrt", 1, &xSize, &ySize, &dataType, &metaOut);
  printDictionary(metaOut);
  fprintf(stderr, "%f", data1[1000, 1000]);
  exit(-1);
  /*
  // Test meta data
  insert_node(&metaIn, "AAA", "XYZ");
  insert_node(&metaIn, "BBB", "234");
  insert_node(&metaIn, "CCC", "89");
  printDictionary(metaIn);
  // Parameters
  int byteSwap = FALSE, band = 1;

  // Create a buffer of test data
  xSize = 500;
  ySize = 600;
  outputDataType = GDT_UInt16;
  data = allocData(outputDataType, xSize, ySize);
  for (i = 0; i < (xSize * ySize); i++)
    data[i] = (uint16_t)10;
  // Now write the buffer as a raster
  fprintf(stderr, "Writing raster...\n");
  // fprintf(stderr, "input %10.1f \n", data[300 * xSize + 200]);
  fprintf(stderr, "input %i \n", data[300 * xSize + 200]);
  writeRasterAsVRT(data, "myTestData", xSize, ySize, outputDataType, band, geoTransform, byteSwap, metaIn);
  // Now read the data back
  // data1 = (float *)readRasterVRT("myTestData.vrt", 1, &xSize, &ySize, &dataType);
  data1 = (uint16_t *)readRasterVRT("myTestData.vrt", 1, &xSize, &ySize, &dataType, &metaOut);
  printDictionary(metaOut);
  fprintf(stderr, "%i %i %i\n", xSize, ySize, dataType);

  // fprintf(stderr, " output  %10.f \n", data1[300 * xSize + 200]);
  fprintf(stderr, " output  %i \n", data1[300 * xSize + 200]);
  */
}