# gdalIO — GDAL Raster I/O and VRT Utilities

## Purpose

Shared library providing GDAL-based raster read/write, VRT construction, GeoTIFF/COG
output, byte-swapping, metadata dictionary management, and coordinate-system helpers.
Used by `geomosaic`, `mosaic3d`, `siminsar`, and other programs requiring GDAL I/O.

Located in `GIT64/gdalIO/gdalIO/`.

---

## Files

| File | Contents |
|------|----------|
| `gdalIO.c` | Core raster I/O, VRT creation, byte-swapping, metadata I/O |
| `tiffWriteCode.c` | GeoTIFF / COG writing, geotransform utilities, EPSG lookup |
| `dictionaryCode.c` | Linked-list key/value dictionary (`dictNode`) |

---

## gdalIO.c

### `allocData`(data_type, width, height) → void\*
Allocates a flat pixel buffer for a GDAL raster of `width × height` pixels of
`data_type` (a `GDALDataType` constant). Uses `CPLMalloc` for GDAL-compatible memory.

---

### `writeDataSetMetaData`(dataSet, metaData) → int
Writes all key/value pairs from a `dictNode` linked list to a GDAL dataset's metadata
domain (default domain, `NULL`). Each node becomes one `GDALSetMetadataItem` call.  
*Calls:* `GDALSetMetadataItem`

---

### `readDataSetMetaData`(dataSet, \*\*metaDictionary) → int
Reads the default metadata domain of a GDAL dataset and inserts each `KEY=VALUE` pair
into the `dictNode` dictionary. Parses the GDAL metadata string array by splitting on `=`.  
*Calls:* `GDALGetMetadata`, `insert_node`

---

### `writeSingleVRT`(nR, nA, metaData, vrtFile, bandFiles[], bandNames[], dataTypes[], byteSwapOption, noDataValue, nBands) → void
Creates a multi-band VRT file backed by flat binary raster files (one file per band)
using the `VRTRawRasterBand` subclass. Each band is added with:
- Source filename (relative to VRT, basename only)
- Byte order (`ByteOrder=MSB` or `ByteOrder=LSB`)
- Band description metadata
- No-data value (if not `DONOTINCLUDENODATA`)

The geotransform is set to a unit pixel-centred grid (`[-0.5, 1, 0, -0.5, 0, 1]`) —
i.e., no georeferencing unless the caller also sets projection.  
*Calls:* `writeDataSetMetaData`, `GDALAddBand`, `GDALSetRasterNoDataValue`

---

### `makeVRT`(vrtFile, xSize, ySize, dataType, \*\*bandNames, nBands, \*geoTransform, byteSwap, metaData) → int
Creates a VRT file with a user-supplied geotransform. Each band is a `VRTRawRasterBand`
sourced from the file whose basename matches `bandNames[i]`. Byte order is set globally
for all bands.  
*Calls:* `writeDataSetMetaData`, `GDALSetGeoTransform`, `GDALAddBand`

---

### `byteSwapData`(buffer, dataType, size) → void\*
In-place byte-swap of a pixel buffer. Handles 2-byte (UInt16, Int16), 4-byte (Float32,
Int32, UInt32), and 8-byte (Float64, CFloat64) GDAL data types. Complex types
(CInt16, CFloat32, etc.) double the element count before swapping. Uses CPL swap macros
(`CPL_SWAP32PTR`, `CPL_SWAP64PTR`).

---

### `writeRasterAsVRT`(buffer, fileName, xSize, ySize, dataType, band, \*geoTransform, byteSwap, metaData) → int
Writes a pixel buffer to disk as an ENVI flat binary file, optionally byte-swapping,
then calls `makeVRT` to create a companion `.vrt` descriptor.  
*Calls:* `byteSwapData`, `GDALRasterIO`, `makeVRT`

---

### `readRasterVRT`(fileName, band, \*xSize, \*ySize, \*dataType, \*\*metaDictionary, data, yMin, yMax) → void\*\*
Reads a single band from any GDAL-supported file (VRT, GeoTIFF, ENVI, etc.) into a
pre-allocated or freshly allocated buffer. Supports partial reads: only rows `yMin`..`yMax`
are actually read from disk; the rest of the buffer is pre-filled with `-LARGEINT`.
Also reads dataset metadata into `metaDictionary`.  
*Calls:* `allocData`, `readDataSetMetaData`, `GDALRasterIO`

---

### `checkForVrt`(filename, vrtBuff) → char\*
Checks whether a `.vrt` companion file exists for `filename`. Returns the VRT path if
found, `NULL` otherwise.  
*Calls:* `appendSuffix`, `access`

---

### `appendSuff`(file, suffix, buf) → char\*
Appends a suffix string to a filename using a caller-supplied buffer. Returns a pointer
into `buf`.

---

### `parseNameValue`(metaBuf, \*\*value) → char\*
Splits a `KEY=VALUE` string (in place) using `strtok`. Returns the key pointer; sets
`*value` to the value portion. Used internally by `readDataSetMetaData`.

---

### `extract_filename`(path) → char\*
Returns a pointer to the filename component of a path (after the last `/` or `\`).

---

### `has_suffix`(str, suffix) → int
Returns 1 if `str` ends with `suffix`, 0 otherwise. Case-sensitive.

---

## tiffWriteCode.c

### `saveAsGeotiff`(filename, data, width, height, geotransform, epsg_code, metaData, driverType, dataType, noDataValue) → void
Writes a single-band raster to a GeoTIFF or COG file. Behaviour depends on `driverType`:

| `driverType` | Behaviour |
|---|---|
| `"GTiff"` | Writes directly with DEFLATE compression and BIGTIFF support |
| `"COG"` | Writes to a MEM dataset, then copies to COG with DEFLATE, 512-pixel tiles, and auto overviews |

The data array is **flipped vertically** before writing (GrIMP uses lower-left origin;
GeoTIFF expects upper-left) and flipped back after. Projection is set from the EPSG code.  
*Calls:* `writeDataSetMetaData`, `GDALSetGeoTransform`, `OSRImportFromEPSG`, `GDALRasterIO`, `GDALCreateCopy`

---

### `computeGeoTransform`(geoTransform[6], x0, y0, xSize, ySize, deltaX, deltaY) → void
Computes a GDAL 6-element geotransform from GrIMP lower-left origin conventions:

$$
\text{GT}[0] = x_0 - \tfrac{\Delta x}{2}, \quad
\text{GT}[1] = \Delta x, \quad
\text{GT}[3] = y_0 + (N_y - 1)\,\Delta y + \tfrac{\Delta y}{2}, \quad
\text{GT}[5] = -\Delta y
$$

where $(x_0, y_0)$ is the lower-left corner origin (km), $\Delta x$, $\Delta y$ are
pixel spacings (km), and $N_y$ is the number of rows. The resulting geotransform places
the upper-left pixel at the correct georeferenced position for north-up GeoTIFFs.

---

### `timeStampMeta`() → char\*
Returns a heap-allocated string with the current local time in `YYYY-MM-DD HH:MM:SS`
format, for use as a metadata timestamp.

---

### `getEPSGFromProjectionParams`(rot, slat, hemisphere) → const char\*
Maps GrIMP polar stereographic projection parameters to an EPSG code:

| rot | slat | hemisphere | EPSG |
|-----|------|------------|------|
| 45° | 70° | NORTH | 3413 (NSIDC Sea Ice Polar Stereographic North) |
| 0°  | 71° | SOUTH | 3031 (Antarctic Polar Stereographic) |

Errors on unrecognised combinations.

---

### `makeTiffVRT`(vrtFile, bands[], nBands, noDataValues[], metaData) → int
Builds a multi-band VRT from an array of GeoTIFF band files using `GDALBuildVRT` with
the `-separate` flag (each file becomes one band). Sets per-band descriptions from the
suffix before `.tif` in each filename, and per-band no-data values. Also writes dataset
metadata.  
*Calls:* `writeDataSetMetaData`, `GDALBuildVRT`, `GDALSetRasterNoDataValue`

---

## dictionaryCode.c — `dictNode` Dictionary

A simple singly-linked list of `(key, value)` string pairs. Used throughout gdalIO for
passing metadata between functions.

### `create_node`(key, value) → dictNode\*
Allocates and initialises a new `dictNode` with `strdup`'d copies of `key` and `value`.

### `insert_node`(\*\*head, key, value) → void
Appends a new node to the tail of the list.

### `free_dictionary`(head) → void
Frees all nodes and their `key`/`value` strings.

### `get_value`(head, key) → char\*
Linear search returning the value for the first matching key, or `NULL`.

### `printDictionary`(head) → void
Prints all key/value pairs to stderr.

---

## Key Data Structures

| Structure | Contents |
|-----------|----------|
| `dictNode` | Linked-list node: `char *key`, `char *value`, `dictNode *next` |

---

## Notes

- **Byte order:** GrIMP flat binary files are MSB (big-endian). `byteSwapData` and the
  `ByteOrder=MSB` VRT option handle the conversion for GDAL reads on little-endian hosts.
- **Vertical flip:** GrIMP images are stored bottom-to-top (lower-left origin); GeoTIFF
  requires top-to-bottom (upper-left origin). `saveAsGeotiff` flips in-place and restores.
- **No-data sentinel:** `-LARGEINT` (~2×10⁹ as float) is the standard GrIMP no-data value;
  `DONOTINCLUDENODATA` suppresses writing a no-data value to the VRT band.
- **COG output:** Uses the two-pass MEM → COG approach required by GDAL's COG driver to
  ensure proper tiling and overview generation.
