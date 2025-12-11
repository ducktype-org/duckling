/**
 * @file comptime_type_operations.cpp
 * @brief Implementation of Native/External functions for Compile-Time-Evaluations of meta types.
 *
 * This file defines the interface that allows the DVM to interact directly with the compiler.
 * Performing calls to the type system etc.
 *
 * Some key concepts:
 * 1. Type Construction: Provides primitives to create modified types (Box, Ref, Const)
 *    and composite types (Tuples, Variants, Functions) during CTE.
 * 2. Builder Patterns: Due to the VM's limitations or supporting variadic type arguments, composite
 * 	  types are constructed using stateful "Builder" objects via a sequence of calls (New -> Push ->
 *    ... -> Finalize).
 * 3. Opaque Pointers: Manages the passing of raw C++ pointers (`tsh::SymbolType*`, builders)
 *    through the VM as `opaque_ptr` types.
 *
 * @note These functions are prefixed with `__comptime_` to avoid namespace collisions
 *       with user-defined functions within the VM context.
 */
#include "comptime_type_operations.hpp"

#include "typesystem/higher/mutability.hpp"
#include "typesystem/higher/symbol_type.hpp"
#include "typesystem/lower/queries.hpp"

#include "base/except/exceptions.hpp"

#include "string_id/string_id.hpp"

#include "vm/api/vm.hpp"
#include "vm/bytecode/bytecode.hpp"
#include "vm/bytecode/extern_c_function.hpp"
#include "vm/bytecode/instructions.hpp"

namespace compiler::helios::comptime_ops {
	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_create_box,  // NOLINT(readability-identifier-naming)
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		auto new_type = type_ptr->withReferenceKind(tsh::ReferenceKind::Box);
		// TODOP: Figure out the memory management.
		auto* result = new tsh::SymbolType<>(new_type);
		return result;
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_create_ref,  // NOLINT(readability-identifier-naming)

		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		auto new_type = type_ptr->withReferenceKind(tsh::ReferenceKind::Ref);
		// TODOP: Figure out the memory management.
		auto* result = new tsh::SymbolType<>(new_type);
		return result;
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_create_const,  // NOLINT(readability-identifier-naming)

		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		auto new_type = type_ptr->withMutability(tsh::Mutability::Immutable);
		// TODOP: Figure out the memory management.
		auto* result = new tsh::SymbolType<>(new_type);
		return result;
	}

	// TODOP: Optionals
	// DEF_VM_EXT_C_FUNC(
	// 	tsh::SymbolType<>*,
	// 	"opaque_ptr",
	// 	__comptime_create_optional,
	// 	(query::Context*, "opaque_ptr", ctx_ptr),
	// 	(tsh::SymbolType<>*, "opaque_ptr", type_ptr),
	// ) {}

	DEF_VM_EXT_C_FUNC(
		i64,
		"i64",
		__comptime_get_size,  // NOLINT(readability-identifier-naming)

		(query::Context*, "opaque_ptr", ctx_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		auto layout = ctx_ptr->query<tsl::QuerySymbolTypeLayout>(*type_ptr);
		return static_cast<i64>(layout->getSize());
	}

	DEF_VM_EXT_C_FUNC(
		TupleTypeBuilder*,
		"opaque_ptr",
		__comptime_tuple_builder_new  // NOLINT(readability-identifier-naming)
	) {
		auto* builder = new TupleTypeBuilder();
		return builder;
	}

	DEF_VM_EXT_C_FUNC(
		void,
		"void",
		__comptime_tuple_builder_push,  // NOLINT(readability-identifier-naming)
		(TupleTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		builder_ptr->subtypes.push_back(*type_ptr);
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_tuple_builder_finalize,  // NOLINT(readability-identifier-naming)

		(query::Context*, "opaque_ptr", ctx_ptr),
		(TupleTypeBuilder*, "opaque_ptr", builder_ptr)
	) {
		auto  tuple_type = builder_ptr->produce(*ctx_ptr);
		auto* result     = new tsh::SymbolType<>(tuple_type);
		delete builder_ptr;
		return result;
	}

	DEF_VM_EXT_C_FUNC(
		VariantTypeBuilder*,
		"opaque_ptr",
		__comptime_variant_builder_new  // NOLINT(readability-identifier-naming)

	) {
		auto* builder = new VariantTypeBuilder();
		return builder;
	}

	DEF_VM_EXT_C_FUNC(
		void,
		"void",
		__comptime_variant_builder_push,  // NOLINT(readability-identifier-naming)

		(VariantTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		builder_ptr->subtypes.push_back(*type_ptr);
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_variant_builder_finalize,  // NOLINT(readability-identifier-naming)

		(query::Context*, "opaque_ptr", ctx_ptr),
		(VariantTypeBuilder*, "opaque_ptr", builder_ptr)
	) {
		auto  tuple_type = builder_ptr->produce(*ctx_ptr);
		auto* result     = new tsh::SymbolType<>(tuple_type);
		delete builder_ptr;
		return result;
	}

	DEF_VM_EXT_C_FUNC(
		FunctionTypeBuilder*,
		"opaque_ptr",
		__comptime_func_type_builder_new  // NOLINT(readability-identifier-naming)

	) {
		auto* builder = new FunctionTypeBuilder();
		return builder;
	}

	DEF_VM_EXT_C_FUNC(
		void,
		"void",
		__comptime_func_type_builder_set_ret_type,  // NOLINT(readability-identifier-naming)

		(FunctionTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		CORE_ASSERT(!builder_ptr->return_type, "Return type set twice");
		builder_ptr->return_type = *type_ptr;
	}

	DEF_VM_EXT_C_FUNC(
		void,
		"void",
		__comptime_func_type_builder_push_arg,  // NOLINT(readability-identifier-naming)

		(FunctionTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		builder_ptr->arg_types.push_back(*type_ptr);
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_func_type_builder_finalize,  // NOLINT(readability-identifier-naming)

		(query::Context*, "opaque_ptr", ctx_ptr),
		(FunctionTypeBuilder*, "opaque_ptr", builder_ptr)
	) {
		CORE_ASSERT(builder_ptr->return_type, "Return type not set");
		auto  func_type = builder_ptr->produce(*ctx_ptr);
		auto* result    = new tsh::SymbolType<>(func_type);
		delete builder_ptr;
		return result;
	}

	std::vector<vm::code::ExternalCFunction> getComptimeTypeExternOperations(vm::PID pid) {
		return {
			VM_INSTANCE_EXT_C_FUNC(__comptime_create_box, __comptime_create_box, pid),
			VM_INSTANCE_EXT_C_FUNC(__comptime_create_ref, __comptime_create_ref, pid),
			VM_INSTANCE_EXT_C_FUNC(__comptime_create_const, __comptime_create_const, pid),
			// TODOP: Optionals now?
			// VM_INSTANCE_EXT_C_FUNC(__comptime_create_optional, __comptime_create_optional, pid),
			VM_INSTANCE_EXT_C_FUNC(__comptime_get_size, __comptime_get_size, pid),
			VM_INSTANCE_EXT_C_FUNC(__comptime_tuple_builder_new, __comptime_tuple_builder_new, pid),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_tuple_builder_push, __comptime_tuple_builder_push, pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_tuple_builder_finalize, __comptime_tuple_builder_finalize, pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_variant_builder_new, __comptime_variant_builder_new, pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_variant_builder_push, __comptime_variant_builder_push, pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_variant_builder_finalize, __comptime_variant_builder_finalize, pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_func_type_builder_new, __comptime_func_type_builder_new, pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_func_type_builder_set_ret_type,
				__comptime_func_type_builder_set_ret_type,
				pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_func_type_builder_push_arg, __comptime_func_type_builder_push_arg, pid
			),
			VM_INSTANCE_EXT_C_FUNC(
				__comptime_func_type_builder_finalize, __comptime_func_type_builder_finalize, pid
			),
		};
	}

	vm::code::CodeCollection getComptimeTypeOperations(vm::PID pid) {
		// A global storing an opaque pointer to `query::Context` needed for performing type
		// system calls during DVM evaluation.
		vm::code::GlobalData context_global{ .name      = base::StrID("__comptime_query_ctx"),
			                                 .type      = base::StrID("opaque_ptr"),
			                                 .ctor_name = {},
			                                 .dtor_name = {} };

		// A function used for initializing the global context pointer. Called by the comptime
		// VM instance, before comptime operations. Takes in an opaque pointer storing the
		// `query::Context*` and sets the value of the global context pointer. The function looks as
		// follows:
		//
		// function __comptime_set_ctx { opaque_ptr } -> void {
		// 		mov_gopq_lopq __comptime_query_ctx, arg0;
		// 		ret;
		// }
		vm::code::Instruction mov_gopq_lopq = vm::code::instructions::Op_mov_gopq_lopq(
			vm::opargs::GlobalOpq(context_global.name),
			vm::opargs::StackLocalOpq(base::StrID("arg0"))
		);
		vm::code::Instruction ret = vm::code::instructions::Op_ret{};

		vm::code::Function init_global_context{ .name = base::StrID("__comptime_set_ctx"),
			                                    .body = { mov_gopq_lopq, ret },
			                                    .signature
			                                    = { .result_type = base::StrID("void"),
			                                        .parameters = { base::StrID("opaque_ptr") } } };

		return {
			.functions            = { init_global_context },
			.types                = {},
			.global_data          = { context_global },
			.external_c_functions = getComptimeTypeExternOperations(pid),
		};
	}


}
