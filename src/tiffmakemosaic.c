/* tiffmakemosaic

 v. 1.4.1

 Copyright (c) 2012-2021 Christophe Deroulers

 Portions are based on libtiff's tiffcp code. tiffcp is
 Copyright (c) 1988-1997 Sam Leffler
 Copyright (c) 1991-1997 Silicon Graphics, Inc.

 Distributed under the GNU General Public License v3 -- contact the 
 author for commercial use */

/* TODO: fix option "-c jpeg:r" -- presently, it is deactivated because 
 it makes the program segfault. */

#include <stdio.h>
#include <stdlib.h> /* exit, strtoul */
#include <string.h>
#include <strings.h> /* strncasecmp */
#include <assert.h>
#include <errno.h>
#include <tiff.h>
#include <tiffio.h>
#include <jpeglib.h>
#include <math.h> /* lroundl */

#include "config.h"

#define JPEG_MAX_DIMENSION 65500L /* in libjpeg's jmorecfg.h */

#define EXIT_SYNTAX_ERROR        1
#define EXIT_IO_ERROR            2
#define EXIT_UNHANDLED_FILE_TYPE 3
#define EXIT_INSUFFICIENT_MEMORY 4
#define EXIT_UNABLE_TO_ACHIEVE_PIECE_DIMENSIONS 5

#define CopyField(tag, v) \
    if (TIFFGetField(in, tag, &v)) TIFFSetField(out, tag, v)
#define CopyField2(tag, v1, v2) \
    if (TIFFGetField(in, tag, &v1, &v2)) TIFFSetField(out, tag, v1, v2)
#define CopyField3(tag, v1, v2, v3) \
    if (TIFFGetField(in, tag, &v1, &v2, &v3)) TIFFSetField(out, tag, v1, v2, v3)

static const char TIFF_SUFFIX[] = ".tif";
static const char JPEG_SUFFIX[] = ".jpg";
static uint64_t mosaicpiecesize = 1 << 30; /* 1 GiB */
static uint32_t overlapinpixels = 0;
static long double overlapinpercent = 0;
static uint32_t requestedpiecewidth = 0;
static uint32_t requestedpiecelength = 0;
static uint32_t requestedpiecewidthdivisor = 0;
static uint32_t requestedpiecelengthdivisor = 0;
static int verbose = 0;
static int dryrun = 0;
static int paddinginx = 0;
static int paddinginy = 0;
static uint16_t numberpaddingvalues = 0;
static char ** paddingvalues = NULL;


#ifndef TIFFMAKEMOSAIC_OUTPUTJPEGFILES
	#define TIFFMAKEMOSAIC_OUTPUTJPEGFILES 0
#endif
static int output_JPEG_files = TIFFMAKEMOSAIC_OUTPUTJPEGFILES;

static uint32_t defg3opts = (uint32_t) -1;
static int quality = -1, default_quality = 75;            /* JPEG quality */
static int jpegcolormode = JPEGCOLORMODE_RGB;
static uint16_t defcompression = (uint16_t) -1;
static uint16_t defpredictor = (uint16_t) -1;
static int defpreset = -1;
/*static uint16_t defphotometric = (uint16_t) -1;*/


static void my_asprintf(char ** ret, const char * format, ...)
{
	int n;
	char * p;
	va_list ap;

	va_start(ap, format);
	n= vsnprintf(NULL, 0, format, ap);
	va_end(ap);
	p= _TIFFmalloc(n+1);
	if (p == NULL) {
		perror("Insufficient memory for a character string ");
		exit(EXIT_INSUFFICIENT_MEMORY);
	}
	va_start(ap, format);
	vsnprintf(p, n+1, format, ap);
	va_end(ap);
	*ret= p;
}


static char * photometricName(uint16_t photometric)
{
	char * s= NULL;
	switch (photometric) {
		case PHOTOMETRIC_MINISWHITE:
			my_asprintf(&s, "MinIsWhite"); break;
		case PHOTOMETRIC_MINISBLACK:
			my_asprintf(&s, "MinIsBlack"); break;
		case PHOTOMETRIC_RGB:
			my_asprintf(&s, "RGB"); break;
		case PHOTOMETRIC_PALETTE:
			my_asprintf(&s, "Palette"); break;
		case PHOTOMETRIC_YCBCR:
			my_asprintf(&s, "YCbCr"); break;
		default:
			my_asprintf(&s, "%u", photometric);
	}
	return s;
}


static int searchNumberOfDigits(uint32_t u)
{
	return snprintf(NULL, 0, "%u", u);
}


static char * searchPrefixBeforeLastDot(const char * path)
{
	char * prefix;
	int l= strlen(path)-1;

	while (l >= 0 && path[l] != '.')
		l--;

	if (l < 0)
		l= strlen(path);

	if ((prefix = _TIFFmalloc(l+1)) == NULL) {
		perror("Insufficient memory for a character string ");
		exit(EXIT_INSUFFICIENT_MEMORY);
	}

	strncpy(prefix, path, l);
	prefix[l]= 0;

	return prefix;
}


static void
tiffCopyFieldsButDimensions(TIFF* in, TIFF* out)
{
	uint16_t bitspersample, samplesperpixel, compression, shortv, *shortav;
	float floatv;
	char *stringv;
	uint32_t longv;

	CopyField(TIFFTAG_SUBFILETYPE, longv);
	CopyField(TIFFTAG_BITSPERSAMPLE, bitspersample);
	CopyField(TIFFTAG_SAMPLESPERPIXEL, samplesperpixel);
	CopyField(TIFFTAG_COMPRESSION, compression);
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


	/* cols resp. rows do not include rightpaddinginpixels resp. 
	bottompadding */
static void cpBufToBuf(uint8_t* out, uint8_t* in, uint8_t* paddingbytes,
	uint32_t rows, uint32_t bottompadding, uint32_t cols,
	uint32_t rightpaddinginpixels, uint16_t bytesperpixel,
	int outskew, int inskew)
{
	while (rows-- > 0) {
		uint32_t jb = cols * bytesperpixel;
		uint32_t jp = rightpaddinginpixels;
		while (jb-- > 0)
			*out++ = *in++;
		while (jp-- > 0) {
			uint16_t k;
			for (k = 0 ; k < bytesperpixel ; k++)
				*out++ = paddingbytes[k];
		}
		out += outskew;
		in += inskew;
	}
	while (bottompadding-- > 0) {
		uint32_t jp = cols + rightpaddinginpixels;
		while (jp-- > 0) {
			uint32_t k;
			for (k = 0 ; k < bytesperpixel ; k++)
				*out++ = paddingbytes[k];
		}
		out += outskew;
	}
}


static int testAndFixParameters(TIFF* in,
	int output_to_jpeg_rather_than_tiff,
	tsize_t* outscanlinesizeinbytes, uint32_t width, TIFF* TIFFout,
	uint16_t* input_compression, uint16_t* bytesperpixel)
{
	uint16_t spp, bitspersample;
	uint16_t input_photometric, compression;

	TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &spp);
	TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	if (output_to_jpeg_rather_than_tiff) {
		assert( bitspersample == 8 );
		assert( spp == 3 );
	} else
		assert( bitspersample % 8 == 0 );
	*bytesperpixel = (bitspersample/8) * spp;

	TIFFGetFieldDefaulted(in, TIFFTAG_COMPRESSION, input_compression);
	TIFFGetFieldDefaulted(in, TIFFTAG_PHOTOMETRIC, &input_photometric);
	if (verbose) {
		char * pn= photometricName(input_photometric);
		fprintf(stderr, "Input file \"%s\" had compression %u "
			"and photometric interpretation %s.\n",
			TIFFFileName(in), *input_compression, pn);
		free(pn);
	}
	if (*input_compression == COMPRESSION_JPEG) {
		/* like in libtiff's tiffcp.c -- otherwise the reserved
		 size for the tiles is too small and the program
		 segfaults */
		/* force conversion to RGB */
		TIFFSetField(in, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
	} else if (input_photometric == PHOTOMETRIC_YCBCR) {
		/* otherwise, can't handle subsampled input */
		uint16_t subsamplinghor, subsamplingver;

		TIFFGetFieldDefaulted(in, TIFFTAG_YCBCRSUBSAMPLING,
				      &subsamplinghor, &subsamplingver);
		if (subsamplinghor!=1 || subsamplingver!=1) {
			fprintf(stderr, "%s: Can't deal with subsampled image.\n",
				TIFFFileName(in));
			return 0;
		}
	}

	if (output_to_jpeg_rather_than_tiff) {
		*outscanlinesizeinbytes = width * *bytesperpixel;
	} else {
		if (defcompression != (uint16_t) -1)
			TIFFSetField(TIFFout, TIFFTAG_COMPRESSION,
				defcompression);

		TIFFGetField(TIFFout, TIFFTAG_COMPRESSION, &compression);
		if (compression == COMPRESSION_JPEG) {
			if (input_photometric == PHOTOMETRIC_RGB &&
			    jpegcolormode == JPEGCOLORMODE_RGB)
				TIFFSetField(TIFFout, TIFFTAG_PHOTOMETRIC,
					PHOTOMETRIC_YCBCR);
			else
				TIFFSetField(TIFFout, TIFFTAG_PHOTOMETRIC,
					input_photometric);
			TIFFSetField(TIFFout, TIFFTAG_JPEGCOLORMODE,
				jpegcolormode);
			TIFFSetField(TIFFout, TIFFTAG_JPEGQUALITY,
				quality);
		} else if (compression == COMPRESSION_SGILOG
				|| compression == COMPRESSION_SGILOG24)
			TIFFSetField(TIFFout, TIFFTAG_PHOTOMETRIC,
			    spp == 1 ? PHOTOMETRIC_LOGL : PHOTOMETRIC_LOGLUV);
		else if (compression == COMPRESSION_LZW ||
			    compression == COMPRESSION_ADOBE_DEFLATE ||
			    compression == COMPRESSION_DEFLATE
			    /* || compression == COMPRESSION_LZMA*/
#ifdef HAVE_ZSTD
			    || compression == COMPRESSION_ZSTD
#endif
#ifdef HAVE_WEBP
			    || compression == COMPRESSION_WEBP
#endif
                        ) {
			if (defpredictor != (uint16_t)-1)
				TIFFSetField(TIFFout, TIFFTAG_PREDICTOR,
					defpredictor);
			if (defpreset != -1) {
				if (compression == COMPRESSION_ADOBE_DEFLATE ||
				    compression == COMPRESSION_DEFLATE)
					TIFFSetField(TIFFout,
						TIFFTAG_ZIPQUALITY,
						defpreset);
				/*else if (compression == COMPRESSION_LZMA)
					TIFFSetField(TIFFout,
						TIFFTAG_LZMAPRESET,
						defpreset);*/
			}
			if (*input_compression == COMPRESSION_JPEG &&
			    input_photometric == PHOTOMETRIC_YCBCR)
				TIFFSetField(TIFFout, TIFFTAG_PHOTOMETRIC,
					PHOTOMETRIC_RGB);
		} else if (compression == COMPRESSION_NONE) {
			if (*input_compression == COMPRESSION_JPEG &&
			    input_photometric == PHOTOMETRIC_YCBCR)
				TIFFSetField(TIFFout, TIFFTAG_PHOTOMETRIC,
					PHOTOMETRIC_RGB);
		}

		/*if (defphotometric != (uint16_t) -1)
			TIFFSetField(TIFFout, TIFFTAG_PHOTOMETRIC,
				defphotometric);*/

		if (verbose) {
			uint16_t output_photometric;
			char * pn;

			TIFFGetField(TIFFout, TIFFTAG_PHOTOMETRIC,
				&output_photometric);
			pn= photometricName(output_photometric);
			fprintf(stderr, "Output file \"%s\" will have "
				"compression %u and photometric "
				"interpretation %u (%s).\n",
				TIFFFileName(TIFFout), compression,
				output_photometric, pn);
			free(pn);
		}

		/* to be done *after* setting compression -- otherwise,
		 * ScanlineSize may be wrong */
		*outscanlinesizeinbytes = TIFFScanlineSize(TIFFout);

		/*fprintf(stderr, "outscanlinesizeinbytes=%u or %u\n",
		    *outscanlinesizeinbytes, width * *bytesperpixel);
		fprintf(stderr, "TIFFStripSize(TIFFout)=%u or %u\n",
		    TIFFStripSize(TIFFout), width * *bytesperpixel * length);*/
	}

	return 1;
}

	/* width resp. length include rightpadding resp. bottompadding */
static int
cpTiles2Strip(TIFF* in, void * ambiguous_out,
	int output_to_jpeg_rather_than_tiff,
	uint32_t xmin, uint32_t ymin, uint32_t width, uint32_t length,
	uint32_t rightpadding, uint32_t bottompadding, uint8_t* paddingbytes,
	uint32_t inimagewidth, uint32_t inimagelength,
	unsigned char * outbuf)
{
	struct jpeg_compress_struct * p_cinfo;
	TIFF* TIFFout;
	tsize_t inbufsize;
	uint16_t input_compression, bytesperpixel;
	uint32_t intilewidth = (uint32_t) -1, intilelength = (uint32_t) -1;
	tsize_t intilewidthinbytes = TIFFTileRowSize(in);
	tsize_t outscanlinesizeinbytes;
	uint32_t y;
	unsigned char * inbuf, * bufp= outbuf;
	int error = 0;

	if (output_to_jpeg_rather_than_tiff) {
		p_cinfo = (struct jpeg_compress_struct *) ambiguous_out;
		TIFFout = NULL;
	} else {
		p_cinfo = NULL;
		TIFFout = (TIFF*) ambiguous_out;
	}

	TIFFGetField(in, TIFFTAG_TILEWIDTH, &intilewidth);
	TIFFGetField(in, TIFFTAG_TILELENGTH, &intilelength);

	if (!testAndFixParameters(in, output_to_jpeg_rather_than_tiff,
		    &outscanlinesizeinbytes, width, TIFFout,
		    &input_compression, &bytesperpixel))
		return 0;

	inbufsize= TIFFTileSize(in);
	inbuf = (unsigned char *)_TIFFmalloc(inbufsize);
	if (!inbuf) {
		TIFFError(TIFFFileName(in),
				"Error, can't allocate space for image buffer");
		return (EXIT_INSUFFICIENT_MEMORY);
	}

	for (y = ymin ; y < ymin + length + intilelength ; y += intilelength) {
		uint32_t x, colb = 0;
		uint32_t yminoftile = (y/intilelength) * intilelength;
		uint32_t ymintocopyintile = ymin > yminoftile ? ymin : yminoftile;
		uint32_t ymaxplusone = yminoftile + intilelength;
		uint32_t lengthtocopy, bottompaddingthistile = 0;
		unsigned char * inbufrow = inbuf +
		    intilewidthinbytes * (ymintocopyintile - yminoftile);

		if (ymaxplusone > ymin + length)
			ymaxplusone = ymin + length;
		if (ymaxplusone <= yminoftile)
			break;
		if (yminoftile >= inimagelength) {
			lengthtocopy = 0;
			bottompaddingthistile = ymaxplusone - yminoftile;
		} else if (ymaxplusone > inimagelength) {
			lengthtocopy = inimagelength - ymintocopyintile;
			bottompaddingthistile = ymaxplusone - inimagelength;
		} else
			lengthtocopy = ymaxplusone - ymintocopyintile;

		for (x = xmin ; x < xmin + width + intilewidth ;
		    x += intilewidth) {
			uint32_t xminoftile = (x/intilewidth) * intilewidth;
			uint32_t xmintocopyintile = xmin > xminoftile ?
			    xmin : xminoftile;
			uint32_t xmaxplusone = xminoftile + intilewidth;
			uint32_t widthtocopy, rightpaddingthistile = 0;
			tsize_t widthtoreadinbytes, widthtowriteinbytes;

			if (xmaxplusone > xmin + width)
				xmaxplusone = xmin + width;
			if (xmaxplusone <= xminoftile)
				break;
			if (xminoftile >= inimagewidth) {
				widthtocopy = 0;
				rightpaddingthistile = xmaxplusone - xminoftile;
			} else if (xmaxplusone > inimagewidth) {
				widthtocopy = inimagewidth - xmintocopyintile;
				rightpaddingthistile = xmaxplusone - inimagewidth;
			} else
				widthtocopy = xmaxplusone - xmintocopyintile;
			widthtoreadinbytes = widthtocopy * bytesperpixel;
			widthtowriteinbytes =
			    (widthtocopy + rightpaddingthistile) * bytesperpixel;

			if (lengthtocopy > 0 && widthtocopy > 0 &&
			    TIFFReadTile(in, inbuf, xminoftile,
			    yminoftile, 0, 0) < 0) {
				TIFFError(TIFFFileName(in),
				    "Error, can't read tile at "
				    UINT32_FORMAT ", " UINT32_FORMAT,
				    xminoftile, yminoftile);
				error = EXIT_IO_ERROR;
				goto done;
			}

			cpBufToBuf(bufp + colb,
			    inbufrow + (xmintocopyintile-xminoftile) *
				bytesperpixel,
			    paddingbytes,
			    lengthtocopy, bottompaddingthistile,
			    widthtocopy, rightpaddingthistile,
			    bytesperpixel,
			    outscanlinesizeinbytes - widthtowriteinbytes,
			    intilewidthinbytes - widthtoreadinbytes);
			colb += (widthtocopy + rightpaddingthistile) *
			    bytesperpixel;
		}
		bufp += outscanlinesizeinbytes *
		    (lengthtocopy + bottompaddingthistile);
	}

	if (output_to_jpeg_rather_than_tiff) {
		JSAMPROW row_pointer;
		JSAMPROW* row_pointers =
			_TIFFmalloc(length * sizeof(JSAMPROW));

		if (row_pointers == NULL) {
			TIFFError(TIFFFileName(in),
				"Error, can't allocate space for row_pointers");
			error = EXIT_INSUFFICIENT_MEMORY;
			goto done;
		}

		for (y = 0, row_pointer = outbuf ; y < length ;
		    y++, row_pointer += width * bytesperpixel)
			row_pointers[y]= row_pointer;

		jpeg_write_scanlines(p_cinfo, row_pointers, length);
	} else {
		if (TIFFWriteEncodedStrip(TIFFout,
			TIFFComputeStrip(TIFFout, 0, 0), outbuf,
			TIFFStripSize(TIFFout)) < 0) {
			TIFFError(TIFFFileName(TIFFout),
				"Error, can't write strip");
			error = EXIT_IO_ERROR;
		}
	}

	done:
	_TIFFfree(inbuf);
	return error;
}


static int
cpStrips2Strip(TIFF* in, void * ambiguous_out,
	int output_to_jpeg_rather_than_tiff,
	uint32_t xmin, uint32_t ymin, uint32_t width, uint32_t length,
	uint32_t rightpadding, uint32_t bottompadding,
	unsigned char * outbuf, uint32_t * y_of_last_read_scanline,
	uint32_t inimagelength)
{
	struct jpeg_compress_struct * p_cinfo;
	TIFF* TIFFout;
	tsize_t inbufsize;
	uint16_t input_compression, bytesperpixel;
	tsize_t inwidthinbytes = TIFFScanlineSize(in);
	tsize_t outscanlinesizeinbytes;
	uint32_t y;
	unsigned char * inbuf, * bufp= outbuf;
	int error = 0;

	if (output_to_jpeg_rather_than_tiff) {
		p_cinfo = (struct jpeg_compress_struct *) ambiguous_out;
		TIFFout = NULL;
	} else {
		p_cinfo = NULL;
		TIFFout = (TIFF*) ambiguous_out;
	}

	if (!testAndFixParameters(in, output_to_jpeg_rather_than_tiff,
		    &outscanlinesizeinbytes, width, TIFFout,
		    &input_compression, &bytesperpixel))
		return 0;

	inbufsize= TIFFScanlineSize(in);
	inbuf = (unsigned char *)_TIFFmalloc(inbufsize);
	if (!inbuf) {
		TIFFError(TIFFFileName(in),
				"Error, can't allocate space for image buffer");
		return (EXIT_INSUFFICIENT_MEMORY);
	}

fprintf(stderr, "cpStrips2Strip warning: padding is yet to be implemented\n");

	/* when compression method doesn't support random access: */
	if (input_compression != COMPRESSION_NONE) {
		uint32_t y;

		if (*y_of_last_read_scanline > ymin) {
		/* If we need to go back, finish reading to the end, 
		 * then a restart will be automatic */
			for (y = *y_of_last_read_scanline + 1 ;
			     y < inimagelength ; y++)
				if (TIFFReadScanline(in, inbuf, y, 0) < 0) {
					TIFFError(TIFFFileName(in),
					    "Error, can't read scanline at "
					    UINT32_FORMAT " for exhausting",
					    y);
					error = EXIT_IO_ERROR;
					goto done;
				} else
					*y_of_last_read_scanline= y;
		}

		/* Then read up to the point we want to start
		 * copying at */
		for (y = 0 ; y < ymin ; y++)
			if (TIFFReadScanline(in, inbuf, y, 0) < 0) {
				TIFFError(TIFFFileName(in),
				    "Error, can't read scanline at "
				    UINT32_FORMAT " for exhausting", y);
				error = EXIT_IO_ERROR;
				goto done;
			} else
				*y_of_last_read_scanline= y;
	}

	for (y = ymin ; y < ymin + length ; y++) {
		unsigned char * inbufrow = inbuf;
		uint32_t xmintocopyinscanline = xmin;
		tsize_t widthtocopyinbytes = outscanlinesizeinbytes;

		if (TIFFReadScanline(in, inbuf, y, 0) < 0) {
			TIFFError(TIFFFileName(in),
			    "Error, can't read scanline at "
			    UINT32_FORMAT " for copying", y);
			error = EXIT_IO_ERROR;
			goto done;
		} else
			*y_of_last_read_scanline= y;

		cpBufToBuf(bufp,
		    inbufrow + xmintocopyinscanline * bytesperpixel,
		    NULL,
		    1, 0, widthtocopyinbytes, 0, bytesperpixel,
		    outscanlinesizeinbytes - widthtocopyinbytes,
		    inwidthinbytes - widthtocopyinbytes);
		bufp += outscanlinesizeinbytes;
	}

	if (output_to_jpeg_rather_than_tiff) {
		JSAMPROW row_pointer;
		JSAMPROW* row_pointers =
			_TIFFmalloc(length * sizeof(JSAMPROW));

		if (row_pointers == NULL) {
			TIFFError(TIFFFileName(in),
				"Error, can't allocate space for row_pointers");
			error = EXIT_INSUFFICIENT_MEMORY;
			goto done;
		}

		for (y = 0, row_pointer = outbuf ; y < length ;
		    y++, row_pointer += width * bytesperpixel)
			row_pointers[y]= row_pointer;

		jpeg_write_scanlines(p_cinfo, row_pointers, length);
	} else {
		if (TIFFWriteEncodedStrip(TIFFout,
			TIFFComputeStrip(TIFFout, 0, 0), outbuf,
			TIFFStripSize(TIFFout)) < 0) {
			TIFFError(TIFFFileName(TIFFout),
				"Error, can't write strip");
			error = EXIT_IO_ERROR;
		}
	}

	done:
	_TIFFfree(inbuf);
	return error;
}


static void
computeMaxPieceMemorySize(uint32_t inimagewidth, uint32_t inimagelength,
	uint16_t spp, uint16_t bitspersample,
	uint32_t outpiecewidth, uint32_t outpiecelength,
	uint32_t overlapinpixels, long double overlapinpercent,
	uint64_t * maxoutmemorysize, uint64_t * ourmaxoutmemorysize,
	uint32_t * hnpieces, uint32_t * vnpieces,
	uint32_t * hoverlap, uint32_t * voverlap)
{
	uint32_t maxpiecewidthwithoverlap, maxpiecelengthwithoverlap;

	if (overlapinpercent > 0) {
		assert(overlapinpixels == 0);
		*hoverlap= lroundl(overlapinpercent * outpiecewidth / 100);
		*voverlap= lroundl(overlapinpercent * outpiecelength / 100);
	} else {
		*hoverlap= overlapinpixels;
		*voverlap= overlapinpixels;
	}

	if (*hoverlap > outpiecewidth)
		*hoverlap= outpiecewidth;
	if (*voverlap > outpiecelength)
		*voverlap= outpiecelength;

	*hnpieces = (inimagewidth+outpiecewidth-1) / outpiecewidth;
	*vnpieces = (inimagelength+outpiecelength-1) / outpiecelength;

	maxpiecewidthwithoverlap= outpiecewidth + *hoverlap * (
		*hnpieces >= 3 ? 2 : *hnpieces-1);
	maxpiecelengthwithoverlap= outpiecelength + *voverlap * (
		*vnpieces >= 3 ? 2 : *vnpieces-1);

	*ourmaxoutmemorysize= (uint64_t) maxpiecewidthwithoverlap *
		maxpiecelengthwithoverlap * (bitspersample/8);
	*maxoutmemorysize= *ourmaxoutmemorysize * (spp == 3 ? 4 : spp);
	*ourmaxoutmemorysize *= spp;

	if (verbose) {
		fprintf(stderr, "Trying with pieces of " UINT32_FORMAT
			" x " UINT32_FORMAT " and horiz. overlap "
			UINT32_FORMAT ", vert. overlap " UINT32_FORMAT
			" (would need %.3f MiB per piece)",
			outpiecewidth, outpiecelength, *hoverlap,
			*voverlap, *maxoutmemorysize / 1048576.0);
		if (requestedpiecewidthdivisor)
			fprintf(stderr, ", width %g times the divisor "
				UINT32_FORMAT,
				1. * outpiecewidth / requestedpiecewidthdivisor,
				requestedpiecewidthdivisor);
		if (requestedpiecelengthdivisor)
			fprintf(stderr, ", length %g times the divisor "
				UINT32_FORMAT,
				1. * outpiecelength /
				    requestedpiecelengthdivisor,
				requestedpiecelengthdivisor);
		fprintf(stderr, "...\n");
	}
}


static int
makeMosaicFromTIFFFile(char * infilename)
{
	TIFF * in;
	uint32_t inimagewidth, inimagelength, outwidth, outlength;
	uint32_t hoverlap, voverlap;
	uint32_t hnpieces, vnpieces;
	uint32_t ndigitshtilenumber, ndigitsvtilenumber, x, y;
	uint16_t planarconfig, spp, bitspersample, sampleformat;
	uint64_t outmemorysize, ouroutmemorysize;
	char * prefix;
	unsigned char * outbuf = NULL;
	int return_code = 0;

	in = TIFFOpen(infilename, "r");
	if (in == NULL) {
		if (verbose)
			fprintf(stderr, "Unable to open file \"%s\".\n",
				infilename);
		return EXIT_IO_ERROR;
	}

	if (verbose)
		fprintf(stderr, "File \"%s\" open.\n", infilename);

	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &inimagewidth);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &inimagelength);
	TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &spp);
	TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLEFORMAT, &sampleformat);

	if (output_JPEG_files && bitspersample != 8 && spp != 3) {
		TIFFError(TIFFFileName(in),
			"Error, can't output JPEG files from file with "
			"bits-per-sample %d (not 8) or "
			"samples-per-pixel %d (not 3)",
			bitspersample, spp);
		TIFFClose(in);
		return EXIT_UNHANDLED_FILE_TYPE;
	} else if (bitspersample % 8 != 0) {
		TIFFError(TIFFFileName(in),
			"Error, can't deal with file with "
			"bits-per-sample %d (not a multiple of 8)",
			bitspersample);
		TIFFClose(in);
		return EXIT_UNHANDLED_FILE_TYPE;
	}

	if (verbose)
		fprintf(stderr, "File \"%s\" has %u bits per sample.\n",
			infilename, (unsigned) bitspersample);

	outwidth= requestedpiecewidth ? requestedpiecewidth : inimagewidth;
	outlength= requestedpiecelength ? requestedpiecelength :
		inimagelength;
	computeMaxPieceMemorySize(inimagewidth, inimagelength, spp,
		bitspersample, outwidth, outlength, overlapinpixels,
		overlapinpercent, &outmemorysize, &ouroutmemorysize,
		&hnpieces, &vnpieces, &hoverlap, &voverlap);
	if (verbose)
		fprintf(stderr, "File \"%s\": dimensions " UINT32_FORMAT
			" x " UINT32_FORMAT "; "
			"%u bits per sample and %u samples per pixel; "
			"takes %.3f MiB of memory (" UINT64_FORMAT
                        " bytes) to open fully.\n",
			infilename, inimagewidth, inimagelength,
			bitspersample, spp,
			outmemorysize / 1048576.0, outmemorysize);
	if (!output_JPEG_files &&
	    (requestedpiecewidth == 0 || inimagewidth <= requestedpiecewidth) &&
	    (requestedpiecelength == 0 ||
		inimagelength <= requestedpiecelength) &&
	    (mosaicpiecesize == 0 || outmemorysize <= mosaicpiecesize)) {
		/* nothing to do */
		TIFFClose(in);
		if (verbose) {
			if (mosaicpiecesize)
				fprintf(stderr, "File \"%s\": nothing to do. "
				"Its memory requirement is already "
				"below the limit %.3f MiB.\n",
				infilename, mosaicpiecesize / 1048576.0);
			else
				fprintf(stderr, "File \"%s\": nothing"
				" to do.\n", infilename);
		}
		return 0;
	}

	TIFFGetField(in, TIFFTAG_PLANARCONFIG, &planarconfig);
	if (planarconfig != PLANARCONFIG_CONTIG) {
		TIFFError(TIFFFileName(in),
			"Error, can't deal with file with "
			"planar configuration %d (non contiguous)",
			planarconfig);
		TIFFClose(in);
		return EXIT_UNHANDLED_FILE_TYPE;
	}

	if (output_JPEG_files &&
	    ( (requestedpiecewidth >= JPEG_MAX_DIMENSION) ||
	      (requestedpiecelength >= JPEG_MAX_DIMENSION) ) ) {
		if (verbose)
			fprintf(stderr, "File \"%s\": at least one requested piece dimension is too large for JPEG files.\n",
				infilename);
		return EXIT_UNABLE_TO_ACHIEVE_PIECE_DIMENSIONS;
	}

	if (requestedpiecewidth == 0 || requestedpiecelength == 0)
		while ( (mosaicpiecesize &&
			 outmemorysize > mosaicpiecesize) ||
		    (output_JPEG_files &&
		    (outwidth >= JPEG_MAX_DIMENSION ||
		    outlength >= JPEG_MAX_DIMENSION) ) ) {
			if (outlength > outwidth && (outlength % 2 == 0 || paddinginy) &&
			    requestedpiecelength == 0 &&
			    (requestedpiecelengthdivisor == 0 || paddinginy ||
			    (outlength/2) % requestedpiecelengthdivisor == 0)) {
				if (outlength % 2 != 0)
					outlength++; /* use padding */
				outlength /= 2;
				if (requestedpiecelengthdivisor != 0 &&
				    outlength % requestedpiecelengthdivisor != 0)
					outlength += requestedpiecelengthdivisor -
					    outlength % requestedpiecelengthdivisor; /* use padding */
			} else if ((outwidth % 2 == 0 || paddinginx) &&
			    requestedpiecewidth == 0 &&
			    (requestedpiecewidthdivisor == 0 || paddinginx ||
			    (outwidth/2) % requestedpiecewidthdivisor == 0)) {
				if (outwidth % 2 != 0)
					outwidth++; /* use padding */
				outwidth /= 2;
				if (requestedpiecewidthdivisor != 0 &&
				    outwidth % requestedpiecewidthdivisor != 0)
				outwidth += requestedpiecewidthdivisor -
				    outwidth % requestedpiecewidthdivisor; /* use padding */
			} else if ((outlength % 2 == 0 || paddinginy) &&
			    requestedpiecelength == 0 &&
			    (requestedpiecelengthdivisor == 0 || paddinginy ||
			     (outlength/2) % requestedpiecelengthdivisor == 0)) {
			     	if (outlength % 2 != 0)
			     		outlength++; /* use padding */
			     	outlength /= 2;
  				if (requestedpiecelengthdivisor != 0 &&
  				    outlength % requestedpiecelengthdivisor != 0)
  					outlength += requestedpiecelengthdivisor -
  					    outlength % requestedpiecelengthdivisor; /* use padding */
			} else {
				outwidth = 0;
				outlength = 0;
				break; /* can't divide any dimension by 2 */
			}

			computeMaxPieceMemorySize(inimagewidth,
			    inimagelength, spp, bitspersample,
			    outwidth, outlength, overlapinpixels,
			    overlapinpercent,
			    &outmemorysize, &ouroutmemorysize,
			    &hnpieces, &vnpieces, &hoverlap, &voverlap);
		}

	if (outwidth == 0 || outlength == 0) {
		if (verbose)
			fprintf(stderr, "File \"%s\": unable to compute"
				" dimensions of mosaic pieces that will"
				" satisfy the memory and/or divisor"
				" requirement(s).\n",
				infilename);
		return EXIT_UNABLE_TO_ACHIEVE_PIECE_DIMENSIONS;
	}

	outbuf= _TIFFmalloc(ouroutmemorysize);
	while ( outbuf == NULL ) {
		if (outlength > outwidth && (outlength % 2 == 0 || paddinginy) &&
		    (requestedpiecelengthdivisor == 0 || paddinginy ||
		     outlength % requestedpiecelengthdivisor == 0)) {
                        if (outlength % 2 != 0)
                                outlength++; /* use padding */
		        outlength /= 2;
		        if (requestedpiecelengthdivisor != 0 &&
	                    outlength % requestedpiecelengthdivisor != 0)
	                        outlength += requestedpiecelengthdivisor -
		                    outlength % requestedpiecelengthdivisor; /* use padding */
		} else if ((outwidth % 2 == 0 || paddinginx ) &&
		    (requestedpiecewidthdivisor == 0 || paddinginx ||
		    outwidth % requestedpiecewidthdivisor == 0)) {
                        if (outwidth % 2 != 0)
                                outwidth++; /* use padding */
			outwidth /= 2;
			if (requestedpiecewidthdivisor != 0 &&
			  outwidth % requestedpiecewidthdivisor != 0)
		                outwidth += requestedpiecewidthdivisor -
		                    outwidth % requestedpiecewidthdivisor; /* use padding */
		} else
			break; /* can't divide any dimension by 2 */

		computeMaxPieceMemorySize(inimagewidth,
		    inimagelength, spp, bitspersample,
		    outwidth, outlength, overlapinpixels,
		    overlapinpercent,
		    &outmemorysize, &ouroutmemorysize,
		    &hnpieces, &vnpieces, &hoverlap, &voverlap);

		outbuf= _TIFFmalloc(ouroutmemorysize);
	}
	if (outbuf == NULL) {
		if (verbose)
			fprintf(stderr, "File \"%s\": unable to compute"
				" dimensions of pieces that will suit"
				" into memory during mosaic creation.\n",
				infilename);
		return EXIT_INSUFFICIENT_MEMORY;
	}

	if (verbose) {
		fprintf(stderr, "File \"%s\": decided for "
			UINT32_FORMAT " x " UINT32_FORMAT " = "
			UINT32_FORMAT " pieces of " UINT32_FORMAT " x "
			UINT32_FORMAT " with horiz. overlap of "
			UINT32_FORMAT " and vert. overlap of "
			UINT32_FORMAT " pixels that will each take at "
			"most %.3f MiB of memory (" UINT64_FORMAT
			" bytes) to open fully.\n",
			infilename, hnpieces, vnpieces,
			hnpieces * vnpieces, outwidth,
			outlength, hoverlap, voverlap,
			outmemorysize / 1048576.0, outmemorysize);
	}

	if ((paddinginx || paddinginy) && spp != numberpaddingvalues) {
		fprintf(stderr, "File \"%s\": number of padding values"
			" (%u) does not match number of samples per"
			" pixel (%u).\n",
			infilename, numberpaddingvalues, spp);
		free(outbuf);
		return EXIT_SYNTAX_ERROR;
	}

	uint16_t bytesperpixel= (bitspersample + 7) / 8;
	uint8_t paddingbytes[bytesperpixel * spp];
	uint16_t s;
	for (s = 0 ; s < spp ; s++) {
		/* Here we should use sampleformat; instead, we assume 
		  unsigned integer format uint8_t or uint16_t or... (rather 
		  than e.g. int16 or float) */
		switch (paddingvalues[s][0]) {
			case 'M': {
				uint16_t b;
				for (b = 0 ; b < bytesperpixel ; b++)
					paddingbytes[s * bytesperpixel + b]=
					    0xff;
				break;
			}
			default: {
				unsigned long long u= strtoull(
				    paddingvalues[s], NULL, 10);
				uint16_t b;
				/* Here we should care for endianness ;
				we write paddingbytes bigendianly */
				for (b = 0 ; b < bytesperpixel ; b++) {
					paddingbytes[
					    (s + 1) * bytesperpixel - 1 - b]=
						u & 0xff;
					u >>= 8;
				}
			}
                }
        }

	/*for (s = 0 ; s < spp ; s++) {
		fprintf(stderr, "[sample %u] %s", s, paddingvalues[s]);
		uint16_t b;
		for (b = 0 ; b < bytesperpixel ; b++)
			fprintf(stderr, " %d", paddingbytes[s * bytesperpixel + b]);
		fprintf(stderr, "\n");
	}*/

	prefix = searchPrefixBeforeLastDot(infilename);

	ndigitshtilenumber = searchNumberOfDigits(hnpieces);
	ndigitsvtilenumber = searchNumberOfDigits(vnpieces);
        /* Loop over x, loop over y in that order, so that, when in is 
         * not tiled, TIFFReadScanline calls are done sequentially from 
         * 0 to H-1 then 0 to H-1 then... Otherwise (0 to h-1 then 0 to 
         * h-1 then h to 2*h-1 then... with h<H), reading fails. */
	for (x = 0 ; x < inimagewidth ; x += outwidth) {
		uint32_t outwidthwithoverlap;
		uint32_t y_of_last_read_scanline= 0;

		uint32_t leftoverlap = x < hoverlap ? x : hoverlap;
		uint32_t xwithleftoverlap = x - leftoverlap;

		uint32_t outwidthwithrightoverlap = outwidth + hoverlap;
		uint32_t amountofpaddingatright= 0;
		uint32_t xrightboundary = x + outwidthwithrightoverlap;
		    /* equal to xwithleftoverlap + outwidth + 2*hoverlap */
		assert(xrightboundary >= x); /* detect overflows */
		/* At this point the computed right boundary of the
		 * piece may lie outside the input image for three
		 * (mutually nonexclusive) reasons:
		 * - The user requested a specific piece width which is
		 * not a divisor of the input image width, and this
		 * piece is the last piece in the row: x + outwidth >
		 * inimagewidth. This piece must have a smaller width:
		 * outwidth must be chosen as inimagewidth-x.
		 * - The user requested an overlap that is larger than
		 * the distance between the right boundary of the piece
		 * without overlap and the right boundary of the input
		 * image: x + outwidth + hoverlap > inimagewidth. The
		 * right overlap must be restricted to
		 * inimagewidth-x-outwidth.
		 * - We use padding in x to achieve a constraint (e.g. 
		 * piece width is a multiple of some integer number). This 
		 * piece must not have a smaller width and we should 
		 * complete it with padding values.
		 * In cases 1 and 2, xrightboundary > inimagewidth and
		 * outwidthwithrightoverlap must be taken equal to
		 * inimagewidth-x; otherwise, it can be taken equal to
		 * outwidth+hoverlap. */
		if (xrightboundary > inimagewidth) {
			if (paddinginx)
				amountofpaddingatright=
				    xrightboundary - inimagewidth;
			else
				outwidthwithrightoverlap = inimagewidth - x;
		}
		outwidthwithoverlap = leftoverlap + outwidthwithrightoverlap;

		assert(xwithleftoverlap < inimagewidth); /* xwlol would be < 0 */
		assert(xwithleftoverlap + outwidthwithoverlap <=
			inimagewidth + amountofpaddingatright);

		for (y = 0 ; y < inimagelength ; y += outlength) {
			char * outfilename;
			void * out; /* TIFF* or FILE* */
			uint32_t outlengthwithoverlap;
			int error = 0;

			uint32_t topoverlap = y < voverlap ? y : voverlap;
			uint32_t ywithtopoverlap = y - topoverlap;

			uint32_t outlengthwithbottomoverlap= outlength + voverlap;
			uint32_t amountofpaddingatbottom= 0;
			uint32_t ybottomboundary = y + outlengthwithbottomoverlap;
			assert(ybottomboundary >= y); /* detect overflows */
			if (ybottomboundary > inimagelength) {
				if (paddinginy)
					amountofpaddingatbottom =
					    ybottomboundary - inimagelength;
				else
					outlengthwithbottomoverlap =
					    inimagelength - y;
			}
			outlengthwithoverlap =
				topoverlap + outlengthwithbottomoverlap;

			assert(ywithtopoverlap < inimagelength);
			 /* ywtol would be < 0 */
			assert(ywithtopoverlap + outlengthwithoverlap <=
				inimagelength + amountofpaddingatbottom);

			if (dryrun)
				continue;

			my_asprintf(&outfilename, "%s_i%0*uj%0*u%s",
			    prefix, ndigitsvtilenumber,
			    y/outlength+1, ndigitshtilenumber,
			    x/outwidth+1,
			    output_JPEG_files ? JPEG_SUFFIX : TIFF_SUFFIX);

			out = output_JPEG_files ?
			    (void *) fopen(outfilename, "wb") :
			    (void *) TIFFOpen(outfilename,
				TIFFIsBigEndian(in)?"wb":"wl");
			if (verbose) {
				if (out == NULL)
					fprintf(stderr, "Error: unable"
						" to open output file"
						" \"%s\".\n",
						outfilename);
				else
					fprintf(stderr, "Output file "
						"\"%s\" open. Will "
						"write piece of size "
						UINT32_FORMAT " x "
						UINT32_FORMAT ".\n",
						outfilename,
						outwidthwithoverlap,
						outlengthwithoverlap);
			}
			_TIFFfree(outfilename);
			if (out == NULL)
				continue;

			if (quality <= 0) {
				uint16_t in_compression;

				TIFFGetField(in, TIFFTAG_COMPRESSION, &in_compression);
				if (in_compression == COMPRESSION_JPEG) {
					uint16_t in_jpegquality;
					TIFFGetField(in,
					    TIFFTAG_JPEGQUALITY,
					    &in_jpegquality);
					quality = in_jpegquality;
				} else
					quality = default_quality;
			}

			if (output_JPEG_files) {
				struct jpeg_compress_struct cinfo;
				struct jpeg_error_mgr jerr;

				cinfo.err = jpeg_std_error(&jerr);
				jpeg_create_compress(&cinfo);
				jpeg_stdio_dest(&cinfo, out);
				cinfo.image_width = outwidthwithoverlap;
				cinfo.image_height = outlengthwithoverlap;
				cinfo.input_components = spp; /* # of
					color components per pixel */
				cinfo.in_color_space = JCS_RGB; /* colorspace
					of input image */
				jpeg_set_defaults(&cinfo);
				if (verbose)
					fprintf(stderr, "Quality of produced JPEG will be %d.\n",
						quality);
				jpeg_set_quality(&cinfo, quality,
				    TRUE /* limit to baseline-JPEG values */);
				jpeg_start_compress(&cinfo, TRUE);

				if (((TIFFIsTiled(in) &&
				      !(error = cpTiles2Strip(in,
					    &cinfo, 1,
					    xwithleftoverlap,
					    ywithtopoverlap,
					    outwidthwithoverlap,
					    outlengthwithoverlap,
					    amountofpaddingatright,
					    amountofpaddingatbottom,
					    paddingbytes,
					    inimagewidth, inimagelength,
					    outbuf))) ||
				     (!TIFFIsTiled(in) &&
				      !(error = cpStrips2Strip(in, 
					    &cinfo, 1,
					    xwithleftoverlap,
					    ywithtopoverlap,
					    outwidthwithoverlap,
					    outlengthwithoverlap,
					    amountofpaddingatright,
					    amountofpaddingatbottom,
					    outbuf,
					    &y_of_last_read_scanline,
					    inimagelength)))))
					if (verbose)
						fprintf(stderr,
							"Piece written.\n");

				jpeg_finish_compress(&cinfo);
				fclose(out);
				jpeg_destroy_compress(&cinfo);
			} else {
				tiffCopyFieldsButDimensions(in, out);
				TIFFSetField(out, TIFFTAG_IMAGEWIDTH,
					outwidthwithoverlap);
				TIFFSetField(out, TIFFTAG_IMAGELENGTH,
					outlengthwithoverlap);
				TIFFSetField(out, TIFFTAG_ROWSPERSTRIP,
					outlengthwithoverlap);

				if ((TIFFIsTiled(in) &&
				      !(error = cpTiles2Strip(in, out,
					0, xwithleftoverlap,
					ywithtopoverlap,
					outwidthwithoverlap,
					outlengthwithoverlap,
					amountofpaddingatright,
					amountofpaddingatbottom,
					paddingbytes,
					inimagewidth, inimagelength,
					outbuf))) ||
				     (!TIFFIsTiled(in) &&
				      !(error = cpStrips2Strip(in, out,
					0, xwithleftoverlap,
					ywithtopoverlap,
					outwidthwithoverlap,
					outlengthwithoverlap,
					amountofpaddingatright,
					amountofpaddingatbottom,
					outbuf,
					&y_of_last_read_scanline,
					inimagelength))))
					if (verbose)
						fprintf(stderr, "Piece written"
							" to output file "
							"\"%s\".\n",
						TIFFFileName(out));

				TIFFClose(out);
			}
			if (error && !return_code) /* error code = 1st error */
				return_code = error;
		} /* for y */
	} /* for x */

	_TIFFfree(outbuf);
	_TIFFfree(prefix);
	return return_code;
}


static void
usage()
{
	fprintf(stderr, "tiffmakemosaic v" PACKAGE_VERSION " license GNU GPL v3 (c) 2012-2021 Christophe Deroulers\n\n");
	fprintf(stderr, "Quote \"Deroulers et al., Diagnostic Pathology 2013, 8:92\" in your production\n       http://doi.org/10.1186/1746-1596-8-92\n\n");
	fprintf(stderr, "Usage: tiffmakemosaic [options] file1.tif [file2.tif...]\n");
	fprintf(stderr, " Produces a mosaic if needed, for each of the given files. A mosaic is a set of one-strip TIFF files of equal size, each of which has bounded size, which reproduce the original file if reassembled contiguously. Options:\n");
	fprintf(stderr, " -v                verbose monitoring\n");
	fprintf(stderr, " -y                dry run (do not write output file(s))\n");
	fprintf(stderr, " -T                report TIFF errors/warnings on stderr (no dialog boxes)\n");
	fprintf(stderr, " -M <size in MiB>  max. memory req. of each piece of the mosaic (default 1024);\n");
	fprintf(stderr, "                   0 for no limit\n");
	fprintf(stderr, " -m [mw]x[mh]      width resp. height in pixels should be multiples of mw / mh\n");
	fprintf(stderr, " -g [w]x[h]        width and height in pixels of each piece (overrides -M\n");
	fprintf(stderr, "                   and/or -m if both width and height are given; 0 or no value\n");
	fprintf(stderr, "                   for either dimension means default; default are largest\n");
	fprintf(stderr, "                   dimensions that satisfy -M and/or -m, divide the full image\n");
	fprintf(stderr, "                   in equal pieces by powers of 2, and are close to each other)\n");
	fprintf(stderr, " -O <overlap>[%%]   overlap of adjacent pieces in pixels [or percent] (default 0)\n");
	fprintf(stderr, " -P[X][Y] #[,#...] pad image if necessary in direction x and/or y (default:\n");
	fprintf(stderr, "                   both) to satisfy -M, -m or -g requirements (e.g. so that\n");
	fprintf(stderr, "                   width is a multiple of larger a power of 2), adding to the\n");
	fprintf(stderr, "                   right and/or to the bottom pixels of value # (if 1\n");
	fprintf(stderr, "                   sample/pixel) or #,# (if 2 samples per pixels), and so on;\n");
	fprintf(stderr, "                   M for # means maximum possible value (e.g. 255 for 8-bit)\n");
	fprintf(stderr, " -j[#]             output JPEG files (with quality #, 0-100, default 75)\n");
	fprintf(stderr, " -c none[:opts]    output TIFF files with no compression \n");
	fprintf(stderr, " -c x[:opts]       output TIFF compressed with encoding x (jpeg, lzw, zip, ...)\n");
	fprintf(stderr, "Default output is TIFF with same compression as input.\n\n");
	fprintf(stderr, "JPEG-compressed TIFF options:\n");
	fprintf(stderr, " #   set compression quality level (0-100, default 75)\n");
/*	fprintf(stderr, " r  output color image as RGB rather than YCbCr\n");*/
	fprintf(stderr, "LZW, Deflate (ZIP) and LZMA2 options:\n");
	fprintf(stderr, " #   set predictor value\n");
	fprintf(stderr, " p#  set compression level (preset)\n");
	fprintf(stderr, "For example, -c lzw:2 to get LZW-encoded data with horizontal differencing,\n");
	fprintf(stderr, "-c zip:3:p9 for Deflate encoding with maximum compression level and floating\n");
	fprintf(stderr, "point predictor, -c jpeg:r:50 for JPEG-encoded RGB data at quality 50%%.\n");
}


static int
processZIPOptions(const char* cp)
{
	if ( (cp = strchr(cp, ':')) ) {
		do {
			cp++;
			if ((*cp) >= '0' && (*cp) <= '9')
				defpredictor = atoi(cp);
			else if (*cp == 'p')
				defpreset = atoi(++cp);
			else
				return 0;
		} while( (cp = strchr(cp, ':')) );
	}
	return 1;
}


static int
processG3Options(const char* cp)
{
	if( (cp = strchr(cp, ':')) ) {
		if (defg3opts == (uint32_t) -1)
			defg3opts = 0;
		do {
			cp++;
			if (strncasecmp(cp, "1d", 2) != 0)
				defg3opts &= ~GROUP3OPT_2DENCODING;
			else if (strncasecmp(cp, "2d", 2) != 0)
				defg3opts |= GROUP3OPT_2DENCODING;
			else if (strncasecmp(cp, "fill", 4) != 0)
				defg3opts |= GROUP3OPT_FILLBITS;
			else
				return 0;
		} while( (cp = strchr(cp, ':')) );
	}
	return 1;
}


static int
processCompressOptions(const char* opt)
{
	if (strncasecmp(opt, "none", 4) == 0) {
		/*char * cp = strchr(opt, ':');*/

		defcompression = COMPRESSION_NONE;
		/*while (cp) {
			if (cp[1] == 'r' )
				defphotometric = PHOTOMETRIC_RGB;
			else if (cp[1] == 'y' )
				defphotometric = PHOTOMETRIC_YCBCR;
			else
				return 0;

			cp = strchr(cp+1,':');
		}*/
	}
	else if (strcasecmp(opt, "packbits") == 0)
		defcompression = COMPRESSION_PACKBITS;
	else if (strncasecmp(opt, "jpeg", 4) == 0) {
		char* cp = strchr(opt, ':');

		defcompression = COMPRESSION_JPEG;
		while( cp ) {
			if (cp[1] >= '0' && cp[1] <= '9') {
				unsigned long u;
				u = strtoul(cp+1, NULL, 10);
				if (u == 0 || u > 100)
					return 0;
				quality = (int) u;
			}
			/*else if (cp[1] == 'r' )
				jpegcolormode = JPEGCOLORMODE_RAW;*/
			else
				return 0;

			cp = strchr(cp+1,':');
		}
	} else if (strncasecmp(opt, "g3", 2) == 0) {
		if (!processG3Options(opt))
			return 0;
		defcompression = COMPRESSION_CCITTFAX3;
	} else if (strcasecmp(opt, "g4") == 0) {
		defcompression = COMPRESSION_CCITTFAX4;
	} else if (strncasecmp(opt, "lzw", 3) == 0) {
		char* cp = strchr(opt, ':');
		if (cp)
			defpredictor = atoi(cp+1);
		defcompression = COMPRESSION_LZW;
	} else if (strncasecmp(opt, "zip", 3) == 0) {
		if (!processZIPOptions(opt))
			return 0;
		defcompression = COMPRESSION_ADOBE_DEFLATE;
	}/* else if (strncasecmp(opt, "lzma", 4) == 0) {
		if (!processZIPOptions(opt) )
			return 0;
		defcompression = COMPRESSION_LZMA;
	}*/ else if (strncasecmp(opt, "jbig", 4) == 0) {
		defcompression = COMPRESSION_JBIG;
	} else if (strncasecmp(opt, "sgilog", 6) == 0) {
		defcompression = COMPRESSION_SGILOG;
#ifdef HAVE_ZSTD
  } else if (strncasecmp(opt, "zstd", 4) == 0) {
		if (!processZIPOptions(opt))
			return 0;
    defcompression = COMPRESSION_ZSTD;
#endif
#ifdef HAVE_WEBP
  } else if (strncasecmp(opt, "webp", 4) == 0) {
		if (!processZIPOptions(opt))
			return 0;
    defcompression = COMPRESSION_WEBP;
#endif
	} else
		return (0);

	return (1);
}


static int
processPieceGeometryOptions(char* cp, uint32_t* width, uint32_t* length)
{
	while (*cp == ' ')
		cp++;
	if (*cp != 'x' && *cp != 'X') {
		uint32_t u= strtoull(cp, &cp, 10);
		if (errno)
			return 0;
		*width = u;
	}
	while (*cp == ' ')
		cp++;
	if (*cp != 'x' && *cp != 'X')
		return 0;
	cp++;
	while (*cp == ' ')
		cp++;
	if (*cp != 0) {
		uint32_t u= strtoull(cp, &cp, 10);
		if (errno)
			return 0;
		*length = u;
	}
	return 1;
}


static int
processPaddingValuesOption(char * cp)
{
	char * cp2 = cp;

	/* first pass to count the values */
	numberpaddingvalues= 0;
	while (*cp == ' ')
		cp++;
	if (*cp != 'M' && (*cp < '0' || *cp > '9')) {
		fprintf(stderr, "Incorrect padding value(s) argument to"
		    " option -p: %s\n", cp);
		return 0;
	}
	numberpaddingvalues= 1;
	while (*cp != 0) {
		if (*cp == ',') {
			(numberpaddingvalues)++;
			if (numberpaddingvalues == 0) /* overflow */
				return 0;
		}
		cp++;
	}

	if ((paddingvalues =
	    _TIFFmalloc(numberpaddingvalues * sizeof(*paddingvalues))) == NULL) {
		perror("Insufficient memory for padding values ");
		exit(EXIT_INSUFFICIENT_MEMORY);
	}

	/* second pass to store the values */
	numberpaddingvalues= 0;
	cp = cp2;
	while (*cp != 0) {
		while (*cp == ' ')
			cp++;
		cp2 = cp;
		switch (*cp) {
		        case 'M': cp++; break;
		        default:
		                strtoull(cp, &cp, 10);
                		if (errno) {
					fprintf(stderr, "Incorrect padding value in argument to option -p:"
					    "%s\n", cp);
                			return 0;
                		}
                }
		paddingvalues[numberpaddingvalues] = cp2;
		numberpaddingvalues++;
		while (*cp != 0 && *cp == ' ')
			cp++;
		if (*cp != 0) {
			if (*cp != ',') {
				fprintf(stderr, "Unexpected char after padding value in argument to option -p:"
				    "%s\n", cp);
				return 0;
			}
			cp++;
		}
	}
	return 1;
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
	if (!verbose)
		return;
	if (module != NULL)
		fprintf(stderr, "%s: ", module);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ".\n");
}

int main(int argc, char * argv[])
{
	int arg = 1;
	int errorcode = 0;

	while (arg < argc && argv[arg][0] == '-') {

		if (argv[arg][1] == 'v')
			verbose = 1;
		else if (argv[arg][1] == 'y')
			dryrun++;
		else if (argv[arg][1] == 'T') {
			TIFFSetErrorHandler(stderrErrorHandler);
			TIFFSetWarningHandler(stderrWarningHandler);
			}
		else if (argv[arg][1] == 'M') {
			long double ld;
			uint64_t ull;
			tsize_t newmosaicpiecesize;

			if (arg+1 >= argc) {
				usage();
				return EXIT_SYNTAX_ERROR;
			}

			arg++;
			ld= strtold(argv[arg], NULL);

			if (errno || !isfinite(ld) || ld < 0) {
				usage();
				return EXIT_SYNTAX_ERROR;
			}

			ull = (uint64_t) floorl(ld * 1048576);
			if (ld == 0) {
				ull = 0;
			} else if (ull == 0)
				ull= 1;
			newmosaicpiecesize = ull;
			if ((uint64_t) newmosaicpiecesize != ull) {
				/* overflow of tsize_t */
				if (verbose)
					fprintf(stderr, "Maximum piece "
						"memory requirement "
						"%Lf MiB too large "
						"for this computer. "
						"Defaulting to %.3f "
						"MiB.\n",
						ld, mosaicpiecesize /
						    1048576.0);
			} else
				mosaicpiecesize = newmosaicpiecesize;
		} else if (argv[arg][1] == 'm') {
			if (arg+1 >= argc ||
			    !processPieceGeometryOptions(argv[arg+1],
				&requestedpiecewidthdivisor,
				&requestedpiecelengthdivisor)) {
				fprintf(stderr, "Syntax error in the "
					"specification of divisors "
					"(option -m).\n");
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			arg++;
		} else if (argv[arg][1] == 'g') {
			if (arg+1 >= argc ||
			    !processPieceGeometryOptions(argv[arg+1],
				&requestedpiecewidth, &requestedpiecelength)) {
				fprintf(stderr, "Syntax error in the "
					"piece geometry specification "
					"(option -g).\n");
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			arg++;
		} else if (argv[arg][1] == 'P') {
			size_t arglength;
			int i = 2;

			while (argv[arg][i] != 0) {
				switch (argv[arg][i]) {
					case 'X':
						paddinginx= -1; break;
					case 'Y':
						paddinginx= -1; break;
					default:
						fprintf(stderr, "Expected x or y for direction of padding after -p, got \"%s\"\n",
						    &(argv[arg][2]));
						usage();
						return EXIT_SYNTAX_ERROR;
					}
				i++;
			}
			if (paddinginx == 0 && paddinginy == 0) {
		  		paddinginx= -1;
				paddinginy= -1;
			}

			if (arg+1 >= argc ||
			    !processPaddingValuesOption(argv[arg+1])) {
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			arg++;
		} else if (argv[arg][1] == 'O') {
			size_t arglength;

			if (arg+1 >= argc ||
                            (arglength= strlen(argv[arg+1])) == 0) {
				usage();
				return EXIT_SYNTAX_ERROR;
			}

			arg++;
			if (argv[arg][arglength-1] == '%') {
				long double ld= strtold(argv[arg], NULL);

				if (errno || !isfinite(ld) ||
				    ld < 0 || ld > 100) {
					usage();
					return EXIT_SYNTAX_ERROR;
				}
				overlapinpercent= ld;
				overlapinpixels= 0;
			} else {
				uint32_t u= strtoull(argv[arg], NULL, 10);

				if (errno) {
					usage();
					return EXIT_SYNTAX_ERROR;
				}

				overlapinpixels= u;
				overlapinpercent= 0;
			}
		} else if (argv[arg][1] == 'c') {
			output_JPEG_files = 0;
			if (arg+1 >= argc ||
			    !processCompressOptions(argv[arg+1])) {
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			arg++;
		} else if (argv[arg][1] == 'j') {
			output_JPEG_files = 1;

			if (argv[arg][2] != 0) {
				unsigned long u;

				u= strtoul(&(argv[arg][2]), NULL, 10);
				if (u == 0 || u > 100) {
					fprintf(stderr, "Expected optional non-null integer percent number after -j, got \"%s\"\n",
					    &(argv[arg][2]));
					usage();
					return EXIT_SYNTAX_ERROR;
				}
				quality = (int) u;
			}
		} else {
			fprintf(stderr, "Unknown option \"%s\"\n", argv[arg]);
			usage();
			return EXIT_SYNTAX_ERROR;
		}

	arg++;
	}

	if (argc < arg+1) {
		usage();
		return EXIT_SYNTAX_ERROR;
	}

	if (verbose) {
		if (dryrun)
			fprintf(stderr, "Dry run --- simulate only, compute"
				" tile dimensions but don't write files.\n");
		if (mosaicpiecesize)
			fprintf(stderr, "Maximum memory requirement of each "
			"produced piece: %.3f MiB (" UINT64_FORMAT
			" bytes).\n",
			mosaicpiecesize / 1048576.0,
			mosaicpiecesize);
		else
			fprintf(stderr, "No memory requirement limit "
				"on produced pieces.\n");
	}

	for (; arg < argc ; arg++) {
		int r= makeMosaicFromTIFFFile(argv[arg]);
		if (r && !errorcode) /* Code indicates 1st error */
			errorcode = r;
	}

	return errorcode;
}
