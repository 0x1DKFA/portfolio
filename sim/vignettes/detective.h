#ifndef DETECTIVE_H
#define DETECTIVE_H
#include "vignette.h"
enum { DET_DIM = 0, DET_FOOTPRINTS, DET_GEAR, DET_FOLLOW, DET_REVEAL, DET_BONK, DET_UNDIM, DET_DONE };
extern const Vignette vignette_detective;
int detective_phase(void);
int detective_dim(void);
int detective_footprints(void);
#endif
