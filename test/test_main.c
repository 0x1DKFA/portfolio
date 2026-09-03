#include "test.h"

int test_checks = 0, test_failures = 0;

void test_harness(void);
void test_rng(void);
void test_events(void);
void test_draw(void);
void test_font(void);
void test_bug(void);
void test_hero(void);

int main(void) {
    RUN(test_harness);
    RUN(test_rng);
    RUN(test_events);
    RUN(test_draw);
    RUN(test_font);
    RUN(test_bug);
    RUN(test_hero);
    fprintf(stderr, "%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
