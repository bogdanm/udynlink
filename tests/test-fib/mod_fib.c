#include <stdio.h>

static int rec_fib(int i) {
    if (i == 0 || i == 1)
        return 1;
    else
        return rec_fib(i - 1) + rec_fib(i - 2);
}

static int iter_fib(int i) {
    int p1 = 1, p2 = 1, k, f = 0;

    if (i == 0 || i == 1)
        return 1;
    for(k = 2; k <= i; k ++) {
        f = p2;
        p2 = p1 + p2;
        p1 = f;
    }
    return p2;
}

static int test_single_value(int i, int expected) {
    return rec_fib(i) == expected && iter_fib(i) == expected;
}

int test(void) {
    return test_single_value(0, 1) && test_single_value(1, 1) && test_single_value(11, 144);
}
