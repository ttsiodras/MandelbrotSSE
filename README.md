WHAT IS THIS?
=============

This is a real-time Mandelbrot fractal zoomer.

COMPILE/INSTALL/RUN
===================

Windows
-------
Windows users can download and run a pre-compiled Windows binary
[here](https://github.com/ttsiodras/MandelbrotSSE/releases/download/2.1/mandelSSE-win32-2.1.zip).

After decompressing, you can simply execute either one of the two .bat
files. The 'autopilot' one zooms in a specific location, while the other
one allows you to zoom interactively using your mouse (left-click zooms in,
right-click zooms out).

For Linux/BSD/OSX users
-----------------------

Make sure you have libSDL installed - then...

    $ ./configure
    $ make

You can then simply...

    $ src/mandelSSE -h

    Usage: ./src/mandelSSE [-a] [-s|-x] [-h] [WIDTH HEIGHT]
    Where:
        -h      Show this help message
        -a      Run in autopilot mode (default: mouse mode)
        -s      Use SSE and OpenMP
        -x      Use XaoS algorithm (default)
    If WIDTH and HEIGHT are not provided, they default to: 800 600

    $ src/mandelSSE -a 1024 768
    (Runs in autopilot in 1024x768 window, using XaoS)

    $ src/mandelSSE -s 1024 768
    (Runs in mouse-driven SSE mode, in a 1024x768 window)
    (left-click zooms-in, right-click zooms out)

    $ src/mandelSSE -x 1024 768
    (same as before, but in XaoS mode - much faster, esp during deep zooms)

WHAT IS THIS, AGAIN?
====================

When I got my hands on an SSE enabled processor (an Athlon-XP, back in 2002),
I wanted to try out SSE programming... And over the better part of a weekend,
I created a simple implementation of a Mandelbrot zoomer in SSE assembly.
I was glad to see that my code was almost 3 times faster than pure C.

But that was just the beginning.
Over the last two decades, I kept coming back to this, enhancing it.

- I learned how to use the GNU autotools, and made it work on most Intel
  platforms: checked with Linux, Windows (MinGW) and OpenBSD.

- After getting acquainted with OpenMP, in Nov 2009 I added OpenMP #pragmas
  to run both the C and the SSE code in all cores/CPUs. The SSE code had to
  be moved from a separate assembly file into inlined code - but the effort
  was worth it. The resulting frame rate - on a tiny Atom 330 running Arch
  Linux - sped up from 58 to 160 frames per second.

- I then coded it in CUDA - a 75$ GPU card gave almost two orders of
  magnitude of speedup!

- Then in May 2011, I made the code switch automatically from single precision
  floating point to double precision - when one zooms "deep enough".

- Around 2012 I added a significant optimization: avoiding fully calculating
  the Mandelbrot lake areas (black color) by drawing at 1/16 resolution and
  skipping black areas in full res...

- I learned enough VHDL in 2018 to [code the algorithm inside a Spartan3
  FPGA](https://www.youtube.com/watch?v=yFIbjiOWYFY). That was quite a
  [learning exercise](https://github.com/ttsiodras/MandelbrotInVHDL).

- In September 2020 I [ported a fixed-point arithmetic](
  https://github.com/ttsiodras/Blue_Pill_Mandelbrot/) version of the
  algorithm [inside a 1.4$ microcontroller](
  https://www.youtube.com/watch?v=5875JOnFDLg).

- And finally (?), in October 2020, I implemented what I understood to be
  the XaoS algorithm - that is, re-using pixels from the previous frame
  to optimally update the next one. Especially in deep-dives and large
  windows, this delivers amazing speedups.

CODERS ONLY
===========

SSE code
--------

This is the main loop (brace yourselves):

        ;  x' = x^2 - y^2 + a
        ;  y' = 2xy + b
        ;
        mov     ecx, 0
        movaps  xmm5, [fours]     ; 4.     4.     4.     4.       ; xmm5
        movaps  xmm6, [re]        ; a0     a1     a2     a3       ; xmm6
        movaps  xmm7, [im]        ; b0     b1     b2     b3       ; xmm7
        xorps   xmm0, xmm0        ; 0.     0.     0.     0.
        xorps   xmm1, xmm1        ; 0.     0.     0.     0.
        xorps   xmm3, xmm3        ; 0.     0.     0.     0.       ; xmm3
    loop1:
        movaps  xmm2, xmm0        ; x0     x1     x2     x3       ; xmm2
        mulps   xmm2, xmm1        ; x0*y0  x1*y1  x2*y2  x3*y3    ; xmm2
        mulps   xmm0, xmm0        ; x0^2   x1^2   x2^2   x3^2     ; xmm0
        mulps   xmm1, xmm1        ; y0^2   y1^2   y2^2   y3^2     ; xmm1
        movaps  xmm4, xmm0
        addps   xmm4, xmm1        ; x0^2+y0^2  x1...              ; xmm4
        subps   xmm0, xmm1        ; x0^2-y0^2  x1...              ; xmm0
        addps   xmm0, xmm6        ; x0'    x1'    x2'    x3'      ; xmm0
        movaps  xmm1, xmm2        ; x0*y0  x1*y1  x2*y2  x3*y3    ; xmm1
        addps   xmm1, xmm1        ; 2x0*y0 2x1*y1 2x2*y2 2x3*y3   ; xmm1
        addps   xmm1, xmm7        ; y0'    y1'    y2'    y3'      ; xmm1
        cmpltps xmm4, xmm5        ; <4     <4     <4     <4 ?     ; xmm2
        movaps  xmm2, xmm4

    ; at this point, xmm2 has all 1s in the non-overflowed pixels

        movmskps eax, xmm4        ; (lower 4 bits reflect comparisons)
        andps   xmm4, [ones]      ; so, prepare to increase the non-over
        addps   xmm3, xmm4        ; by updating the 4 bailout counters
        or      eax, eax          ; have all 4 pixels overflowed ?
        jz      short nomore      ; yes, we're done

        inc     ecx
        cmp     ecx, 119
        jnz     short loop1

XaoS
----

The idea behind the XaoS algorithm is simple: don't redraw the pixels;
instead re-use as many as you can from the previous frame.

The devil, as ever, is in the details.

The way I implemented this is as follows: the topmost scaline goes
from X coordinate `xld` to `xru` - in `xstep` steps (see code
for details). I store these computed coordinates in array `xcoord`;
and in the next frame, I compare the new coordinates with the old 
ones. For each pixel, I basically find the closest X coordinate match.

I do the same for the Y coordinates. In both cases, we are talking
about a 1-dimensional array, of MAXX or MAXY length.

After I have the matches, I sort them - based on distance to the
coordinates of the previous frame. The `mandel` function then forces
a redraw for the worst N columns/rows - where N comes as a percentage
parameter in the function call. Simply put, if the pixel's
X and Y coordinates fall on "slots" that are close enough to the
old frame's `xcoord` and `ycoord`, the pixel color is taken
from the previous frame without doing the expensive Mandelbrot
calculation.

This works perfectly - the zoom becomes nice and smooth, and is
also improved with a full Mandelbrot render the moment the user
stops zoooming.

The code has a lot of comments explaining the inner-workings in detail.
Have a look!

Cross compiling for Windows via MinGW
-------------------------------------
After decompressing the SDL 1.2.15 tarball, install MinGW:

    $ sudo apt install gcc-mingw-w64

Then compile libSDL:

    $ cd SDL-1.2.15
    $ ./configure --host=x86_64-w64-mingw32 --disable-dga \
            --disable-video-dga --disable-video-x11-dgamouse \
            --disable-video-x11 --disable-x11-shared \
            --prefix=/usr/local/packages/SDL-1.2.15-win32
    $ make
    $ sudo make install

Finally, come back to this source folder, and compile the XaoS
version:

    $ ./configure --host=x86_64-w64-mingw32 \
            --disable-sse --disable-sse2 --disable-ssse3 \
            --with-sdl-prefix=/usr/local/packages/SDL-1.2.15-win32 \
            --disable-sdltest
    $ make
    $ cp src/mandelSSE.exe \
            /usr/local/packages/SDL-1.2.15-win32/bin/SDL.dll \
            /some/path/for/Windows/

This will suffice for "-x" (XaoS) execution; if you want SSE/OpenMP,
turn the --disable above into --enable... and remember to copy the
libgomp DLL, too.

MISC
====
Since it reports frame rate at the end, you can use this as a benchmark
for SSE instructions - it puts the SSE registers under quite a load.

I've also coded a
[CUDA version](https://www.thanassis.space/mandelcuda-1.0.tar.bz2),
which you can play with, if you have an NVIDIA card.
Some details about it, in the blog post I wrote back in 2009 about
it [here](https://www.thanassis.space/mandelSSE.html).
