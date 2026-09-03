#include "vignettes/table.h"
#include "vignettes/patrol.h"
#include "vignettes/race.h"
#include "vignettes/detective.h"
#include "vignettes/regression.h"
#include "vignettes/crossover.h"

const Vignette *const VIGNETTES[VIG_COUNT] = {
    [VIG_PATROL]     = &vignette_patrol,
    [VIG_RACE]       = &vignette_race,
    [VIG_DETECTIVE]  = &vignette_detective,
    [VIG_REGRESSION] = &vignette_regression,
    [VIG_CROSSOVER]  = &vignette_crossover,
};
