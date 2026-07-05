#pragma once

#include <base/pointers/box_or_ref.hpp>
#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

#include <variant>
#include <vector>

namespace abi::type_system {

	/**
	 * @brief A C-compatible integer type, parameterised by its bit width and
	 * signedness. Width must be one of 8, 16, 32 or 64.
	 */
	struct IntType final {
		u64  width_bits;
		bool is_signed;
	};

	/**
	 * @brief A C-compatible floating-point type, parameterised by its bit width.
	 */
	struct FloatType final {
		u64 width_bits;
	};

	/**
	 * @brief The C `_Bool` type: a well-defined 1-byte ABI type. Carries no
	 * payload because its size and alignment are fixed.
	 */
	struct BoolType final {};

	/**
	 * @brief The C `char` type: a 1-byte integer. Carries no payload; its
	 * signedness is implementation-defined in C but does not affect layout.
	 */
	struct CharType final {};

	/**
	 * @brief A C-compatible pointer. Size and alignment of a pointer is fully
	 * determined by the target, so this variant intentionally carries no
	 * payload: `int*`, `void*` and `MyStruct*` all share the same layout.
	 */
	struct PointerType final {};

	struct AbiType;

	/**
	 * @brief A child AbiType node that either owns its target (a `Box`) or
	 * borrows it (a `CRef`). Borrowing lets a converted type reference a
	 * cached `QueryCAbiTypeOf` result for a sub-type without cloning it.
	 */
	using AbiTypePtr = base::BoxOrCRef<AbiType>;

	/**
	 * @brief A fixed-size C array. `count` must be strictly positive;
	 * zero-length arrays are rejected.
	 */
	struct ArrayType final {
		AbiTypePtr element;
		usize      count;
	};

	/**
	 * @brief A C-compatible struct, represented as an ordered sequence of
	 * field types. Empty structs are not legal in C and are rejected by the
	 * layout algorithm.
	 */
	struct StructType final {
		std::vector<AbiTypePtr> fields;
	};

	/**
	 * @brief Tagged union of every C-representable type the library
	 * understands.
	 */
	struct AbiType final {
		std::variant<IntType, FloatType, BoolType, CharType, PointerType, ArrayType, StructType>
			value;
	};

	/** @brief Wraps an AbiType value in an owning box. */
	AbiTypePtr makeBoxAbiType(AbiType type);

	/** @brief Builds an AbiType from an IntType. */
	AbiType intType(u64 width_bits, bool is_signed);

	/** @brief Builds an AbiType from a FloatType. */
	AbiType floatType(u64 width_bits);

	/** @brief Builds a C `_Bool` AbiType. */
	AbiType boolType();

	/** @brief Builds a C `char` AbiType. */
	AbiType charType();

	/** @brief Builds an opaque pointer AbiType. */
	AbiType pointerType();

	/**
	 * @brief Builds a fixed-size array AbiType from an element node. The element
	 * may own its target (a `Box`) or borrow it (a `CRef` into, e.g., a cached
	 * conversion) — the latter avoids cloning.
	 */
	AbiType arrayType(AbiTypePtr element, usize count);

	/** @brief Builds a struct AbiType from a list of field types. */
	AbiType structType(std::vector<AbiTypePtr> fields);

	/** @brief Deep-clones an AbiType tree into newly allocated owning nodes. */
	AbiType cloneAbiType(const AbiType& type);
}
