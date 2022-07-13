#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdarg.h>

#include <SDL.h>

// Number of Mandelbrot iterations per pixel
#define ITERA 2048

// Number of frames to zoom-in
#define ZOOM_FRAMES 2500

// The global variables.
// Only one file defines GLOBAL (to nothing) before #include-ing this file.
#ifndef GLOBAL
#define GLOBAL extern
#endif

// Window dimensions
GLOBAL long MAXX, MAXY;

// 60 fps maximum => minimum milliseconds per frame = 1000/60 = 17
// Default value set in getopt parsing in main.
GLOBAL unsigned minimum_ms_per_frame;

// SDL stuff...
GLOBAL SDL_Window *window;
GLOBAL SDL_Renderer *renderer;
GLOBAL SDL_Surface *surface;

// Window title
GLOBAL const char *windowTitle;

// Dispatching in AVX/non-AVX code.
GLOBAL void (*CoreLoopDouble)(double xcur, double ycur, double xstep, unsigned char **p);

// Print message and exit
void panic(const char *fmt, ...);

// returns 1 if ESC is hit
// returns 2 if left click (and updates xx and yy with mouse coord)
// returns 3 if right click (and updates xx and yy with mouse coord)
int kbhit(int *xx, int *yy);

// Creates the window and sets up the palette of colors.
void init256();

#endif
