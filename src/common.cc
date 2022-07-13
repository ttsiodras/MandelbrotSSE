#include "common.h"

// Helper to report a fatal error
void panic(const char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
    exit(1);
}

// Creates the window and sets up the palette of colors.
void init256()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        panic("[x] Couldn't initialize SDL: %d\n", SDL_GetError());
    atexit(SDL_Quit);

    window = SDL_CreateWindow(
        windowTitle,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        MAXX, MAXY, 0);
    if (!window)
        panic("[x] Couldn't create window: %d", SDL_GetError());

    if (!minimum_ms_per_frame)
        renderer = SDL_CreateRenderer(window, -1, 0);
    else
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        panic("[x] Couldn't create renderer: %d", SDL_GetError());

    surface = SDL_CreateRGBSurface(
        SDL_SWSURFACE, MAXX, MAXY, 8, 0, 0, 0, 0);

    // A palette for Mandelbrot zooms...
    {
        SDL_Color palette[256];
        for (int value = 0; value<256; value++) {
            unsigned char red, green, blue;
            unsigned char quadrant = value / 32;
            if (quadrant == 0) {
                blue = 0;
                green = 8 * (value % 32);
                red = 0;
            } else if (quadrant == 1) {
                blue = 8*(value % 32);
                green = 248;
                red = 0;
            } else if (quadrant == 2) {
                blue = 248;
                green = 248;
                red = 8*(value % 32);
            } else if (quadrant == 3) {
                blue = 8*(31 - (value % 32));
                green = 248;
                red = 248;
            } else if (quadrant == 4) {
                blue = 0;
                green = 8*(31 - (value % 32));
                red = 248;
            } else if (quadrant == 5) {
                blue = 8*(value % 32);
                green = 0;
                red = 8*(31 - (value % 32));
            } else if (quadrant == 6) {
                blue = 248;
                green = 8*(value % 32);
                red = 0;
            } else {
                blue = 8*(31 - (value % 32));
                green = 8*(31 - (value % 32));
                red = 0;
            }
            palette[value].r = red;
            palette[value].g = green;
            palette[value].b = blue;
            palette[value].a = 255;
        }
        palette[0].r = 0;
        palette[0].g = 0;
        palette[0].b = 0;
        palette[0].a = 255;
        SDL_SetPaletteColors(surface->format->palette, palette, 0, 256);
    }
}

// returns 1 if ESC is hit
// returns 2 if left click (and updates xx and yy with mouse coord)
// returns 3 if right click (and updates xx and yy with mouse coord)
int kbhit(int *xx, int *yy)
{
    int x,y;
    SDL_Event event;

    const Uint8 *keystate = SDL_GetKeyboardState(NULL);
    if ( keystate[SDL_SCANCODE_ESCAPE] )
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
