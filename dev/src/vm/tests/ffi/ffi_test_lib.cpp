// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <cstdint>
#include <cstdlib>

// Native functions loaded by the FFI VM tests. Compiled into a shared object and called via
// libffi from bytecode `call_ffifunc` instructions. C-style names and raw allocation are
// intentional here - this is the "foreign" side of the boundary.

// NOLINTBEGIN(readability-identifier-naming,cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
extern "C" {
	int64_t ffi_add(int64_t a, int64_t b) { return a + b; }

	// Exercises the void, argument-taking path plus a state read-back.
	static int64_t g_stored = 0;

	void ffi_set(int64_t v) { g_stored = v; }

	int64_t ffi_get() { return g_stored; }

	// Small (< sizeof(ffi_arg)) integer return, to exercise result widening.
	int32_t ffi_small() { return 12'345; }

	// Pointer (cptr) round-trip helpers.
	void* ffi_alloc8() { return std::malloc(8); }

	// Sized allocation, for the typed cpointer instructions (malloc workflow).
	void* ffi_alloc(int64_t n) {
		if (n <= 0) return nullptr;
		return std::malloc(static_cast<size_t>(n));
	}

	void ffi_free(void* p) { std::free(p); }

	void ffi_fill8(void* p, int64_t v) { *static_cast<int64_t*>(p) = v; }

	int64_t ffi_read8(void* p) { return *static_cast<int64_t*>(p); }

	void ffi_free8(void* p) { std::free(p); }

	// Floating-point args and returns - misclassified as integers unless the `f32`/`f64`
	// name-based mapping puts them in the SSE argument class.
	float ffi_addf(float a, float b) { return a + b; }

	double ffi_addd(double a, double b) { return a + b; }

	// Mixed INTEGER/SSE argument classification.
	double ffi_mix(int64_t a, double b, int64_t c, double d) {
		return static_cast<double>(a + c) + b + d;
	}

	// Struct with float fields - an all-SSE aggregate, passed and returned by value.
	struct FPair {
		float a;
		float b;
	};

	FPair ffi_fpair_swap(FPair p) { return { .a = p.b, .b = p.a }; }

	// Struct mixing a pointer (cptr) field with an integer, passed by value.
	struct CPair {
		void*   p;
		int64_t v;
	};

	int64_t ffi_cpair_sum(CPair c) { return *static_cast<int64_t*>(c.p) + c.v; }

	// Struct with a pointer field passed by value; C writes through the pointer.
	struct Tagged {
		void*   p;
		int64_t tag;
	};

	void ffi_tagged_store(Tagged t) { *static_cast<int64_t*>(t.p) = t.tag; }

	// Struct whose C layout needs alignment padding (b sits at offset 8, size is 16).
	struct Mix {
		int8_t  a;
		int64_t b;
	};

	int64_t ffi_mix_sum(Mix m) { return static_cast<int64_t>(m.a) + m.b; }

	// Struct with an array field - flattened in the libffi descriptor (libffi has no array type).
	struct WithArr {
		int32_t v[4];
		int64_t tail;
	};

	int64_t ffi_arr_sum(WithArr w) {
		int64_t sum = w.tail;
		for (int32_t x: w.v) sum += x;
		return sum;
	}

	// Struct with an array field, returned by value (the return path differs from the argument
	// path - a 24-byte aggregate comes back through a hidden pointer).
	WithArr ffi_arr_make(int32_t base) {
		WithArr w{};
		int32_t next = base;
		for (int32_t& x: w.v) x = next++;
		w.tail = static_cast<int64_t>(base) * 2;
		return w;
	}

	// All-SSE aggregate with an array field, passed by value.
	struct FQuad {
		float v[4];
	};

	float ffi_fquad_sum(FQuad q) { return q.v[0] + q.v[1] + q.v[2] + q.v[3]; }

	// Nested plain structs, passed by value.
	struct Inner {
		int32_t x;
		int32_t y;
	};

	struct Outer {
		Inner   first;
		int64_t z;
	};

	int64_t ffi_nested_sum(Outer o) { return o.first.x + o.first.y + o.z; }

	// Struct with an array-of-structs field, passed by value.
	struct PtTab {
		Inner   pts[2];
		int64_t tail;
	};

	int64_t ffi_pt_sum(PtTab t) {
		int64_t sum = t.tail;
		for (const Inner& p: t.pts) sum += p.x + p.y;
		return sum;
	}
}

// NOLINTEND(readability-identifier-naming,cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
