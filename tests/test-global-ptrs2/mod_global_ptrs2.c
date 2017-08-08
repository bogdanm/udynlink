#include <stdio.h>

static volatile int g1 = 10;
volatile int g2 = 10;

static int f1(volatile int *x) {
    return (*x) * (*x);
}

int f2(volatile int *x) {
    return (*x) + (*x);
}

typedef int (*fptr_t)(volatile int*);

// Use both static and non-static pointers
const fptr_t const pcc_f1 = f1;
static volatile fptr_t const pc_f2 = f2;
static volatile int *p1_arg = &g1;
volatile int *p2_arg = &g2;

int test(void) {
    volatile fptr_t p_f;
    int res1, res2;

    printf("Running test '%s'\n", "mod_global_ptrs2");
    p_f = pcc_f1;
    p1_arg = &g1;
    res1 = p_f(p1_arg);
    p_f = pc_f2;
    p2_arg = &g2;
    res2 = p_f(p2_arg);
    return (res1 == 100) && (res2 == 20);
}

