#ifndef RACE_H
#define RACE_H
#include "vignette.h"
enum { RACE_SETUP = 0, RACE_RUSH, RACE_COLLIDE, RACE_LOCK, RACE_QUEUE, RACE_RESOLVED, RACE_DONE };
extern const Vignette vignette_race;
int race_phase(void);
int race_box_state(void);
int race_value(void);
#endif
