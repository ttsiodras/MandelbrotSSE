AC_INIT([mandelSSE], [2.6], [ttsiodras@gmail.com])

AC_LANG(C++)
AC_CONFIG_HEADERS([src/config.h])

AC_CONFIG_MACRO_DIR([ac-macros])
AM_REVISION_UTILS

# Don't clutter this dir, store in build-aux
AC_CONFIG_AUX_DIR([build-aux])

if test x"${CXXFLAGS}" = x ; then
    CXXFLAGS="-O3 -g -Wall -Wextra"
fi

# Detect the canonical host and target build environment
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([-Wall])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_ARG_ENABLE(openmp, 
    [AS_HELP_STRING([--enable-openmp],[use OpenMP Multithreading if available (automatic, use --disable-openmp to disable it)])], 
    [enable_openmp="$enableval"], 
    [enable_openmp="yes"])

AC_ARG_ENABLE(debug, 
    [AS_HELP_STRING([--enable-debug],[Compile in debug mode])], 
    [enable_debug="$enableval"], 
    [enable_debug="no"])

AX_CXXFLAGS_GCC_OPTION(-Wall)
AX_CXXFLAGS_GCC_OPTION(-Wextra)

# Best optimization flags for our compiler
#        AX_CXXFLAGS_GCC_OPTION(-pedantic)
AX_CXXFLAGS_GCC_OPTION(-fomit-frame-pointer)
AX_CXXFLAGS_GCC_OPTION(-ffast-math)
AX_CXXFLAGS_GCC_OPTION(-funsafe-math-optimizations)
AX_CXXFLAGS_GCC_OPTION(-mtune=native)

# GCC support for -flto is still buggy, internal compiler error triggered...
#        AX_CXXFLAGS_GCC_OPTION(-flto)
#	 AX_CHECK_LINKER_FLAGS(-fwhole-program,[WHOLEPROGRAM="-fwhole-program"],[])
#	 AC_SUBST(WHOLEPROGRAM)

AX_CXXFLAGS_GCC_OPTION(-msse)
AX_CXXFLAGS_GCC_OPTION(-msse2)
AX_CXXFLAGS_GCC_OPTION(-mavx)
AX_CXXFLAGS_GCC_OPTION(-mrecip)
AX_CXXFLAGS_GCC_OPTION(-mfpmath=sse)
CXXFLAGS="$CXXFLAGS -DNDEBUG"

AM_PROG_CC_C_O

AC_CHECK_FUNCS([atexit])
AC_CHECK_FUNCS([memset])
AC_CHECK_HEADERS([stdlib.h string.h])
#AC_FUNC_MALLOC
AC_FUNC_VPRINTF
AC_HEADER_STDC

AX_CXXFLAGS_GCC_OPTION(-mstackrealign)

# Check for OpenMP (unless not desired)
OPENMP_CXXFLAGS=""
OPENMP_LIBS=""
HAVE_OPENMP=no
if test x"${enable_openmp}" = xyes ; then
    AX_OPENMP([HAVE_OPENMP=yes])
else
    AC_MSG_NOTICE([Instructed to disable OpenMP. Disabled...])
fi

if test x"${enable_debug}" = xyes ; then
    AC_MSG_NOTICE([Disabling optimizations])
    CXXFLAGS=`echo "$CXXFLAGS" | sed 's,-O3,,g'`
    HAVE_OPENMP=no
fi

if test x"${HAVE_OPENMP}" = xyes ; then
    OPENMP_LIBS=${OPENMP_CXXFLAGS}
fi
if test x"${HAVE_OPENMP}" = xyes ; then
    AC_DEFINE([USE_OPENMP], 1, [Define this to use OpenMP.])
    CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
    AC_SUBST(OPENMP_LIBS)
fi

# Check for SDL (minimum: 1.2.0)
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION, :,
            AC_MSG_ERROR([*** SDL version $SDL_VERSION or later was not found!]))
AC_SUBST(SDL_CXXFLAGS)
AC_SUBST(SDL_LIBS)

# Finally create all the generated files
AC_CONFIG_FILES([Makefile src/Makefile src/version.h]) 
AC_OUTPUT
