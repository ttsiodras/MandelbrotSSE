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

    DECLARE_ALIGNED(32,double,outputs[4]);

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
	"vmovapd %3,%%ymm5\n\t"                //  4.     4.        ; ymm5
	"vmovapd %1,%%ymm6\n\t"                //  a0     a1        ; ymm6
	"vmovaps %2,%%ymm7\n\t"                //  b0     b1        ; ymm7
	"vxorpd  %%ymm0,%%ymm0,%%ymm0\n\t"     //  0.     0.        ; rez in ymm0
	"vxorpd  %%ymm1,%%ymm1,%%ymm1\n\t"     //  0.     0.        ; imz in ymm1
	"vxorpd  %%ymm3,%%ymm3,%%ymm3\n\t"     //  0.     0.        ; bailout counters
	"vxorpd  %%ymm8,%%ymm8,%%ymm8\n\t"     //  0.     0.        ; bailout counters
	"vxorpd  %%ymm9,%%ymm9,%%ymm9\n\t"     //  0.     0.        ; bailout counters

	"1:\n\t"                               //  Main Mandelbrot computation
	"vmovapd %%ymm0,%%ymm2\n\t"            //  x0     x1        ; ymm2
	"vmulpd  %%ymm1,%%ymm2,%%ymm2\n\t"     //  x0*y0  x1*y1     ; ymm2
	"vmulpd  %%ymm0,%%ymm0,%%ymm0\n\t"     //  x0^2   x1^2      ; ymm0
	"vmulpd  %%ymm1,%%ymm1,%%ymm1\n\t"     //  y0^2   y1^2      ; ymm1
	"vmovapd %%ymm0,%%ymm4\n\t"            //  
	"vaddpd  %%ymm1,%%ymm4,%%ymm4\n\t"     //  x0^2+y0^2  x1... ; ymm4
	"vsubpd  %%ymm1,%%ymm0,%%ymm0\n\t"     //  x0^2-y0^2  x1... ; ymm0
	"vaddpd  %%ymm6,%%ymm0,%%ymm0\n\t"     //  x0'    x1'       ; ymm0
	"vmovapd %%ymm2,%%ymm1\n\t"            //  x0*y0  x1*y1     ; ymm1
	"vaddpd  %%ymm1,%%ymm1,%%ymm1\n\t"     //  2x0*y0 2x1*y1    ; ymm1
	"vaddpd  %%ymm7,%%ymm1,%%ymm1\n\t"     //  y0'    y1'       ; ymm1

	"vcmpltpd %%ymm5,%%ymm4,%%ymm4\n\t"    //  <4     <4        ; ymm2
	"vmovapd %%ymm4,%%ymm2\n\t"            //  ymm2 has all 1s in the non-overflowed pixels
	"vmovmskpd %%ymm4,%%eax\n\t"           //  (lower 2 bits reflect comparisons)
	"vandpd  %4,%%ymm4,%%ymm4\n\t"         //  so, prepare to increase the non-overflowed (and with ones)
	"vaddpd  %%ymm4,%%ymm3,%%ymm3\n\t"     //  by updating their counters

	"or     %%eax,%%eax\n\t"               //  have both pixels overflowed ?

	"je     2f\n\t"                        //  yes, jump forward to label 2 (hence, 2f) and end the loop
	"dec    %%ecx\n\t"                     //  otherwise, repeat the loop ITERA times...
	"jnz    22f\n\t"                       //  but before redoing the loop, first do periodicity checking

                                               //  We've done the loop ITERA times.
                                               //  Set non-overflowed outputs to 0 (inside ymm3). Here's how:
	"vmovapd %%ymm2,%%ymm4\n\t"            //  ymm4 has all 1s in the non-overflowed pixels...
	"vxorpd  %5,%%ymm4,%%ymm4\n\t"         //  ymm4 has all 1s in the overflowed pixels (toggled, via xoring with allbits)
	"vandpd  %%ymm4,%%ymm3,%%ymm3\n\t"     //  zero out the ymm3 parts that belong to non-overflowed (set to black)
	"jmp    2f\n\t"                        //  And jump to end of everything, where ymm3 is written into outputs

	"22:\n\t"                              //  Periodicity checking
        "inc %%bl\n\t"                         //  period++
        "and $0xF, %%bl\n\t"                   //  period &= 0xF
        "jnz 11f\n\t"                          //  if period is not zero, continue to check if we're seeing xold, yold again
        "vmovapd %%ymm0, %%ymm8\n\t"           //  time to update xold[2], yold[2] - store xold[2] in ymm8
        "vmovapd %%ymm1, %%ymm9\n\t"           //  and yold[2] in ymm9
	"jmp    1b\n\t"                        //  and jump back to the loop beginning

        "11:\n\t"                              //  are we seeing xold[2], yold[2] into our rez[2], imz[2]?
        "vmovapd %%ymm8, %%ymm10\n\t"          //  the comparison instruction will modify the target XMM register, so use ymm10
        "vcmpeqpd %%ymm0, %%ymm10,%%ymm10\n\t" //  compare ymm10 (which now has xold[2]) with rez[2]. Set all 1s into ymm10 if equal
	"vmovmskpd %%ymm10,%%eax\n\t"          //  the lower 2 bits of EAX now reflect the result of the comparison. 
        "or %%eax, %%eax\n\t"                  //  are they BOTH zero?
        "jz 1b\n\t"                            //  Yes - so, neither of the two rez matched with the two xold. Repeat the loop
        "vmovapd %%ymm9, %%ymm10\n\t"          //  Set ymm10 to contain yold[2]
        "vcmpeqpd %%ymm1, %%ymm10,%%ymm10\n\t" //  compare ymm10 with imz[2]. Set all 1s into ymm10 if equal
	"vmovmskpd %%ymm10,%%eax\n\t"          //  the lower 2 bits of EAX now reflect the result of the comparison.
        "or %%eax, %%eax\n\t"                  //  are they BOTH zero?
        "jz 1b\n\t"                            //  Yes - so, neither of the two imz matched with the two yold. Repeat the loop
	"vxorpd  %%ymm3,%%ymm3,%%ymm3\n\t"     //  Repetition detected. Set both results to 0.0 (both pixels black)

	"2:\n\t"
	"vmovapd %%ymm3,%0\n\t"
	:"=m"(outputs[0])
	:"m"(re[0]),"m"(im[0]),"m"(fours[0]),"m"(ones[0]),"m"(allbits[0]),"i"(ITERA)
	:"%eax","%ebx","%ecx","ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7","ymm8","ymm9","ymm10","memory");

    int tmp;
    tmp = (int)(outputs[0]); *(*p)++ = tmp == ITERA ? 0: tmp;
    tmp = (int)(outputs[1]); *(*p)++ = tmp == ITERA ? 0: tmp;
    tmp = (int)(outputs[2]); *(*p)++ = tmp == ITERA ? 0: tmp;
    tmp = (int)(outputs[3]); *(*p)++ = tmp == ITERA ? 0: tmp;
}
