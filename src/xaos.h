#ifndef __MANDEL_XAOS_H__
#define __MANDEL_XAOS_H__

double autopilot();
double mousedriven();
void mandel(
    double xld, double yld, double xru, double yru,
    double percentageOfPixelsToRedraw);

#endif
