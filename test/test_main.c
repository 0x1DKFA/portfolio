#include "test.h"

int test_checks = 0, test_failures = 0;

void test_harness(void);
void test_rng(void);

int main(void) {
    RUN(test_harness);
    RUN(test_rng);
    fprintf(stderr, "%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
