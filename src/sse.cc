#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "sse.h"

#ifdef __GNUC__
    #define DECLARE_ALIGNED(n,t,v)       t v __attribute__ ((aligned (n)))
#else
    #define DECLARE_ALIGNED(n,t,v)      __declspec(align(n)) t v
#endif

DECLARE_ALIGNED(16,double,ones[4]) = { 1.0, 1.0, 1.0, 1.0 };
DECLARE_ALIGNED(16,double,fours[4]) = { 4.0, 4.0, 4.0, 4.0 };

DECLARE_ALIGNED(32,unsigned,allbits[8]) = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};

void CoreLoopDoubleDefault(double xcur, double ycur, double xstep, unsigned char **p)
{

#define CLEAR_ARRAY(x) memset(&x, 0, sizeof(x))
#define COPY_ARRAY4(src, dst) \
    dst[0] = src[0]; \
    dst[1] = src[1]; \
    dst[2] = src[2]; \
    dst[3] = src[3];

    DECLARE_ALIGNED(32,double,re[4]);
    DECLARE_ALIGNED(32,double,im[4]);
    DECLARE_ALIGNED(32,unsigned,k1[4]);

    DECLARE_ALIGNED(32,double,rez[4]);
    DECLARE_ALIGNED(32,double,imz[4]);
    DECLARE_ALIGNED(32,double,xold[4]);
    DECLARE_ALIGNED(32,double,yold[4]);
    double t1, t2, o1, o2;
    int k=0, period=0;

    re[0] = xcur;
    re[1] = (xcur + xstep);
    re[2] = (xcur + 2*xstep);
    re[3] = (xcur + 3*xstep);

    im[0] = im[1] = im[2] = im[3] = ycur;

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
    *(*p)++ = k1[0];
    *(*p)++ = k1[1];
    *(*p)++ = k1[2];
    *(*p)++ = k1[3];
}

#ifdef __x86_64__

void CoreLoopDoubleSSE(double xcur, double ycur, double xstep, unsigned char **p)
{
    DECLARE_ALIGNED(16,double,re[2]);
    DECLARE_ALIGNED(16,double,im[2]);
    DECLARE_ALIGNED(16,unsigned,k1[2]);

    DECLARE_ALIGNED(16,double,outputs[2]);

    re[0] = xcur;
    re[1] = (xcur + xstep);

    im[0] = im[1] = ycur;

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
    *(*p)++ = tmp;
    tmp = (int)(outputs[1]);
    *(*p)++ = tmp;

    re[0] = xcur + 2*xstep;
    re[1] = xcur + 3*xstep;

    im[0] = im[1] = ycur;

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

    tmp = (int)(outputs[0]);
    *(*p)++ = tmp;
    tmp = (int)(outputs[1]);
    *(*p)++ = tmp;
}

void CoreLoopDoubleAVX(double xcur, double ycur, double xstep, unsigned char **p)
{
    DECLARE_ALIGNED(32,double,re[4]);
    DECLARE_ALIGNED(32,double,im[4]);

    DECLARE_ALIGNED(32,unsigned,outputs[4]);

    re[0] = xcur;
    re[1] = (xcur + xstep);
    re[2] = (xcur + 2*xstep);
    re[3] = (xcur + 3*xstep);

    im[0] = im[1] = im[2] = im[3] = ycur;
					       // x' = x^2 - y^2 + a
					       // y' = 2xy + b
					       //
    asm("mov    %6,%%ecx\n\t"                  //  ecx is ITERA
        "xor    %%ebx, %%ebx\n\t"              //  period = 0
	"vmovapd %3,%%ymm5\n\t"                //  4.     4.      4.     4.   ; ymm5
	"vmovapd %1,%%ymm6\n\t"                //  a0     a1      a2     a3   ; ymm6
	"vmovaps %2,%%ymm7\n\t"                //  b0     b1      b2     b3   ; ymm7
	"vmovaps %4,%%ymm11\n\t"               //  1.     1.      1.     1.   ; ymm11
	"vmovaps %5,%%ymm12\n\t"               //  allbits                    ; ymm12
	"vxorpd  %%ymm0,%%ymm0,%%ymm0\n\t"     //  0.     0.      0.     0.   ; rez in ymm0
	"vxorpd  %%ymm1,%%ymm1,%%ymm1\n\t"     //  0.     0.      0.     0.   ; imz in ymm1
	"vxorpd  %%ymm3,%%ymm3,%%ymm3\n\t"     //  0.     0.      0.     0.   ; bailout counters
	"vxorpd  %%ymm8,%%ymm8,%%ymm8\n\t"     //  0.     0.      0.     0.   ; periodicity check for x
	"vxorpd  %%ymm9,%%ymm9,%%ymm9\n\t"     //  0.     0.      0.     0.   ; periodicity check for y

	"1:\n\t"                               //  Main Mandelbrot computation loop (label: 1)
                                               //
	"vmulpd  %%ymm1,%%ymm0,%%ymm2\n\t"     //  x0*y0  x1*y1   x2*y2  x3*y3   ; ymm2
	"vmulpd  %%ymm0,%%ymm0,%%ymm0\n\t"     //  x0^2   x1^2    x2^2   x3^2    ; ymm0
	"vmulpd  %%ymm1,%%ymm1,%%ymm1\n\t"     //  y0^2   y1^2    y2^2   y3^2    ; ymm1
	"vaddpd  %%ymm1,%%ymm0,%%ymm4\n\t"     //  x0^2+y0^2  x1... ; ymm4
	"vsubpd  %%ymm1,%%ymm0,%%ymm0\n\t"     //  x0^2-y0^2  x1... ; ymm0
	"vaddpd  %%ymm6,%%ymm0,%%ymm0\n\t"     //  x0'    x1'       ; ymm0
	"vaddpd  %%ymm2,%%ymm2,%%ymm1\n\t"     //  2x0*y0 2x1*y1    ; ymm1
	"vaddpd  %%ymm7,%%ymm1,%%ymm1\n\t"     //  y0'    y1'       ; ymm1

	"vcmpltpd %%ymm5,%%ymm4,%%ymm4\n\t"    //  <4     <4        ; ymm2
	"vmovapd %%ymm4,%%ymm2\n\t"            //  ymm2 and ymm4 have all 1s in the non-overflowed pixels
	"vmovmskpd %%ymm4,%%eax\n\t"           //  lower 4 bits of EAX reflect comparisons with 4.0
	"vandpd  %%ymm11,%%ymm4,%%ymm4\n\t"    //  AND with 4 ones...
	"vaddpd  %%ymm4,%%ymm3,%%ymm3\n\t"     //  ...so you only update the counters of non-overflowed pixels

	"or     %%eax,%%eax\n\t"               //  have all 4 pixels overflowed ?
	"je     2f\n\t"                        //  yes, jump forward to label 2 (hence, 2f) and end the loop
                                               //
	"dec    %%ecx\n\t"                     //  otherwise, repeat the loop up to ITERA times...
	"jnz    22f\n\t"                       //  but before redoing the loop, first do periodicity checking

                                               //  We've done the loop ITERA times.
                                               //  Set non-overflowed outputs to 0 (inside ymm3). Here's how:
	"vmovapd %%ymm2,%%ymm4\n\t"            //  ymm4 has all 1s in the non-overflowed pixels...
	"vxorpd  %%ymm12,%%ymm4,%%ymm4\n\t"    //  ymm4 has all 1s in the overflowed pixels (toggled, via xoring with allbits)
	"vandpd  %%ymm4,%%ymm3,%%ymm3\n\t"     //  zero out the ymm3 parts that belong to non-overflowed (set to black)
	"jmp    2f\n\t"                        //  And jump to end of everything, where ymm3 is written into outputs

	"22:\n\t"                              //  Periodicity checking
        "and $0xF, %%bl\n\t"                   //  period &= 0xF
        "jnz 11f\n\t"                          //  if period is not zero, continue to check if we're seeing xolds, yolds again
        "inc %%bl\n\t"                         //  period++
        "vmovapd %%ymm0, %%ymm8\n\t"           //  time to update xolds and yolds - store xolds in ymm8...
        "vmovapd %%ymm1, %%ymm9\n\t"           //  ...and yolds in ymm9
	"jmp    1b\n\t"                        //  ..and jump back to the loop beginning.

        "11:\n\t"                              //  are we seeing xolds, yolds in our rez, imz?
        "vcmpeqpd %%ymm0, %%ymm8,%%ymm10\n\t"  //  compare ymm8 (which has the xolds) with rez. Set all 1s into ymm10 if equal
	"vmovmskpd %%ymm10,%%eax\n\t"          //  the lower 4 bits of EAX now reflect the result of the 4 comparisons. 
        "or %%eax, %%eax\n\t"                  //  are they ALL zero?
        "jz 1b\n\t"                            //  Yes - so, none of the four rez matched with the four xold. Repeat the loop
        "vcmpeqpd %%ymm1, %%ymm9,%%ymm10\n\t"  //  compare ymm9 with imz. Set all 1s into ymm10 if equal
	"vmovmskpd %%ymm10,%%eax\n\t"          //  the lower 4 bits of EAX now reflect the result of the 4 comparisons.
        "or %%eax, %%eax\n\t"                  //  are they ALL zero?
        "jz 1b\n\t"                            //  Yes - so, none of the four imz matched with the four yold. Repeat the loop
	"vxorpd  %%ymm3,%%ymm3,%%ymm3\n\t"     //  Repetition detected in at least one of the 4! Set results to 0.0 (pixels black)
                                               //  Normally, we should do this if ALL 4 repeated. But... good enough. And speedy!
	"2:\n\t"
        "vcvttpd2dq %%ymm3, %%xmm0\n\t"        //  Convert 4 doubles into 4 ints.
	"movapd %%xmm0,%0\n\t"
	:"=m"(outputs[0])
	:"m"(re[0]),"m"(im[0]),"m"(fours[0]),"m"(ones[0]),"m"(allbits[0]),"i"(ITERA)
	:"%eax","%ebx","%ecx","%ymm0","%ymm1","%ymm2","%ymm3","%ymm4","%ymm5","%ymm6","%ymm7","%ymm8","%ymm9","%ymm10","%xmm0","%ymm11","%ymm12","memory");

    *(*p)++ = outputs[0];
    *(*p)++ = outputs[1];
    *(*p)++ = outputs[2];
    *(*p)++ = outputs[3];
}
#endif
