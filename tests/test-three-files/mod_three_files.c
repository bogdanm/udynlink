// This module is implemented in 3 different files

#include <stdio.h>

extern int f1_square(int);
extern int f2_sum(int);

int test(void) {
    printf("Running test '%s'\n", "mod_three_files");
    return (f1_square(10) == 100) && (f2_sum(10) == 20);
}
