#ifndef TEST_H
#define TEST_H
#include <stdio.h>
#include <string.h>
#include <math.h>

extern int test_checks, test_failures;

#define CHECK(cond) do { test_checks++; if (!(cond)) { test_failures++; \
    fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); test_checks++; \
    if (_a != _b) { test_failures++; \
    fprintf(stderr, "  FAIL %s:%d: %s == %s (%ld != %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

#define CHECK_NEAR(a, b, eps) do { double _a = (a), _b = (b); test_checks++; \
    if (fabs(_a - _b) > (eps)) { test_failures++; \
    fprintf(stderr, "  FAIL %s:%d: %s ~= %s (%f vs %f)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

#define RUN(fn) do { fprintf(stderr, "%s\n", #fn); fn(); } while (0)

#endif
