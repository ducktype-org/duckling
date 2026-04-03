/**
 * @file comptime_type_operations.cpp
 * @brief Implementation of Native/External functions for Compile-Time-Evaluations of meta types.
 *
 * This file defines the interface that allows the DVM to interact directly with the compiler.
 * Performing calls to the type system etc.
 *
 * Some important notes:
 * 1. Type Construction: Provides primitives to create modified types (Box, Ref, Const)
 *    and composite types (Tuples, Variants, Functions) during CTE.
 * 2. Builder Patterns: Due to the VM's limitations or supporting variadic type arguments, composite
 * 	  types are constructed using stateful "Builder" objects via a sequence of calls (New -> Push ->
 *    ... -> Finalize).
 * 3. Opaque Pointers: Manages the passing of raw C++ pointers (`tsh::SymbolType*`, builders)
 *    through the VM as `opaque_ptr` types.
 */
#include "comptime_type_operations.hpp"

#include "meta_type_memory_manager.hpp"

#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <query_framework/context/context.hpp>

#include <vm/bytecode/extern_c_function.hpp>
#include <vm/bytecode/instructions.hpp>

namespace compiler::helios::comptime_ops {
	namespace {
		/**
		 * @brief Builders for constructing types with a variadic number of subtypes.
		 * Since DVM doesn't support functions taking a variadic number of arguments, complex types
		 * (with an arbitrary number of arguments) are built with help of builders.
		 * They are used as follows:
		 *	```
		 *	builder_ptr = comptime_X_builder_new()
		 *	comptime_X_builder_push(builder_ptr, type_ptr)
		 *	(...)
		 *	comptime_X_builder_push(builder_ptr, type_ptr)
		 *	new_type_ptr = comptime_X_builder_finalize(builder_ptr)
		 *	```
		 */
		struct VariantTypeBuilder {
			std::vector<tsh::SymbolType<>> subtypes;

			tsh::SymbolType<> finalize(query::Context& ctx) {
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryVariantType>({ subtypes }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
		};

		struct TupleTypeBuilder {
			std::vector<tsh::SymbolType<>> subtypes;

			tsh::SymbolType<> finalize(query::Context& ctx) {
				return tsh::SymbolType<>{
					ctx.query<tsh::QueryTupleType>({ subtypes }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
		};
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		comptime_create_box,
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		auto new_type = type_ptr->withReferenceKind(tsh::ReferenceKind::Box);
		return MetaTypeMemoryManager::instance().allocateType(new_type);
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		comptime_create_ref,
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		auto new_type = type_ptr->withReferenceKind(tsh::ReferenceKind::Ref);
		return MetaTypeMemoryManager::instance().allocateType(new_type);
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		comptime_create_const,
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		auto new_type = type_ptr->withMutability(tsh::Mutability::Immutable);
		return MetaTypeMemoryManager::instance().allocateType(new_type);
	}

	DEF_VM_EXT_C_FUNC(TupleTypeBuilder*, "opaque_ptr", comptime_tuple_builder_new) {
		auto* builder = new TupleTypeBuilder();
		return builder;
	}

	DEF_VM_EXT_C_FUNC(
		void,
		"void",
		comptime_tuple_builder_push,
		(TupleTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		builder_ptr->subtypes.push_back(*type_ptr);
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		comptime_tuple_builder_finalize,
		(query::Context*, "opaque_ptr", ctx_ptr),
		(TupleTypeBuilder*, "opaque_ptr", builder_ptr)
	) {
		auto tuple_type = builder_ptr->finalize(*ctx_ptr);
		delete builder_ptr;
		return MetaTypeMemoryManager::instance().allocateType(tuple_type);
	}

	DEF_VM_EXT_C_FUNC(VariantTypeBuilder*, "opaque_ptr", comptime_variant_builder_new) {
		auto* builder = new VariantTypeBuilder();
		return builder;
	}

	DEF_VM_EXT_C_FUNC(
		void,
		"void",
		comptime_variant_builder_push,
		(VariantTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		builder_ptr->subtypes.push_back(*type_ptr);
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		comptime_variant_builder_finalize,
		(query::Context*, "opaque_ptr", ctx_ptr),
		(VariantTypeBuilder*, "opaque_ptr", builder_ptr)
	) {
		auto variant_type = builder_ptr->finalize(*ctx_ptr);
		delete builder_ptr;
		return MetaTypeMemoryManager::instance().allocateType(variant_type);
	}

	DEF_VM_EXT_C_FUNC(
		bool,
		"i8",
		comptime_types_equal,
		(tsh::SymbolType<>*, "opaque_ptr", left),
		(tsh::SymbolType<>*, "opaque_ptr", right)
	) {
		return *left == *right;
	}

	DEF_VM_EXT_C_FUNC(
		bool,
		"byte",
		comptime_types_not_equal,
		(tsh::SymbolType<>*, "opaque_ptr", left),
		(tsh::SymbolType<>*, "opaque_ptr", right)
	) {
		return *left != *right;
	}

	std::vector<vm::code::ExternalCFunction> getComptimeTypeExternOperations(vm::PID pid) {
		return {
			VM_INSTANCE_EXT_C_FUNC(comptime_create_box, comptime_create_box, pid),
			VM_INSTANCE_EXT_C_FUNC(comptime_create_ref, comptime_create_ref, pid),
			VM_INSTANCE_EXT_C_FUNC(comptime_create_const, comptime_create_const, pid),
			VM_INSTANCE_EXT_C_FUNC(comptime_tuple_builder_new, comptime_tuple_builder_new, pid),
			VM_INSTANCE_EXT_C_FUNC(comptime_tuple_builder_push, comptime_tuple_builder_push, pid),
			VM_INSTANCE_EXT_C_FUNC(
				comptime_tuple_builder_finalize, comptime_tuple_builder_finalize, pid
			),
			VM_INSTANCE_EXT_C_FUNC(comptime_variant_builder_new, comptime_variant_builder_new, pid),
			VM_INSTANCE_EXT_C_FUNC(
				comptime_variant_builder_push, comptime_variant_builder_push, pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				comptime_variant_builder_finalize, comptime_variant_builder_finalize, pid
			),
			VM_INSTANCE_EXT_C_FUNC(comptime_types_equal, comptime_types_equal, pid),
			VM_INSTANCE_EXT_C_FUNC(comptime_types_not_equal, comptime_types_not_equal, pid),
		};
	}

	vm::code::CodeCollection getComptimeTypeOperations(vm::PID pid) {
		// A global storing an opaque pointer to `query::Context` needed for performing type
		// system calls during DVM evaluation.
		vm::code::GlobalData context_global;
		context_global.name = base::StrID("comptime_query_ctx"),
		context_global.type = base::StrID("opaque_ptr");

		// A function used for initializing the global context pointer. Called by the comptime
		// VM instance, before comptime operations. Takes in an opaque pointer storing the
		// `query::Context*` and sets the value of the global context pointer. The function looks as
		// follows:
		//
		// function comptime_set_ctx { opaque_ptr } -> void {
		// 		mov_gopq_lopq comptime_query_ctx, arg0;
		// 		ret;
		// }
		vm::code::Instruction mov_gopq_lopq = vm::code::instructions::Op_mov_gopq_lopq(
			vm::opargs::GlobalOpq(context_global.name),
			vm::opargs::StackLocalOpq(base::StrID("arg0"))
		);
		vm::code::Instruction ret = vm::code::instructions::Op_ret{};

		vm::code::Function init_global_context;
		init_global_context.name = base::StrID("comptime_set_ctx");
		init_global_context.body = { mov_gopq_lopq, ret };
		init_global_context.signature
			= { .result_types = {}, .parameters = { base::StrID("opaque_ptr") } };

		return {
			.functions            = { init_global_context },
			.types                = {},
			.global_data          = { context_global },
			.external_c_functions = getComptimeTypeExternOperations(pid),
		};
	}
}
