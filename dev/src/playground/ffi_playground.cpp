#include <ffi.h>

#include <array>
#include <cstdio>
#include <print>

int main() {
	ffi_cif    cif;
	std::array arg_types = { &ffi_type_pointer };

	if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint, arg_types.data()) != FFI_OK) {
		std::println(stderr, "ffi_prep_cif failed");
		return 1;
	}

	const char*          message    = "Hello from libffi!";
	std::array<void*, 1> arg_values = { &message };
	ffi_arg              result     = 0;

	ffi_call(&cif, reinterpret_cast<void (*)()>(std::puts), &result, arg_values.data());

	std::println("puts returned {} via libffi", static_cast<int>(result));
	return 0;
}
