#include "common.h"

// Helper to report a fatal error
void panic(char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
    exit(1);
}

// Setup window we will draw in
void init256()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        panic("[x] Couldn't initialize SDL: %d\n", SDL_GetError());
    atexit(SDL_Quit);

    surface = SDL_SetVideoMode(MAXX,
                               MAXY, 8, SDL_HWSURFACE | SDL_HWPALETTE);
    if (!surface)
        panic("[x] Couldn't set video mode: %d", SDL_GetError());

    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) < 0)
            panic("[x] Couldn't lock surface: %d", SDL_GetError());
    }
    buffer = (Uint8*)surface->pixels;

    // A palette for Mandelbrot zooms...
    {
        SDL_Color palette[256];
        for (int value = 0; value<128; value++) {
            unsigned char red, green, blue;
            unsigned char quadrant = value / 32;
            if (quadrant == 0) {
                blue = 248;
                green = 8 * (value % 32);
                red = 0;
            } else if (quadrant == 1) {
                blue = 8*(31 - (value % 32));
                green = 248;
                red = 0;
            } else if (quadrant == 2) {
                blue = 0;
                green = 248;
                red = 8*(value % 32);
            } else {
                blue = 0;
                green = 248 - 8 * (value % 32);
                red = 248;
            }
            palette[value].r = red;
            palette[value].g = green;
            palette[value].b = blue;
        }
        palette[128].r = 0;
        palette[128].g = 0;
        palette[128].b = 0;

        SDL_SetColors(surface, palette, 0, 256);
    }
}

int kbhit(int *xx, int *yy)
{
    int x,y;
    SDL_Event event;

    Uint8 *keystate = SDL_GetKeyState(NULL);
    if ( keystate[SDLK_ESCAPE] )
        return 1;

    if(SDL_PollEvent(&event)) {
        switch(event.type) {
        case SDL_QUIT:
            return 1;
            break;
        default:
            break;
        }
    }

    Uint8 btn = SDL_GetMouseState (&x, &y);
    if (btn & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        *xx = x;
        *yy = y;
        return 2;
    }
    if (btn & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
        *xx = x;
        *yy = y;
        return 3;
    }
    return 0;
}
