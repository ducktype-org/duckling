#include <abi/type_system/type.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <utility>
#include <vector>

namespace abi::type_system {

	// The static analyzer cannot model `base::Box` ownership when a Box is nested
	// inside a returned value tree, so it reports false-positive leaks for the
	// allocating helpers below.
	// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)

	AbiTypePtr makeAbiType(AbiType type) { return base::makeBox<AbiType>(std::move(type)); }

	AbiType intType(u8 width_bits, bool is_signed) {
		return AbiType{ IntType{ .width_bits = width_bits, .is_signed = is_signed } };
	}

	AbiType floatType(u8 width_bits) { return AbiType{ FloatType{ .width_bits = width_bits } }; }

	AbiType boolType() { return AbiType{ BoolType{} }; }

	AbiType charType() { return AbiType{ CharType{} }; }

	AbiType pointerType() { return AbiType{ PointerType{} }; }

	AbiType arrayType(AbiType element, usize count) {
		return AbiType{ ArrayType{ .element = makeAbiType(std::move(element)), .count = count } };
	}

	AbiType structType(std::vector<AbiTypePtr> fields) {
		return AbiType{ StructType{ .fields = std::move(fields) } };
	}

	AbiType opaqueType(Bytes size, Bytes alignment) {
		return AbiType{ OpaqueType{ .size = size, .alignment = alignment } };
	}

	AbiType cloneAbiType(const AbiType& type) {
		variant_match(type.value) {
			variant_case(ArrayType, array) {
				return arrayType(cloneAbiType(*array.element), array.count);
			}
			variant_case(StructType, strct) {
				std::vector<AbiTypePtr> cloned_fields;
				cloned_fields.reserve(strct.fields.size());
				for (const auto& field: strct.fields)
					cloned_fields.push_back(makeAbiType(cloneAbiType(*field)));
				return structType(std::move(cloned_fields));
			}
			variant_case(IntType, value) { return AbiType{ value }; }
			variant_case(FloatType, value) { return AbiType{ value }; }
			variant_case(BoolType, value) { return AbiType{ value }; }
			variant_case(CharType, value) { return AbiType{ value }; }
			variant_case(PointerType, value) { return AbiType{ value }; }
			variant_case(OpaqueType, value) { return AbiType{ value }; }
		}
		CORE_UNREACHABLE();
	}

	// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
