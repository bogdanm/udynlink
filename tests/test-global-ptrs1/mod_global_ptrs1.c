#include <stdio.h>

volatile int g = 10;

static int f1(volatile int *x) {
    return (*x) * (*x);
}

int f2(volatile int *x) {
    return (*x) + (*x);
}

typedef int (*fptr_t)(volatile int*);

// Use both static and non-static pointers
volatile fptr_t p_f1 = f1;
static volatile fptr_t p_f2 = f2;
static volatile int *p_arg = &g;

int test(void) {
    volatile fptr_t p_f;
    int res1, res2;

    printf("Running test '%s'\n", "mod_global_ptrs1");
    p_f = p_f1;
    p_arg = &g;
    res1 = p_f(p_arg);
    p_f = p_f2;
    res2 = p_f(p_arg);
    return (res1 == 100) && (res2 == 20);
}

