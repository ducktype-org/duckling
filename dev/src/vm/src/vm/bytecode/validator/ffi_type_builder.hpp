#pragma once

#include <ffi.h>

#include <base/pointers/box.hpp>

#include <vm/bytecode/validator/valid_type/valid_type.hpp>

namespace vm::code::ffi_detail {
	/**
	 * @brief Owning storage for libffi struct type descriptors. Must outlive any `ffi_cif`
	 * prepared with types built by `buildFFIType`.
	 */
	struct FFITypeStorage {
		std::vector<Box<ffi_type>>               struct_types;
		std::vector<Box<std::vector<ffi_type*>>> struct_elements;
	};

	/**
	 * @brief Returns the libffi type describing the given VM type.
	 * Supports primitives of size 1, 2, 4 or 8 (mapped to signed integers), opaque types
	 * (mapped to a pointer, used for the builtin `cptr`) and data structures of the above.
	 * @note The caller must ensure the type was validated as FFI-compatible.
	 */
	ffi_type* buildFFIType(
		const valid_type::ValidType&    type,
		const valid_type::ValidTypeMap& types,
		FFITypeStorage&                 storage
	);
}
