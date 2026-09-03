#ifndef CROSSOVER_H
#define CROSSOVER_H
#include "vignette.h"
enum { XO_WALK = 0, XO_HIDDEN = 1, XO_EMERGE = 2, XO_DONE = 3 };
extern const Vignette vignette_crossover;
int crossover_phase(void);
#endif
