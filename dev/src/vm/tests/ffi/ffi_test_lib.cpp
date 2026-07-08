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
}

// NOLINTEND(readability-identifier-naming,cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
