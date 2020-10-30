#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#define GLOBAL
#include "common.h"

#include "version.h"

// XaoS algorithm implemenation
#include "xaos.h"

// SSE algorithm implemenation
#include "sse.h"

void usage(char *argv[])
{
    printf("Usage: %s [-a] [-s|-x] [-h] [WIDTH HEIGHT]\n", argv[0]);
    puts("Where:");
    puts("\t-h\tShow this help message");
    puts("\t-a\tRun in autopilot mode (default: mouse mode)");
    puts("\t-s\tUse SSE and OpenMP");
    puts("\t-x\tUse XaoS algorithm (default)");
    puts("If WIDTH and HEIGHT are not provided, they default to: 800 600");
    exit(1);
}

int main(int argc, char *argv[])
{
    int i, bAutoPilot = 0, bSSE = 0;

    for(i=1; i<argc; i++) {
        if (!strcmp(argv[i], "-a"))
            bAutoPilot = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?"))
            usage(argv);
        else if (!strcmp(argv[i], "-s"))
            bSSE = 1;
        else if (!strcmp(argv[i], "-x"))
            bSSE = 0;
        else
            break;
    }
    if (i == argc) {
        MAXX = 800;
        MAXY = 600;
    } else if (i == argc-2) {
        MAXX = atoi(argv[i]);
        MAXY = atoi(argv[i+1]);
        if (!MAXX || !MAXY) {
            panic(
                "[x] Failed to parse integer values "
                "for WIDTH(%s) and HEIGHT(%s)\n", argv[i], argv[i+1]);
        }
        MAXX = 16*(MAXX/16);
        MAXY = 16*(MAXY/16);
    } else
        usage(argv);

    printf("\n[-] Mandelbrot Zoomer by Thanassis, version: %s\n", version);
    if (!bAutoPilot)
        puts("[-] Note you can launch with option '-a' to enable autopilot.");

    init256(bSSE);

    const char *usage =
        "Left click to zoom-in, right-click to zoom-out, ESC to quit...";
    SDL_WM_SetCaption(usage, usage);

    if (bSSE)
        return mandelSSE(bAutoPilot);

    unsigned en, st = SDL_GetTicks();
    unsigned frames;
    if (bAutoPilot)
        frames = autopilot();
    else
        frames = mousedriven();
    en = SDL_GetTicks();

    printf("[-] Frames/sec:%5.2f\n\n",
           ((float) frames) / ((en - st) / 1000.0f));
    fflush(stdout);
    return 0;
}
