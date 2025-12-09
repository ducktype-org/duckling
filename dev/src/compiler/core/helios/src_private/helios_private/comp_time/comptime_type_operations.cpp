#include "comptime_type_operations.hpp"

#include "typesystem/higher/mutability.hpp"
#include "typesystem/higher/symbol_type.hpp"
#include "typesystem/lower/queries.hpp"

#include "base/except/exceptions.hpp"

#include "vm/api/vm.hpp"
#include "vm/bytecode/bytecode.hpp"
#include "vm/bytecode/extern_c_function.hpp"
#include "vm/utils/interpret.hpp"

// TODOP: Comments in this file.
namespace compiler::helios::comptime_ops {
	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_create_box,
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
		__comptime_create_ref,
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
		__comptime_create_const,
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
		__comptime_get_size,
		(query::Context*, "opaque_ptr", ctx_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		auto layout = ctx_ptr->query<tsl::QuerySymbolTypeLayout>(*type_ptr);
		return static_cast<i64>(layout->getSize());
	}

	DEF_VM_EXT_C_FUNC(TupleTypeBuilder*, "opaque_ptr", __comptime_tuple_builder_new) {
		auto* builder = new TupleTypeBuilder();
		return builder;
	}

	DEF_VM_EXT_C_FUNC(
		i64,  // TODOP: Fix the macro so it works with voids
		"i64",
		__comptime_tuple_builder_push,
		(TupleTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		builder_ptr->subtypes.push_back(*type_ptr);
		return 0;
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_tuple_builder_finalize,
		(query::Context*, "opaque_ptr", ctx_ptr),
		(TupleTypeBuilder*, "opaque_ptr", builder_ptr)
	) {
		auto  tuple_type = builder_ptr->produce(*ctx_ptr);
		auto* result     = new tsh::SymbolType<>(tuple_type);
		delete builder_ptr;
		return result;
	}

	DEF_VM_EXT_C_FUNC(VariantTypeBuilder*, "opaque_ptr", __comptime_variant_builder_new) {
		auto* builder = new VariantTypeBuilder();
		return builder;
	}

	DEF_VM_EXT_C_FUNC(
		i64,  // TODOP: Fix the macro so it works with voids
		"i64",
		__comptime_variant_builder_push,
		(VariantTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		builder_ptr->subtypes.push_back(*type_ptr);
		return 0;
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_variant_builder_finalize,
		(query::Context*, "opaque_ptr", ctx_ptr),
		(VariantTypeBuilder*, "opaque_ptr", builder_ptr)
	) {
		auto  tuple_type = builder_ptr->produce(*ctx_ptr);
		auto* result     = new tsh::SymbolType<>(tuple_type);
		delete builder_ptr;
		return result;
	}

	DEF_VM_EXT_C_FUNC(FunctionTypeBuilder*, "opaque_ptr", __comptime_func_type_builder_new) {
		auto* builder = new FunctionTypeBuilder();
		return builder;
	}

	DEF_VM_EXT_C_FUNC(
		i64,  // TODOP: Fix the macro so it works with voids
		"i64",
		__comptime_func_type_builder_set_ret_type,
		(FunctionTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		CORE_ASSERT(!builder_ptr->return_type, "Return type set twice");
		builder_ptr->return_type = *type_ptr;
		return 0;
	}

	DEF_VM_EXT_C_FUNC(
		i64,  // TODOP: Fix the macro so it works with voids
		"i64",
		__comptime_func_type_builder_push_arg,
		(FunctionTypeBuilder*, "opaque_ptr", builder_ptr),
		(tsh::SymbolType<>*, "opaque_ptr", type_ptr)
	) {
		builder_ptr->arg_types.push_back(*type_ptr);
		return 0;
	}

	DEF_VM_EXT_C_FUNC(
		tsh::SymbolType<>*,
		"opaque_ptr",
		__comptime_func_type_builder_finalize,
		(query::Context*, "opaque_ptr", ctx_ptr),
		(FunctionTypeBuilder*, "opaque_ptr", builder_ptr)
	) {
		CORE_ASSERT(builder_ptr->return_type, "Return type not set");
		auto  func_type = builder_ptr->produce(*ctx_ptr);
		auto* result    = new tsh::SymbolType<>(func_type);
		delete builder_ptr;
		return result;
	}

	std::vector<vm::code::ExternalCFunction> getComptimeTypeOperations(vm::PID pid) {
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


}
