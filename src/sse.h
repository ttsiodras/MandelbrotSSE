#ifndef __MANDELSSE_H__
#define __MANDELSSE_H__

void CoreLoopDoubleDefault(double xcur, double ycur, double xstep, unsigned char **p);
void CoreLoopDoubleAVX(double xcur, double ycur, double xstep, unsigned char **p);

#endif
