# LargeTIFFTools

## About
LargeTIFFTools is a collection of software that can help managing (very) large TIFF files, especially files that are too large to fit entirely into your computer's memory. It is composed of the following programs:

- `tiffmakemosaic` opens a TIFF file and makes a mosaic out of it.
- `tifffastcrop` crops (extracts) a rectangular region from a TIFF file without opening the whole image into memory and saves it as a TIFF, JPEG or PNG file.
- `tiffsplittiles` copies the tiles of a tiled TIFF file into independent files (one for each tile).
Getting the software

The software is open source, distributed under the GNU General Public License v. 3.0. It uses noticeably the libtiff and libjpeg or libjpeg-turbo software, made free and open by its authors, which we acknowledge.

Original page: https://www.imnc.in2p3.fr/pagesperso/deroulers/software/largetifftools/

This repository version was modified by Tim Mensch to support the BigTIFF output format.

The original source page above includes pre-compiled Windows versions (without the added BigTIFF support). The 32-bit Windows version should run under all versions of Windows (XP, Vista, 7; either 32- or 64-bits) but will produce images which require at most ca. 1 GiB memory. The 64-bit version goes beyond that limit.

If you install the software from source, you need the following installed libraries: libtiff (we recommend a version >= 4.0.0) and libjpeg (and possibly other libraries that your version of libtiff might use, e.g. zlib). Basic instructions for installing from source are: extract with tar xfj <file>, compile and install with ./configure ; make ; make install.

## Using the software
Under Windows, you can simply drag and drop a TIFF (.tif) file over the .exe file or icon of the relevant program. tiffmakemosaic.exe and tiffsplittiles.exe will open the file and, if appropriate, split it into multiple TIFF image files (while preserving the original). tiffmakemosaic-j.exe does the same as tiffmakemosaic-j.exe, but stores the resulting images into JPEG files (option -j is hardcoded). For options, please refer to the documentation of each program for details.

On all platforms (including Windows, Linux, Mac OS X), you can also use it with a command line. Open a shell (e.g. command interpreter, Terminal.app, xterm...) and launch the program by typing its name (preceded by its path if needed) followed by a space then the path to the TIFF file. The resulting TIFF or JPEG file(s) will be produced in the directory where the original TIFF file resides.

A typical use is for anatomopathology or microscopy slide images, like the one that the NDPITools produce.

## Performance
In principle, generating pieces from a large TIFF file can also be achieved with several tools, as tiffcrop from the libtiff, ImageMagick, and GraphicsMagick (one has to first compute and specify explicitly the dimensions and positions of the pieces, though). However, most of the software start with opening and deciphering the whole image either in memory or in a huge temporary file on the disk, which makes them quite slow or often unable to complete the task by lack of memory.

In contrast, tiffmakemosaic, tifffastcrop and tiffsplittiles avoid opening the whole image, which yields speedup and guarantees successful termination of the process even on computers with modest memory. Eg. to make a mosaic of 64 JPEG files requesting less than 512 Mib of memory to open from a RGB image of 103168x63232 pixels, on a computer with 16 Gib of RAM and an i5 CPU, tiffmakemosaic needs 2.5 minutes while GraphicsMagick needs 70 minutes.

## Non-exhaustive list of enhancements
- Release 1.4.1-bigtiff: Added BigTIFF export support to tifffastcrop.
- Release 1.3.6: tifffastcrop is now able to deal with many bit-depths (e.g. RGB with alpha channel, or 1 bit-per-pixel black and white images, or 4 bits-per-pixel grayscale images...) and to write PNG files in addition to TIFF and JPEG files. Now, it tries to guess the output file format from the output filename, if any.
- Release 1.3.7: the software is able to build even with version 1.2 of the libpng library.
- Release 1.3.8: improved exit codes especially when input files are corrupted; fixed a regression preventing cross-compilation for Windows; fixed syntax errors in manpages.
- Release 1.3.9: tifffastcrop now supports Fax-like compression; corrected a bug in tiffmakemosaic in the case of lossless compression and non-zero overlap.
- Release 1.3.10: further support by tifffastcrop and tiffmakemosaic of Fax-like compression.
- Release 1.4: add optional padding to large image before making mosaic in tiffmakemosaic; new options to handle TIFF files with multiple directories in tifffastcrop; support of WebP and Zstd compression by both programs.
- Release 1.4.1: in tifffastcrop, fixed regression which prevented to write JPEG files and fixed support of images with 2 or 4 bits per sample.

## Credits and acknowledgments
This software was developed by the modelling team of IJCLab (formerly IMNC) near Paris, France, during a research project funded by the IN2P3 and INSB Institutes of the CNRS and by Université de Paris (formerly the Paris Diderot-Paris 7 university) and the university Paris-Saclay (formerly the university Paris South-11).

Contact: Christophe Deroulers

Linux, Mac OS X and Windows are registered trademarks of their respective owners.

Modified by Tim Mensch to add BigTIFF support. Modifications released under GPL-3 license.
