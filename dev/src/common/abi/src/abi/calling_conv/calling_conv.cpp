#include "calling_conv.hpp"

#include "abi/type_system/type.hpp"

#include "base/extend_cpp/variant_match.hpp"

namespace abi::calling_conv {

	std::vector<type_system::AbiType> flattenType(
		const TargetABI& target, type_system::AbiTypePtr type
	) {
		std::vector<type_system::AbiType> result;
		auto flatten_rec = [&](auto&& self, const type_system::AbiTypePtr& type) -> void {
			variant_match(type->value) {
				variant_case(type_system::IntType, v) { result.push_back({ v }); }
				variant_case(type_system::FloatType, v) { result.push_back({ v }); }
				variant_case(type_system::BoolType, v) {
					result.push_back(type_system::intType(8, false));
				}
				variant_case(type_system::CharType, v) {
					result.push_back(type_system::intType(8, false));
				}
				variant_case(type_system::PointerType, v) {
					result.push_back(
						type_system::intType(target.data_layout.pointer_size.asInt() * 8, false)
					);
				}
				variant_case(type_system::StructType, v) {
					for (auto& field: v.fields) self(self, field);
				}
				variant_case(type_system::ArrayType, v) {
					for (int i{ 0 }; i < v.count; i++) self(self, v.element);
				}
			}
		};
		flatten_rec(flatten_rec, type);
		return result;
	}


    /**
     * @brief For x86_64 target combine into the structure with two fields. 
     */
    type_system::AbiType combineToLowHigh(std::vector<type_system::AbiType> flattened) {

    }

	FunctionInfo X86_64ABIInfo::computeInfo(const FunctionType& ft) const {}

	FunctionInfo AArch64ABIInfo::computeInfo(const FunctionType& ft) const {}
}
