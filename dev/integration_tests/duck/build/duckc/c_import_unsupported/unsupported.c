#include "unsupported.h"

#include <stdarg.h>

int us_union_get_i(union us_union u) { return u.i; }

int us_bitfield_sum(struct us_bitfield b) { return (int)(b.a + b.b); }

int us_packed_get_b(struct us_packed p) { return p.b; }

int us_overaligned_get_a(struct us_overaligned o) { return o.a; }

int us_flexible_count(const struct us_flexible* f) { return f->count; }

int us_holds_union_tag(struct us_holds_union h) { return h.tag; }

int us_anonymous_tag(struct us_anonymous a) { return a.tag; }

int match(int a) { return a + 1; }

int in(int a) { return a + 2; }

int us_apply(int (*fn)(int), int value) { return fn(value); }

int us_sum_varargs(int count, ...) {
    va_list args;
    va_start(args, count);
    int total = 0;
    for (int i = 0; i < count; i++) total += va_arg(args, int);
    va_end(args);
    return total;
}

__int128 us_int128(__int128 a) { return a + 1; }

long double us_long_double(long double a) { return a + 1.0L; }
