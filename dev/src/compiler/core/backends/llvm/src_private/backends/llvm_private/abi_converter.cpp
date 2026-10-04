#include "abi_converter.hpp"

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/DerivedTypes.h>

LLVM_INCLUDE_END()

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>

#include <vector>

namespace compiler::backend_llvm {

	llvm::Type* abiTypeToLLVMType(llvm::LLVMContext& context, const abi::types::AbiType& type) {
		namespace at = abi::types;

		variant_match(type.value) {
			variant_case(at::IntType, int_type) {
				return llvm::Type::getIntNTy(
					context, base::safeIntConv<unsigned>(int_type.width_bits)
				);
			}
			variant_case(at::FloatType, float_type) {
				// See https://llvm.org/docs/LangRef.html#floating-point-types.
				switch (float_type.width_bits) {
				case 32:
					return llvm::Type::getFloatTy(context);
				case 64:
					return llvm::Type::getDoubleTy(context);
				default:
					CORE_PANIC("ABI float width other than 32 or 64 is not supported");
				}
			}
			// The calling-convention library lowers bool/char leaves to a single byte.
			variant_case_novalue(at::BoolType) { return llvm::Type::getInt8Ty(context); }
			variant_case_novalue(at::CharType) { return llvm::Type::getInt8Ty(context); }
			variant_case_novalue(at::PointerType) { return llvm::PointerType::getUnqual(context); }
			variant_case(at::ArrayType, array_type) {
				return llvm::ArrayType::get(
					abiTypeToLLVMType(context, *array_type.element), array_type.count
				);
			}
			variant_case(at::StructType, struct_type) {
				std::vector<llvm::Type*> field_types;
				field_types.reserve(struct_type.fields.size());
				for (const auto& field: struct_type.fields)
					field_types.push_back(abiTypeToLLVMType(context, *field));
				return llvm::StructType::get(context, field_types);
			}
		}
		CORE_UNREACHABLE();
	}
}
