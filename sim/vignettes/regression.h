#ifndef REGRESSION_H
#define REGRESSION_H
#include "vignette.h"
enum { REG_BONK1 = 0, REG_BONK2, REG_THINK, REG_SHIELD, REG_BOUNCE, REG_CLEAR, REG_DONE };
extern const Vignette vignette_regression;
int regression_phase(void);
int regression_shield(void);
int regression_bounced(void);
#endif
