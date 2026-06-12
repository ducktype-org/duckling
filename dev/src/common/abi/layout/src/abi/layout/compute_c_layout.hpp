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
