#include "mylib.h"

int mylib_add(int a, int b) { return a + b; }
int mylib_point_sum(struct mylib_point p) { return p.x + p.y; }

struct mylib_point mylib_point_make(int x, int y) {
    struct mylib_point p;
    p.x = x;
    p.y = y;
    return p;
}

int mylib_node_value(const struct mylib_node *n) {
    return n->next ? n->next->value : n->value;
}

signed char mylib_neg_char(signed char c) { return (signed char)(-c); }

int mylib_printf(const char *fmt, ...) { (void)fmt; return 0; }

int mylib_extra_scaled_sum(struct mylib_point p, int scale) {
    return (p.x + p.y) * scale;
}
