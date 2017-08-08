#include <stdio.h>

volatile int g = 50;
volatile static int g2 = 30;

int test(void) {
    printf("Running test '%s'\n", "mod_globals1");
    g += 10;
    g2 += 20;
    return (g == 60) && (g2 == 50);
}

