#include "llvm_lowering_context.hpp"

#include "context.hpp"
#include "psackling/type.hpp"

#include "base/except/exceptions.hpp"
#include "base/extend_cpp/variant_match.hpp"
#include "base/types/bits_and_bytes.hpp"

#include "llvm/IR/LLVMContext.h"

llvm::Type* pg::LlvmLoweringContext::lowerType(tp::TypeID type_id, Context& ctx) {
	if (types.contains(type_id)) return types[type_id];

	tp::TypeInfo& type_info = ctx.getTypeInfo(type_id);
	CORE_ASSERT(!std::holds_alternative<tp::Incomplete>(type_info.kind), "Expected a complete type");

	variant_match(type_info.kind) {
		variant_case(tp::Integral, integral) {
			llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
				auto new_type = llvm::Type::getIntNTy(
					llvm_ctx, static_cast<u32>(base::bytes2bits(integral.size).asInt())
				);
				types[type_id] = new_type;
			});
			return types[type_id];
		}
		variant_case(tp::Boolean, boolean) {
			llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
				auto new_type  = llvm::Type::getInt1Ty(llvm_ctx);
				types[type_id] = new_type;
			});
			return types[type_id];
		}
		variant_case(tp::Void, void_type) {
			llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
				auto new_type  = llvm::Type::getVoidTy(llvm_ctx);
				types[type_id] = new_type;
			});
			return types[type_id];
		}
		variant_case(tp::Namespace, namespace_type) {
			CORE_PANIC("Cannot lower namespace type to LLVM type");
		}
		variant_case(tp::Pointer, pointer) {
			llvm::Type* pointed_to_type = lowerType(pointer.points_to, ctx);
			llvm_module.inCtxDo([&](llvm::LLVMContext&, llvm::Module&) {
				auto new_type  = llvm::PointerType::getUnqual(pointed_to_type);
				types[type_id] = new_type;
			});
			return types[type_id];
		}
		variant_case(tp::Structure, structure) {
            CORE_PANIC("Struct lowering not implemented yet");
		}
		variant_case(tp::Function, function) {
			std::vector<llvm::Type*> param_types;
			param_types.reserve(function.param_types.size());
			for (tp::TypeID param_type_id: function.param_types)
				param_types.push_back(lowerType(param_type_id, ctx));
			llvm::Type* return_type = lowerType(function.return_type, ctx);
			llvm_module.inCtxDo([&](llvm::LLVMContext&, llvm::Module&) {
				auto new_type  = llvm::FunctionType::get(return_type, param_types, false);
				types[type_id] = new_type;
			});
			return types[type_id];
		}
        variant_case(tp::Incomplete, incomplete) {
            CORE_PANIC("Cannot lower incomplete type to LLVM type");
        }
	}

	CORE_UNREACHABLE();
}
