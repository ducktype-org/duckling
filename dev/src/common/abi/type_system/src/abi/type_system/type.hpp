#pragma once

#include <base/pointers/box.hpp>
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
		u8   width_bits;
		bool is_signed;
	};

	/**
	 * @brief A C-compatible floating-point type, parameterised by its bit width.
	 */
	struct FloatType final {
		u8 width_bits;
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
	using AbiTypePtr = base::Box<AbiType>;

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
	 * @brief A pre-computed blob with known size and alignment. Represents a
	 * nested type whose internal layout has already been determined.
	 */
	struct OpaqueType final {
		Bytes size;
		Bytes alignment;
	};

	/**
	 * @brief Tagged union of every C-representable type the library
	 * understands.
	 */
	struct AbiType final {
		std::variant<IntType, FloatType, BoolType, CharType, PointerType, ArrayType, StructType, OpaqueType>
			value;
	};

	/** @brief Wraps an AbiType value in an owning box. */
	AbiTypePtr makeAbiType(AbiType type);

	/** @brief Builds an AbiType from an IntType. */
	AbiType intType(u8 width_bits, bool is_signed);

	/** @brief Builds an AbiType from a FloatType. */
	AbiType floatType(u8 width_bits);

	/** @brief Builds a C `_Bool` AbiType. */
	AbiType boolType();

	/** @brief Builds a C `char` AbiType. */
	AbiType charType();

	/** @brief Builds an opaque pointer AbiType. */
	AbiType pointerType();

	/** @brief Builds a fixed-size array AbiType. */
	AbiType arrayType(AbiType element, usize count);

	/** @brief Builds a struct AbiType from a list of field types. */
	AbiType structType(std::vector<AbiTypePtr> fields);

	/** @brief Builds an opaque blob AbiType with known size and alignment. */
	AbiType opaqueType(Bytes size, Bytes alignment);

	/** @brief Deep-clones an AbiType tree into newly allocated owning nodes. */
	AbiType cloneAbiType(const AbiType& type);
}
