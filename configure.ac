AC_INIT([mandelSSE], [1.2], [ttsiod@softlab.ntua.gr])

AC_LANG(C)
AC_CONFIG_HEADERS([src/config.h])

AC_CONFIG_MACRO_DIR([ac-macros])

# Don't clutter this dir, store in build-aux
AC_CONFIG_AUX_DIR([build-aux])

if test x"${CFLAGS}" = x ; then
    CFLAGS="-O3 -g -Wall -Wextra"
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

AX_CFLAGS_GCC_OPTION(-Wall)
AX_CFLAGS_GCC_OPTION(-Wextra)
# Best optimization flags for our compiler
if test x"${enable_debug}" = xyes ; then
	AC_MSG_NOTICE([Disabling optimizations])
	CFLAGS=`echo "$CFLAGS" | sed 's,-O3,,g'`
else
#        AX_CFLAGS_GCC_OPTION(-pedantic)
        AX_CFLAGS_GCC_OPTION(-fomit-frame-pointer)
        AX_CFLAGS_GCC_OPTION(-ffast-math)
        AX_CFLAGS_GCC_OPTION(-funsafe-math-optimizations)
        AX_CFLAGS_GCC_OPTION(-mtune=native)

# GCC support for -flto is still buggy, internal compiler error triggered...
#        AX_CFLAGS_GCC_OPTION(-flto)
#	 AX_CHECK_LINKER_FLAGS(-fwhole-program,[WHOLEPROGRAM="-fwhole-program"],[])
#	 AC_SUBST(WHOLEPROGRAM)

	# SIMD detection - core logic from mplayer

	AC_MSG_CHECKING(for SSE)
	AC_RUN_IFELSE([AC_LANG_SOURCE([#include <stdlib.h>
#include <signal.h>
void catchme(int a) { exit(1); }
int main(void) {
signal(SIGILL, catchme);
__asm__ volatile ("xorps %%xmm0, %%xmm0":::"memory"); return 0;
}
	])], [HAVESSE=yes], [HAVESSE=no])
	AC_MSG_RESULT($HAVESSE)
	if test x"${HAVESSE}" = xyes ; then
	    AC_DEFINE([SIMD_SSE], 1, [Define this to use SSE intrinsics.])
	fi

	AC_MSG_CHECKING(for SSE2)
	AC_RUN_IFELSE([AC_LANG_SOURCE([#include <stdlib.h>
#include <signal.h>
void catchme(int a) { exit(1); }
int main(void) {
signal(SIGILL, catchme);
__asm__ volatile ("xorpd %%xmm0, %%xmm0":::"memory"); return 0;
}
	])], [HAVESSE2=yes], [HAVESSE2=no])
	AC_MSG_RESULT($HAVESSE2)

	AC_MSG_CHECKING(for SSSE3)
	AC_RUN_IFELSE([AC_LANG_SOURCE([#include <stdlib.h>
#include <signal.h>
void catchme(int a) { exit(1); }
int main(void) {
signal(SIGILL, catchme);
__asm__ volatile ("pabsd %%xmm0, %%xmm0":::"memory"); return 0;
}
	])], [HAVESSSE3=yes], [HAVESSSE3=no])
	AC_MSG_RESULT($HAVESSSE3)

	if test x"${HAVESSE}" = xyes ; then
	    AX_CFLAGS_GCC_OPTION(-msse)
	    AX_CFLAGS_GCC_OPTION(-mrecip)
	    AX_CFLAGS_GCC_OPTION(-mfpmath=sse)
	fi
	if test x"${HAVESSE2}" = xyes ; then
	    AX_CFLAGS_GCC_OPTION(-msse2)
	fi
	if test x"${HAVESSSE3}" = xyes ; then
	    AX_CFLAGS_GCC_OPTION(-mssse3)
	fi
        CFLAGS="$CFLAGS -DNDEBUG"
fi

AM_PROG_CC_C_O

AC_CHECK_FUNCS([atexit])
AC_CHECK_HEADERS([stdlib.h string.h])
AC_FUNC_MALLOC
AC_FUNC_VPRINTF
AC_HEADER_STDC

AX_CFLAGS_GCC_OPTION(-mstackrealign)

# Check for OpenMP (unless not desired)
OPENMP_CFLAGS=""
OPENMP_LIBS=""
HAVE_OPENMP=no
if test x"${enable_openmp}" = xyes ; then
    AX_OPENMP([HAVE_OPENMP=yes])
else
    AC_MSG_NOTICE([Instructed to disable OpenMP. Disabled...])
fi
if test x"${HAVE_OPENMP}" = xyes ; then
    OPENMP_LIBS=${OPENMP_CFLAGS}
fi
if test x"${HAVE_OPENMP}" = xyes ; then
    AC_DEFINE([USE_OPENMP], 1, [Define this to use OpenMP.])
    CFLAGS="$CFLAGS $OPENMP_CFLAGS"
    AC_SUBST(OPENMP_LIBS)
fi

# Check for SDL (minimum: 1.2.0)
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION, :,
            AC_MSG_ERROR([*** SDL version $SDL_VERSION or later was not found!]))
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

# Finally create all the generated files
AC_CONFIG_FILES([Makefile src/Makefile]) 
AC_OUTPUT
