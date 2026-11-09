// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <ffi.h>

#include <base/pointers/box.hpp>

#include <vm/bytecode/validator/valid_type/valid_type.hpp>

namespace vm::code::ffi_detail {
	/// Flattening materializes one `ffi_type*` per element, so the flattened element count of a
	/// struct must stay bounded; over-cap types must be rejected before calling `buildFFIType`.
	inline constexpr usize MAX_FLATTENED_FFI_ELEMENTS = 65'536;

	/**
	 * @brief Owning storage for libffi struct type descriptors. Must outlive any `ffi_cif`
	 * prepared with types built by `buildFFIType`.
	 */
	struct FFITypeStorage final {
		std::vector<Box<ffi_type>>               struct_types;
		std::vector<Box<std::vector<ffi_type*>>> struct_elements;
	};

	/**
	 * @brief Returns the libffi type describing the given VM type.
	 * Supports primitives of size 1, 2, 4 or 8 (mapped to signed integers), C pointers
	 * (mapped to `void*`) and data structures of the above, including nested structures and
	 * fixed-size table fields.
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
	 * (recursively), any other type contributes one element. Saturates at `usize` max instead of
	 * overflowing.
	 * @note A nested structure counts as one element here (it is a single `ffi_type*` in the
	 * enclosing descriptor); use `totalFFIDescriptorElementCount` to bound descriptor memory.
	 */
	usize flattenedFFIElementCount(
		const valid_type::ValidType& type, const valid_type::ValidTypeMap& types
	);

	/**
	 * @brief Total number of descriptor elements `buildFFIType` materializes for the given type,
	 * including the elements of every nested structure descriptor. This is the count
	 * `MAX_FLATTENED_FFI_ELEMENTS` bounds; unlike `flattenedFFIElementCount` it cannot be dodged
	 * by hiding large tables behind nested structure fields. Saturates at `usize` max; a table of
	 * structures overcounts (the shared element descriptor is multiplied), erring toward
	 * rejection.
	 */
	usize totalFFIDescriptorElementCount(
		const valid_type::ValidType& type, const valid_type::ValidTypeMap& types
	);
}
