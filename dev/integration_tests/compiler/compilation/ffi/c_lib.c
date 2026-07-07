
// NOLINTBEGIN
#include <stdint.h>
// Struct passed/returned in a single eight-byte (<= 8 bytes): SysV classifies it as
// one INTEGER register, coerced to `{ i64 }` on the LLVM side.
typedef struct {
	int32_t x;
	int32_t y;
} Small;

// Struct passed/returned in two eight-bytes (> 8 and <= 16 bytes): SysV classifies it
// as two INTEGER registers, coerced to `{ i64, i64 }` on the LLVM side.
typedef struct {
	int64_t a;
	int64_t b;
} Mid;

// Struct larger than 16 bytes: SysV passes it in memory (`byval` pointer argument) and
// returns it indirectly through a hidden `sret` pointer.
typedef struct {
	int64_t a;
	int64_t b;
	int64_t c;
} Big;

// --- Pass a struct by value, return a scalar. ---

int64_t take_small(Small s) { return (int64_t) s.x + s.y; }

int64_t take_mid(Mid s) { return s.a + s.b; }

int64_t take_big(Big s) { return s.a + s.b + s.c; }

// --- Take a scalar, return a struct by value. ---

Small make_small(int64_t k) {
	Small s = { (int32_t) k, (int32_t) (k * 2) };
	return s;
}

Mid make_mid(int64_t k) {
	Mid s = { k, k * 2 };
	return s;
}

Big make_big(int64_t k) {
	Big s = { k, k * 2, k * 3 };
	return s;
}

//NOLINTEND
