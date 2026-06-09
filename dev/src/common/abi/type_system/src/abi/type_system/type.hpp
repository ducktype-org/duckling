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
 * Recursive nodes (array element, struct field) are owned through `base::Box`,
 * so the tree is value-owned with no sharing; copying an `AbiType` therefore
 * means a deep copy via `cloneAbiType`.
 *
 * Zero-sized types are intentionally not representable here: C has no zero
 * sized struct members, and the layout algorithm panics if it ever encounters
 * one. Callers must filter such fields out before calling into the library.
 */
#pragma once

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>
#include <base/types/bits_and_bytes.hpp>
#include <base/types/ints.hpp>

#include <string>
#include <type_traits>
#include <utility>
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
	 * @brief One field inside a struct. The name is optional and is only used
	 * for diagnostics; the layout algorithm does not depend on it.
	 */
	struct Field final {
		base::Optional<std::string> name;
		AbiTypePtr                  type;
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
		std::variant<IntType, PointerType, ArrayType, StructType, OpaqueType> value;
	};

	// The static analyzer cannot model `base::Box` ownership when a Box is nested
	// inside a returned value tree, so it reports false-positive leaks for the
	// allocating helpers below.
	// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)

	/** @brief Wraps an AbiType value in an owning box. */
	inline AbiTypePtr makeAbiType(AbiType type) { return base::makeBox<AbiType>(std::move(type)); }

	/** @brief Builds an AbiType from an IntType. */
	inline AbiType intType(u8 width_bits, bool is_signed) {
		return AbiType{ IntType{ .width_bits = width_bits, .is_signed = is_signed } };
	}

	/** @brief Builds an opaque pointer AbiType. */
	inline AbiType pointerType() { return AbiType{ PointerType{} }; }

	/** @brief Builds a fixed-size array AbiType. */
	inline AbiType arrayType(AbiType element, usize count) {
		return AbiType{ ArrayType{ .element = makeAbiType(std::move(element)), .count = count } };
	}

	/** @brief Builds a struct AbiType from a list of fields. */
	inline AbiType structType(std::vector<Field> fields) {
		return AbiType{ StructType{ .fields = std::move(fields) } };
	}

	/** @brief Builds an opaque blob AbiType with known size and alignment. */
	inline AbiType opaqueType(Bytes size, Bytes alignment) {
		return AbiType{ OpaqueType{ .size = size, .alignment = alignment } };
	}

	/** @brief Builds a Field with an optional name. */
	inline Field field(base::Optional<std::string> name, AbiType type) {
		return Field{ .name = std::move(name), .type = makeAbiType(std::move(type)) };
	}

	/** @brief Deep-clones an AbiType tree into newly allocated owning nodes. */
	inline AbiType cloneAbiType(const AbiType& type) {
		return std::visit(
			[&](const auto& value) -> AbiType {
				using T = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<T, IntType> || std::is_same_v<T, PointerType>
			                  || std::is_same_v<T, OpaqueType>) {
					return AbiType{ value };
				} else if constexpr (std::is_same_v<T, ArrayType>) {
					return arrayType(cloneAbiType(*value.element), value.count);
				} else if constexpr (std::is_same_v<T, StructType>) {
					std::vector<Field> cloned_fields;
					cloned_fields.reserve(value.fields.size());
					for (const auto& f: value.fields) {
						cloned_fields.push_back(Field{
							.name = f.name, .type = makeAbiType(cloneAbiType(*f.type)) });
					}
					return structType(std::move(cloned_fields));
				}
				CORE_UNREACHABLE();
			},
			type.value
		);
	}

	// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
