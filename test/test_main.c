#include "test.h"

int test_checks = 0, test_failures = 0;

void test_harness(void);
void test_rng(void);
void test_events(void);
void test_draw(void);
void test_font(void);
void test_stage(void);
void test_trig(void);
void test_palette(void);
void test_map(void);
void test_textures(void);
void test_camera(void);
void test_decals(void);
void test_sprites(void);
void test_raycast(void);

int main(void) {
    RUN(test_harness);
    RUN(test_rng);
    RUN(test_events);
    RUN(test_draw);
    RUN(test_font);
    RUN(test_stage);
    RUN(test_trig);
    RUN(test_palette);
    RUN(test_map);
    RUN(test_textures);
    RUN(test_camera);
    RUN(test_decals);
    RUN(test_sprites);
    RUN(test_raycast);
    fprintf(stderr, "%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
