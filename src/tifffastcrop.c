/* tifffastcrop

 v. 1.4.1

 Copyright (c) 2013-2021 Christophe Deroulers

 Portions are based on libtiff's tiffcp code. tiffcp is:
 Copyright (c) 1988-1997 Sam Leffler
 Copyright (c) 1991-1997 Silicon Graphics, Inc.

 Distributed under the GNU General Public License v3 -- contact the
 author for commercial use */

/* TODO: fix option "-c jpeg:r" -- presently, it is deactivated */

#include <stdio.h>
#include <stdlib.h> /* exit, strtoul */
#include <string.h>
#include <strings.h> /* strncasecmp */
#include <ctype.h> /* isdigit */
#include <assert.h>
#include <errno.h>
#include <tiff.h>
#include <tiffio.h>
#include <jpeglib.h>
#include <math.h> /* lroundl */

#include "config.h"

#ifdef HAVE_PNG
 #include <png.h>
# ifndef HAVE_PNG_CONST_BYTEP
 typedef png_byte * png_const_bytep;
# endif
#endif

#define JPEG_MAX_DIMENSION 65500L /* in libjpeg's jmorecfg.h */

#define EXIT_SYNTAX_ERROR         1
#define EXIT_IO_ERROR             2
#define EXIT_UNHANDLED_INPUT_IMAGE_TYPE 3
#define EXIT_INSUFFICIENT_MEMORY  4
#define EXIT_UNABLE_TO_ACHIEVE_TILE_DIMENSIONS 5
#define EXIT_GEOMETRY_ERROR	  6
#define EXIT_UNHANDLED_OUTPUT_FILE_TYPE 7

#define CopyField(tag, v) \
    if (TIFFGetField(in, tag, &v)) TIFFSetField(TIFFout, tag, v)
#define CopyField2(tag, v1, v2) \
    if (TIFFGetField(in, tag, &v1, &v2)) TIFFSetField(TIFFout, tag, v1, v2)
#define CopyField3(tag, v1, v2, v3) \
    if (TIFFGetField(in, tag, &v1, &v2, &v3)) TIFFSetField(TIFFout, tag, v1, v2, v3)

static uint32_t requestedxmin = 0;
static uint32_t requestedymin = 0;
static uint32_t requestedwidth = 0;
static uint32_t requestedlength = 0;
static uint64_t diroff = 0;
static uint16_t number_of_dirnum_ranges = 0;
static uint16_t * dirnum_ranges_starts = NULL;
static uint16_t * dirnum_ranges_ends = NULL;
static int verbose = 0;

#define OUTPUT_FORMAT_TIFF 0
#define OUTPUT_FORMAT_JPEG 1
#define OUTPUT_FORMAT_PNG  2
static int output_format = -1;
static const char TIFF_SUFFIX[] = "tif";
static const char JPEG_SUFFIX[] = "jpg";
static const char PNG_SUFFIX[] = "png";
static const char * OUTPUT_SUFFIX[]= {TIFF_SUFFIX, JPEG_SUFFIX, PNG_SUFFIX};

static int big_tiff = 0;
static uint32_t defg3opts = (uint32_t)-1;
static int jpeg_quality = -1, default_jpeg_quality = 75; /* JPEG quality */
static int png_quality = -1, default_png_quality = 6; /* PNG quality */
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
	p= malloc(n+1);
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

	if ((prefix = malloc(l+1)) == NULL) {
		perror("Insufficient memory for a character string ");
		exit(EXIT_INSUFFICIENT_MEMORY);
	}

	strncpy(prefix, path, l);
	prefix[l]= 0;

	return prefix;
}


static const char * searchSuffix(const char * path)
{
	int l= strlen(path)-1;

	while (l >= 0 && path[l] != '.')
		l--;

	if (l < 0)
		l= strlen(path);
	else
		l++;

	return &(path[l]);
}


static int shouldBeHandled(uint16_t dirnum)
{
	if (number_of_dirnum_ranges == 0)
		return 1;

	uint16_t rn;
	for (rn= 0 ; rn < number_of_dirnum_ranges ; rn++)
		if (dirnum_ranges_starts[rn] <= dirnum &&
		    dirnum_ranges_ends[rn] >= dirnum)
			return 1;

	return 0;
}


static void tiffCopyFieldsButDimensions(TIFF* in, TIFF* TIFFout)
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
/*	CopyField(TIFFTAG_GROUP3OPTIONS, longv);
	CopyField(TIFFTAG_GROUP4OPTIONS, longv);*/
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
	CopyField(TIFFTAG_FAXDCS, stringv);
}


static tsize_t computeWidthInBytes(uint32_t width_in_pixels, uint16_t bitsperpixel)
{
	if (bitsperpixel % 8 == 0)
		return (tsize_t) width_in_pixels * (bitsperpixel/8);
	else if (8 % bitsperpixel == 0) {
		uint32_t pixelsperbyte = 8/bitsperpixel;
		return (tsize_t) ((width_in_pixels + pixelsperbyte - 1)/
			pixelsperbyte);
	} else
		return 0;
}


static void cpBufToBuf(uint8_t* out_beginningofline, uint32_t out_x,
	uint8_t* in_beginningofline, uint32_t in_x,
	uint32_t widthtocopyinsamples, uint16_t bitspersample,
	uint32_t rows, int out_linewidthinbytes, int in_linewidthinbytes)
{
	if (bitspersample % 8 == 0) { /* Easy case */
		uint8_t* in  = in_beginningofline  + in_x  * (bitspersample/8);
		uint8_t* out = out_beginningofline + out_x * (bitspersample/8);
		while (rows-- > 0) {
			memcpy(out, in, widthtocopyinsamples * (bitspersample/8));
			in += in_linewidthinbytes;
			out += out_linewidthinbytes;
		}
		return;
	}

	/* Hard case. Do computations to prepare steps 1, 2, 3: */
	static const uint8_t left_masks[] =
	    { 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

	assert(8 % bitspersample == 0);
	int samplesperbyte = 8/bitspersample;
	int out_x_start_whole_outbytes = out_x;
	int in_x_start_whole_outbytes  = in_x;
	uint32_t in_skew  = in_linewidthinbytes;
	uint32_t out_skew = out_linewidthinbytes;

	 /* 1. Copy samples to complete the first byte (if incomplete) of dest. */
	int out_startpositioninincompletefirstbyte = out_x % samplesperbyte;
	int in_startpositioninfirstbyte = in_x % samplesperbyte;
	int out_samplesinincompletefirstbyte = 0;
	int shift_first_inbyte_to_first_incompl_outbyte =
		(out_startpositioninincompletefirstbyte -
		in_startpositioninfirstbyte) * bitspersample;
	int shift_second_inbyte_to_first_incompl_outbyte = 0;
	uint8_t mask_of_first_inbyte_to_first_incompl_outbyte = 0;
	uint8_t mask_of_second_inbyte_to_first_incompl_outbyte = 0;

	if (out_startpositioninincompletefirstbyte) {
		out_samplesinincompletefirstbyte =
		    samplesperbyte - out_startpositioninincompletefirstbyte;
		if (out_samplesinincompletefirstbyte > widthtocopyinsamples)
			out_samplesinincompletefirstbyte = widthtocopyinsamples;
		int samples_available_in_first_inbyte =
		    samplesperbyte - in_startpositioninfirstbyte;

		if (samples_available_in_first_inbyte >= out_samplesinincompletefirstbyte) {
			mask_of_first_inbyte_to_first_incompl_outbyte =
			    left_masks[out_samplesinincompletefirstbyte * bitspersample] >>
				(in_startpositioninfirstbyte * bitspersample);
		} else {
			mask_of_first_inbyte_to_first_incompl_outbyte =
			    left_masks[samples_available_in_first_inbyte *
					bitspersample] >>
				(in_startpositioninfirstbyte * bitspersample);
			mask_of_second_inbyte_to_first_incompl_outbyte =
			    left_masks[(out_samplesinincompletefirstbyte -
				samples_available_in_first_inbyte) * bitspersample];
			shift_second_inbyte_to_first_incompl_outbyte =
			    samples_available_in_first_inbyte * bitspersample;
			in_skew--;
		}

		in_x_start_whole_outbytes += out_samplesinincompletefirstbyte;
		out_x_start_whole_outbytes += out_samplesinincompletefirstbyte;
	}

	/* 2. Write as many whole bytes as possible in dest. */
	/* Examples of bits in in_bytes:
	  6+2|8|1+7 -> (6+)2|6+2|6(+2)  11 bits -> 2 bytes, 1 whole byte
	  6+2|8|7+1 -> (6+)2|6+2|6+2    17 bits -> 3 bytes, 2 whole bytes
	   Strategy: copy whole bytes. Then make an additional,
	  incomplete byte (which shall have
	  out_samplesinincompletelastbyte samples) with the remaining (not
	  yet copied) bits of current byte in input and, if these bits
	  are not enough, the first in_bitstoreadinlastbyte bits of next
	  byte in input. */
	uint8_t* out_wholeoutbytes = out_beginningofline +
		out_x_start_whole_outbytes / samplesperbyte;
	uint8_t* in = in_beginningofline + in_x / samplesperbyte;
	uint32_t wholebytesperline =
	    ((widthtocopyinsamples-out_samplesinincompletefirstbyte) *
		bitspersample) / 8;
	int in_bitoffset = (in_x_start_whole_outbytes % samplesperbyte) * bitspersample;
	if (in_bitoffset) {
		in_skew -= wholebytesperline + 1;
		out_skew -= wholebytesperline;
	}

	/* 3. Copy samples to complete the last byte (if incomplete) of dest. */
	int out_samplesinincompletelastbyte =
		(out_x + widthtocopyinsamples) % samplesperbyte;
	if (out_samplesinincompletelastbyte) {
		if (in_bitoffset == 0)
			wholebytesperline++; /* Let memcpy start writing last byte */
		else
			out_skew--;
	}
	int in_bitstoreadinlastbyte =
	    out_samplesinincompletelastbyte * bitspersample - (8-in_bitoffset);
	if (in_bitstoreadinlastbyte < 0)
		in_bitstoreadinlastbyte = 0;

	/* Perform steps 1, 2, 3: */
	while (rows-- > 0) {
		/* 1. */
		if (out_startpositioninincompletefirstbyte) {
			uint8_t a = (*in) & mask_of_first_inbyte_to_first_incompl_outbyte;
			if (shift_first_inbyte_to_first_incompl_outbyte >= 0)
				a >>= shift_first_inbyte_to_first_incompl_outbyte;
			else
				a <<= -shift_first_inbyte_to_first_incompl_outbyte;
			if (shift_second_inbyte_to_first_incompl_outbyte) {
				in++;
				a |= ((*in) & mask_of_second_inbyte_to_first_incompl_outbyte)
				    >> shift_second_inbyte_to_first_incompl_outbyte;
			}

			*(out_wholeoutbytes-1) &=
			    left_masks[
				out_startpositioninincompletefirstbyte *
				bitspersample];
			*(out_wholeoutbytes-1) |= a;
		}

		/* 2. & 3. */
		if (in_bitoffset == 0) {
			memcpy(out_wholeoutbytes, in, wholebytesperline);
			/* out_samplesinincompletelastbyte =
			 in_bitstoreadinlastbyte / bitspersample in this case */
			if (in_bitstoreadinlastbyte)
				*(out_wholeoutbytes + wholebytesperline) |=
				    (*(in+wholebytesperline))
				    >> (8-in_bitstoreadinlastbyte);
		} else {
			uint32_t j = wholebytesperline;
			uint8_t acc = (*in++) << in_bitoffset;

			while (j-- > 0) {
				acc |= (*in) >> (8-in_bitoffset);
				*out_wholeoutbytes++ = acc;
				acc = (*in++) << in_bitoffset;
			}
			if (out_samplesinincompletelastbyte) {
				if (in_bitstoreadinlastbyte)
					acc |= (*in) >> (8-in_bitstoreadinlastbyte);
				*out_wholeoutbytes++ = acc;
			}
		}

		in += in_skew;
		out_wholeoutbytes += out_skew;
	}
}


static int testAndFixInTIFFPhotoAndCompressionParameters(TIFF* in,
	uint16_t* input_compression)
{
	uint16_t bitspersample, input_photometric;

	TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);

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
		/* Force conversion to RGB */
		TIFFSetField(in, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
	} else if (input_photometric == PHOTOMETRIC_YCBCR) {
		/* Otherwise, can't handle subsampled input */
		uint16_t subsamplinghor, subsamplingver;

		TIFFGetFieldDefaulted(in, TIFFTAG_YCBCRSUBSAMPLING,
				      &subsamplinghor, &subsamplingver);
		if (subsamplinghor!=1 || subsamplingver!=1) {
			fprintf(stderr, "%s: Can't deal with subsampled image.\n",
				TIFFFileName(in));
			return 0;
		}
	}

	return 1;
}


static int testAndFixOutTIFFPhotoAndCompressionParameters(TIFF* in,
	TIFF* TIFFout)
{
	uint16_t input_photometric, input_compression, compression, spp;

	TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &spp);
	TIFFGetFieldDefaulted(in, TIFFTAG_COMPRESSION, &input_compression);
	TIFFGetFieldDefaulted(in, TIFFTAG_PHOTOMETRIC, &input_photometric);

	if (defcompression != (uint16_t) -1)
		TIFFSetField(TIFFout, TIFFTAG_COMPRESSION, defcompression);

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
			jpeg_quality);
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
					TIFFTAG_ZIPQUALITY, defpreset);
			/*else if (compression == COMPRESSION_LZMA)
				TIFFSetField(TIFFout,
					TIFFTAG_LZMAPRESET,
					defpreset);*/
		}
		if (input_compression == COMPRESSION_JPEG &&
		    input_photometric == PHOTOMETRIC_YCBCR)
			TIFFSetField(TIFFout, TIFFTAG_PHOTOMETRIC,
				PHOTOMETRIC_RGB);
	} else if (compression == COMPRESSION_NONE) {
		if (input_compression == COMPRESSION_JPEG &&
		    input_photometric == PHOTOMETRIC_YCBCR)
			TIFFSetField(TIFFout, TIFFTAG_PHOTOMETRIC,
				PHOTOMETRIC_RGB);
	} else if (compression == COMPRESSION_CCITTFAX3 ||
		   compression == COMPRESSION_CCITTFAX4) {
		uint32_t longv;
		char *stringv;
		if (compression == COMPRESSION_CCITTFAX3 &&
		    input_compression == COMPRESSION_CCITTFAX3)
			{ CopyField(TIFFTAG_GROUP3OPTIONS, longv); }
		else if (compression == COMPRESSION_CCITTFAX4 &&
		    input_compression == COMPRESSION_CCITTFAX4)
			CopyField(TIFFTAG_GROUP4OPTIONS, longv);
		CopyField(TIFFTAG_BADFAXLINES, longv);
		CopyField(TIFFTAG_CLEANFAXDATA, longv);
		CopyField(TIFFTAG_CONSECUTIVEBADFAXLINES, longv);
		CopyField(TIFFTAG_FAXRECVPARAMS, longv);
		CopyField(TIFFTAG_FAXRECVTIME, longv);
		CopyField(TIFFTAG_FAXSUBADDRESS, stringv);
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

	return 1;
}


static int cpTiles2Strip(TIFF* in, uint32_t xmin, uint32_t ymin,
	uint32_t width, uint32_t length, unsigned char * outbuf,
	tsize_t outscanlinesizeinbytes, uint16_t bitspersample,
	uint16_t samplesperpixel)
{
	tmsize_t inbufsize;
	uint32_t intilewidth = (uint32_t) -1, intilelength = (uint32_t) -1;
	tsize_t intilewidthinbytes = TIFFTileRowSize(in);
	uint32_t y;
	unsigned char * inbuf, * bufp= outbuf;
	int error = 0;

	TIFFGetField(in, TIFFTAG_TILEWIDTH, &intilewidth);
	TIFFGetField(in, TIFFTAG_TILELENGTH, &intilelength);

	inbufsize= TIFFTileSize(in);
	inbuf = (unsigned char *)_TIFFmalloc(inbufsize); /* not malloc
	    because TIFFTileSize returns a tmsize_t */
	if (!inbuf) {
		TIFFError(TIFFFileName(in),
				"Error, can't allocate space for image buffer");
		return (EXIT_INSUFFICIENT_MEMORY);
	}

	for (y = ymin ; y < ymin + length + intilelength ; y += intilelength) {
		uint32_t x, out_x = 0;
		uint32_t yminoftile = (y/intilelength) * intilelength;
		uint32_t ymintocopy = ymin > yminoftile ? ymin : yminoftile;
		uint32_t ymaxplusone = yminoftile + intilelength;
		uint32_t lengthtocopy;
		unsigned char * inbufrow = inbuf +
		    intilewidthinbytes * (ymintocopy-yminoftile);

		if (ymaxplusone > ymin + length)
			ymaxplusone = ymin + length;
		if (ymaxplusone <= yminoftile)
			break;
		lengthtocopy = ymaxplusone - ymintocopy;

		for (x = xmin ; x < xmin + width + intilewidth ;
		    x += intilewidth) {
			uint32_t xminoftile = (x/intilewidth) * intilewidth;
			uint32_t xmintocopyintile = xmin > xminoftile ?
			    xmin : xminoftile;
			uint32_t xmaxplusone = xminoftile + intilewidth;

			if (xmaxplusone > xmin + width)
				xmaxplusone = xmin + width;
			if (xmaxplusone <= xminoftile)
				break;

			uint32_t widthtocopyinpixels =
			    xmaxplusone - xmintocopyintile;

			if (TIFFReadTile(in, inbuf, xminoftile,
			    yminoftile, 0, 0) < 0) {
				TIFFError(TIFFFileName(in),
				    "Error, can't read tile at "
				    UINT32_FORMAT ", " UINT32_FORMAT,
				    xminoftile, yminoftile);
				error = EXIT_IO_ERROR;
				goto done;
			}

			cpBufToBuf(bufp, out_x * samplesperpixel, inbufrow,
			    (xmintocopyintile-xminoftile) * samplesperpixel,
			    widthtocopyinpixels * samplesperpixel,
			    bitspersample,
			    lengthtocopy, outscanlinesizeinbytes,
			    intilewidthinbytes);
			out_x += widthtocopyinpixels;
		}
		bufp += outscanlinesizeinbytes * lengthtocopy;
	}

	done:
	_TIFFfree(inbuf);
	return error;
}

static int cpStrips2Strip(TIFF* in,
	uint32_t xmin, uint32_t ymin, uint32_t width, uint32_t length,
	unsigned char * outbuf, tsize_t outscanlinesizeinbytes,
	uint16_t bitspersample, uint16_t samplesperpixel,
	uint32_t * y_of_last_read_scanline, uint32_t inimagelength)
{
	tsize_t inbufsize;
	uint16_t input_compression;
	tsize_t inwidthinbytes = TIFFScanlineSize(in);
	uint32_t y;
	unsigned char * inbuf, * bufp= outbuf;
	int error = 0;

	TIFFGetFieldDefaulted(in, TIFFTAG_COMPRESSION, &input_compression);
	inbufsize= TIFFScanlineSize(in);  /* not malloc because
	    TIFFScanlineSize returns a tmsize_t */
	inbuf = (unsigned char *)_TIFFmalloc(inbufsize);
	if (!inbuf) {
		TIFFError(TIFFFileName(in),
				"Error, can't allocate space for image buffer");
		return (EXIT_INSUFFICIENT_MEMORY);
	}

	/* When compression method doesn't support random access: */
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
		uint32_t xmintocopyinscanline = xmin;

		if (TIFFReadScanline(in, inbuf, y, 0) < 0) {
			TIFFError(TIFFFileName(in),
			    "Error, can't read scanline at "
			    UINT32_FORMAT " for copying", y);
			error = EXIT_IO_ERROR;
			goto done;
		} else
			*y_of_last_read_scanline= y;

		cpBufToBuf(bufp, 0, inbuf,
		    xmintocopyinscanline * samplesperpixel,
		    requestedwidth * samplesperpixel, bitspersample, 1,
		    outscanlinesizeinbytes, inwidthinbytes);
		bufp += outscanlinesizeinbytes;
	}

	done:
	_TIFFfree(inbuf);
	return error;
}


	/* Return 0 if the requested memory size exceeds the machine's
	  addressing size type (size_t) capacity or if bitspersample is
	  unhandled */
static size_t computeMemorySize(uint16_t spp, uint16_t bitspersample,
	uint32_t outwidth, uint32_t outlength)
{
	uint16_t bitsperpixel = spp * bitspersample;
	uint64_t memorysize = outlength;
	if (bitsperpixel % 8 == 0)
		memorysize *= (uint64_t) outwidth * (bitsperpixel/8);
	else /*if (8 % bitsperpixel == 0)*/ {
		int pixelsperbyte = 8/bitsperpixel;
		int bytesperrow = (outwidth * bitsperpixel + 7) / 8;
		memorysize *= bytesperrow;
	} /*else
		return 0;*/

	if ((size_t) memorysize != memorysize)
		return 0;
	return memorysize;
}


static int makeExtractFromTIFFDirectory(const char * infilename, TIFF * in,
        uint64_t diroff, uint16_t dirnum, uint16_t numberdirs,
        const char * outfilename)
{
	uint32_t inimagewidth, inimagelength;
	uint32_t outwidth = 0, outlength = 0;
	uint16_t planarconfig, spp, bitspersample;
	size_t outmemorysize;
	char * ouroutfilename = NULL;
	unsigned char * outbuf = NULL;
	void * out; /* TIFF* or FILE* */
	uint32_t y_of_last_read_scanline = 0;
	int return_code = 0; /* Success */

	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &inimagewidth);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &inimagelength);
	TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &spp);
	TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);

	if (output_format == OUTPUT_FORMAT_JPEG &&
	    (bitspersample != 8 || spp != 3)) {
		TIFFError(TIFFFileName(in),
			"Can't output JPEG file from image with "
			"bits-per-sample %u (not 8) or "
			"samples-per-pixel %u (not 3). Aborting",
			bitspersample, spp);
		return EXIT_UNHANDLED_INPUT_IMAGE_TYPE;
	} else if (output_format == OUTPUT_FORMAT_PNG) {
		    /* We support only PNG_COLOR_TYPE_GRAY,
		     PNG_COLOR_TYPE_RGB and PNG_COLOR_TYPE_RGB_ALPHA */
		int success = 1;
		if (spp != 1 && spp != 3 && spp != 4) {
			TIFFError(TIFFFileName(in),
				"Can't output PNG file from image with "
				"samples-per-pixel %d (not 1, 3 or 4). Aborting",
				spp);
			success = 0;
		} else switch(spp) {
			case 1:
			if (bitspersample != 1 && bitspersample != 2 &&
			    bitspersample != 4 && bitspersample != 8 &&
			    bitspersample != 16) {
				TIFFError(TIFFFileName(in),
					"Error, can't output PNG file from image with 1 sample per pixel and "
					"bitdepth (bits-per-sample) %u (not 1, 2, 4, 8 or 16). Aborting",
					bitspersample);
				success = 0;
			}
			break;

			case 3:
			case 4:
			if (bitspersample != 8 && bitspersample != 16) {
				TIFFError(TIFFFileName(in),
					"Error, can't output PNG file from image with %u samples per pixel and "
					"bitdepth (bits-per-sample) %u (not 8 or 16). Aborting",
					spp, bitspersample);
				success = 0;
			}
			break;

			default:
			fprintf(stderr, "Internal error.\n");
			return EXIT_UNHANDLED_INPUT_IMAGE_TYPE;
		}
		if (!success)
			return EXIT_UNHANDLED_INPUT_IMAGE_TYPE;
	} else if (bitspersample % 8 != 0 && 8 % bitspersample != 0) {
		TIFFError(TIFFFileName(in),
			"Error, can't deal with image with "
			"bits-per-sample %d (not a multiple nor a "
			"divisor of 8)",
			bitspersample);
		return EXIT_UNHANDLED_INPUT_IMAGE_TYPE;
	}

	if (verbose)
		fprintf(stderr, "File \"%s\", directory %u has %u bits per sample and %u samples per pixel.\n",
			infilename, dirnum, (unsigned) bitspersample, (unsigned) spp);

	if (requestedwidth == (uint32_t) -1)
		requestedwidth = inimagewidth - requestedxmin;
	if (requestedlength == (uint32_t) -1)
		requestedlength = inimagelength - requestedymin;
	if (verbose)
		fprintf(stderr, "Requested rectangle: " UINT32_FORMAT
			"x" UINT32_FORMAT ".\n",
			requestedwidth, requestedlength);

	if (requestedxmin > inimagewidth || requestedymin > inimagelength) {
		fprintf(stderr, "Requested top left corner is outside the image. Aborting.\n");
		return EXIT_GEOMETRY_ERROR;
	}
	if (requestedxmin + requestedwidth > inimagewidth ||
	    requestedymin + requestedlength > inimagelength) {
		if (requestedxmin + requestedwidth > inimagewidth)
			requestedwidth = inimagewidth - requestedxmin;
		if (requestedymin + requestedlength > inimagelength)
			requestedlength = inimagelength - requestedymin;
		if (verbose)
			fprintf(stderr, "Requested rectangle extends "
				"outside the image. Adjusting "
				"dimensions to " UINT32_FORMAT "x"
				UINT32_FORMAT ".\n",
				requestedwidth, requestedlength);
	}

	TIFFGetField(in, TIFFTAG_PLANARCONFIG, &planarconfig);
	if (planarconfig != PLANARCONFIG_CONTIG) {
		TIFFError(TIFFFileName(in),
			"Error, can't deal with image with "
			"planar configuration %d (non contiguous)",
			planarconfig);
		return EXIT_UNHANDLED_INPUT_IMAGE_TYPE;
	}

	if (output_format == OUTPUT_FORMAT_JPEG &&
	    ( (requestedwidth >= JPEG_MAX_DIMENSION) ||
	      (requestedlength >= JPEG_MAX_DIMENSION) ) ) {
		fprintf(stderr, "At least one requested extract dimension is too large for JPEG files.\n");
		return EXIT_UNABLE_TO_ACHIEVE_TILE_DIMENSIONS;
	}

	outwidth = requestedwidth; outlength = requestedlength;
	outmemorysize= computeMemorySize(spp, bitspersample, outwidth,
		outlength);
	outbuf= outmemorysize == 0 ? NULL : malloc(outmemorysize);
	if (outbuf == NULL) {
		fprintf(stderr, "Unable to allocate enough memory to"
			" prepare extract (%zu bytes needed).\n",
		        outmemorysize);
		return EXIT_INSUFFICIENT_MEMORY;
	}

	{
	char * prefix = searchPrefixBeforeLastDot(outfilename != NULL ?
			    outfilename : infilename);
	if (outfilename == NULL || diroff || numberdirs > 1) {
		uint32_t ndigitsx = searchNumberOfDigits(inimagewidth),
		    ndigitsy = searchNumberOfDigits(inimagelength);
		if (diroff)
			my_asprintf(&ouroutfilename, "%s-d0x" UINT64_HEX_FORMAT "-%0*u-%0*u-%0*ux%0*u.%s",
			    prefix, diroff, ndigitsx, requestedxmin,
			    ndigitsy, requestedymin, ndigitsx,
			    requestedwidth, ndigitsy, requestedlength,
			    OUTPUT_SUFFIX[output_format]);
		else if (numberdirs > 1) {
			uint32_t ndigitsdirnum = searchNumberOfDigits(numberdirs);
			my_asprintf(&ouroutfilename, "%s-d%0*u-%0*u-%0*u-%0*ux%0*u.%s",
			    prefix, ndigitsdirnum, dirnum,
			    ndigitsx, requestedxmin,
			    ndigitsy, requestedymin, ndigitsx,
			    requestedwidth, ndigitsy, requestedlength,
			    OUTPUT_SUFFIX[output_format]);
		} else
			my_asprintf(&ouroutfilename, "%s-%0*u-%0*u-%0*ux%0*u.%s",
			    prefix, ndigitsx, requestedxmin, ndigitsy,
			    requestedymin, ndigitsx,
			    requestedwidth, ndigitsy, requestedlength,
			    OUTPUT_SUFFIX[output_format]);
		outfilename = ouroutfilename;
	}
	free(prefix);
	}

	char tiffOpenMode[6] = "w";
	if (TIFFIsBigEndian(in)) {
		strcat(tiffOpenMode, "b");
	} else {
		strcat(tiffOpenMode, "l");
	}
	if (big_tiff) {
		strcat(tiffOpenMode, "8");
	}

	out = output_format != OUTPUT_FORMAT_TIFF ?
	    (void *) fopen(outfilename, "wb") :
	    (void *) TIFFOpen(outfilename, tiffOpenMode);
	if (verbose) {
		if (out == NULL)
			fprintf(stderr, "Error: unable to open output file"
				" \"%s\".\n", outfilename);
			else
				fprintf(stderr, "Output file \"%s\" open."
					" Will write extract of size "
					UINT32_FORMAT " x "
					UINT32_FORMAT ".\n",
					outfilename,
					requestedwidth, requestedlength);
	}
	if (ouroutfilename != NULL)
		free(ouroutfilename);
	if (out == NULL)
		return EXIT_IO_ERROR;

	{
		uint16_t in_compression;
		testAndFixInTIFFPhotoAndCompressionParameters(in,
		    &in_compression);

		if (jpeg_quality <= 0) {
			if (in_compression == COMPRESSION_JPEG) {
				uint16_t in_jpegquality;
				TIFFGetField(in, TIFFTAG_JPEGQUALITY, &in_jpegquality);
				jpeg_quality = in_jpegquality;
			} else
				jpeg_quality = default_jpeg_quality;
		}
	}
	if (png_quality <= 0)
		png_quality = default_png_quality;

	switch(output_format) {
	case OUTPUT_FORMAT_JPEG:
		{
		struct jpeg_compress_struct cinfo;
		struct jpeg_error_mgr jerr;
		uint16_t bitsperpixel = bitspersample * spp;
		tsize_t outscanlinesizeinbytes =
		    (requestedwidth * bitsperpixel + 7) / 8;
		int error = 0;

		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_compress(&cinfo);
		jpeg_stdio_dest(&cinfo, out);
		cinfo.image_width = requestedwidth;
		cinfo.image_height = requestedlength;
		cinfo.input_components = spp; /* # of color comp. per pixel */
		cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
		jpeg_set_defaults(&cinfo);
		if (verbose)
			fprintf(stderr, "Quality of produced JPEG will be %d.\n",
				jpeg_quality);
		jpeg_set_quality(&cinfo, jpeg_quality,
		    TRUE /* limit to baseline-JPEG values */);
		jpeg_start_compress(&cinfo, TRUE);

		if (((TIFFIsTiled(in) &&
		      !(error = cpTiles2Strip(in,
			    requestedxmin, requestedymin,
			    requestedwidth, requestedlength,
			    outbuf, outscanlinesizeinbytes,
			    bitspersample, spp))) ||
		     (!TIFFIsTiled(in) &&
		      !(error = cpStrips2Strip(in,
			    requestedxmin, requestedymin,
			    requestedwidth, requestedlength,
			    outbuf, outscanlinesizeinbytes,
			    bitspersample, spp,
			    &y_of_last_read_scanline,
			    inimagelength))))) {
			if (verbose)
				fprintf(stderr, "Extract prepared.\n");

			uint32_t y;
			JSAMPROW row_pointer;
			JSAMPROW* row_pointers =
				malloc(requestedlength * sizeof(JSAMPROW));

			if (row_pointers == NULL) {
				fprintf(stderr, "Error, can't allocate space for row_pointers.\n");
				fclose(out);
				free(outbuf);
				return EXIT_INSUFFICIENT_MEMORY;
			}

			for (y = 0, row_pointer = outbuf ; y < requestedlength ;
			    y++, row_pointer += outscanlinesizeinbytes)
				row_pointers[y]= row_pointer;

			jpeg_write_scanlines(&cinfo, row_pointers, requestedlength);
			free(row_pointers);
		} else {
			fprintf(stderr, "Error, can't write extract.\n");
			return_code = error;
		}

		jpeg_finish_compress(&cinfo);
		fclose(out);
		jpeg_destroy_compress(&cinfo);
		}
		break;

#ifdef HAVE_PNG
	case OUTPUT_FORMAT_PNG:
		{
		uint16_t bitsperpixel = bitspersample * spp;
		tsize_t outscanlinesizeinbytes = computeWidthInBytes(
		    requestedwidth, bitsperpixel);
		int error = 0;

		png_structp png_ptr = png_create_write_struct(
		    PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (!png_ptr)
			{ fclose(out); return EXIT_INSUFFICIENT_MEMORY; }
		png_infop info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr) {
			fclose(out);
			png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
			return EXIT_INSUFFICIENT_MEMORY;
		}
		if (setjmp(png_jmpbuf(png_ptr))) {
			fclose(out);
			png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
			png_destroy_write_struct(&png_ptr, (png_infopp) &info_ptr);
			fprintf(stderr, "Error, can't write extract.\n");
			return EXIT_INSUFFICIENT_MEMORY;
		}
		png_init_io(png_ptr, out);

		png_set_IHDR(png_ptr, info_ptr, requestedwidth,
		    requestedlength, bitspersample,
		    spp == 4 ? PNG_COLOR_TYPE_RGB_ALPHA :
		    (spp == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY),
		    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
		    PNG_FILTER_TYPE_BASE);
		#ifdef WORDS_BIGENDIAN
		png_set_swap_alpha(png_ptr);
		#else
		png_set_bgr(png_ptr);
		#endif
		png_color_8 sig_bit;
		switch (spp) {
			case 1:
			sig_bit.gray = bitspersample;
			break;

			case 4:
			sig_bit.alpha = bitspersample;
			case 3:
			sig_bit.red = bitspersample;
			sig_bit.green = bitspersample;
			sig_bit.blue = bitspersample;
			break;
		}
		png_set_sBIT(png_ptr, info_ptr, &sig_bit);
		png_set_compression_level(png_ptr, png_quality);
		png_write_info(png_ptr, info_ptr);
		/*png_set_shift(png_ptr, &sig_bit);*/ /* useless
		 because we support only 1, 2, 4, 8, 16 bit-depths */
		/*png_set_packing(png_ptr);*/ /* Use *only* if bits are
		 not yet packed */

		if (((TIFFIsTiled(in) &&
		      !(error = cpTiles2Strip(in,
			    requestedxmin, requestedymin,
			    requestedwidth, requestedlength,
			    outbuf, outscanlinesizeinbytes,
			    bitspersample, spp))) ||
		     (!TIFFIsTiled(in) &&
		      !(error = cpStrips2Strip(in,
			    requestedxmin, requestedymin,
			    requestedwidth, requestedlength,
			    outbuf, outscanlinesizeinbytes,
			    bitspersample, spp,
			    &y_of_last_read_scanline,
			    inimagelength))))) {
			if (verbose)
				fprintf(stderr, "Extract prepared.\n");

			png_const_bytep row_pointer = outbuf;
			uint32_t y;
			for (y = 0 ; y < requestedlength ;
			    y++, row_pointer += outscanlinesizeinbytes)
				png_write_row(png_ptr, row_pointer);
		} else
			return_code = error;

		png_write_end(png_ptr, info_ptr);
		fclose(out);
		png_destroy_write_struct(&png_ptr, (png_infopp) & info_ptr);
		png_destroy_info_struct(png_ptr, (png_infopp) & info_ptr);
		}
		break;
#endif

	case OUTPUT_FORMAT_TIFF:
		{
		uint16_t bitsperpixel = bitspersample * spp;
		tsize_t outscanlinesizeinbytes;
		int error = 0;

		tiffCopyFieldsButDimensions(in, out);
		TIFFSetField(out, TIFFTAG_IMAGEWIDTH, requestedwidth);
		TIFFSetField(out, TIFFTAG_IMAGELENGTH, requestedlength);
		TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, requestedlength);

		testAndFixOutTIFFPhotoAndCompressionParameters(in, out);
			/* To be done *after* setting compression --
			 * otherwise, ScanlineSize may be wrong */
		outscanlinesizeinbytes = TIFFScanlineSize(out);

		if (((TIFFIsTiled(in) &&
		      !(error = cpTiles2Strip(in, requestedxmin,
			requestedymin, requestedwidth, requestedlength,
			outbuf, outscanlinesizeinbytes, bitspersample,
			spp))) ||
		     (!TIFFIsTiled(in) &&
		      !(error = cpStrips2Strip(in, requestedxmin,
			requestedymin, requestedwidth, requestedlength,
			outbuf, outscanlinesizeinbytes,
			bitspersample, spp, &y_of_last_read_scanline,
			inimagelength))))) {
			if (verbose)
				fprintf(stderr, "Extract prepared.\n");

			if (TIFFWriteEncodedStrip(out,
				TIFFComputeStrip(out, 0, 0), outbuf,
				TIFFStripSize(out)) < 0) {
				TIFFError(TIFFFileName(out),
					"Error, can't write strip");
			} else if (verbose)
				fprintf(stderr, "Extract written to "
					"output file \"%s\".\n",
					TIFFFileName(out));
		} else
			return_code = error;

		TIFFClose(out);
		}
		break;

	default:
		fprintf(stderr, "Unsupported output file format.\n");
		free(outbuf);
		return EXIT_UNHANDLED_OUTPUT_FILE_TYPE;
	}

	if (return_code == 0 && verbose)
		fprintf(stderr, "Extract written.\n");
	free(outbuf);
	return return_code;
}


static int makeExtractFromTIFFFile(const char * infilename,
	const char * outfilename)
{
	TIFF * in;
	int return_code = 0; /* Success */

	if (requestedwidth == 0 || requestedlength == 0) {
		fprintf(stderr, "Requested extract is empty. Can't do it. Aborting.\n");
		return EXIT_GEOMETRY_ERROR;
	}

	in = TIFFOpen(infilename, "r");
	if (in == NULL) {
		if (verbose)
			fprintf(stderr, "Unable to open file \"%s\".\n",
				infilename);
		return EXIT_IO_ERROR;
	}

	if (verbose)
		fprintf(stderr, "File \"%s\" open.\n", infilename);

	if (diroff != 0) {
		if (TIFFSetSubDirectory(in, diroff))
			makeExtractFromTIFFDirectory(infilename, in,
			    diroff, 0, 0, outfilename);
	} else if (number_of_dirnum_ranges == 1 &&
	    dirnum_ranges_starts[0] == 0 && dirnum_ranges_ends[0] == 0) {
		 /* special case for speed (?) */
		if (TIFFSetDirectory(in, (tdir_t) dirnum_ranges_starts[0]))
			makeExtractFromTIFFDirectory(infilename, in,
			    0, 0, 1, outfilename);
	} else {
		uint16_t numberofdirectories= TIFFNumberOfDirectories(in);

		if (verbose)
			fprintf(stderr, "File \"%s\" has %u directories.\n",
			    infilename, numberofdirectories);

		uint16_t curdir= 0;
		do {
			if (shouldBeHandled(curdir))
				makeExtractFromTIFFDirectory(infilename, in,
				    0, curdir, numberofdirectories,
				    outfilename);
			curdir++;
		} while (TIFFReadDirectory(in));
	}
	TIFFClose(in);
	return return_code;
}


static void usage()
{
	fprintf(stderr, "tifffastcrop v" PACKAGE_VERSION " license GNU GPL v3 (c) 2013-2021 Christophe Deroulers\n\n");
	fprintf(stderr, "Quote \"Deroulers et al., Diagnostic Pathology 2013, 8:92\" in your production\n       http://doi.org/10.1186/1746-1596-8-92\n\n");
	fprintf(stderr, "Usage: tifffastcrop [options] input.tif [output_name]\n\n");
	fprintf(stderr, " Extracts (crops), without loading the full image input.tif into memory, a\nrectangular region from it, and saves it. Output file name is output_name if\ngiven, otherwise a name derived from input.tif. Output file format is guessed\nfrom output_name's extension if possible. Options:\n");
	fprintf(stderr, " -v                verbose monitoring\n");
	fprintf(stderr, " -B                write a BigTIFF format file\n");
	fprintf(stderr, " -T                report TIFF errors/warnings on stderr (no dialog boxes)\n");
	fprintf(stderr, " -E x,y,w,l        region to extract/crop (x,y: coordinates of top left corner,\n");
	fprintf(stderr, "   w,l: width and length in pixels)\n");
	fprintf(stderr, " -o offset         extracts only from directory at position offset in file\n");
	fprintf(stderr, " -d range1[,range2...] extracts from dir. having numbers in the given ranges\n");
	fprintf(stderr, "                   (numbers start at 0; ranges are like 3-3, 5:8, 4-, -0)\n");
	fprintf(stderr, " -j[#]             output JPEG file (with quality #, 0-100, default 75)\n");
#ifdef HAVE_PNG
	fprintf(stderr, " -p[#]             output PNG file (with quality #, 0-9, default 6)\n");
#endif
	fprintf(stderr, " -c none[:opts]    output TIFF file with no compression \n");
	fprintf(stderr, " -c x[:opts]       output TIFF compressed with encoding x (jpeg, lzw, zip,...)\n");
	fprintf(stderr, "When output file format can't be guessed from the output filename extension, it is TIFF with same compression as input.\n\n");
	fprintf(stderr, "JPEG-compressed TIFF options:\n");
	fprintf(stderr, " #   set compression quality level (0-100, default 75)\n");
/*	fprintf(stderr, " r  output color image as RGB rather than YCbCr\n");*/
	fprintf(stderr, "LZW, Deflate (ZIP), LZMA2, ZSTD and WEBP options:\n");
	fprintf(stderr, " #   set predictor value\n");
	fprintf(stderr, " p#  set compression level (preset)\n");
	fprintf(stderr, "For example, -c lzw:2 to get LZW-encoded data with horizontal differencing,\n");
	fprintf(stderr, "-c zip:3:p9 for Deflate encoding with maximum compression level and floating\n");
	fprintf(stderr, "point predictor, -c jpeg:r:50 for JPEG-encoded RGB data at quality 50%%.\n");
}


static int processZIPOptions(char* cp)
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


static int processG3Options(char* cp)
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


static int processCompressOptions(char* opt)
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
				jpeg_quality = (int) u;
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


static int processExtractGeometryOptions(char* cp)
{
	while (*cp == ' ')
		cp++;
	if (sscanf(cp, UINT32_FORMAT "," UINT32_FORMAT ","
	    UINT32_FORMAT "," UINT32_FORMAT,
	    &requestedxmin, &requestedymin, &requestedwidth,
	    &requestedlength) != 4)
		return 0;
	return 1;
}


 /* Add dirnum ranges to ranges that have already been stored */
static unsigned parseDirnumRanges(const char* c, int dryrun)
{
	const char* c_depart = c;
	uint16_t number_of_read_ranges = 0;
	uint16_t debut_de_plage_courante = 0;
	uint16_t fin_de_plage_courante = (uint16_t) -1;
	int separateur_debut_fin_vu = 0, premier_nombre_lu = 0,
	    fin_de_la_chaine = 0;

	do {
		if (*c == 0)
			fin_de_la_chaine = 1;
		if (*c == ',' || *c == 0) {
			c++;
			if (premier_nombre_lu) {
				if (! dryrun) {
					dirnum_ranges_starts[number_of_dirnum_ranges + number_of_read_ranges] =
					    debut_de_plage_courante;
					dirnum_ranges_ends[number_of_dirnum_ranges + number_of_read_ranges]=
					    separateur_debut_fin_vu
					    ? fin_de_plage_courante /* range like "25:45" */
					    : debut_de_plage_courante; /* range like "30" -> 30:30 */
				}
			number_of_read_ranges++;
			}
		debut_de_plage_courante = 0;
		fin_de_plage_courante = (uint16_t) -1;
		separateur_debut_fin_vu = 0;
		premier_nombre_lu = 0;
		continue;
		}
		if (*c == ':' || *c == '-') {
			c++;
			separateur_debut_fin_vu = 1;
			continue;
		}
		if (isdigit(*c)) {
			unsigned u;
			int n;
			if (sscanf(c, "%u%n", &u, &n) != 1) {
				fprintf(stderr, "Syntax error in the list of directory number ranges \"%s\" at position \"%s\": integer number was expected.\n",
				    c_depart, c);
				return 0;
			}
			c += n;
			if (separateur_debut_fin_vu)
				fin_de_plage_courante = u;
			else
				debut_de_plage_courante = u;
			premier_nombre_lu = 1;
			continue;
		}
		fprintf(stderr, "Syntax error in the list of directory number ranges \"%s\" at position \"%s\": integer number was expected.\n",
			c_depart, c);
		return 0;
	} while (! fin_de_la_chaine);

	if (! dryrun)
		number_of_dirnum_ranges += number_of_read_ranges;

	return number_of_read_ranges;
}

 /* Add dirnum ranges to ranges that have already been stored (in case of
   multiple lists of ranges on command line) */
static int processDirnumRangesOption(const char* cp)
{
	uint16_t number_of_read_ranges = parseDirnumRanges(cp, 1);

	if (number_of_read_ranges == 0) {
		fprintf(stderr, "Expected argument to -d option: non empty list of directory number ranges \"%s\".\n",
		    cp);
		return 0;
	}

	dirnum_ranges_starts = realloc(dirnum_ranges_starts,
		sizeof(*dirnum_ranges_starts) *
		(number_of_dirnum_ranges + number_of_read_ranges));
	dirnum_ranges_ends = realloc(dirnum_ranges_ends,
		sizeof(*dirnum_ranges_ends) *
		(number_of_dirnum_ranges + number_of_read_ranges));
	if (dirnum_ranges_starts == NULL || dirnum_ranges_ends == NULL) {
		perror("Insufficient memory for directory number ranges ");
		exit(EXIT_INSUFFICIENT_MEMORY);
	}

	parseDirnumRanges(cp, 0);

	return 1;
}


static void stderrErrorHandler(const char* module, const char* fmt, va_list ap)
{
	if (module != NULL)
		fprintf(stderr, "%s: ", module);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ".\n");
}


static void stderrWarningHandler(const char* module, const char* fmt, va_list ap)
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
	int seen_extract_geometry_on_the_command_line = 0;

	while (arg < argc && argv[arg][0] == '-') {

		if (argv[arg][1] == 'v')
			verbose = 1;
		else if (argv[arg][1] == 'B') {
			big_tiff = 1;
		} else if (argv[arg][1] == 'T') {
			TIFFSetErrorHandler(stderrErrorHandler);
			TIFFSetWarningHandler(stderrWarningHandler);
			}
		else if (argv[arg][1] == 'E') {
			if (arg+1 >= argc ||
			    !processExtractGeometryOptions(argv[arg+1])) {
				fprintf(stderr, "Syntax error in the "
					"specification of region to "
					"extract (option -E).\n");
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			seen_extract_geometry_on_the_command_line = 1;
			arg++;
		} else if (argv[arg][1] == 'o') {
			if (arg+1 >= argc) {
				fprintf(stderr, "Option -o requires "
					"an argument.\n");
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			diroff = strtoul(argv[arg+1], NULL, 0);
			arg++;
		} else if (argv[arg][1] == 'd') {
			if (arg+1 >= argc) {
				fprintf(stderr, "Option -d requires "
					"an argument.\n");
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			if (!processDirnumRangesOption(argv[arg+1])) {
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			arg++;
		} else if (argv[arg][1] == 'c') {
			output_format = OUTPUT_FORMAT_TIFF;
			if (arg+1 >= argc ||
			    !processCompressOptions(argv[arg+1])) {
				usage();
				return EXIT_SYNTAX_ERROR;
			}
			arg++;
		} else if (argv[arg][1] == 'j') {
			output_format = OUTPUT_FORMAT_JPEG;

			if (argv[arg][2] != 0) {
				unsigned long u;

				u= strtoul(&(argv[arg][2]), NULL, 10);
				if (u == 0 || u > 100) {
					fprintf(stderr, "Expected optional non-null integer percent number after -j, got \"%s\"\n",
					    &(argv[arg][2]));
					usage();
					return EXIT_SYNTAX_ERROR;
				}
				jpeg_quality = (int) u;
			}
#ifdef HAVE_PNG
		} else if (argv[arg][1] == 'p') {
			output_format = OUTPUT_FORMAT_PNG;

			if (argv[arg][2] != 0) {
				unsigned long u;

				u= strtoul(&(argv[arg][2]), NULL, 10);
				if (u > 9) {
					fprintf(stderr, "Expected optional non-null integer percent number after -j, got \"%s\"\n",
					    &(argv[arg][2]));
					usage();
					return EXIT_SYNTAX_ERROR;
				}
				png_quality = (int) u;
			}
#endif
		} else {
			fprintf(stderr, "Unknown option \"%s\"\n", argv[arg]);
			usage();
			return EXIT_SYNTAX_ERROR;
		}

	arg++;
	}

	if (argc > 1 && !seen_extract_geometry_on_the_command_line) {
		fprintf(stderr, "The extract's position and size must be specified on the command line as argument to the '-E' option. Aborting.\n");
		return EXIT_GEOMETRY_ERROR;
	}

	if (output_format < 0) {
		output_format = OUTPUT_FORMAT_TIFF;
		if (argc >= arg+2) { /* Try to guess from output file name */
			const char * suffix = searchSuffix(argv[arg+1]);
			if (strcasecmp(suffix, "png") == 0)
				output_format = OUTPUT_FORMAT_PNG;
			else if (strcasecmp(suffix, "jpeg") == 0 ||
				 strcasecmp(suffix, "jpg") == 0)
				output_format = OUTPUT_FORMAT_JPEG;
		}
	}
	if (verbose)
		fprintf(stderr, "Output file will have format %s.\n",
			OUTPUT_SUFFIX[output_format]);

	if (argc >= arg+2) {
		return makeExtractFromTIFFFile(argv[arg], argv[arg+1]);
	} else if (argc >= arg+1) {
		return makeExtractFromTIFFFile(argv[arg], NULL);
	} else {
		usage();
		return EXIT_SYNTAX_ERROR;
	}

}
