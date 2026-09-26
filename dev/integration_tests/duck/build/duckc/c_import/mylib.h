#pragma once

#define MYLIB_MAX 128

enum mylib_color { MYLIB_RED = 0, MYLIB_GREEN = 1, MYLIB_BLUE = 7 };

struct mylib_point {
	int x;
	int y;
};
typedef struct mylib_point mylib_point_t;

/* Self-referential through a pointer. */
struct mylib_node {
	int                value;
	struct mylib_node* next;
};

/* Neither has a C ABI mapping, so both have to be skipped rather than guessed at. */
union mylib_bad {
	int    i;
	double d;
};

struct mylib_bits {
	unsigned a: 3;
	unsigned b: 5;
};

int                mylib_add(int a, int b);
int                mylib_point_sum(struct mylib_point p);
struct mylib_point mylib_point_make(int x, int y);
int                mylib_node_value(const struct mylib_node* n);
signed char        mylib_neg_char(signed char c);

/* Variadic and `static` declarations are skipped too. */
int mylib_printf(const char* fmt, ...);

static inline int mylib_inline(int a) { return a; }
