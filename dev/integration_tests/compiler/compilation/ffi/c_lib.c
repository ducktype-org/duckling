#include<stdio.h>
#include <stdint.h>

int64_t say_hello(int64_t times) {
    for (int64_t i = 0; i < times; i++) {
        printf("Hello from C!\n");
    }
    return 0;
}
