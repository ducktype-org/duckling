/**
 * @file type.hpp
 *
 * @brief Minimal type system used by the ABI library to describe C-compatible
 * types: integers, pointers, fixed-size arrays and structures.
 *
 * The library only accepts types that are representable in C. Higher-level
 * notions like references, owning pointers, sum types or dynamic arrays do
 * not exist here; it is the caller's responsibility (compiler / VM) to
 * translate its own type representation into `AbiType` and to report a
 * diagnostic for any type that cannot be translated.
 *
 * Zero-sized types are intentionally not representable here: C has no zero
 * sized struct members, and the layout algorithm panics if it ever encounters
 * one. Callers must filter such fields out before calling into the library.
 */
#pragma once

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <string>
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
	 * @brief A C-compatible pointer. Size and alignment of a pointer is fully
	 * determined by the target, so this variant intentionally carries no
	 * payload: `int*`, `void*` and `MyStruct*` all share the same layout.
	 */
	struct PointerType final {};

	struct AbiType;

	/**
	 * @brief A fixed-size C array. `count` must be strictly positive;
	 * zero-length arrays are rejected.
	 */
	struct ArrayType final {
		base::Box<AbiType> element;
		usize              count;
	};

	struct StructType;

	/**
	 * @brief One field inside a struct. The name is optional and is only used
	 * for diagnostics / pretty-printing in tests; the layout algorithm does
	 * not depend on it.
	 */
	struct Field final {
		base::Optional<std::string> name;
		base::Box<AbiType>          type;
	};

	/**
	 * @brief A C-compatible struct, represented as an ordered sequence of
	 * fields. Empty structs are not legal in C and are rejected by the layout
	 * algorithm.
	 */
	struct StructType final {
		std::vector<Field> fields;
	};

	/**
	 * @brief Tagged union of every C-representable type the library
	 * understands.
	 */
	struct AbiType final {
		std::variant<IntType, PointerType, ArrayType, StructType> value;
	};

	// The helpers below construct Boxes through aggregate initialisation; the
	// clang static analyser cannot prove that ownership is transferred into the
	// returned aggregate and reports a false positive NewDeleteLeaks. Box owns
	// the allocation and frees it in its destructor.
	// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)

	/** @brief Convenience helper: wraps an `AbiType` value in a fresh Box. */
	inline base::Box<AbiType> makeAbiType(AbiType type) {
		return base::makeBox<AbiType>(std::move(type));
	}

	/** @brief Convenience helper: builds an `AbiType` from an `IntType`. */
	inline AbiType intType(u8 width_bits, bool is_signed) {
		return AbiType{ IntType{ .width_bits = width_bits, .is_signed = is_signed } };
	}

	/** @brief Convenience helper: builds an opaque pointer `AbiType`. */
	inline AbiType pointerType() { return AbiType{ PointerType{} }; }

	/** @brief Convenience helper: builds a fixed-size array `AbiType`. */
	inline AbiType arrayType(AbiType element, usize count) {
		return AbiType{ ArrayType{ .element = makeAbiType(std::move(element)), .count = count } };
	}

	/** @brief Convenience helper: builds a struct `AbiType` from a list of fields. */
	inline AbiType structType(std::vector<Field> fields) {
		return AbiType{ StructType{ .fields = std::move(fields) } };
	}

	/** @brief Convenience helper: builds a `Field` with an optional name. */
	inline Field field(base::Optional<std::string> name, AbiType type) {
		return Field{ .name = std::move(name), .type = makeAbiType(std::move(type)) };
	}

	// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

}
