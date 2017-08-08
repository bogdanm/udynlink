#include <stdio.h>

volatile int g = 10;

static int f1(volatile int *x) {
    return (*x) * (*x);
}

int f2(volatile int *x) {
    return (*x) + (*x);
}

typedef int (*fptr_t)(volatile int*);

int test(void) {
    volatile fptr_t p_f;
    volatile int *p_arg;
    int res1, res2;

    printf("Running test '%s'\n", "mod_local_ptrs");
    p_f = f1;
    p_arg = &g;
    res1 = p_f(p_arg);
    p_f = f2;
    res2 = p_f(p_arg);
    return (res1 == 100) && (res2 == 20);
}

