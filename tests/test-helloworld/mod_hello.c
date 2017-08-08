#include <stdio.h>

int hello(int arg) {
    printf("Hello World! arg=%d\n", arg);
    return -arg;
}

int test(void) {
    return (hello(0) == 0 && hello(10) == -10);
}
