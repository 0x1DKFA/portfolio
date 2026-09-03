#include "test.h"

int test_checks = 0, test_failures = 0;

void test_harness(void);
void test_rng(void);
void test_events(void);
void test_draw(void);
void test_font(void);
void test_bug(void);
void test_hero(void);
void test_world(void);
void test_sprites(void);
void test_stage(void);
void test_patrol(void);
void test_scene(void);
void test_crossover(void);
void test_race(void);
void test_detective(void);
void test_regression(void);

int main(void) {
    RUN(test_harness);
    RUN(test_rng);
    RUN(test_events);
    RUN(test_draw);
    RUN(test_font);
    RUN(test_bug);
    RUN(test_hero);
    RUN(test_world);
    RUN(test_sprites);
    RUN(test_stage);
    RUN(test_patrol);
    RUN(test_scene);
    RUN(test_crossover);
    RUN(test_race);
    RUN(test_detective);
    RUN(test_regression);
    fprintf(stderr, "%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
