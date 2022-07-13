#include <math.h>
#include <float.h>

#include "common.h"
#include "xaos.h"
#include "sse.h"

// Beyond this, things get shaky...
#define ZOOM_LIMIT 1e295*MAXX*DBL_MIN

// Structure used to sort the coordinate distances from the previous frame.
// Keep reading further below to understand how this is used.
typedef struct tagPoint {
    double distance;
    int idx_original;
    int idx_best;
} Point;

// Sorting function for quicksort.
int compare_points(const void *p1, const void *p2)
{
    double d1 = ((Point*)p1)->distance;
    double d2 = ((Point*)p2)->distance;
    return \
        (d2 < d1) ? -1 :
        (d2 > d1) ?  1 :
        0;
}

#if defined(__x86_64__) && !defined(__WIN64__)
#define AUTO_DISPATCH __attribute__((target_clones("default","avx")))
#else
#define AUTO_DISPATCH
#endif

AUTO_DISPATCH
void mandel(
    double xld, double yld, double xru, double yru,
    double percentageOfPixelsToRedraw)
{
    // Helper counters
    int i, j;

    // The steps we move per each iteration of the loop (at x and y directions)
    // as well as the current X and Y coordinate on the complex plane.
    double xstep, ystep, xcur, ycur;

    // The buffer index we draw in; 0 -> 1 -> 0 -> 1 ...
    static int bufIdx = 0;

    // The double-buffered X and Y coordinates
    static double *xcoords[2] = {NULL, NULL}, *ycoords[2] = {NULL, NULL};

    // The lookup tables telling us where a close enough pixel can be found
    static int *xlookup, *ylookup;

    // For Xaos algorithm to work, I need two screen buffers;
    // one to draw in, and one with the previous frame.
    // XaoS algorithm will try to reuse as many of the pixels
    // from the old frame as possible.
    static Uint8 *bufferMem[2];

    // As we zoom deeper and deeper, the reused pixels lie further and
    // further away from the one we are drawing. Inevitably, we end up
    // needing to recalculate. When that happens, if it impacts many
    // pixels, we get a sudden - and very noticeable - drop in framerate.
    // To avoid this, we need to strike a balance between reusing old
    // pixels and recomputing new ones. If we constantly use old pixels,
    // we will go...
    //
    //      fast/fast/fast/SLOW/fast/fast/fast/SLOW...
    //
    // It's much better - visually - to be fast_but_not_fast_as we_could,
    // but to do this consistently - with a near constant frame rate.
    //
    // To that end, after we compute the distances from the new coordinates
    // to those in the old frame, we will sort them - and the topmost pixels
    // in the list (based on percentageOfPixelsToRedraw)
    // will be forced to update.
    static Point *points;

    int bFirstFrameEver = !xcoords[0];

    // In the first execution of this function, allocate the buffers
    if (bFirstFrameEver) {
        // We need to store the X and Y coordinates of the pixels in both
        // horizontal and vertical directions.
        // And we need to do it for this frame, and the previous frame.
        // Ergo, allocate two buffers for each.
        for (i=0; i<2; i++) {
            xcoords[i] = new double[MAXX];
            ycoords[i] = new double[MAXY];
	    if (!xcoords[i] || !ycoords[i])
	       panic("Out of memory");
            memset(xcoords[i], 0, MAXX*sizeof(double));
            memset(ycoords[i], 0, MAXY*sizeof(double));

            // We also need two screen buffers - one to draw in,
            // and one with the old frame.
            bufferMem[i] = new Uint8[MAXX*MAXY];
	    if (!bufferMem[i])
	       panic("Out of memory");
        }

        // In this frame, we will compute a lookup for each X and Y;
        // it will be -1 if the xcoords/ycoords are too different
        // from those of the previous frame (forcing a recalculation).
        // Otherwise, it will be an index into the "close enough" pixel.
        xlookup = new int[MAXX];
        ylookup = new int[MAXY];
        if (!xlookup || !ylookup)
           panic("Out of memory");

        // To determine the pixels whose coordinates are close enough to
        // those of the previous frame, MAXX slots will suffice;
        // for both X and Y directions, since MAXX > MAXY.
        points = new Point[MAXX];
	if (!points)
           panic("Out of memory");
    }

    // Move to the next buffer
    bufIdx ^= 1;

    // Steps from each pixel to the next
    xstep = (xru - xld)/MAXX;
    ystep = (yru - yld)/MAXY;

    xcur = xld;
    for (i=0; i<MAXX; i++) {
        int idx_best = -1;
        double diff = 1e10;
        // we compute xcoords with a linear step from xld to xru.
        xcoords[bufIdx][i] = xcur;
        // We then scan the xcoords of the previous frame, to find
        // the one with the closest coordinate.
        for (j=i-30; j<i+30; j++) {
            if(j<0) continue;
            if(j>MAXX-1) continue;
            double ndiff = fabs(xcur - xcoords[bufIdx^1][j]);
            if (ndiff < diff) {
                diff = ndiff;
                idx_best = j;
            }
        }
        // ...and store the data with computed distances for all X pixels
        points[i].distance = diff;
        points[i].idx_best = idx_best;
        points[i].idx_original = i;
        xcur += xstep;
    }
    // We then sort based on distance. The pixels that have the 
    // greatest distances will be in the front of the array.
    qsort(points, MAXX, sizeof(Point), compare_points);
    // We scan the array, starting from the pixels with the largest distance
    // and moving to better ones
    for(i=0; i<MAXX; i++) {
        int orig_idx = points[i].idx_original;
        int idx_best = points[i].idx_best;
        if (bFirstFrameEver || (i<MAXX*percentageOfPixelsToRedraw/100))
            // we force a redraw for the requested percentage of pixels
            xlookup[orig_idx] = -1;
        else {
            // For the rest, we store the index into the best approximation
            xlookup[orig_idx] = idx_best;
            // But also update the xcoord, to indicate we didn't
            // really compute an accurate one; we re-used the old one!
            xcoords[bufIdx][orig_idx] = xcoords[bufIdx^1][idx_best];
        }
    }

    // The same code, but for the Y coordinates.
    // See comments above to understand how this works.
    ycur = yru;
    for (i=0; i<MAXY; i++) {
        int idx_best = -1;
        double diff = 1e10;
        ycoords[bufIdx][i] = ycur;
        for (j=i-30; j<i+30; j++) {
            if(j<0) continue;
            if(j>MAXY-1) continue;
            double ndiff = fabs(ycur - ycoords[bufIdx^1][j]);
            if (ndiff < diff) {
                diff = ndiff;
                idx_best = j;
            }
        }
        points[i].distance = diff;
        points[i].idx_best = idx_best;
        points[i].idx_original = i;
        ycur -= ystep;
    }
    qsort(points, MAXY, sizeof(Point), compare_points);
    for(i=0; i<MAXY; i++) {
        int orig_idx = points[i].idx_original;
        int idx_best = points[i].idx_best;
        if (bFirstFrameEver || (i<MAXY*percentageOfPixelsToRedraw/100))
            ylookup[orig_idx] = -1;
        else {
            ylookup[orig_idx] = idx_best;
            ycoords[bufIdx][orig_idx] = ycoords[bufIdx^1][idx_best];
        }
    }

    // Armed now with the xlookup and ylookup, we can render the frame.
#pragma omp parallel for private(xcur, j)
    for (int i=0; i<MAXY; i++) {
        // ...and start descending scanlines, from yru down to yld,
        // one ystep at a time.
        double ycur = yru - i*ystep;
        // Set the output pointer to the proper buffer
        unsigned char *p = &bufferMem[bufIdx][i*MAXX];
        int yclose = ylookup[i];
        // Start moving from xld to xru, one xstep at a time
        xcur = xld;
        for (j=0; j<MAXX; j+=4) {
            // if both the xlookup and ylookup indicate that we can
            // lookup a pixel from the old frame...
            int xclose = xlookup[j];
            int xclose2 = xlookup[j+1];
            int xclose3 = xlookup[j+2];
            int xclose4 = xlookup[j+3];
            if (xclose != -1 && xclose2 != -1 && xclose3 != -1 && xclose4 != -1 && yclose != -1) {
                // ...then just re-use it!
                *p++ = bufferMem[bufIdx^1][yclose*MAXX + xclose];
                *p++ = bufferMem[bufIdx^1][yclose*MAXX + xclose2];
                *p++ = bufferMem[bufIdx^1][yclose*MAXX + xclose3];
                *p++ = bufferMem[bufIdx^1][yclose*MAXX + xclose4];
            } else {
                // Otherwise, perform a full computation.
                CoreLoopDouble(xcur, ycur, xstep, &p);
            }
            xcur += 4*xstep;
        }
    }
    // Copy the memory-based buffer into the SDL one...
    Uint8 *pixels = (Uint8*)surface->pixels;
    Uint8 *src = bufferMem[bufIdx];
    for (int i=0; i<MAXY; i++) {
        memcpy(pixels, src, MAXX);
        src += MAXX;
        pixels += surface->pitch;
    }
    // ...and blit it on the screen.
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(texture);
}

AUTO_DISPATCH
double autopilot()
{
    static double interesting_points[][2] = {
        {-0.72996052273553402312, -0.24047620199671820851},
        {-0.73162093699311890000, -0.25655927868100719680},
        {-1.03178026025649338671, -0.36035584735925418887},
        {-0.73174633145360257203,  0.21761907852168510535},
        {-1.25616009010536688884,  0.39944527454476780326},
        {-0.03804043691413014350, -0.98541408335385771711}
    };
    const int total_interesting_points =
        sizeof(interesting_points) / sizeof(interesting_points[0]);
    int start_idx = rand() % total_interesting_points;

    int frames = 0;
    unsigned long ticks = 0;
    while(1) {
        // Where shall we zoom this time?
        int rand_idx = start_idx % total_interesting_points;
        double targetx = interesting_points[rand_idx][0];
        double targety = interesting_points[rand_idx][1];
        start_idx++;

        // Re-initialize the window location...
        double xld = -2.2, yld=-1.1, xru=-2+(MAXX/MAXY)*3., yru=1.1;

        // Start by drawing everything...
        double percentage_of_pixels = 100.0;

        // Go!
        while(1) {
            unsigned st = SDL_GetTicks();
            mandel(xld, yld, xru, yru, percentage_of_pixels);
            unsigned en = SDL_GetTicks();
            ticks += en-st;

            // After the 1st frame, re-use 99.25% of the pixels:
            percentage_of_pixels = 0.75;

            // Limit frame rate to 60 fps.
            if (en - st < minimum_ms_per_frame)
                SDL_Delay(minimum_ms_per_frame - en + st);

            int x,y;
            int result = kbhit(&x, &y);
            if (result == 1)
                return ((double)frames)*1000.0/ticks;
                
            // Did we zoom too much?
            double xrange = xru-xld;
            if (xrange < ZOOM_LIMIT)
                break;
            xld += (targetx - xld)/100.;
            xru += (targetx - xru)/100.;
            yld += (targety - yld)/100.;
            yru += (targety - yru)/100.;
            frames++;
        }
    }
    return ((double)frames)*1000.0/ticks;
}

AUTO_DISPATCH
double mousedriven()
{
    int x,y;
    double xld = -2.2, yld=-1.1, xru=-2+(MAXX/MAXY)*3., yru=1.1;
    unsigned time_since_we_moved = SDL_GetTicks();
    int frames = 0;
    unsigned long ticks = 0;

    while(1) {
        if (SDL_GetTicks() - time_since_we_moved > 200)
            // If we haven't moved for more than 200ms,
            // go to sleep - no need to waste the CPU
            SDL_Delay(minimum_ms_per_frame);
        else if (SDL_GetTicks() - time_since_we_moved > 50) {
            // if we haven't moved for 50 to 200ms,
            // draw an accurate frame (0% reuse)
            unsigned st = SDL_GetTicks();
            mandel(xld, yld, xru, yru, 100);
            unsigned en = SDL_GetTicks();
            ticks += en-st;
            frames++;
        } else {
            // Otherwise draw a low-accuracy frame
            // (reuse 99.25% of the pixels)
            unsigned st = SDL_GetTicks();
            mandel(xld, yld, xru, yru, 0.75);
            unsigned en = SDL_GetTicks();
            ticks += en-st;
            frames++;
            // Limit frame rate to 60 fps.
            if (en - st < minimum_ms_per_frame)
                SDL_Delay(minimum_ms_per_frame - en + st);
        }
        int result = kbhit(&x, &y);
        if (result == 1)
            break;
        else if (result == 2 || result == 3) {
            time_since_we_moved = SDL_GetTicks();
            double ratiox = ((double)x)/MAXX;
            double ratioy = ((double)y)/MAXY;
            double xrange = xru-xld;
            double yrange = yru-yld;
            double direction = result==2?1.:-1.;
            // Don't zoom beyond the level allowed by IEEE754
            if (result == 2 && xrange < ZOOM_LIMIT)
                continue;
            xld += direction*0.01*ratiox*xrange;
            xru -= direction*0.01*(1.-ratiox)*xrange;
            yld += direction*0.01*(1.-ratioy)*yrange;
            yru -= direction*0.01*ratioy*yrange;
        }
    }
    // Inform point reached, for potential autopilot target
    return ((double)frames)*1000.0/ticks;
}

