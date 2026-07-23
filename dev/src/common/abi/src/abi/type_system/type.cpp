#include <abi/type_system/type.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace abi::types {

	// The static analyzer cannot model `base::Box` ownership when a Box is nested
	// inside a returned value tree, so it reports false-positive leaks for the
	// allocating helpers below.
	// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)

	AbiTypePtr makeBoxAbiType(AbiType type) { return base::makeBox<AbiType>(std::move(type)); }

	ArrayType::ArrayType(AbiTypePtr element, usize count):
		  element(std::move(element)),
		  count(count) {}

	ArrayType::ArrayType(const ArrayType& other):
		  element(makeBoxAbiType(cloneAbiType(*other.element))),
		  count(other.count) {}

	ArrayType& ArrayType::operator=(const ArrayType& other) {
		if (this != &other) {
			element = makeBoxAbiType(cloneAbiType(*other.element));
			count   = other.count;
		}
		return *this;
	}

	bool ArrayType::operator==(const ArrayType& other) const {
		return count == other.count && *element == *other.element;
	}

	namespace {
		std::vector<AbiTypePtr> cloneFields(const std::vector<AbiTypePtr>& fields) {
			std::vector<AbiTypePtr> cloned;
			cloned.reserve(fields.size());
			for (const auto& field: fields) cloned.push_back(makeBoxAbiType(cloneAbiType(*field)));
			return cloned;
		}
	}

	StructType::StructType(std::vector<AbiTypePtr> fields): fields(std::move(fields)) {}

	StructType::StructType(const StructType& other): fields(cloneFields(other.fields)) {}

	StructType& StructType::operator=(const StructType& other) {
		if (this != &other) fields = cloneFields(other.fields);
		return *this;
	}

	bool StructType::operator==(const StructType& other) const {
		return std::ranges::equal(
			fields, other.fields, [](const AbiTypePtr& a, const AbiTypePtr& b) { return *a == *b; }
		);
	}

	AbiType intType(u64 width_bits, bool is_signed) {
		return AbiType{ IntType{ .width_bits = width_bits, .is_signed = is_signed } };
	}

	AbiType floatType(u64 width_bits) { return AbiType{ FloatType{ .width_bits = width_bits } }; }

	AbiType boolType() { return AbiType{ BoolType{} }; }

	AbiType charType() { return AbiType{ CharType{} }; }

	AbiType pointerType() { return AbiType{ PointerType{} }; }

	AbiType arrayType(AbiTypePtr element, usize count) {
		return AbiType{ ArrayType{ std::move(element), count } };
	}

	AbiType structType(std::vector<AbiTypePtr> fields) {
		return AbiType{ StructType{ std::move(fields) } };
	}

	AbiType cloneAbiType(const AbiType& type) {
		variant_match(type.value) {
			variant_case(ArrayType, array) {
				return arrayType(makeBoxAbiType(cloneAbiType(*array.element)), array.count);
			}
			variant_case(StructType, strct) {
				std::vector<AbiTypePtr> cloned_fields;
				cloned_fields.reserve(strct.fields.size());
				for (const auto& field: strct.fields)
					cloned_fields.push_back(makeBoxAbiType(cloneAbiType(*field)));
				return structType(std::move(cloned_fields));
			}
			variant_case(IntType, value) { return AbiType{ value }; }
			variant_case(FloatType, value) { return AbiType{ value }; }
			variant_case(BoolType, value) { return AbiType{ value }; }
			variant_case(CharType, value) { return AbiType{ value }; }
			variant_case(PointerType, value) { return AbiType{ value }; }
		}
		CORE_UNREACHABLE();
	}

	// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}
