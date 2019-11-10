#include <config.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <SDL.h>

// Number of iterations per pixel
// If you change this, change it in the ASM too...
#define ITERA 240

int MAXX;
int MAXY;
SDL_Surface *surface;
Uint8 *buffer = NULL;
Uint8 *previewBufferOriginal = NULL;
Uint8 *previewBufferFiltered = NULL;

void panic(char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
    exit(0);
}

void init256()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        panic("Couldn't initialize SDL: %d\n", SDL_GetError());
    atexit(SDL_Quit);

    surface = SDL_SetVideoMode(MAXX,
                               MAXY, 8, SDL_HWSURFACE | SDL_HWPALETTE);
    if (!surface)
        panic("Couldn't set video mode: %d", SDL_GetError());

    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) < 0)
            panic("Couldn't lock surface: %d", SDL_GetError());
    }
    buffer = (Uint8*)surface->pixels;

    // A palette for Mandelbrot zooms...
    {
        SDL_Color palette[256];
        int i;
	int ofs=0;
        for (i = 0; i < 16; i++) {
            palette[i+ofs].r = 16*(16-abs(i-16));
            palette[i+ofs].g = 0;
            palette[i+ofs].b = 16*abs(i-16);
        }
	ofs= 16;
        for (i = 0; i < 16; i++) {
            palette[i+ofs].r = 0;
            palette[i+ofs].g = 16*(16-abs(i-16));
            palette[i+ofs].b = 0;
        }
	ofs= 32;
        for (i = 0; i < 16; i++) {
            palette[i+ofs].r = 0;
            palette[i+ofs].g = 0;
            palette[i+ofs].b = 16*(16-abs(i-16));
        }
	ofs= 48;
        for (i = 0; i < 16; i++) {
            palette[i+ofs].r = 16*(16-abs(i-16));
            palette[i+ofs].g = 16*(16-abs(i-16));
            palette[i+ofs].b = 0;
        }
	ofs= 64;
        for (i = 0; i < 16; i++) {
            palette[i+ofs].r = 0;
            palette[i+ofs].g = 16*(16-abs(i-16));
            palette[i+ofs].b = 16*(16-abs(i-16));
        }
	ofs= 80;
        for (i = 0; i < 16; i++) {
            palette[i+ofs].r = 16*(16-abs(i-16));
            palette[i+ofs].g = 0;
            palette[i+ofs].b = 16*(16-abs(i-16));
        }
	ofs= 96;
        for (i = 0; i < 16; i++) {
            palette[i+ofs].r = 16*(16-abs(i-16));
            palette[i+ofs].g = 16*(16-abs(i-16));
            palette[i+ofs].b = 16*(16-abs(i-16));
        }
	ofs= 112;
        for (i = 0; i < 16; i++) {
            palette[i+ofs].r = 16*(8-abs(i-8));
            palette[i+ofs].g = 16*(8-abs(i-8));
            palette[i+ofs].b = 16*(8-abs(i-8));
        }
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

// SSE requires data to be aligned to 16bytes, so...

#ifdef __GNUC__
    #define DECLARE_ALIGNED(n,t,v)       t v __attribute__ ((aligned (n)))
#else
    #define DECLARE_ALIGNED(n,t,v)      __declspec(align(n)) t v
#endif

DECLARE_ALIGNED(16,double,ones[2]) = { 1.0, 1.0 };
DECLARE_ALIGNED(16,double,fours[2]) = { 4.0, 4.0 };

DECLARE_ALIGNED(16,float,onesf[4]) = { 1.0f, 1.0f, 1.0f, 1.0f };
DECLARE_ALIGNED(16,float,foursf[4]) = { 4.0f, 4.0f, 4.0f, 4.0f };

DECLARE_ALIGNED(16,unsigned,allbits[4]) = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};

void CoreLoopFloat(double xcur, double ycur, double xstep, unsigned char **p)
{
    DECLARE_ALIGNED(16,float,re[4]);
    DECLARE_ALIGNED(16,float,im[4]);
    DECLARE_ALIGNED(16,unsigned,k1[4]);

#ifndef SIMD_SSE
    DECLARE_ALIGNED(16,float,rez[4]);
    DECLARE_ALIGNED(16,float,imz[4]);
    float t1, t2, o1, o2;
    int k;
#else
    DECLARE_ALIGNED(16,float,outputs[4]);
#endif

    re[0] = (float) xcur;
    re[1] = (float) (xcur + xstep);
    re[2] = (float) (xcur + 2*xstep);
    re[3] = (float) (xcur + 3*xstep);

    im[0] = im[1] = im[2] = im[3] = (float) ycur;

#ifndef SIMD_SSE
    rez[0] = 0.0f;
    rez[1] = 0.0f;
    rez[2] = 0.0f;
    rez[3] = 0.0f;
    imz[0] = 0.0f;
    imz[1] = 0.0f;
    imz[2] = 0.0f;
    imz[3] = 0.0f;

    k1[0] = k1[1] = k1[2] = k1[3] = 0;
    k = 1;
    while (k < ITERA) {
	if (!k1[0]) {
	    o1 = rez[0] * rez[0];
	    o2 = imz[0] * imz[0];
	    t2 = 2 * rez[0] * imz[0];
	    t1 = o1 - o2;
	    rez[0] = t1 + re[0];
	    imz[0] = t2 + im[0];
	    if (o1 + o2 > 4)
		k1[0] = k;
	}

	if (!k1[1]) {
	    o1 = rez[1] * rez[1];
	    o2 = imz[1] * imz[1];
	    t2 = 2 * rez[1] * imz[1];
	    t1 = o1 - o2;
	    rez[1] = t1 + re[1];
	    imz[1] = t2 + im[1];
	    if (o1 + o2 > 4)
		k1[1] = k;
	}
	
	if (!k1[2]) {
	    o1 = rez[2] * rez[2];
	    o2 = imz[2] * imz[2];
	    t2 = 2 * rez[2] * imz[2];
	    t1 = o1 - o2;
	    rez[2] = t1 + re[2];
	    imz[2] = t2 + im[2];		    
	    if (o1 + o2 > 4)
		k1[2] = k;
	}
	
	if (!k1[3]) {
	    o1 = rez[3] * rez[3];
	    o2 = imz[3] * imz[3];
	    t2 = 2 * rez[3] * imz[3];
	    t1 = o1 - o2;
	    rez[3] = t1 + re[3];
	    imz[3] = t2 + im[3];
	    if (o1 + o2 > 4)
		k1[3] = k;
	}

	if (k1[0]*k1[1]*k1[2]*k1[3])
	    break;

	k++;
    }
    
#else
    k1[0] = k1[1] = k1[2] = k1[3] = 0;

					      // x' = x^2 - y^2 + a
					      // y' = 2xy + b
					      //
    asm("mov    $0xEF,%%ecx\n\t"
	"movaps %4,%%xmm5\n\t"                //  4.     4.     4.     4.       ; xmm5
	"movaps %2,%%xmm6\n\t"                //  a0     a1     a2     a3       ; xmm6
	"movaps %3,%%xmm7\n\t"                //  b0     b1     b2     b3       ; xmm7
	"xorps  %%xmm0,%%xmm0\n\t"            //  0.     0.     0.     0.
	"xorps  %%xmm1,%%xmm1\n\t"            //  0.     0.     0.     0.
	"xorps  %%xmm3,%%xmm3\n\t"            //  0.     0.     0.     0.       ; bailout counters
	"1:\n\t"
	"movaps %%xmm0,%%xmm2\n\t"            //  x0     x1     x2     x3       ; xmm2
	"mulps  %%xmm1,%%xmm2\n\t"            //  x0*y0  x1*y1  x2*y2  x3*y3    ; xmm2
	"mulps  %%xmm0,%%xmm0\n\t"            //  x0^2   x1^2   x2^2   x3^2     ; xmm0
	"mulps  %%xmm1,%%xmm1\n\t"            //  y0^2   y1^2   y2^2   y3^2     ; xmm1
	"movaps %%xmm0,%%xmm4\n\t"            //  
	"addps  %%xmm1,%%xmm4\n\t"            //  x0^2+y0^2  x1...              ; xmm4
	"subps  %%xmm1,%%xmm0\n\t"            //  x0^2-y0^2  x1...              ; xmm0
	"addps  %%xmm6,%%xmm0\n\t"            //  x0'    x1'    x2'    x3'      ; xmm0
	"movaps %%xmm2,%%xmm1\n\t"            //  x0*y0  x1*y1  x2*y2  x3*y3    ; xmm1
	"addps  %%xmm1,%%xmm1\n\t"            //  2x0*y0 2x1*y1 2x2*y2 2x3*y3   ; xmm1
	"addps  %%xmm7,%%xmm1\n\t"            //  y0'    y1'    y2'    y3'      ; xmm1
	"cmpltps %%xmm5,%%xmm4\n\t"           //  <4     <4     <4     <4 ?     ; xmm2
	"movaps %%xmm4,%%xmm2\n\t"            //  xmm2 has all 1s in the non-overflowed pixels
	"movmskps %%xmm4,%%eax\n\t"           //  (lower 4 bits reflect comparisons)
	"andps  %5,%%xmm4\n\t"                //  so, prepare to increase the non-overflowed ("and" with onesf)
	"addps  %%xmm4,%%xmm3\n\t"            //  by updating their counters
	"or     %%eax,%%eax\n\t"              //  have all 4 pixels overflowed ?
	"je     2f\n\t"                       //  yes, jump forward to label 2 (hence, 2f)
	"dec    %%ecx\n\t"                    //  otherwise,repeat 239 times...
	"jnz    1b\n\t"                       //  by jumping backward to label 1 (1b)
	"movaps %%xmm2,%%xmm4\n\t"            //  xmm4 has all 1s in the non-overflowed pixels
	"xorps  %6,%%xmm4\n\t"                //  xmm4 has all 1s in the overflowed pixels (xoring with allbits)
	"andps  %%xmm4,%%xmm3\n\t"            //  zero out the xmm3 parts that belong to non-overflowed (set to black)
	"2:\n\t"
	"movaps %%xmm3,%0\n\t"
	:"=m"(outputs[0]),"=m"(outputs[2])
	:"m"(re[0]),"m"(im[0]),"m"(foursf[0]),"m"(onesf[0]),"m"(allbits[0])
	:"%eax","%ecx","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","memory");

    k1[0] = (int)(outputs[0]);
    k1[1] = (int)(outputs[1]);
    k1[2] = (int)(outputs[2]);
    k1[3] = (int)(outputs[3]);

#endif

    *(*p)++ = k1[0];
    *(*p)++ = k1[1];
    *(*p)++ = k1[2];
    *(*p)++ = k1[3];
}

void CoreLoopDouble(double xcur, double ycur, double xstep, unsigned char **p)
{
    DECLARE_ALIGNED(16,double,re[2]);
    DECLARE_ALIGNED(16,double,im[2]);
    DECLARE_ALIGNED(16,unsigned,k1[2]);

#ifndef SIMD_SSE
    DECLARE_ALIGNED(16,double,rez[2]);
    DECLARE_ALIGNED(16,double,imz[2]);
    double t1, t2, o1, o2;
    int k;
#else
    DECLARE_ALIGNED(16,double,outputs[2]);
#endif

    re[0] = xcur;
    re[1] = (xcur + xstep);

    im[0] = im[1] = ycur;

#ifndef SIMD_SSE
    rez[0] = 0.0f;
    rez[1] = 0.0f;
    imz[0] = 0.0f;
    imz[1] = 0.0f;

    k = k1[0] = k1[1] = 0;
    while (k < ITERA) {
	if (!k1[0]) {
	    o1 = rez[0] * rez[0];
	    o2 = imz[0] * imz[0];
	    t2 = 2 * rez[0] * imz[0];
	    t1 = o1 - o2;
	    rez[0] = t1 + re[0];
	    imz[0] = t2 + im[0];
	    if (o1 + o2 > 4)
		k1[0] = k;
	}

	if (!k1[1]) {
	    o1 = rez[1] * rez[1];
	    o2 = imz[1] * imz[1];
	    t2 = 2 * rez[1] * imz[1];
	    t1 = o1 - o2;
	    rez[1] = t1 + re[1];
	    imz[1] = t2 + im[1];
	    if (o1 + o2 > 4)
		k1[1] = k;
	}
	
	if (k1[0]*k1[1])
	    break;

	k++;
    }
    
#else
    k1[0] = k1[1] = 0;
					      // x' = x^2 - y^2 + a
					      // y' = 2xy + b
					      //
    asm("mov    $0xEF,%%ecx\n\t"
	"movapd %3,%%xmm5\n\t"                //  4.     4.        ; xmm5
	"movapd %1,%%xmm6\n\t"                //  a0     a1        ; xmm6
	"movaps %2,%%xmm7\n\t"                //  b0     b1        ; xmm7
	"xorpd  %%xmm0,%%xmm0\n\t"            //  0.     0.    
	"xorpd  %%xmm1,%%xmm1\n\t"            //  0.     0.    
	"xorpd  %%xmm3,%%xmm3\n\t"            //  0.     0.        ; bailout counters
	"1:\n\t"
	"movapd %%xmm0,%%xmm2\n\t"            //  x0     x1        ; xmm2
	"mulpd  %%xmm1,%%xmm2\n\t"            //  x0*y0  x1*y1     ; xmm2
	"mulpd  %%xmm0,%%xmm0\n\t"            //  x0^2   x1^2      ; xmm0
	"mulpd  %%xmm1,%%xmm1\n\t"            //  y0^2   y1^2      ; xmm1
	"movapd %%xmm0,%%xmm4\n\t"            //  
	"addpd  %%xmm1,%%xmm4\n\t"            //  x0^2+y0^2  x1... ; xmm4
	"subpd  %%xmm1,%%xmm0\n\t"            //  x0^2-y0^2  x1... ; xmm0
	"addpd  %%xmm6,%%xmm0\n\t"            //  x0'    x1'       ; xmm0
	"movapd %%xmm2,%%xmm1\n\t"            //  x0*y0  x1*y1     ; xmm1
	"addpd  %%xmm1,%%xmm1\n\t"            //  2x0*y0 2x1*y1    ; xmm1
	"addpd  %%xmm7,%%xmm1\n\t"            //  y0'    y1'       ; xmm1
	"cmpltpd %%xmm5,%%xmm4\n\t"           //  <4     <4        ; xmm2
	"movapd %%xmm4,%%xmm2\n\t"            //  xmm2 has all 1s in the non-overflowed pixels
	"movmskpd %%xmm4,%%eax\n\t"           //  (lower 2 bits reflect comparisons)
	"andpd  %4,%%xmm4\n\t"                //  so, prepare to increase the non-overflowed (and with ones)
	"addpd  %%xmm4,%%xmm3\n\t"            //  by updating their counters
	"or     %%eax,%%eax\n\t"              //  have both pixels overflowed ?
	"je     2f\n\t"                       //  yes, jump forward to label 2 (hence, 2f)
	"dec    %%ecx\n\t"                    //  otherwise,repeat 239 times...
	"jnz    1b\n\t"                       //  by jumping backward to label 1 (1b)
	"movapd %%xmm2,%%xmm4\n\t"            //  xmm4 has all 1s in the non-overflowed pixels
	"xorpd  %5,%%xmm4\n\t"                //  xmm4 has all 1s in the overflowed pixels (xoring with allbits)
	"andpd  %%xmm4,%%xmm3\n\t"            //  zero out the xmm3 parts that belong to non-overflowed (set to black)
	"2:\n\t"
	"movapd %%xmm3,%0\n\t"
	:"=m"(outputs[0])
	:"m"(re[0]),"m"(im[0]),"m"(fours[0]),"m"(ones[0]),"m"(allbits[0])
	:"%eax","%ecx","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","memory");

    k1[0] = (int)(outputs[0]);
    k1[1] = (int)(outputs[1]);

#endif

    *(*p)++ = k1[0];
    *(*p)++ = k1[1];
}

void preMandel(double xld, double yld, double xru, double yru)
{
    unsigned char *p = (unsigned char *) previewBufferOriginal;
    double xstep, ystep, xcur, ycur;
    int i, j;

    int MINI_MAXX = MAXX/4;
    int MINI_MAXY = MAXY/4;

    xstep = (xru - xld)/MINI_MAXX;
    ystep = (yru - yld)/MINI_MAXY;

#if defined(USE_OPENMP)
    #pragma omp parallel for schedule(dynamic,4) private(p,xcur,ycur,i,j)
#endif
    for (i=0; i<MINI_MAXY; i++) {
	xcur = xld;
	ycur = yru - i*ystep;
	p = &previewBufferOriginal[i*MINI_MAXX];
        for (j=0; j<MINI_MAXX; j+=4) {
	    CoreLoopDouble(xcur, ycur, xstep, &p);
	    xcur += 2*xstep;
	    CoreLoopDouble(xcur, ycur, xstep, &p);
	    xcur += 2*xstep;
        }
    }

    // We now have a 1/16 of the total picture (in the previewBufferOriginal)
    // Each of these mini-pixels corresponds to a 4x4 block of pixels.
    //
    // But how can we use this as an accelerator for the black areas?
    //
    // Well, we can simply map and see if a 4x1 area maps to a black pixel
    // in the mini-preview, and if so, plot all 4 pixels as black.
    //
    // Only... this wont work.
    //
    // The mini-preview is just a sampling - perhaps one of the 4
    // pixels is black, and we just happened to "fall" on it in
    // the preview. Not all 4 pixels must be black, only one is!
    //
    // We need to "shrink" the black areas in the preview
    // to avoid this problem.
    //
    // And that's what the code below does: it only outputs a black pixel
    // (in the previewBufferFiltered) if the original pixel from
    // previewBufferOriginal AND all the 4 neighbors, were black.
    //
    // This isn't bullet-proof, but it is close enough.
    //
    Uint8 *pSrc = previewBufferOriginal;
    Uint8 *pDst = previewBufferFiltered;
    for (i=0; i<MINI_MAXY; i++) {
        for (j=0; j<MINI_MAXX; j++) {
	    Uint8 up=0,left=0,down=0,right=0;
	    if (i>0) up = *(pSrc-MINI_MAXX);
	    if (i<MINI_MAXY-1) down = *(pSrc+MINI_MAXX);
	    if (j>0) left = *(pSrc-1);
	    if (j<MINI_MAXX-1) right = *(pSrc+1);
	    // set final preview pixel to black only if 
	    // both itself and all 4 neighbors are black
	    *pDst++ = *pSrc++ | up | down | left | right;
	}
    }
}

void mandelFloat(double xld, double yld, double xru, double yru)
{
    int i, j;
    double xstep, ystep, xcur, ycur;
    unsigned char *p = (unsigned char *) buffer;
    #ifndef NDEBUG
    int saved = 0;
    #endif

    xstep = (xru - xld)/MAXX;
    ystep = (yru - yld)/MAXY;

#if defined(USE_OPENMP)
#pragma omp parallel for schedule(dynamic,4) private(p,xcur,ycur,i,j)
#endif
    for (i=0; i<MAXY; i++) {
	Uint32 offset = (i>>2)*(MAXX >> 2);
	xcur = xld;
	ycur = yru - i*ystep;
	p = &buffer[i*MAXX];
        for (j=0; j<MAXX; j+=4, offset++) {
	    // Avoid calculating black areas - see comment in preMandel
	    if (0 == previewBufferFiltered[offset]) {
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		xcur += 4*xstep;
		#ifndef NDEBUG
		saved ++;
		#endif
		continue;
	    }
	    CoreLoopFloat(xcur, ycur, xstep, &p);
	    xcur += 4*xstep;
        }
    }
    SDL_UpdateRect(surface, 0, 0, MAXX, MAXY);
    #ifndef NDEBUG
    printf("Saved due to preview: %4.4f%%\n", (100.0*saved)/(MAXX*MAXY/4));
    #endif
}

void mandelDouble(double xld, double yld, double xru, double yru)
{
    int i, j;
    double xstep, ystep, xcur, ycur;
    unsigned char *p = (unsigned char *) buffer;
    #ifndef NDEBUG
    int saved = 0;
    #endif

    xstep = (xru - xld)/MAXX;
    ystep = (yru - yld)/MAXY;

#if defined(USE_OPENMP)
#pragma omp parallel for schedule(dynamic,4) private(p,xcur,ycur,i,j)
#endif
    for (i=0; i<MAXY; i++) {
	Uint32 offset = (i>>2)*(MAXX >> 2);
	xcur = xld;
	ycur = yru - i*ystep;
	p = &buffer[i*MAXX];
        for (j=0; j<MAXX; j+=4, offset++) {
	    // Avoid calculating black areas - see comment in preMandel
	    if (0 == previewBufferFiltered[offset]) {
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		xcur += 4*xstep;
		#ifndef NDEBUG
		saved ++;
		#endif
		continue;
	    }
	    CoreLoopDouble(xcur, ycur, xstep, &p);
	    xcur += 2*xstep;
	    CoreLoopDouble(xcur, ycur, xstep, &p);
	    xcur += 2*xstep;
        }
    }
    SDL_UpdateRect(surface, 0, 0, MAXX, MAXY);
    #ifndef NDEBUG
    printf("Saved due to preview: %4.4f%%\n", (100.0*saved)/(MAXX*MAXY/4));
    #endif
}

#ifdef _WIN32

#define CHECK(x) {							    \
    unsigned of = (unsigned) (unsigned long) &x[0];			    \
    char soThatGccDoesntOptimizeAway[32];				    \
    sprintf(soThatGccDoesntOptimizeAway, "%08x", of);			    \
    if (soThatGccDoesntOptimizeAway[7] != '0') {			    \
	MessageBox(0,							    \
	    "Your compiler is buggy... "				    \
	    "it didn't align the SSE variables...\n"			    \
	    "The application would crash. Aborting.",			    \
	    "Fatal Error", MB_OK | MB_ICONERROR);			    \
	exit(1);							    \
    }									    \
}

#else

#define CHECK(x) {							    \
    unsigned of = (unsigned) (unsigned long) &x[0];			    \
    char soThatGccDoesntOptimizeAway[32];				    \
    sprintf(soThatGccDoesntOptimizeAway, "%08x", of);			    \
    if (soThatGccDoesntOptimizeAway[7] != '0') {			    \
	fprintf(stderr,							    \
	    "Your compiler is buggy... "				    \
	    "it didn't align the SSE variables...\n"			    \
	    "The application would crash. Aborting.\n");		    \
	fflush(stderr);							    \
	exit(1);							    \
    }									    \
}

#endif

int main(int argc, char *argv[])
{
    DECLARE_ALIGNED(16,float,testAlignment[4]);

    CHECK(testAlignment)
    CHECK(ones)
    CHECK(onesf)
    CHECK(fours)
    CHECK(foursf)
    CHECK(allbits)

    switch (argc) {
    case 3:
        MAXX = atoi(argv[1]);
        MAXY = atoi(argv[2]);
	MAXX = 16*(MAXX/16);
	MAXY = 16*(MAXY/16);
        break;
    default:
        MAXX = 480;
        MAXY = 360;
        break;
    }

#ifndef _WIN32
    printf("\nMandelbrot Zoomer by Thanassis (an experiment in SSE).\n");
#ifndef SIMD_SSE
    printf("(Pipelined floating point calculation)\n\n");
#else
    printf("(SSE calculation)\n\n");
#endif
#endif

    previewBufferOriginal = (Uint8 *) malloc(MAXX*MAXY/16);
    previewBufferFiltered = (Uint8 *) malloc(MAXX*MAXY/16);
    init256();

    int x,y;
    unsigned i=0, st, en;

    const char *floats  ="Single-precision mode"
#ifdef SIMD_SSE
    " (SSE)"
#endif
    ;
    const char *doubles ="Double-precision mode"
#ifdef SIMD_SSE
    " (SSE)"
#endif
    ;

    const char *usage = "Left click to zoom-in, right-click to zoom-out, ESC to quit...";
    SDL_WM_SetCaption(usage,usage);

    st = SDL_GetTicks();

    double xld = -2.2, yld=-1.1, xru=-2+(MAXX/MAXY)*3., yru=1.1;
    int mode = 0;
    while(1) {
	preMandel(xld, yld, xru, yru);
	if ((xru-xld)<0.00002) {
	    if (mode != 1) { SDL_WM_SetCaption(doubles,doubles); mode = 1; }
	    mandelDouble(xld, yld, xru, yru);
	} else {
	    if (mode != 0) { SDL_WM_SetCaption(floats,floats); mode = 0; }
	    mandelFloat(xld, yld, xru, yru);
	}
        int result = kbhit(&x, &y);
	if (result == 1)
            break;
	else if (result == 2 || result == 3) {
	    double ratiox = ((double)x)/MAXX;
	    double ratioy = ((double)y)/MAXY;
	    double xrange = xru-xld;
	    double yrange = yru-yld;
	    double direction = result==2?1.:-1.;
	    xld += direction*0.01*ratiox*xrange;
	    xru -= direction*0.01*(1.-ratiox)*xrange;
	    yld += direction*0.01*(1.-ratioy)*yrange;
	    yru -= direction*0.01*ratioy*yrange;
	} 
	i++;
    }
    en = SDL_GetTicks();

#ifdef _WIN32
    char speed[256];
    sprintf(speed, "Frames/sec:%5.2f\n\n", ((float) i) / ((en - st) / 1000.0f));
    MessageBoxA(0, (LPCSTR) speed, (const char *)"Speed of rendering", MB_OK);
#else
    printf("Frames/sec:%5.2f\n\n", ((float) i) / ((en - st) / 1000.0f));
    fflush(stdout);
#endif
    return 0;
}
