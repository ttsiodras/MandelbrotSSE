#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdarg.h>

#include <SDL.h>

// Number of frames to zoom-in
#define ZOOM_FRAMES 2500

// The global variables.
// Only one file defines GLOBAL (to nothing) before #include-ing this file.
#ifndef GLOBAL
#define GLOBAL extern
#endif

// Fractal resolution
GLOBAL long MAXX, MAXY;

// Number of Mandelbrot iterations per pixel
GLOBAL int iterations;

// 60 fps maximum => minimum milliseconds per frame = 1000/60 = 17
// Default value set in getopt parsing in main.
GLOBAL unsigned minimum_ms_per_frame;

// SDL global state
GLOBAL SDL_Window *window;
GLOBAL SDL_Renderer *renderer;
GLOBAL SDL_Surface *surface;
GLOBAL int window_width, window_height;

// Function pointer, dispatching to AVX/non-AVX code.
GLOBAL void (*CoreLoopDouble)(double xcur, double ycur, double xstep, unsigned char **p);

// Print message and exit
void panic(const char *fmt, ...);

// returns SDL_QUIT if ESC is hit or the user closes the window
// returns SDL_BUTTON_LEFT if left click (and updates xx and yy with mouse coord)
// returns SDL_BUTTON_RIGHT if right click (and updates xx and yy with mouse coord)
// returns SDL_WINDOWEVENT if we need to repaint (e.g. resize)
int kbhit(int *xx, int *yy);

// Creates the window and sets up the palette of colors.
void init256colorsMode(const char *windowTitle);

#endif
