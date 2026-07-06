#pragma once

#include <base/pointers/box_or_ref.hpp>
#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

#include <variant>
#include <vector>

namespace abi::types {

	/**
	 * @brief A C-compatible integer type, parameterised by its bit width and
	 * signedness. Width must be one of 8, 16, 32 or 64.
	 */
	struct IntType final {
		u64  width_bits;
		bool is_signed;

		bool operator==(const IntType&) const = default;
	};

	/**
	 * @brief A C-compatible floating-point type, parameterised by its bit width.
	 */
	struct FloatType final {
		u64 width_bits;

		bool operator==(const FloatType&) const = default;
	};

	/**
	 * @brief The C `_Bool` type: a well-defined 1-byte ABI type. Carries no
	 * payload because its size and alignment are fixed.
	 */
	struct BoolType final {
		bool operator==(const BoolType&) const = default;
	};

	/**
	 * @brief The C `char` type: a 1-byte integer. Carries no payload; its
	 * signedness is implementation-defined in C but does not affect layout.
	 */
	struct CharType final {
		bool operator==(const CharType&) const = default;
	};

	/**
	 * @brief A C-compatible pointer. Size and alignment of a pointer is fully
	 * determined by the target, so this variant intentionally carries no
	 * payload: `int*`, `void*` and `MyStruct*` all share the same layout.
	 */
	struct PointerType final {
		bool operator==(const PointerType&) const = default;
	};

	struct AbiType;

	/**
	 * @brief A child AbiType node that either owns its target (a `Box`) or
	 * borrows it (a `CRef`). Borrowing lets a converted type reference a
	 * cached `QueryCAbiTypeOf` result for a sub-type without cloning it.
	 */
	using AbiTypePtr = base::BoxOrCRef<AbiType>;
	using AbiTypeCRef = base::CRef<AbiType>;

	/**
	 * @brief A fixed-size C array. `count` must be strictly positive;
	 * zero-length arrays are rejected.
	 */
	struct ArrayType final {
		AbiTypePtr element;
		usize      count;

		ArrayType(AbiTypePtr element, usize count);

		// `element` is an owning Box, so a copy must deep-clone it (a shallow
		// copy is deleted by BoxOrCRef). Moves stay cheap.
		ArrayType(const ArrayType& other);
		ArrayType(ArrayType&&) noexcept            = default;
		ArrayType& operator=(const ArrayType& other);
		ArrayType& operator=(ArrayType&&) noexcept = default;
		~ArrayType()                               = default;

		bool operator==(const ArrayType& other) const;
	};

	/**
	 * @brief A C-compatible struct, represented as an ordered sequence of
	 * field types. Empty structs are not legal in C and are rejected by the
	 * layout algorithm.
	 */
	struct StructType final {
		std::vector<AbiTypePtr> fields;

		explicit StructType(std::vector<AbiTypePtr> fields);

		// Fields are owning Boxes, so a copy must deep-clone each one (a shallow
		// copy is deleted by BoxOrCRef). Moves stay cheap.
		StructType(const StructType& other);
		StructType(StructType&&) noexcept            = default;
		StructType& operator=(const StructType& other);
		StructType& operator=(StructType&&) noexcept = default;
		~StructType()                                = default;

		bool operator==(const StructType& other) const;
	};

	/**
	 * @brief Tagged union of every C-representable type the library
	 * understands.
	 */
	struct AbiType final {
		std::variant<IntType, FloatType, BoolType, CharType, PointerType, ArrayType, StructType>
			value;

		bool operator==(const AbiType&) const = default;
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
