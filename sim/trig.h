#ifndef TRIG_H_HEADER
#define TRIG_H_HEADER

#define TRIG_PI 3.14159265f
#define TRIG_TAU 6.28318531f
#define TRIG_HALF_PI 1.57079633f
#define TRIG_TABLE 1024

void  trig_init(void);
float trig_sin(float a);
float trig_cos(float a);
float trig_atan2(float y, float x);   /* (-pi, pi] */
float trig_wrap(float a);             /* into (-pi, pi] */

#endif
