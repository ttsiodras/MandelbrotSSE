#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "sse.h"

static Uint8 *previewBufferOriginal = NULL;
static Uint8 *previewBufferFiltered = NULL;

#define CLEAR_ARRAY(x) memset(&x, 0, sizeof(x))
#define COPY_ARRAY4(src, dst) \
    dst[0] = src[0]; \
    dst[1] = src[1]; \
    dst[2] = src[2]; \
    dst[3] = src[3];
#define COPY_ARRAY2(src, dst) \
    dst[0] = src[0]; \
    dst[1] = src[1];

// SSE requires data to be aligned to 16bytes, so...

#ifdef __GNUC__
    #define DECLARE_ALIGNED(n,t,v)       t v __attribute__ ((aligned (n)))
#else
    #define DECLARE_ALIGNED(n,t,v)      __declspec(align(n)) t v
#endif

DECLARE_ALIGNED(16,double,ones[4]) = { 1.0, 1.0, 1.0, 1.0 };
DECLARE_ALIGNED(16,double,fours[4]) = { 4.0, 4.0, 4.0, 4.0 };

DECLARE_ALIGNED(32,float,onesf[8]) = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
DECLARE_ALIGNED(32,float,foursf[8]) = { 4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f };

DECLARE_ALIGNED(32,unsigned,allbits[8]) = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};

void CoreLoopFloat(double xcur, double ycur, double xstep, unsigned char **p)
{
    DECLARE_ALIGNED(32,float,re[8]);
    DECLARE_ALIGNED(32,float,im[8]);
    DECLARE_ALIGNED(32,unsigned,k1[8]);

#ifndef SIMD_SSE
    DECLARE_ALIGNED(32,float,rez[8]);
    DECLARE_ALIGNED(32,float,imz[8]);
    DECLARE_ALIGNED(32,float,xold[8]);
    DECLARE_ALIGNED(32,float,yold[8]);
    float t1, t2, o1, o2;
    int k=0, period=0;
#else
    DECLARE_ALIGNED(32,float,outputs[8]);
#endif

    re[0] = (float) xcur;
    re[1] = (float) (xcur + xstep);
    re[2] = (float) (xcur + 2*xstep);
    re[3] = (float) (xcur + 3*xstep);
    re[4] = (float) (xcur + 4*xstep);
    re[5] = (float) (xcur + 5*xstep);
    re[6] = (float) (xcur + 6*xstep);
    re[7] = (float) (xcur + 7*xstep);

    im[0] = im[1] = im[2] = im[3] = im[4] = im[5] = im[6] = im[7] = (float) ycur;

#ifndef SIMD_SSE
    CLEAR_ARRAY(rez);
    CLEAR_ARRAY(imz);
    CLEAR_ARRAY(xold);
    CLEAR_ARRAY(yold);
    CLEAR_ARRAY(k1);
    while (k < ITERA) {

#define WORK_ON_SLOT(x)                                 \
	if (!k1[x]) {                                   \
	    o1 = rez[x] * rez[x];                       \
	    o2 = imz[x] * imz[x];                       \
	    t2 = 2 * rez[x] * imz[x];                   \
	    t1 = o1 - o2;                               \
	    rez[x] = t1 + re[x];                        \
	    imz[x] = t2 + im[x];                        \
	    if (o1 + o2 > 4)                            \
		k1[x] = k;                              \
            if (rez[x] == xold[x] && imz[x] == yold[x]) \
                k1[x] = ITERA;                          \
	}

        WORK_ON_SLOT(0)
        WORK_ON_SLOT(1)
        WORK_ON_SLOT(2)
        WORK_ON_SLOT(3)

	if (k1[0]*k1[1]*k1[2]*k1[3] != 0)
	    break;

	k++;

        period = (period+1) & 0xF;
        if (period == 0xF) {
            COPY_ARRAY4(rez, xold);
            COPY_ARRAY4(imz, yold);
        }
    }
#define EMIT_SLOT(x)                              \
    if (!k1[x]) k1[x] = ITERA;                    \
    *(*p)++ = k1[x] == ITERA ? 128 : k1[x] & 127;

    EMIT_SLOT(0)
    EMIT_SLOT(1)
    EMIT_SLOT(2)
    EMIT_SLOT(3)

#else
    k1[0] = k1[1] = k1[2] = k1[3] = k1[4] = k1[5] = k1[6] = k1[7] = 0;

					      // x' = x^2 - y^2 + a
					      // y' = 2xy + b
					      //
    asm("mov    %6,%%ecx\n\t"                 //  ecx is ITERA
        "xor    %%ebx, %%ebx\n\t"             //  period = 0
	"vmovaps %3,%%ymm5\n\t"               //  4.     4.     4.     4.     4.     4.     4.     4.        ; ymm5
	"vmovaps %1,%%ymm6\n\t"               //  a0     a1     a2     a3     a4     a5     a6     a7        ; ymm6
	"vmovaps %2,%%ymm7\n\t"               //  b0     b1     b2     b3     b4     b5     b6     b7        ; ymm7
	"vxorps  %%ymm0,%%ymm0,%%ymm0\n\t"    //  0.     0.     0.     0.     0.     0.     0.     0.
	"vxorps  %%ymm1,%%ymm1,%%ymm1\n\t"    //  0.     0.     0.     0.     0.     0.     0.     0.
	"vxorps  %%ymm3,%%ymm3,%%ymm3\n\t"    //  0.     0.     0.     0.     0.     0.     0.     0.        ; bailout counters

	"1:\n\t"                              //  Main Mandelbrot computation
	"vmovaps %%ymm0,%%ymm2\n\t"           //  x0     x1     x2     x3     ...  ; ymm2
	"vmulps  %%ymm1,%%ymm2,%%ymm2\n\t"    //  x0*y0  x1*y1  x2*y2  x3*y3  ...  ; ymm2
	"vmulps  %%ymm0,%%ymm0,%%ymm0\n\t"    //  x0^2   x1^2   x2^2   x3^2   ...  ; ymm0
	"vmulps  %%ymm1,%%ymm1,%%ymm1\n\t"    //  y0^2   y1^2   y2^2   y3^2   ...  ; ymm1
	"vmovaps %%ymm0,%%ymm4\n\t"           //  
	"vaddps  %%ymm1,%%ymm4,%%ymm4\n\t"    //  x0^2+y0^2  x1...            ...  ; ymm4
	"vsubps  %%ymm1,%%ymm0,%%ymm0\n\t"    //  x0^2-y0^2  x1...            ...  ; ymm0
	"vaddps  %%ymm6,%%ymm0,%%ymm0\n\t"    //  x0'    x1'    x2'    x3'    ...  ; ymm0
	"vmovaps %%ymm2,%%ymm1\n\t"           //  x0*y0  x1*y1  x2*y2  x3*y3  ...  ; ymm1
	"vaddps  %%ymm1,%%ymm1,%%ymm1\n\t"    //  2x0*y0 2x1*y1 2x2*y2 2x3*y3 ...  ; ymm1
	"vaddps  %%ymm7,%%ymm1,%%ymm1\n\t"    //  y0'    y1'    y2'    y3'    ...  ; ymm1

	"vcmpltps %%ymm5,%%ymm4,%%ymm4\n\t"   //  <4     <4     <4     <4 ?   ...  ; ymm2
	"vmovaps %%ymm4,%%ymm2\n\t"           //  ymm2 has all 1s in the non-overflowed pixels
	"vmovmskps %%ymm4,%%eax\n\t"          //  (lower 8 bits reflect comparisons)
	"vandps  %4,%%ymm4,%%ymm4\n\t"        //  so, prepare to increase the non-overflowed ("and" with onesf)
	"vaddps  %%ymm4,%%ymm3,%%ymm3\n\t"    //  by updating their counters

	"or     %%eax,%%eax\n\t"              //  have all 8 pixels overflowed ?

	"je     2f\n\t"                       //  yes, jump forward to label 2 (hence, 2f) and end the loop
	"dec    %%ecx\n\t"                    //  otherwise, repeat the loop ITERA times...
	"jnz    22f\n\t"                      //  but before redoing the loop, first do periodicity checking

                                              //  We've done the loop ITERA times.
                                              //  Set non-overflowed outputs to 0 (inside ymm3). Here's how:
	"vmovaps %%ymm2,%%ymm4\n\t"           //  ymm4 has all 1s in the non-overflowed pixels...
	"vxorps  %5,%%ymm4,%%ymm4\n\t"        //  ymm4 has all 1s in the overflowed pixels (toggled, via xoring with allbits)
	"vandps  %%ymm4,%%ymm3,%%ymm3\n\t"    //  zero out the ymm3 parts that belong to non-overflowed (set to black)
	"jmp    2f\n\t"                       //  And jump to end of everything, where ymm3 is written into outputs

	"22:\n\t"                             //  Periodicity checking
        "inc %%bl\n\t"                        //  period++
        "and $0xF, %%bl\n\t"                  //  period &= 0xF
        "jnz 11f\n\t"                         //  if period is not zero, continue to check if we're seeing xold, yold again
        "vmovaps %%ymm0, %%ymm8\n\t"          //  time to update xold[2], yold[2] - store xold[2] in ymm8
        "vmovaps %%ymm1, %%ymm9\n\t"          //  and yold[2] in ymm9
	"jmp    1b\n\t"                       //  and jump back to the loop beginning

        "11:\n\t"                             //  are we seeing xold[2], yold[2] into our rez[2], imz[2]?
        "vmovaps %%ymm8, %%ymm10\n\t"         //  the comparison instruction will modify the target XMM register, so use ymm10
        "vcmpeqps %%ymm0, %%ymm10,%%ymm10\n\t" //  compare ymm10 (which now has xold[2]) with rez[2]. Set all 1s into ymm10 if equal
	"vmovmskps %%ymm10,%%eax\n\t"         //  the lower 4 bits of EAX now reflect the result of the comparison. 
        "or %%eax, %%eax\n\t"                 //  are they ALL zero?
        "jz 1b\n\t"                           //  Yes - so, none of the four rez matched with the two xold. Repeat the loop
        "vmovaps %%ymm9, %%ymm10\n\t"         //  Set ymm10 to contain yold[2]
        "vcmpeqps %%ymm1, %%ymm10,%%ymm10\n\t" //  compare ymm10 with imz[2]. Set all 1s into ymm10 if equal
	"vmovmskps %%ymm10,%%eax\n\t"         //  the lower 4 bits of EAX now reflect the result of the comparison.
        "or %%eax, %%eax\n\t"                 //  are they ALL zero?
        "jz 1b\n\t"                           //  Yes - so, none of the four imz matched with the two yold. Repeat the loop
	"vxorps  %%ymm3,%%ymm3,%%ymm3\n\t"    //  Repetition detected. Set results to 0.0 (four pixels black)

	"2:\n\t"
	"vmovaps %%ymm3,%0\n\t"
	:"=m"(outputs[0])
	:"m"(re[0]),"m"(im[0]),"m"(foursf[0]),"m"(onesf[0]),"m"(allbits[0]),"i"(ITERA)
	:"%eax","%ebx","%ecx","ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7","ymm8","ymm9","ymm10","memory");

    int tmp;
    tmp = (int)(outputs[0]); *(*p)++ = !(tmp & 127) ? 128 : (tmp&127);
    tmp = (int)(outputs[1]); *(*p)++ = !(tmp & 127) ? 128 : (tmp&127);
    tmp = (int)(outputs[2]); *(*p)++ = !(tmp & 127) ? 128 : (tmp&127);
    tmp = (int)(outputs[3]); *(*p)++ = !(tmp & 127) ? 128 : (tmp&127);
    tmp = (int)(outputs[4]); *(*p)++ = !(tmp & 127) ? 128 : (tmp&127);
    tmp = (int)(outputs[5]); *(*p)++ = !(tmp & 127) ? 128 : (tmp&127);
    tmp = (int)(outputs[6]); *(*p)++ = !(tmp & 127) ? 128 : (tmp&127);
    tmp = (int)(outputs[7]); *(*p)++ = !(tmp & 127) ? 128 : (tmp&127);

#endif
}

void CoreLoopDouble(double xcur, double ycur, double xstep, unsigned char **p)
{
    DECLARE_ALIGNED(16,double,re[2]);
    DECLARE_ALIGNED(16,double,im[2]);
    DECLARE_ALIGNED(16,unsigned,k1[2]);

#ifndef SIMD_SSE
    DECLARE_ALIGNED(16,double,rez[2]);
    DECLARE_ALIGNED(16,double,imz[2]);
    DECLARE_ALIGNED(16,double,xold[2]);
    DECLARE_ALIGNED(16,double,yold[2]);
    double t1, t2, o1, o2;
    int k=0, period=0;
#else
    DECLARE_ALIGNED(16,double,outputs[2]);
#endif

    re[0] = xcur;
    re[1] = (xcur + xstep);

    im[0] = im[1] = ycur;

#ifndef SIMD_SSE
    CLEAR_ARRAY(rez);
    CLEAR_ARRAY(imz);
    CLEAR_ARRAY(xold);
    CLEAR_ARRAY(yold);
    CLEAR_ARRAY(k1);

    while (k < ITERA) {
        WORK_ON_SLOT(0)
        WORK_ON_SLOT(1)

	if (k1[0]*k1[1] != 0)
	    break;

	k++;

        period = (period+1) & 0xF;
        if (period == 0xF) {
            COPY_ARRAY2(rez, xold);
            COPY_ARRAY2(imz, yold);
        }
    }
    EMIT_SLOT(0)
    EMIT_SLOT(1)
    
#else

    k1[0] = k1[1] = 0;
					      // x' = x^2 - y^2 + a
					      // y' = 2xy + b
					      //
    asm("mov    %6,%%ecx\n\t"                 //  ecx is ITERA
        "xor    %%ebx, %%ebx\n\t"             //  period = 0
	"movapd %3,%%xmm5\n\t"                //  4.     4.        ; xmm5
	"movapd %1,%%xmm6\n\t"                //  a0     a1        ; xmm6
	"movaps %2,%%xmm7\n\t"                //  b0     b1        ; xmm7
	"xorpd  %%xmm0,%%xmm0\n\t"            //  0.     0.        ; rez in xmm0
	"xorpd  %%xmm1,%%xmm1\n\t"            //  0.     0.        ; imz in xmm1
	"xorpd  %%xmm3,%%xmm3\n\t"            //  0.     0.        ; bailout counters
	"xorpd  %%xmm8,%%xmm8\n\t"            //  0.     0.        ; bailout counters
	"xorpd  %%xmm9,%%xmm9\n\t"            //  0.     0.        ; bailout counters

	"1:\n\t"                              //  Main Mandelbrot computation
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

	"je     2f\n\t"                       //  yes, jump forward to label 2 (hence, 2f) and end the loop
	"dec    %%ecx\n\t"                    //  otherwise, repeat the loop ITERA times...
	"jnz    22f\n\t"                      //  but before redoing the loop, first do periodicity checking

                                              //  We've done the loop ITERA times.
                                              //  Set non-overflowed outputs to 0 (inside xmm3). Here's how:
	"movapd %%xmm2,%%xmm4\n\t"            //  xmm4 has all 1s in the non-overflowed pixels...
	"xorpd  %5,%%xmm4\n\t"                //  xmm4 has all 1s in the overflowed pixels (toggled, via xoring with allbits)
	"andpd  %%xmm4,%%xmm3\n\t"            //  zero out the xmm3 parts that belong to non-overflowed (set to black)
	"jmp    2f\n\t"                       //  And jump to end of everything, where xmm3 is written into outputs

	"22:\n\t"                             //  Periodicity checking
        "inc %%bl\n\t"                        //  period++
        "and $0xF, %%bl\n\t"                  //  period &= 0xF
        "jnz 11f\n\t"                         //  if period is not zero, continue to check if we're seeing xold, yold again
        "movapd %%xmm0, %%xmm8\n\t"           //  time to update xold[2], yold[2] - store xold[2] in xmm8
        "movapd %%xmm1, %%xmm9\n\t"           //  and yold[2] in xmm9
	"jmp    1b\n\t"                       //  and jump back to the loop beginning

        "11:\n\t"                             //  are we seeing xold[2], yold[2] into our rez[2], imz[2]?
        "movapd %%xmm8, %%xmm10\n\t"          //  the comparison instruction will modify the target XMM register, so use xmm10
        "cmpeqpd %%xmm0, %%xmm10\n\t"         //  compare xmm10 (which now has xold[2]) with rez[2]. Set all 1s into xmm10 if equal
	"movmskpd %%xmm10,%%eax\n\t"          //  the lower 2 bits of EAX now reflect the result of the comparison. 
        "or %%eax, %%eax\n\t"                 //  are they BOTH zero?
        "jz 1b\n\t"                           //  Yes - so, neither of the two rez matched with the two xold. Repeat the loop
        "movapd %%xmm9, %%xmm10\n\t"          //  Set xmm10 to contain yold[2]
        "cmpeqpd %%xmm1, %%xmm10\n\t"         //  compare xmm10 with imz[2]. Set all 1s into xmm10 if equal
	"movmskpd %%xmm10,%%eax\n\t"          //  the lower 2 bits of EAX now reflect the result of the comparison.
        "or %%eax, %%eax\n\t"                 //  are they BOTH zero?
        "jz 1b\n\t"                           //  Yes - so, neither of the two imz matched with the two yold. Repeat the loop
	"xorpd  %%xmm3,%%xmm3\n\t"            //  Repetition detected. Set both results to 0.0 (both pixels black)

	"2:\n\t"
	"movapd %%xmm3,%0\n\t"
	:"=m"(outputs[0])
	:"m"(re[0]),"m"(im[0]),"m"(fours[0]),"m"(ones[0]),"m"(allbits[0]),"i"(ITERA)
	:"%eax","%ebx","%ecx","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","memory");

    int tmp = (int)(outputs[0]);
    *(*p)++ = tmp == 0 ? 128 : (tmp&127);
    tmp = (int)(outputs[1]);
    *(*p)++ = tmp == 0 ? 128 : (tmp&127);

#endif
}

void preMandel(double xld, double yld, double xru, double yru)
{
    unsigned char *p = (unsigned char *) previewBufferOriginal;
    double xstep, ystep, xcur, ycur;
    int i, j;

    int MINI_MAXX = MAXX/8;
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

    // We now have a 1/32 of the total picture (in the previewBufferOriginal)
    // Each of these mini-pixels corresponds to a 8x4 block of pixels.
    //
    // But how can we use this as an accelerator for the black areas?
    //
    // Well, we can simply map and see if a 8x1 area maps to a black pixel
    // in the mini-preview, and if so, plot all 8 pixels as black.
    //
    // Only... this wont work.
    //
    // The mini-preview is just a sampling - perhaps one of the 8
    // pixels is black, and we just happened to "fall" on it in
    // the preview. Not all 8 pixels must be black, only one is!
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
	    Uint8 up=128,left=128,down=128,right=128;
	    if (i>0) up = *(pSrc-MINI_MAXX);
	    if (i<MINI_MAXY-1) down = *(pSrc+MINI_MAXX);
	    if (j>0) left = *(pSrc-1);
	    if (j<MINI_MAXX-1) right = *(pSrc+1);
	    // set final preview pixel to black (128) only if 
	    // both itself and all 4 neighbors are black
	    *pDst++ = *pSrc++ & up & down & left & right;
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
	Uint32 offset = (i>>2)*(MAXX >> 3);
	xcur = xld;
	ycur = yru - i*ystep;
	p = &buffer[i*MAXX];
        for (j=0; j<MAXX; j+=8, offset++) {
	    // Avoid calculating black areas - see comment in preMandel
	    if (128 == previewBufferFiltered[offset]) {
	        *p++ = 128; *p++ = 128; *p++ = 128; *p++ = 128;
	        *p++ = 128; *p++ = 128; *p++ = 128; *p++ = 128;
	        xcur += 8*xstep;
	        #ifndef NDEBUG
	        saved ++;
	        #endif
	        continue;
	    }
	    CoreLoopFloat(xcur, ycur, xstep, &p);
	    xcur += 8*xstep;
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
	Uint32 offset = (i>>2)*(MAXX >> 3);
	xcur = xld;
	ycur = yru - i*ystep;
	p = &buffer[i*MAXX];
        for (j=0; j<MAXX; j+=8, offset++) {
	    // Avoid calculating black areas - see comment in preMandel
	    if (128 == previewBufferFiltered[offset]) {
		*p++ = 128; *p++ = 128; *p++ = 128; *p++ = 128;
		*p++ = 128; *p++ = 128; *p++ = 128; *p++ = 128;
		xcur += 8*xstep;
		#ifndef NDEBUG
		saved ++;
		#endif
		continue;
	    }
	    CoreLoopDouble(xcur, ycur, xstep, &p); xcur += 2*xstep;
	    CoreLoopDouble(xcur, ycur, xstep, &p); xcur += 2*xstep;
	    CoreLoopDouble(xcur, ycur, xstep, &p); xcur += 2*xstep;
	    CoreLoopDouble(xcur, ycur, xstep, &p); xcur += 2*xstep;
        }
    }
    SDL_UpdateRect(surface, 0, 0, MAXX, MAXY);
    #ifndef NDEBUG
    printf("Saved due to preview: %4.4f%%\n", (100.0*saved)/(MAXX*MAXY/4));
    #endif
}

#define CHECK(x) {						\
    unsigned long of = (unsigned long) &x[0];			\
    char so[32];				    		\
    sprintf(so, "%lx", of);			    		\
    if (so[strlen(so)-1] != '0') {			    	\
	fprintf(stderr,						\
	    "Your compiler is buggy... "			\
	    "it didn't align the SSE variables...\n"		\
	    "The application would crash. Aborting.\n");	\
	fflush(stderr);						\
	exit(1);						\
    }								\
}

int mandelSSE(int bAutoPilot)
{
    DECLARE_ALIGNED(16,float,testAlignment[4]);

    CHECK(testAlignment)
    CHECK(ones)
    CHECK(onesf)
    CHECK(fours)
    CHECK(foursf)
    CHECK(allbits)

#ifndef SIMD_SSE
    printf("[-] SSE2 mode:  off (using pipelined floating point)\n");
#else
    printf("[-] SSE2 mode:  on\n");
#endif

    previewBufferOriginal = (Uint8 *) malloc(MAXX*MAXY/16);
    previewBufferFiltered = (Uint8 *) malloc(MAXX*MAXY/16);

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
    const double
        targetx = -0.72996052273553402312, targety = -0.24047620199671820851;
    int mode = 0;
    while(1) {
        {
            unsigned st = SDL_GetTicks();
            preMandel(xld, yld, xru, yru);
            if ((xru-xld)<0.00002) {
                if (mode != 1) { SDL_WM_SetCaption(doubles,doubles); mode = 1; }
                mandelDouble(xld, yld, xru, yru);
            } else {
                if (mode != 0) { SDL_WM_SetCaption(floats,floats); mode = 0; }
                mandelFloat(xld, yld, xru, yru);
            }
            unsigned en = SDL_GetTicks();
            if (en - st < minimum_ms_per_frame)
                // Limit frame rate to 60 fps.
                SDL_Delay(minimum_ms_per_frame - en + st);
        }
        int result = kbhit(&x, &y);
        if (result == 1)
            break;
        if (bAutoPilot) {
            xld += (targetx - xld)/100.;
            xru += (targetx - xru)/100.;
            yld += (targety - yld)/100.;
            yru += (targety - yru)/100.;
        } else {
            if (result == 2 || result == 3) {
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
        }
	i++;
        if (bAutoPilot && i>ZOOM_FRAMES)
            break;
    }
    en = SDL_GetTicks();

    printf("[-] Frames/sec: %5.2f\n\n", ((float) i) / ((en - st) / 1000.0f));
    fflush(stdout);
    return 0;
}
