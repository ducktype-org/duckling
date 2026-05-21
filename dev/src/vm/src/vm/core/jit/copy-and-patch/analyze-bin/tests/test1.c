#include <stdint.h>

int foo(int x);

int goo(int x) {
    return foo(x + 2) + 1;
}

int hoo(int x) {
    extern int _arg0;
    return x + (intptr_t)_arg0;
}
