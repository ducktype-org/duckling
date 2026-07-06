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
}

// NOLINTEND(readability-identifier-naming,cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
