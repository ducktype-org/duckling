#include "llvm_lowering_context.hpp"

#include "context.hpp"
#include "psackling/symbol.hpp"
#include "psackling/type.hpp"

#include "base/except/exceptions.hpp"
#include "base/extend_cpp/variant_match.hpp"
#include "base/types/bits_and_bytes.hpp"

#include "llvm/IR/LLVMContext.h"

llvm::Type* pg::LlvmLoweringContext::lowerType(tp::TypeID type_id, Context& ctx) {
	if (types.contains(type_id)) return types[type_id];

	tp::TypeInfo& type_info = ctx.getTypeInfo(type_id);
	CORE_ASSERT(
		!std::holds_alternative<tp::Incomplete>(type_info.kind),
		"Expected a complete type, got an incomplete one: " + type_info.name.str()
	);

	variant_match(type_info.kind) {
		variant_case(tp::Integral, integral) {
			llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
				auto new_type = llvm::Type::getIntNTy(
					llvm_ctx, static_cast<u32>(base::bytes2bits(integral.size).asInt())
				);
				types.insertOrAssign(type_id, new_type);
			});
			return types[type_id];
		}
		variant_case(tp::Boolean, boolean) {
			llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
				auto new_type = llvm::Type::getInt1Ty(llvm_ctx);
				types.insertOrAssign(type_id, new_type);
			});
			return types[type_id];
		}
		variant_case(tp::Void, void_type) {
			llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
				auto new_type = llvm::Type::getVoidTy(llvm_ctx);
				types.insertOrAssign(type_id, new_type);
			});
			return types[type_id];
		}
		variant_case(tp::Namespace, namespace_type) {
			CORE_PANIC("Cannot lower namespace type to LLVM type");
		}
		variant_case(tp::Pointer, pointer) {
			llvm::Type* pointed_to_type = lowerType(pointer.points_to, ctx);
			llvm_module.inCtxDo([&](llvm::LLVMContext&, llvm::Module&) {
				auto new_type = llvm::PointerType::getUnqual(pointed_to_type);
				types.insertOrAssign(type_id, new_type);
			});
			return types[type_id];
		}
		variant_case(tp::Structure, structure) {
			llvm::StructType* struct_type = nullptr;
			llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
				struct_type = llvm::StructType::create(llvm_ctx, type_info.name.strView());
				types.insertOrAssign(type_id, struct_type);
			});

			std::vector<llvm::Type*> field_llvm_types;
			field_llvm_types.reserve(structure.field_symbols.size());
			for (sym::SymbolID field_sym_id: structure.field_symbols) {
				const sym::Symbol& field_sym = ctx.getSymbol(field_sym_id);
				field_llvm_types.push_back(lowerType(field_sym.type_id, ctx));
			}

			struct_type->setBody(field_llvm_types, /*isPacked=*/false);
			return struct_type;
		}
		variant_case(tp::Function, function) {
			std::vector<llvm::Type*> param_types;
			param_types.reserve(function.param_types.size());
			for (tp::TypeID param_type_id: function.param_types)
				param_types.push_back(lowerType(param_type_id, ctx));
			llvm::Type* return_type = lowerType(function.return_type, ctx);
			llvm_module.inCtxDo([&](llvm::LLVMContext&, llvm::Module&) {
				auto new_type = llvm::FunctionType::get(return_type, param_types, false);
				types.insertOrAssign(type_id, new_type);
			});
			return types[type_id];
		}
		variant_case(tp::Incomplete, incomplete) {
			CORE_PANIC("Cannot lower incomplete type to LLVM type");
		}
	}

	CORE_UNREACHABLE();
}

llvm::Function* pg::LlvmLoweringContext::getOrCreateFunction(sym::SymbolID fun_sym_id, Context& ctx) {
	if (functions.contains(fun_sym_id)) return functions[fun_sym_id];

	sym::Symbol& fun_sym = ctx.getSymbol(fun_sym_id);
	CORE_ASSERT(
		std::holds_alternative<sym::FunctionSymbol>(fun_sym.kind),
		"getOrCreateFunction expects a FunctionSymbol: " + fun_sym.name.str()
	);
	const auto& fn_kind = std::get<sym::FunctionSymbol>(fun_sym.kind);

	auto* fn_ty = llvm::cast<llvm::FunctionType>(lowerType(fun_sym.type_id, ctx));
	llvm_module.inCtxDo([&](llvm::LLVMContext&, llvm::Module& mod) {
		functions.insertOrAssign(
			fun_sym_id,
			llvm::Function::Create(
				fn_ty, llvm::Function::ExternalLinkage, fn_kind.link_name.strView(), &mod
			)
		);
	});
	return functions[fun_sym_id];
}
