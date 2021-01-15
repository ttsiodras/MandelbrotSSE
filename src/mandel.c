#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

// With this macro defined... 
#define GLOBAL
// ...common.h defines (instead of just declaring) the global variables.
#include "common.h"

// Auto-generated from ac-macros/revision_utils.m4 (autoconf)
#include "version.h"

// XaoS algorithm implemenation
#include "xaos.h"

// SSE algorithm implemenation
#include "sse.h"

// For getopt (command-line argument processing)
#include <unistd.h>

void usage(char *argv[])
{
    printf("Usage: %s [-a] [-s|-x] [-h] [-f rate] [WIDTH HEIGHT]\n", argv[0]);
    puts("Where:");
    puts("\t-h\tShow this help message");
    puts("\t-m\tRun in mouse-driven mode");
    puts("\t-a\tRun in autopilot mode (default)");
    puts("\t-x\tUse XaoS algorithm with SSE2 and OpenMP (default)");
    puts("\t-s\tUse naive algorithm with SSE, SSE2 and OpenMP");
    puts("\t-f fps\tEnforce upper bound of frames per second (default: 60)");
    puts("\t      \t(use 0 to run at full possible speed)\n");
    puts("If WIDTH and HEIGHT are not provided, they default to: 1024 768");
    exit(1);
}

int main(int argc, char *argv[])
{
    int opt, bAutoPilot = 1, bSSE = 0, fps = 60;

    while ((opt = getopt(argc, argv, "hmaxsf:")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv);
                break;
            case 'm':
                bAutoPilot = 0;
                break;
            case 'a':
                bAutoPilot = 1;
                break;
            case 'x':
                bSSE = 0;
                break;
            case 's':
                bSSE = 1;
                break;
            case 'f':
                if (1 != sscanf(optarg, "%d", &fps))
                    panic("[x] Not a valid frame rate: '%s'", optarg);
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (optind == argc) {
        MAXX = 1024;
        MAXY = 768;
    } else if (optind == argc-1) {
        puts("[x] You didn't pass the MAXY argument!\n");
        usage(argv);
    } else if (optind == argc-2) {
        MAXX = atoi(argv[optind]);
        MAXY = atoi(argv[optind+1]);
        if (!MAXX || !MAXY) {
            panic(
                "[x] Failed to parse integer values for "
                "WIDTH(%s) and HEIGHT(%s)\n", argv[optind], argv[optind+1]);
        }
        MAXX = 16*(MAXX/16);
        MAXY = 16*(MAXY/16);
    } else {
        puts("[x] Too many parameters... Only MAXX and MAXY expected.\n\n");
        usage(argv);
    }
    if (!fps)
        // run with no brakes!
        minimum_ms_per_frame = 0;
    else {
        // enforce upper boundary of frame rate
        minimum_ms_per_frame = 1000/fps;
        if (minimum_ms_per_frame < 10) {
            minimum_ms_per_frame = 10; // SDL_Delay granularity
            fps = 100;
        }
    }

    printf("\n[-] Mandelbrot Zoomer by Thanassis, version: %s\n", version);
    if (!bAutoPilot)
        puts("[-] NOTE: you can launch with option '-a' to enable autopilot.");
    else
        puts("[-] NOTE: you can launch with option '-m' to pilot with your mouse.");
    printf("[-]\n[-] Mode:       %s\n", bSSE ? "naive SSE" : "XaoS");
    printf("[-] Autopilot:  %s\n", bAutoPilot ? "On" : "Off");
    printf("[-] Dimensions: %ld x %ld\n", MAXX, MAXY);
    if (!minimum_ms_per_frame)
        puts("[-] FPS Limit:  unlimited");
    else
        printf("[-] FPS Limit:  %d frames/sec\n", fps);

    init256(bSSE);

    const char *usage;
    if (bAutoPilot)
        usage = "ESC to quit...";
    else
        usage = "Left click to zoom-in, right-click to zoom-out, ESC to quit...";
    SDL_WM_SetCaption(usage, usage);

    if (bSSE)
        return mandelSSE(bAutoPilot);

    unsigned en, st = SDL_GetTicks();
    unsigned frames;
    if (bAutoPilot) {
        srand(time(NULL));
        frames = autopilot();
    } else
        frames = mousedriven();
    en = SDL_GetTicks();

    printf("[-] Frames/sec: %5.2f\n\n",
           ((float) frames) / ((en - st) / 1000.0f));
    fflush(stdout);
    return 0;
}
