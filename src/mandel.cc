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
    printf("Usage: %s [-a|-m] [-h] [-f rate] [WIDTH HEIGHT]\n", argv[0]);
    puts("Where:");
    puts("\t-h\tShow this help message");
    puts("\t-m\tRun in mouse-driven mode");
    puts("\t-a\tRun in autopilot mode (default)");
    puts("\t-f fps\tEnforce upper bound of frames per second (default: 60)");
    puts("\t      \t(use 0 to run at full possible speed)\n");
    puts("If WIDTH and HEIGHT are not provided, they default to: 1024 768");
    exit(1);
}

int main(int argc, char *argv[])
{
    int opt, bAutoPilot = 1, fps = 60;

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
            case 'f':
                if (1 != sscanf(optarg, "%d", &fps))
                    panic("[x] Not a valid frame rate: '%s'", optarg);
                break;
            default: /* '?' */
                usage(argv);
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
        puts("[-] NOTE: you can launch with option '-h' to see available options.");
    if (!bAutoPilot)
        puts("[-] e.g. you can use '-a' to enable autopilot.");
    else
        puts("[-] e.g. you can use '-m' to pilot with your mouse.");
    printf("[-] Autopilot:  %s\n", bAutoPilot ? "On" : "Off");
    printf("[-] Dimensions: %ld x %ld\n", MAXX, MAXY);
    if (!minimum_ms_per_frame)
        puts("[-] FPS Limit:  unlimited");
    else
        printf("[-] FPS Limit:  %d frames/sec\n", fps);
    CoreLoopDouble =  (__builtin_cpu_supports("avx")) ? CoreLoopDoubleAVX : CoreLoopDoubleDefault;
    printf("[-] Mode: %s\n", (__builtin_cpu_supports("avx")) ? "AVX" : "non-AVX");

    init256();

    const char *usage;
    if (bAutoPilot)
        usage = "ESC to quit...";
    else
        usage = "Left click to zoom-in, right-click to zoom-out, ESC to quit...";
    SDL_WM_SetCaption(usage, usage);

    double fps_reported;
    if (bAutoPilot) {
        srand(time(NULL));
        fps_reported = autopilot();
    } else
        fps_reported = mousedriven();
    SDL_Quit();
    printf("[-] Frames/sec: %5.2f\n\n", fps_reported);
    fflush(stdout);
    return 0;
}
