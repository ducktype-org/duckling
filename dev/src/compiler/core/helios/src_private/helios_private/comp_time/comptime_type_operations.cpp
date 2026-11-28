// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file comptime_type_operations.cpp
 * @brief Implementation of Native/External functions for Compile-Time-Evaluations of meta types.
 *
 * This file defines the interface that allows the DVM to interact directly with the compiler.
 * Performing calls to the type system etc.
 *
 * Some important notes:
 * 1. Type Construction: Provides primitives to create modified types (Box, Ref, Mut, Imm)
 *    and composite types (Tuples, Variants, Functions) during CTE.
 * 2. Builder Patterns: Due to the VM's limitations or supporting variadic type arguments, composite
 * 	  types are constructed using stateful "Builder" objects via a sequence of calls (New -> Push ->
 *    ... -> Finalize).
 * 3. Opaque Pointers: Manages the passing of raw C++ pointers (`tsh::SymbolType*`, builders)
 *    through the VM as `opaque_ptr` types.
 */
#include "comptime_type_operations.hpp"

#include "meta_type_memory_manager.hpp"

#include <helios/symbols/lang_primitives.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <tsl/queries.hpp>
#include <tsl/type_layout.hpp>

#include <base/types/bits_and_bytes.hpp>

#include <query_framework/context/context.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/core/extc/extc_function.hpp>

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
				return tsh::SymbolType<>::withDefaultsMut(
					ctx.query<tsh::QueryVariantType>({ subtypes })
				);
			}
		};

		struct TupleTypeBuilder {
			std::vector<tsh::SymbolType<>> subtypes;

			tsh::SymbolType<> finalize(query::Context& ctx) {
				return tsh::SymbolType<>::withDefaultsMut(
					ctx.query<tsh::QueryTupleType>({ subtypes })
				);
			}
		};
	}

	namespace {
		tsh::SymbolType<>* createBox(tsh::SymbolType<>* type_ptr) {
			auto new_type = type_ptr->toBox();
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		tsh::SymbolType<>* createRef(tsh::SymbolType<>* type_ptr) {
			auto new_type = type_ptr->toRef();
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		tsh::SymbolType<>* createMut(tsh::SymbolType<>* type_ptr) {
			auto new_type = type_ptr->withMutability(tsh::Mutability::Mutable);
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		tsh::SymbolType<>* createImm(tsh::SymbolType<>* type_ptr) {
			auto new_type = type_ptr->withMutability(tsh::Mutability::Immutable);
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		tsh::SymbolType<>* createPtr(query::Context* ctx_ptr, tsh::SymbolType<>* type_ptr) {
			auto new_type = tsh::SymbolType<>::withDefaultsMut(
				ctx_ptr->query<tsh::QueryPointerType>({ *type_ptr })
			);
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		tsh::SymbolType<>* createOptional(query::Context* ctx_ptr, tsh::SymbolType<>* type_ptr) {
			auto new_type
				= tsh::SymbolType<>::withDefaultsMut(getOptionalType(*ctx_ptr, *type_ptr));
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		tsh::SymbolType<>* createManyPtr(query::Context* ctx_ptr, tsh::SymbolType<>* type_ptr) {
			auto new_type = tsh::SymbolType<>::withDefaultsMut(
				ctx_ptr->query<tsh::QueryManyPointerType>({ *type_ptr })
			);
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		tsh::SymbolType<>* createCptr(query::Context* ctx_ptr, tsh::SymbolType<>* type_ptr) {
			auto new_type = tsh::SymbolType<>::withDefaultsMut(
				ctx_ptr->query<tsh::QueryCPointerType>({ *type_ptr })
			);
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		tsh::SymbolType<>* createSlice(query::Context* ctx_ptr, tsh::SymbolType<>* type_ptr) {
			auto new_type = tsh::SymbolType<>::withDefaultsMut(
				ctx_ptr->query<tsh::QuerySliceType>({ *type_ptr })
			);
			return MetaTypeMemoryManager::instance().allocateType(new_type);
		}

		TupleTypeBuilder* tupleBuilderNew() {
			auto* builder = new TupleTypeBuilder();
			return builder;
		}

		void tupleBuilderPush(TupleTypeBuilder* builder_ptr, tsh::SymbolType<>* type_ptr) {
			builder_ptr->subtypes.push_back(*type_ptr);
		}

		tsh::SymbolType<>* tupleBuilderFinalize(
			query::Context* ctx_ptr, TupleTypeBuilder* builder_ptr
		) {
			auto tuple_type = builder_ptr->finalize(*ctx_ptr);
			delete builder_ptr;
			return MetaTypeMemoryManager::instance().allocateType(tuple_type);
		}

		VariantTypeBuilder* variantBuilderNew() {
			auto* builder = new VariantTypeBuilder();
			return builder;
		}

		void variantBuilderPush(VariantTypeBuilder* builder_ptr, tsh::SymbolType<>* type_ptr) {
			builder_ptr->subtypes.push_back(*type_ptr);
		}

		tsh::SymbolType<>* variantBuilderFinalize(
			query::Context* ctx_ptr, VariantTypeBuilder* builder_ptr
		) {
			auto variant_type = builder_ptr->finalize(*ctx_ptr);
			delete builder_ptr;
			return MetaTypeMemoryManager::instance().allocateType(variant_type);
		}

		i64 sizeOf(query::Context* ctx_ptr, tsh::SymbolType<>* type_ptr) {
			const auto& layout
				= ctx_ptr->query<tsl::QuerySymbolTypeLayout>(*type_ptr)->valueOrThrow();
			return base::safeIntConv<i64>(base::bits2bytesRoundUp(layout.getSize()).asInt());
		}

		i64 alignOf(query::Context* ctx_ptr, tsh::SymbolType<>* type_ptr) {
			const auto& layout
				= ctx_ptr->query<tsl::QuerySymbolTypeLayout>(*type_ptr)->valueOrThrow();
			return base::safeIntConv<i64>(layout.getAlignment().asInt());
		}

		bool typesEqual(tsh::SymbolType<>* left, tsh::SymbolType<>* right) {
			return *left == *right;
		}

		bool typesNotEqual(tsh::SymbolType<>* left, tsh::SymbolType<>* right) {
			return *left != *right;
		}

		std::vector<vm::code::ExternalCFunction> getComptimeTypeExternOperations() {
			return {
				VM_EXT_C_FUNC_AS("comptime_create_box", createBox),
				VM_EXT_C_FUNC_AS("comptime_create_ref", createRef),
				VM_EXT_C_FUNC_AS("comptime_create_mut", createMut),
				VM_EXT_C_FUNC_AS("comptime_create_imm", createImm),
				VM_EXT_C_FUNC_AS("comptime_create_ptr", createPtr),
				VM_EXT_C_FUNC_AS("comptime_create_optional", createOptional),
				VM_EXT_C_FUNC_AS("comptime_create_many_ptr", createManyPtr),
				VM_EXT_C_FUNC_AS("comptime_create_cptr", createCptr),
				VM_EXT_C_FUNC_AS("comptime_create_slice", createSlice),
				VM_EXT_C_FUNC_AS("comptime_tuple_builder_new", tupleBuilderNew),
				VM_EXT_C_FUNC_AS("comptime_tuple_builder_push", tupleBuilderPush),
				VM_EXT_C_FUNC_AS("comptime_tuple_builder_finalize", tupleBuilderFinalize),
				VM_EXT_C_FUNC_AS("comptime_variant_builder_new", variantBuilderNew),
				VM_EXT_C_FUNC_AS("comptime_variant_builder_push", variantBuilderPush),
				VM_EXT_C_FUNC_AS("comptime_variant_builder_finalize", variantBuilderFinalize),
				VM_EXT_C_FUNC_AS("comptime_size_of", sizeOf),
				VM_EXT_C_FUNC_AS("comptime_align_of", alignOf),
				VM_EXT_C_FUNC_AS("comptime_types_equal", typesEqual),
				VM_EXT_C_FUNC_AS("comptime_types_not_equal", typesNotEqual),
			};
		}
	}

	vm::code::CodeCollection getComptimeTypeOperations() {
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
		// 		mov_popq_popq comptime_query_ctx, arg0;
		// 		ret;
		// }
		vm::code::Instruction mov_popq_popq = vm::code::instructions::Op_mov_popq_popq(
			vm::opargs::PlaceOpq(context_global.name), vm::opargs::PlaceOpq(base::StrID("arg0"))
		);
		vm::code::Instruction ret = vm::code::instructions::Op_ret{};

		vm::code::Function init_global_context;
		init_global_context.name = base::StrID("comptime_set_ctx");
		init_global_context.body = { mov_popq_popq, ret };
		init_global_context.signature
			= { .result_types = {}, .parameters = { base::StrID("opaque_ptr") } };

		return {
			.functions            = { init_global_context },
			.types                = {},
			.global_data          = { context_global },
			.external_c_functions = getComptimeTypeExternOperations(),
			.ffi_functions        = {},
			.object_files         = {},
		};
	}
}
