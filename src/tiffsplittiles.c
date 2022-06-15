/* tiffsplittiles

 v. 1.4.1

 Copyright (c) 2012-2021 Christophe Deroulers

 Portions are based on libtiff's tiffcp code. tiffcp is
 Copyright (c) 1988-1997 Sam Leffler
 Copyright (c) 1991-1997 Silicon Graphics, Inc.

 Distributed under the GNU General Public License v3 -- contact the 
 author for commercial use */

#include <stdio.h>
#include <stdlib.h> /* exit */
#include <string.h>
#include <tiff.h>
#include <tiffio.h>

#include "config.h"

#define EXIT_SYNTAX_ERROR        1
#define EXIT_IO_ERROR            2
#define EXIT_UNHANDLED_FILE_TYPE 3
#define EXIT_INSUFFICIENT_MEMORY 4

#define CopyField(tag, v) \
    if (TIFFGetField(in, tag, &v)) TIFFSetField(out, tag, v)
#define CopyField2(tag, v1, v2) \
    if (TIFFGetField(in, tag, &v1, &v2)) TIFFSetField(out, tag, v1, v2)
#define CopyField3(tag, v1, v2, v3) \
    if (TIFFGetField(in, tag, &v1, &v2, &v3)) TIFFSetField(out, tag, v1, v2, v3)


static void my_asprintf(char **ret, const char *format, ...)
{
  int n;
  char * p;
  va_list ap;

  va_start(ap, format);
  n= vsnprintf(NULL, 0, format, ap);
  va_end(ap);
  p= _TIFFmalloc(n+1);
  if (p == NULL)
    {
    perror("Insufficient memory for a character string ");
    exit(EXIT_INSUFFICIENT_MEMORY);
    }
  va_start(ap, format);
  vsnprintf(p, n+1, format, ap);
  va_end(ap);
  *ret= p;
}


static int searchNumberOfDigits(uint32_t u)
{
return snprintf(NULL, 0, "%u", u);
}


static char* searchPrefixBeforeLastDot(const char * path)
{
char * prefix;
int l= strlen(path)-1;

while (l >= 0 && path[l] != '.')
  l--;

if (l < 0)
  l= strlen(path);

if ((prefix = _TIFFmalloc(l+1)) == NULL)
  {
  perror("Insufficient memory for a character string ");
  exit(EXIT_INSUFFICIENT_MEMORY);
  }

strncpy(prefix, path, l);
prefix[l]= 0;

return prefix;
}


static void copyOtherFields(TIFF* in, TIFF* out)
{ /* after tiffcp in libtiff's tiffsplit.c */
  uint16_t bitspersample, samplesperpixel, shortv, *shortav;
  float floatv;
  char *stringv;
  uint32_t longv;

  CopyField(TIFFTAG_SUBFILETYPE, longv);
  CopyField(TIFFTAG_BITSPERSAMPLE, bitspersample);
  CopyField(TIFFTAG_SAMPLESPERPIXEL, samplesperpixel);
  CopyField(TIFFTAG_PHOTOMETRIC, shortv);
  CopyField(TIFFTAG_PREDICTOR, shortv);
  CopyField(TIFFTAG_THRESHHOLDING, shortv);
  CopyField(TIFFTAG_FILLORDER, shortv);
  CopyField(TIFFTAG_ORIENTATION, shortv);
  CopyField(TIFFTAG_MINSAMPLEVALUE, shortv);
  CopyField(TIFFTAG_MAXSAMPLEVALUE, shortv);
  CopyField(TIFFTAG_XRESOLUTION, floatv);
  CopyField(TIFFTAG_YRESOLUTION, floatv);
  CopyField(TIFFTAG_GROUP3OPTIONS, longv);
  CopyField(TIFFTAG_GROUP4OPTIONS, longv);
  CopyField(TIFFTAG_RESOLUTIONUNIT, shortv);
  CopyField(TIFFTAG_PLANARCONFIG, shortv);
  CopyField(TIFFTAG_ROWSPERSTRIP, longv);
  CopyField(TIFFTAG_XPOSITION, floatv);
  CopyField(TIFFTAG_YPOSITION, floatv);
  CopyField(TIFFTAG_IMAGEDEPTH, longv);
  CopyField(TIFFTAG_TILEDEPTH, longv);
  CopyField(TIFFTAG_SAMPLEFORMAT, shortv);
  CopyField2(TIFFTAG_EXTRASAMPLES, shortv, shortav);
  { uint16_t *red, *green, *blue;
    CopyField3(TIFFTAG_COLORMAP, red, green, blue);
  }
  { uint16_t shortv2;
    CopyField2(TIFFTAG_PAGENUMBER, shortv, shortv2);
  }
  CopyField(TIFFTAG_ARTIST, stringv);
  CopyField(TIFFTAG_IMAGEDESCRIPTION, stringv);
  CopyField(TIFFTAG_MAKE, stringv);
  CopyField(TIFFTAG_MODEL, stringv);
  CopyField(TIFFTAG_SOFTWARE, stringv);
  CopyField(TIFFTAG_DATETIME, stringv);
  CopyField(TIFFTAG_HOSTCOMPUTER, stringv);
  CopyField(TIFFTAG_PAGENAME, stringv);
  CopyField(TIFFTAG_DOCUMENTNAME, stringv);
  CopyField(TIFFTAG_BADFAXLINES, longv);
  CopyField(TIFFTAG_CLEANFAXDATA, longv);
  CopyField(TIFFTAG_CONSECUTIVEBADFAXLINES, longv);
  CopyField(TIFFTAG_FAXRECVPARAMS, longv);
  CopyField(TIFFTAG_FAXRECVTIME, longv);
  CopyField(TIFFTAG_FAXSUBADDRESS, stringv);
  CopyField(TIFFTAG_FAXDCS, stringv);
}


static void
stderrErrorHandler(const char* module, const char* fmt, va_list ap)
{
  if (module != NULL)
    fprintf(stderr, "%s: ", module);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, ".\n");
}


static void
stderrWarningHandler(const char* module, const char* fmt, va_list ap)
{
  if (module != NULL)
    fprintf(stderr, "%s: ", module);
  fprintf(stderr, "Warning, ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, ".\n");
}


static void usage()
{
  fprintf(stderr, "tiffsplittiles v" PACKAGE_VERSION " License GNU GPL v3 (c) 2012-2017 Christophe Deroulers\n\n");
  fprintf(stderr, "Quote \"Deroulers et al., Diagnostic Pathology 2013, 8:92\" in your production\n       http://doi.org/10.1186/1746-1596-8-92\n\n");
  fprintf(stderr, "Usage: tiffsplittiles [options] file.tif\n");
  fprintf(stderr, " Options:\n");
  fprintf(stderr, "  -t  output tiled rather than stripped TIFF files\n");
  fprintf(stderr, "  -T  report TIFF errors/warnings on stderr rather than in dialog boxes\n");
}


int main(int argc, char * argv[])
{
TIFF* in;
char* inpathbeforelastdot;
int output_tiled_tiffs = 0, arg = 1;
int number_digits_horiz_tile_numbers, number_digits_vert_tile_numbers;
uint32_t imagewidth, imagelength;
uint32_t tilewidth, tilelength;
uint32_t x, y;
uint32_t imagedepth;
uint16_t planarconfig, compression;
tsize_t bufsize;
tdata_t buf;
int io_error= 0;

while (arg < argc && argv[arg][0] == '-')
  {
  if (argv[arg][1] == 't')
    output_tiled_tiffs = 1;
  else if (argv[arg][1] == 'T')
    {
    TIFFSetErrorHandler(stderrErrorHandler);
    TIFFSetWarningHandler(stderrWarningHandler);
    }
  else
    {
    fprintf(stderr, "Unknown option \"%s\"\n", argv[arg]);
    usage();
    return EXIT_SYNTAX_ERROR;
    }
  arg++;
  }

if (arg != argc-1)
  {
  fprintf(stderr, "Exactly one file name should be given on the command line, after options.\n");
  usage();
  return EXIT_SYNTAX_ERROR;
  }

if(! (in = TIFFOpen(argv[arg], "r")) )
  {
  perror("Unable to open TIFF file.");
  return EXIT_IO_ERROR;
  }

if (!TIFFIsTiled(in))
  {
  TIFFError(TIFFFileName(in), "Provided file is not tiled -- I can't deal with it");
  return EXIT_UNHANDLED_FILE_TYPE;
  }

TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &imagewidth);
TIFFGetField(in, TIFFTAG_IMAGELENGTH, &imagelength);
TIFFGetField(in, TIFFTAG_TILEWIDTH, &tilewidth);
TIFFGetField(in, TIFFTAG_TILELENGTH, &tilelength);
TIFFGetField(in, TIFFTAG_COMPRESSION, &compression);
TIFFGetField(in, TIFFTAG_PLANARCONFIG, &planarconfig);
TIFFGetFieldDefaulted(in, TIFFTAG_IMAGEDEPTH, &imagedepth);

if (planarconfig != PLANARCONFIG_CONTIG)
  {
  TIFFError(TIFFFileName(in), "Provided file has a noncontiguous configuration -- I can't deal with it");
  return EXIT_UNHANDLED_FILE_TYPE;
  }

if (imagedepth != 1)
  {
  TIFFError(TIFFFileName(in), "Provided file has image depth %u different from 1 -- I can't deal with it",
            imagedepth);
  return EXIT_UNHANDLED_FILE_TYPE;
  }

/* Use this if the tiles are read with TIFFReadTile rather than 
  TIFFReadRawTile */
if (compression == COMPRESSION_JPEG)
  { /* like in libtiff's tiffcp.c -- otherwise the reserved size for
      the tiles is too small and the program segfaults */
  /*TIFFSetField(in, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
  TIFFSetField(out, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);*/
  }

bufsize= TIFFTileSize(in);

/*fprintf(stderr, "Allocating %u bytes; tiles are %ux%u large.\n", 
        bufsize, tilewidth, tilelength);*/

if (! (buf = _TIFFmalloc(bufsize)) )
  {
  TIFFError(TIFFFileName(in), "Error: insufficient memory");
  TIFFClose(in);
  return EXIT_INSUFFICIENT_MEMORY;
  }

inpathbeforelastdot= searchPrefixBeforeLastDot(argv[arg]);
number_digits_horiz_tile_numbers=
  searchNumberOfDigits((imagewidth+tilewidth-1)/tilewidth);
number_digits_vert_tile_numbers=
  searchNumberOfDigits((imagelength+tilelength-1)/tilelength);

/* While debugging: */
/*imagelength= tilelength < imagelength ? tilelength : imagelength;*/

#pragma omp parallel for shared(io_error)
for (y = 0; y < imagelength; y += tilelength)
  {
  if (! io_error)
  {
  uint32_t outputimageslength= tilelength;

fprintf(stderr, "Dealing with line " UINT32_FORMAT "/" UINT32_FORMAT
        " y=" UINT32_FORMAT " -> outputimageslength=" UINT32_FORMAT
        "\n",
        (y/tilelength)+1, imagelength/tilelength, y,
        outputimageslength);

  for (x = 0; x < imagewidth; x += tilewidth)
    {
    char * outpath;
    TIFF * out;
    uint32_t tilenumber= TIFFComputeTile(in, x, y, 0, 0);
    uint32_t outputimagewidth= tilewidth;

/*fprintf(stderr, "Reading tile at (%u, %u), which has the number %u -> i%0*uj%0*u\n", 
        x, y, tilenumber, number_digits_horiz_tile_numbers,
        x/tilewidth+1, number_digits_vert_tile_numbers, y/tilelength+1);*/

    //TIFFReadTile(in, buf, x, y, 0, 0);
    if (TIFFReadRawTile(in, tilenumber, buf, bufsize) == -1)
      {
      TIFFError(TIFFFileName(in), "Error while reading tile #%u", tilenumber);
      TIFFClose(in);
      #pragma omp atomic
      io_error++;
      continue;
      }

    my_asprintf(&outpath, "%s_t_i%0*uj%0*u.tif", inpathbeforelastdot,
                number_digits_horiz_tile_numbers, x/tilewidth+1,
                number_digits_vert_tile_numbers, y/tilelength+1);
    out = TIFFOpen(outpath, "w");
    if (out == NULL)
      {
      TIFFError(TIFFFileName(out), "Error while creating output file %s", TIFFFileName(out));
      TIFFClose(in);
      #pragma omp atomic
      io_error++;
      continue;
      }

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, outputimagewidth);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, outputimageslength);
    TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
    if (output_tiled_tiffs)
      {
      TIFFSetField(out, TIFFTAG_TILEWIDTH, tilewidth);
      TIFFSetField(out, TIFFTAG_TILELENGTH, tilelength);
      }
    if (compression == COMPRESSION_JPEG)
      {
      uint32_t count = 0;
      void *table = NULL;
      uint16_t subsamplinghor, subsamplingver;
      if (TIFFGetField(in, TIFFTAG_JPEGTABLES, &count, &table)
          && count > 0 && table)
        TIFFSetField(out, TIFFTAG_JPEGTABLES, count, table);
      //TIFFSetField(out, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
      CopyField2(TIFFTAG_YCBCRSUBSAMPLING, subsamplinghor,
                 subsamplingver);
      }
    copyOtherFields(in, out);

    if ((output_tiled_tiffs && TIFFWriteRawTile(out, 0, buf, bufsize) == -1) ||
        (!output_tiled_tiffs && TIFFWriteRawStrip(out, 0, buf, bufsize) == -1))
      {
      TIFFError(TIFFFileName(out), "Error while writing tile #%u", tilenumber);
      TIFFClose(out);
      TIFFClose(in);
      #pragma omp atomic
      io_error++;
      continue;
      }

    TIFFClose(out);
    _TIFFfree(outpath);
    }
  } /* if (! io_error) */
  } /* for y */

if (io_error)
  return EXIT_IO_ERROR;

TIFFClose(in);
_TIFFfree(buf);
_TIFFfree(inpathbeforelastdot);

return 0;
}
