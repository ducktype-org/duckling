
// NOLINTBEGIN
#include <stdint.h>

// <= 8 bytes: one INTEGER register, coerced to `{ i64 }`.
typedef struct {
	int32_t x;
	int32_t y;
} Small;

// Nested {{i32, f32}, f32}, 12 bytes: two eightbytes; eightbyte 0 mixes INTEGER
// (i32) + SSE (f32) -> INTEGER wins, eightbyte 1 is SSE. Coerced to `{ i64, float }`.
typedef struct {
	int32_t i;
	float f;
} MidInner;

typedef struct {
	MidInner inner;
	float g;
} Mid;

// Homogeneous {f64, f64, f64, f64}, 32 bytes: > 16 bytes, passed in memory
// (`byval`) / returned via `sret`. (On AArch64 this is an HFA in 4 SSE regs.)
typedef struct {
	double a;
	double b;
	double c;
	double d;
} Mid2;

// > 16 bytes: memory (`byval` argument) / `sret` return.
typedef struct {
	int64_t a;
	int64_t b;
	int64_t c;
} Big;

// --- Pass a struct by value, return a scalar. ---

int64_t take_small(Small s) { return (int64_t) s.x + s.y; }

int64_t take_mid(Mid s) {
	return (int64_t) s.inner.i + (int64_t) s.inner.f + (int64_t) s.g;
}

int64_t take_mid2(Mid2 s) { return (int64_t) (s.a + s.b + s.c + s.d); }

int64_t take_big(Big s) { return s.a + s.b + s.c; }

// --- Take a scalar, return a struct by value. ---

Small make_small(int64_t k) {
	Small s = { (int32_t) k, (int32_t) (k * 2) };
	return s;
}

Mid make_mid(int64_t k) {
	Mid s = { { (int32_t) k, (float) (k * 2) }, (float) (k * 3) };
	return s;
}

Mid2 make_mid2(int64_t k) {
	Mid2 s = { (double) k, (double) (k * 2), (double) (k * 3), (double) (k * 4) };
	return s;
}

Big make_big(int64_t k) {
	Big s = { k, k * 2, k * 3 };
	return s;
}

int32_t add_shorts(int16_t a, int16_t b) { return (int32_t) a + (int32_t) b; }
int32_t add_bytes(int8_t a, int8_t b) { return (int32_t) a + (int32_t) b; }

//NOLINTEND
