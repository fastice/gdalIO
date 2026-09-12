#include <stdlib.h>
#include <stdio.h>
#include <gdal.h>

  // Return the metadata.
  typedef struct vrt_metadata {
    int nBands;
    int nWidth; 
    int nHeight;
    GDALDataType eDataType;
 } vrtMeta;
  
vrtMeta read_vrt_metadata(char *filename) {
  // Open the VRT file.
  vrtMeta result; 

  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error opening VRT file: %s\n", filename);
    return result;
  }

  // Get the number of bands.
  result.nBands = 0;
  char line[1024];
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == 'b' && line[1] == 'a' && line[2] == 'n') {
      result.nBands++;
    }
  }

  // Get the band width and height.
  result.nWidth = 0;
  result.nHeight = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == 'w' && line[1] == 'i' && line[2] == 'd') {
      result.nWidth = atoi(line + 3);
    } else if (line[0] == 'h' && line[1] == 'e' && line[2] == 'i') {
      result.nHeight = atoi(line + 3);
    }
  }

  // Get the band data type.
  GDALDataType eDataType = GDT_Byte;
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == 'd' && line[1] == 'a' && line[2] == 't') {
      if (line[5] == 'B') {
        result.eDataType = GDT_Byte;
      } else if (line[5] == 'U') {
        result.eDataType = GDT_UInt16;
      } else if (line[5] == 'S') {
        result.eDataType = GDT_Int16;
      } else if (line[5] == 'F') {
        result.eDataType = GDT_Float32;
      } else if (line[5] == 'D') {
        result.eDataType = GDT_Float64;
      }
    }
  }
  // Close the VRT file.
  fclose(fp);
  return result;
}

int main() {
  // Get the metadata for the VRT file.
  struct vrt_metadata metadata = read_vrt_metadata("myvrt.vrt");

  // Print the metadata.
  printf("Number of bands: %d\n", metadata.nBands);
  printf("Width: %d\n", metadata.nWidth);
  printf("Height: %d\n", metadata.nHeight);
  printf("Data type: %s\n", GDALGetDataTypeName(metadata.eDataType));

  return 0;
}
