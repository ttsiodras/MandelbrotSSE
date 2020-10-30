#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdarg.h>

#include <SDL.h>

// Number of Mandelbrot iterations per pixel
#define ITERA 2048

// The global variables.
// Only one file defines GLOBAL (to nothing) before #include-ing this file.
#ifndef GLOBAL
#define GLOBAL extern
#endif

// Window dimensions
GLOBAL long MAXX, MAXY;

// SDL surface we draw in
GLOBAL SDL_Surface *surface;

// The surface buffer; this is where we place our pixel data in
GLOBAL Uint8 *buffer;

// Remote message and exit
void panic(char *fmt, ...);

// returns 1 if ESC is hit
// returns 2 if left click (and updates xx and yy with mouse coord)
// returns 3 if right click (and updates xx and yy with mouse coord)
int kbhit(int *xx, int *yy);

// Creates the window and sets up the palette of colors.
void init256(int bSSE);

#endif
