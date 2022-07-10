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

void CoreLoopDouble(double xcur, double ycur, double xstep, unsigned char **p)
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

    unsigned char *tmp = *p;
    *tmp++ = outputs[0];
    *tmp++ = outputs[1];
    *tmp++ = outputs[2];
    *tmp++ = outputs[3];
    *p = tmp;
}
