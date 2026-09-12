#include "gdal.h"
#include "cpl_conv.h" // for CPLMalloc()

int main() {
  GDALAllRegister(); // register all GDAL drivers

  GDALDatasetH hDataset;
  GDALRasterBandH hBand;
  int nRows, nCols;
  float *pafData;

  // open VRT file
  hDataset = GDALOpen("filt_topophase.unw.vrt", GA_ReadOnly);
  if (hDataset == NULL) {
    printf("Failed to open file.\n");
    exit(1);
  }

  // get raster band
  hBand = GDALGetRasterBand(hDataset, 1);

  // get number of rows and columns
  nRows = GDALGetRasterYSize(hDataset);
  nCols = GDALGetRasterXSize(hDataset);

  // allocate memory for data
  pafData = (float*) CPLMalloc(sizeof(float) * nRows * nCols);

  // read data into memory
  GDALRasterIO(hBand, GF_Read, 0, 0, nCols, nRows, pafData, nCols, nRows, GDT_Float32, 0, 0);
  fprintf(stderr, "%f %f\n", pafData[nCols*1000 + 1000], pafData[nCols*1000 + 1000]);    
  // do something with the data
  // ...

  // free memory
  CPLFree(pafData);
  GDALClose(hDataset);

  return 0;
}
