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
	 * Primitives named `f32` (size 4) and `f64` (size 8) map to `float`/`double` — the names
	 * the DVM backend emits for floating-point types; without this they would be classified
	 * as integers, which breaks the C calling convention (e.g. SysV passes floats in XMM
	 * registers).
	 * libffi has no array type, so a fixed-size table structure field is flattened: its element
	 * type is repeated `element_count` times in the structure's element list.
	 * @note The caller must ensure the type is FFI-compliant (see `ValidType::isFFICompliant`).
	 */
	ffi_type* buildFFIType(
		const valid_type::ValidType&    type,
		const valid_type::ValidTypeMap& types,
		FFITypeStorage&                 storage
	);

	/**
	 * @brief Number of libffi struct elements a structure field of the given type expands to in
	 * `buildFFIType`: a fixed-size table contributes its element type once per element
	 * (recursively), any other type contributes one element.
	 */
	usize flattenedFFIElementCount(
		const valid_type::ValidType& type, const valid_type::ValidTypeMap& types
	);
}
