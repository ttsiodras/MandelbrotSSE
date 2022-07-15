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
    printf("Usage: %s [-a|-m] [-h] [-b] [-v|-s|-d] [-i iter] [-p pct] [-f rate] [WIDTH HEIGHT]\n", argv[0]);
    puts("Where:");
    puts("\t-h\tShow this help message");
    puts("\t-m\tRun in mouse-driven mode");
    puts("\t-a\tRun in autopilot mode (default)");
    puts("\t-b\tRun in benchmark mode (implies autopilot)");
    puts("\t-v\tForce use of AVX");
    puts("\t-s\tForce use of SSE");
    puts("\t-d\tForce use of non-AVX, non-SSE code");
    puts("\t-i iter\tThe maximum number of iterations of the Mandelbrot loop (default: 2048)");
    puts("\t-p pct\tThe percentage of pixels computed per frame (default: 0.75)");
    puts("\t      \t(the rest are copied from the previous frame)");
    puts("\t-f fps\tEnforce upper bound of frames per second (default: 60)");
    puts("\t      \t(use 0 to run at full possible speed)\n");
    puts("If WIDTH and HEIGHT are not provided, they default to: 1024 768");
    exit(1);
}

int main(int argc, char *argv[])
{
    int opt, fps = 60;
    bool autoPilot = true, benchmark = false;
    bool forceAVX = false, forceSSE = false, forceDefault = false;
    double percent = 0.75;

    iterations = 2048;

    while ((opt = getopt(argc, argv, "hmabvsdi:p:f:")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv);
                break;
            case 'm':
                autoPilot = false;
                break;
            case 'a':
                autoPilot = true;
                break;
            case 'b':
                autoPilot = true;
                benchmark = true;
                break;
            case 'v':
                forceAVX = true;
                break;
            case 's':
                forceSSE = true;
                break;
            case 'd':
                forceDefault = true;
                break;
            case 'i':
                if (1 != sscanf(optarg, "%d", &iterations))
                    panic("[x] Invalid number of iterations: '%s'", optarg);
                break;
            case 'f':
                if (1 != sscanf(optarg, "%d", &fps))
                    panic("[x] Invalid frame rate: '%s'", optarg);
                break;
            case 'p':
                if (1 != sscanf(optarg, "%lf", &percent))
                    panic("[x] Invalid percentage: '%s'", optarg);
                if (0.05 > percent || 100.0 < percent)
                    panic("[x] Invalid percentage: '%s' (0.05 <= percent <= 100.0)", optarg);
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
    if (!fps || benchmark)
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
    if (!autoPilot)
        puts("[-] e.g. you can use '-a' to enable autopilot.");
    else
        puts("[-] e.g. you can use '-m' to pilot with your mouse.");
    printf("[-]\n[-] Autopilot:  %s\n", autoPilot ? "On" : "Off");
    printf("[-] Benchmark:  %s\n", benchmark ? "On" : "Off");
    printf("[-] Dimensions: %ld x %ld\n", MAXX, MAXY);
    if (!minimum_ms_per_frame)
        puts("[-] FPS Limit:  unlimited");
    else
        printf("[-] FPS Limit:  %d frames/sec\n", fps);
#ifdef __x86_64__
    if (forceAVX)
        CoreLoopDouble = CoreLoopDoubleAVX;
    else if (forceSSE)
        CoreLoopDouble = CoreLoopDoubleSSE;
    else if (forceDefault)
        CoreLoopDouble = CoreLoopDoubleDefault;
    else
        CoreLoopDouble = 
            __builtin_cpu_supports("avx") ?  CoreLoopDoubleAVX
            : __builtin_cpu_supports("sse") ?  CoreLoopDoubleSSE
            : CoreLoopDoubleDefault;
    printf("[-] Mode:       %s\n", 
        CoreLoopDouble == CoreLoopDoubleAVX ? "AVX" 
        : CoreLoopDouble == CoreLoopDoubleSSE ? "SSE" 
        : "non-AVX/non-SSE");
    printf("[-] Iterations: %d\n", iterations);
#else
    CoreLoopDouble = CoreLoopDoubleDefault;
    printf("[-] Mode: %s\n", "non-AVX");
#endif

    const char *windowTitle;
    if (autoPilot)
        windowTitle = "ESC to quit...";
    else
        windowTitle = "Left click/hold zooms-in, right zooms-out, ESC quits.";
    init256colorsMode(windowTitle);

    double fps_reported;
    if (autoPilot) {
        srand(time(NULL));
        fps_reported = autopilot(percent, benchmark);
    } else
        fps_reported = mousedriven(percent);
    SDL_Quit();
    printf("[-] Frames/sec: %5.2f\n\n", fps_reported);
    fflush(stdout);
    return 0;
}
