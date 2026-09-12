#include <gdal.h>

int main(int argc, char *argv[])
{
  // Check the number of arguments
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <vrt_file>\n", argv[0]);
    return 1;
  }

  // Get the VRT file name
  const char *pszVRTFilename = argv[1];

  // Open the VRT file
  GDALDatasetH poDS = GDALOpen(pszVRTFilename, GA_Update);

  // Check if the VRT file was opened successfully
  if (poDS == NULL)
  {
    fprintf(stderr, "Could not open VRT file: %s\n", pszVRTFilename);
    return 1;
  }

  // Get the number of bands in the VRT file
  int nBands = GDALGetRasterCount(poDS);

  // Get the width and height of the VRT file
  int nWidth = GDALGetRasterXSize(poDS);
  int nHeight = GDALGetRasterYSize(poDS);

  // Print the number of bands and the width and height of the VRT file
  printf("Number of bands: %d\n", nBands);
  printf("Width: %d\n", nWidth);
  printf("Height: %d\n", nHeight);

  // Close the GDALDataset object
  GDALClose(poDS);

  return 0;
}
