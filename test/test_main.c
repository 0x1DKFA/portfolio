#include "test.h"

int test_checks = 0, test_failures = 0;

void test_harness(void);

int main(void) {
    RUN(test_harness);
    fprintf(stderr, "%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
