#include <stdio.h>

volatile int g = 50;
extern int ext_i;

int test(void) {
    printf("Running test '%s'\n", "mod_globals2");
    g = g + 10 + ext_i;
    ext_i = -ext_i;
    return 1; // the test driver will perform the actual code
}

