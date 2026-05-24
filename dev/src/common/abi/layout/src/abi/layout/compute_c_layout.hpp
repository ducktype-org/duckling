/**
 * @file compute_c_layout.hpp
 *
 * @brief Pure functions that compute the memory layout of a sequence of
 * C-compatible fields under a given target ABI, following standard C
 * struct packing rules.
 *
 * The library does not raise diagnostics: defensive checks (zero-sized
 * fields, zero-length arrays, empty structs) trigger `CORE_PANIC`. Callers
 * are responsible for filtering and validating inputs before calling in.
 */
#pragma once

#include <abi/layout/target.hpp>
#include <abi/type_system/type.hpp>

#include <base/types/bits_and_bytes.hpp>

#include <vector>

namespace abi::layout {

	/**
	 * @brief Result of laying out a sequence of fields: total padded size,
	 * struct-wide alignment and the byte offset of each field, in input order.
	 */
	struct ComputedLayout final {
		Bytes              size;
		Bytes              alignment;
		std::vector<Bytes> field_offsets;
	};

	/**
	 * @brief Size and alignment of a single `AbiType` under the given target.
	 */
	struct SizeAlign final {
		Bytes size;
		Bytes alignment;
	};

	/**
	 * @brief Computes the size and alignment of a single type. Panics on
	 * zero-sized constructs (arrays with count 0, empty structs).
	 */
	SizeAlign sizeAlignOf(const TargetABI& target, const type_system::AbiType& t);

	/**
	 * @brief Computes the C layout of a sequence of fields. Each field is
	 * placed at the next offset satisfying its alignment; the total size is
	 * padded up to the struct's overall alignment.
	 *
	 * Pre: every field must have a positive size; callers must filter out
	 * zero-sized inputs beforehand.
	 */
	ComputedLayout computeCLayout(
		const TargetABI& target, const std::vector<type_system::Field>& fields
	);

}
