/*
 * Every C construct `duck_c_import` cannot translate yet. Each one is skipped with a reason
 * rather than approximated, because every one of them would otherwise produce a program that
 * links and then reads the wrong memory.
 *
 * The enabled test locks in that they are reported. The disabled tests next to it are what
 * should pass once the language or ABI gap behind each one is closed.
 */
#pragma once

/* ---- #3647: records the C ABI cannot express ---- */

union us_union {
	int    i;
	double d;
};

int us_union_get_i(union us_union u);

struct us_bitfield {
	unsigned a: 3;
	unsigned b: 5;
};

int us_bitfield_sum(struct us_bitfield b);

struct __attribute__((packed)) us_packed {
	char a;
	int  b;
};

int us_packed_get_b(struct us_packed p);

struct __attribute__((aligned(16))) us_overaligned {
	int a;
};

int us_overaligned_get_a(struct us_overaligned o);

struct us_flexible {
	int count;
	int rest[];
};

int us_flexible_count(const struct us_flexible* f);

/* A record holding any of the above is unsupported too. */
struct us_holds_union {
	int            tag;
	union us_union payload;
};

int us_holds_union_tag(struct us_holds_union h);

/* ---- #3648: anonymous struct and union members ---- */

struct us_anonymous {
	int tag;
	union {
		int   i;
		float f;
	};
};

int us_anonymous_tag(struct us_anonymous a);

/* ---- #3649: C symbols whose name is a Duckling keyword ---- */

int match(int a);
int in(int a);

/* ---- #3650: C function pointers as callable values ---- */

int us_apply(int (*fn)(int), int value);

/* ---- #3271: variadic functions ---- */

int us_sum_varargs(int count, ...);

/* ---- #1498: scalars with no Duckling type ---- */

__int128    us_int128(__int128 a);
long double us_long_double(long double a);
