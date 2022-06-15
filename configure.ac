#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([LargeTIFFTools Software], [1.4.1], [], [largetifftools])
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

AC_OPENMP()
CFLAGS="$CFLAGS $OPENMP_CFLAGS"

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h strings.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
#AC_FUNC_MALLOC # Do *not* use this outdated macro suggested by autoscan
AC_CHECK_FUNCS([strcasecmp strchr strncasecmp strtoul strtoull])

AC_CHECK_LIB(m, lroundl)

# ---------------------------------------------------------------------------
# Support large files.
# ---------------------------------------------------------------------------

AC_SYS_LARGEFILE

# ---------------------------------------------------------------------------
# Check for JPEG.
# ---------------------------------------------------------------------------

AC_ARG_WITH(jpeg-include-dir,
            AS_HELP_STRING([--with-jpeg-include-dir=DIR],
                           [location of the JPEG library headers]),,)
AC_ARG_WITH(jpeg-lib-dir,
            AS_HELP_STRING([--with-jpeg-lib-dir=DIR],
                           [location of the JPEG library binary]),,)

if test "x$with_jpeg_lib_dir" != "x" ; then
  LDFLAGS="-L$with_jpeg_lib_dir $LDFLAGS"
  if test "$HAVE_RPATH" = "yes" -a "x$with_jpeg_lib_dir" != "x" ; then
    LIBDIR="-R $with_jpeg_lib_dir $LIBDIR"
  fi
fi
AC_CHECK_LIB(jpeg, jpeg_read_scanlines, [jpeg_lib=yes], [jpeg_lib=no],)
if test "$jpeg_lib" = "no" ; then
  if test "x$with_jpeg_lib_dir" != "x" ; then
    AC_MSG_ERROR([JPEG library not found at $with_jpeg_lib_dir])
  else
    AC_MSG_ERROR([JPEG library not found -- please specify --with-jpeg-lib-dir=<dir>])
  fi
fi
    
if test "x$with_jpeg_include_dir" != "x" ; then
  CPPFLAGS="-I$with_jpeg_include_dir $CPPFLAGS"
fi
AC_CHECK_HEADER(jpeglib.h, [jpeg_h=yes], [jpeg_h=no])
if test "$jpeg_h" = "no" ; then
  if test "x$with_jpeg_include_dir" != "x" ; then
    AC_MSG_ERROR([JPEG library headers not found at $with_jpeg_include_dir])
  else
    AC_MSG_ERROR([JPEG library headers not found -- please specify --with-jpeg-include-dir=<dir>])
  fi
fi

LIBS="-ljpeg $LIBS"

# ---------------------------------------------------------------------------
# Check for libpng.
# ---------------------------------------------------------------------------

HAVE_LIBPNG=no

AC_ARG_WITH(png-include-dir,
            AS_HELP_STRING([--with-png-include-dir=DIR],
                           [location of libpng headers]),,)
AC_ARG_WITH(png-lib-dir,
            AS_HELP_STRING([--with-png-lib-dir=DIR],
                           [location of libpng library binary]),,)

if test "x$with_png_lib_dir" != "x" ; then
  LDFLAGS="-L$with_png_lib_dir $LDFLAGS"
fi
  
AC_CHECK_LIB(png, png_create_write_struct, [png_lib=yes], [png_lib=no],)
  if test "$png_lib" = "no" -a "x$with_png_lib_dir" != "x"; then
    AC_MSG_ERROR([libpng library not found at $with_png_lib_dir])
  fi
    
if test "x$with_png_include_dir" != "x" ; then
  CPPFLAGS="-I$with_png_include_dir $CPPFLAGS"
fi
AC_CHECK_HEADER(png.h, [png_h=yes], [png_h=no])
if test "$png_h" = "no" -a "x$with_png_include_dir" != "x" ; then
  AC_MSG_ERROR([libpng headers not found at $with_png_include_dir])
fi

if test "$png_lib" = "yes" -a "$png_h" = "yes" ; then
  HAVE_PNG=yes
fi

if test "$HAVE_PNG" = "yes" ; then
  AC_DEFINE(HAVE_PNG,1,[Support writing of PNG files])
  LIBS="-lpng $LIBS"
fi

 # For libpng versions older than 1.6: png_const_bytep was undefined
AC_MSG_CHECKING(if libpng defines type png_const_bytep)
AC_LANG_PUSH(C)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include <png.h>
                                    png_const_bytep p;]])],
  [HAVE_PNG_CONST_BYTEP=yes], [HAVE_PNG_CONST_BYTEP=no])
AC_LANG_POP()
if test "$HAVE_PNG_CONST_BYTEP" = "yes" ; then
  AC_DEFINE([HAVE_PNG_CONST_BYTEP], [1], [Define to 1 if libpng defines png_const_bytep])
fi
AC_MSG_RESULT($HAVE_PNG_CONST_BYTEP)

dnl ---------------------------------------------------------------------------
dnl Check for libzstd.
dnl ---------------------------------------------------------------------------

HAVE_ZSTD=no

AC_ARG_ENABLE(zstd,
              AS_HELP_STRING([--disable-zstd],
                             [disable libzstd usage (required for zstd compression, enabled by default)]),,)
AC_ARG_WITH(zstd-include-dir,
            AS_HELP_STRING([--with-zstd-include-dir=DIR],
                           [location of libzstd headers]),,)
AC_ARG_WITH(zstd-lib-dir,
            AS_HELP_STRING([--with-zstd-lib-dir=DIR],
                           [location of libzstd library binary]),,)

if test "x$enable_zstd" != "xno" ; then

  if test "x$with_zstd_lib_dir" != "x" ; then
    LDFLAGS="-L$with_zstd_lib_dir $LDFLAGS"
  fi
  
  AC_CHECK_LIB(zstd, ZSTD_decompressStream, [zstd_lib=yes], [zstd_lib=no],)
  if test "$zstd_lib" = "no" -a "x$with_zstd_lib_dir" != "x"; then
    AC_MSG_ERROR([zstd library not found at $with_zstd_lib_dir])
  fi
    
  if test "x$with_zstd_include_dir" != "x" ; then
    CPPFLAGS="-I$with_zstd_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(zstd.h, [zstd_h=yes], [zstd_h=no])
  if test "$zstd_h" = "no" -a "x$with_zstd_include_dir" != "x" ; then
    AC_MSG_ERROR([Libzstd headers not found at $with_zstd_include_dir])
  fi

  if test "$zstd_lib" = "yes" -a "$zstd_h" = "yes" ; then
    HAVE_ZSTD=yes
  fi

fi

if test "$HAVE_ZSTD" = "yes" ; then
  AC_DEFINE(ZSTD_SUPPORT,1,[Support zstd compression])
  LIBS="-lzstd $LIBS"
  tiff_libs_private="-lzstd ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_zstd_lib_dir" != "x" ; then
    LIBDIR="-R $with_zstd_lib_dir $LIBDIR"
  fi

fi

AM_CONDITIONAL(HAVE_ZSTD, test "$HAVE_ZSTD" = 'yes')

dnl ---------------------------------------------------------------------------
dnl Check for libwebp.
dnl ---------------------------------------------------------------------------

HAVE_WEBP=no

AC_ARG_ENABLE(webp,
              AS_HELP_STRING([--disable-webp],
                             [disable libwebp usage (required for webp compression, enabled by default)]),,)
AC_ARG_WITH(webp-include-dir,
            AS_HELP_STRING([--with-webp-include-dir=DIR],
                           [location of libwebp headers]),,)
AC_ARG_WITH(webp-lib-dir,
            AS_HELP_STRING([--with-webp-lib-dir=DIR],
                           [location of libwebp library binary]),,)

if test "x$enable_webp" != "xno" ; then

  if test "x$with_webp_lib_dir" != "x" ; then
    LDFLAGS="-L$with_webp_lib_dir $LDFLAGS"
  fi
  
  AC_CHECK_LIB(webp, WebPDecode, [webp_lib=yes], [webp_lib=no],)
  if test "$webp_lib" = "no" -a "x$with_webp_lib_dir" != "x"; then
    AC_MSG_ERROR([webp library not found at $with_webp_lib_dir])
  fi
    
  if test "x$with_webp_include_dir" != "x" ; then
    CPPFLAGS="-I$with_webp_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(webp/decode.h, [webp_h=yes], [webp_h=no])
  if test "$webp_h" = "no" -a "x$with_webp_include_dir" != "x" ; then
    AC_MSG_ERROR([Libwebp headers not found at $with_webp_include_dir])
  fi

  if test "$webp_lib" = "yes" -a "$webp_h" = "yes" ; then
    HAVE_WEBP=yes
  fi

fi

if test "$HAVE_WEBP" = "yes" ; then
  AC_DEFINE(WEBP_SUPPORT,1,[Support webp compression])
  LIBS="-lwebp $LIBS"
  tiff_libs_private="-lwebp ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_webp_lib_dir" != "x" ; then
    LIBDIR="-R $with_webp_lib_dir $LIBDIR"
  fi

fi

AM_CONDITIONAL(HAVE_WEBP, test "$HAVE_WEBP" = 'yes')

# ---------------------------------------------------------------------------
# Check for LibTIFF.
# ---------------------------------------------------------------------------

AC_ARG_WITH(tiff-include-dir,
            AS_HELP_STRING([--with-tiff-include-dir=DIR],
                           [location of TIFF library headers]),,)
AC_ARG_WITH(tiff-lib-dir,
            AS_HELP_STRING([--with-tiff-lib-dir=DIR],
                           [location of TIFF library binary]),,)

if test "x$with_tiff_lib_dir" != "x" ; then
  LDFLAGS="-L$with_tiff_lib_dir $LDFLAGS"
fi
AC_CHECK_LIB(tiff, TIFFReadScanline, [tiff_lib=yes], [tiff_lib=no],)
if test "$tiff_lib" = "no" ; then
  if test "x$with_tiff_lib_dir" != "x" ; then
    AC_MSG_ERROR([Tiff library not found at $with_tiff_lib_dir])
  else
    AC_MSG_ERROR([Tiff library not found -- please specify --with-tiff-lib-dir=<dir>])
  fi
fi
AC_CHECK_LIB(tiff, TIFFTileSize64, [HAVE_BIGTIFF=yes], [HAVE_BIGTIFF=no],)
if test "$HAVE_BIGTIFF" = "no" ; then
  AC_MSG_WARN([Tiff library handling BigTIFF's not found -- built programs won't cope with huge TIFF files])
fi
    
if test "x$with_tiff_include_dir" != "x" ; then
  CPPFLAGS="-I$with_tiff_include_dir $CPPFLAGS"
fi
AC_CHECK_HEADER(tiff.h, [tiff_h=yes], [tiff_h=no])
AC_CHECK_HEADER(tiffio.h, [tiffio_h=yes], [tiffio_h=no])
if test \( "$tiff_h" = "no" -o "$tiffio_h" = "no" \) ; then
  if test "x$with_tiff_include_dir" != "x" ; then
    AC_MSG_ERROR([Tiff library headers not found at $with_tiff_include_dir])
  else
    AC_MSG_ERROR([Tiff library headers not found -- please specify --with-tiff-include-dir=<dir>])
  fi
fi

LIBS="-ltiff $LIBS"

# ---------------------------------------------------------------------------
# Compute sized types for current CPU and compiler options
# ---------------------------------------------------------------------------

# Obtain size of an 'signed short' and define as SIZEOF_SIGNED_SHORT
#AC_CHECK_SIZEOF(signed short)

# Obtain size of an 'unsigned short' and define as SIZEOF_UNSIGNED_SHORT
#AC_CHECK_SIZEOF(unsigned short)

# Obtain size of an 'signed int' and define as SIZEOF_SIGNED_INT
AC_CHECK_SIZEOF(signed int)

# Obtain size of an 'unsigned int' and define as SIZEOF_UNSIGNED_INT
AC_CHECK_SIZEOF(unsigned int)

# Obtain size of a 'signed long' and define as SIZEOF_SIGNED_LONG
AC_CHECK_SIZEOF(signed long)

# Obtain size of a 'unsigned long' and define as SIZEOF_UNSIGNED_LONG
AC_CHECK_SIZEOF(unsigned long)

# Obtain size of a 'long long' and define as SIZEOF_SIGNED_LONG_LONG.
# If 'long long' is not supported then the value defined is zero.
#AC_CHECK_SIZEOF(signed long long)

# Obtain size of a 'unsigned long long' and define as
# SIZEOF_UNSIGNED_LONG_LONG.  If 'unsigned long long' is not
# supported then the value defined is zero.
AC_CHECK_SIZEOF(unsigned long long)

# Obtain the size of 'size_t' and define as SIZEOF_SIZE_T.  Result is
# available in ac_cv_sizeof_size_t
AC_CHECK_SIZEOF([size_t])

AC_MSG_CHECKING(for signed 32-bit format)
#INT32_T='none'
INT32_FORMAT='none'
if test $ac_cv_sizeof_signed_int -eq 4
then
  #INT32_T='signed int'
  INT32_FORMAT='"%d"'
elif test $ac_cv_sizeof_signed_long -eq 4
then
  #INT32_T='signed long'
  INT32_FORMAT='"%ld"'
fi
# INT32_T is defined in libtiff's tiff.h, no need to define it here
AC_DEFINE_UNQUOTED(INT32_FORMAT,$INT32_FORMAT,[Signed 32-bit type formatter])
AC_MSG_RESULT($INT32_FORMAT)

AC_MSG_CHECKING(for unsigned 32-bit format)
#UINT32_T='none'
UINT32_FORMAT='none'
if test $ac_cv_sizeof_unsigned_int -eq 4
then
  #UINT32_T='unsigned int'
  UINT32_FORMAT='"%u"'
elif test $ac_cv_sizeof_unsigned_long -eq 4
then
  #UINT32_T='unsigned long'
  UINT32_FORMAT='"%lu"'
fi
# UINT32_T is defined in libtiff's tiff.h, no need to define it here
AC_DEFINE_UNQUOTED(UINT32_FORMAT,$UINT32_FORMAT,[Unsigned 32-bit type formatter])
AC_MSG_RESULT($UINT32_FORMAT)

AC_MSG_CHECKING(for signed 64-bit format)
#INT64_T='none'
INT64_FORMAT='none'
if test $ac_cv_sizeof_signed_long -eq 8
then
  #INT64_T='signed long'
  INT64_FORMAT='"%ld"'
elif test $ac_cv_sizeof_signed_long_long -eq 8
then
  #INT64_T='signed long long'
  case "${host_os}" in
    mingw32*)
      # MinGW32 understands 'long long', but uses printf from WIN32 CRT
      INT64_FORMAT='"%I64d"'
      ;;
    *)
      INT64_FORMAT='"%lld"'
      ;;
  esac
fi
#AC_EGREP_HEADER(\\ int64, tiff.h,
#  AC_DEFINE([HAVE_INT64], [], [Define to 1 if tiff.h defines int64]))
#AC_DEFINE_UNQUOTED(INT64_T,$INT64_T,[Signed 64-bit type])
AC_DEFINE_UNQUOTED(INT64_FORMAT,$INT64_FORMAT,[Signed 64-bit type formatter])
AC_MSG_RESULT($INT64_FORMAT)


AC_MSG_CHECKING(for unsigned 64-bit format)
#UINT64_T='none'
UINT64_FORMAT='none'
UINT64_HEX_FORMAT='none'
if test $ac_cv_sizeof_unsigned_long_long -eq 8
then
  #UINT64_T='unsigned long long'
  case "${host_os}" in
    mingw32*)
      # MinGW32 understands 'unsigned long long', but uses printf from WIN32 CRT
      UINT64_FORMAT='"%I64u"'
      UINT64_HEX_FORMAT='"%I64x"'
      ;;
    *)
      UINT64_FORMAT='"%llu"'
      UINT64_HEX_FORMAT='"%llx"'
      ;;
  esac
elif test $ac_cv_sizeof_unsigned_long -eq 8
then
  #UINT64_T='unsigned long'
  UINT64_FORMAT='"%lu"'
  UINT64_HEX_FORMAT='"%lx"'
fi
#AC_EGREP_HEADER(\\ uint64, tiff.h,
#  AC_DEFINE([HAVE_UINT64], [], [Define to 1 if tiff.h defines uint64]))
#AC_DEFINE_UNQUOTED(UINT64_T,$UINT64_T,[Unsigned 64-bit type])
AC_DEFINE_UNQUOTED(UINT64_FORMAT,$UINT64_FORMAT,[Unsigned 64-bit type formatter])
AC_DEFINE_UNQUOTED(UINT64_HEX_FORMAT,$UINT64_HEX_FORMAT,[Unsigned 64-bit type hexadecimal formatter])
AC_MSG_RESULT($UINT64_FORMAT)

AC_MSG_CHECKING([for 'size_t' format specifier])
SIZE_FORMAT='unknown'
if test $ac_cv_sizeof_unsigned_int -eq $ac_cv_sizeof_size_t
then
  SIZE_FORMAT='"%u"'
elif test $ac_cv_sizeof_unsigned_long -eq $ac_cv_sizeof_size_t
then
  SIZE_FORMAT='"%lu"'
elif test $ac_cv_sizeof_unsigned_long_long -eq $ac_cv_sizeof_size_t
then
  case "${host_os}" in
    mingw32*)
      # MinGW32 understands 'unsigned long long', but uses printf from WIN32 CRT
      SIZE_FORMAT='"%I64u"'
      ;;
    *)
      SIZE_FORMAT='"%llu"'
      ;;
  esac
fi
AC_MSG_RESULT([$SIZE_FORMAT])
AC_DEFINE_UNQUOTED([SIZE_FORMAT],[$SIZE_FORMAT],[Size type formatter])

# ---------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 man/Makefile
                 src/Makefile
                 test/Makefile])
AC_OUTPUT

# ---------------------------------------------------------------------------
# Display configuration status
# ---------------------------------------------------------------------------

AC_MSG_RESULT([
LargeTIFFTools are now configured with:

  Installation directory:             ${prefix}
  C compiler:                         ${CC} ${CFLAGS}
  OpenMP compiler flags:              ${OPENMP_CFLAGS}

  Support BigTIFF:                    ${HAVE_BIGTIFF}
  Support PNG writing:                ${HAVE_PNG}
  Support ZSTD compression:           ${HAVE_ZSTD}
  Support WebP compression:           ${HAVE_WEBP}
]);
