#include <stdio.h>
#include <inttypes.h>

void print_i64_no_space(int64_t value) {
    printf("%" PRId64, value);
}


void print_space() {
    printf(" ");
}

