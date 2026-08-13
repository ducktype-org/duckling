#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::backend_vm::internal {
	namespace {
		/**
		 * @brief Given a pointer type as an argument, return the name of the type it points to.
		 * For a `manyptr T` that is the dynamic table of `T`, for a `ptr T` it is `T` itself.
		 */
		base::StrID extractPointeeTypeName(const vm::code::TypeOfData& ptr_type) {
			const auto* pointer = std::get_if<vm::code::PointerType>(&ptr_type);
			CORE_ASSERT(pointer != nullptr, "DVM builtin expects a `ptr`/`manyptr` operand");
			return pointer->inner;
		}
	}

	void InstructionLowerer::lower(const BuiltinCallOperation& op) {
		switch (op.kind) {
		case lir::BuiltinFunctionKind::DvmAllocArr: {
			CORE_ASSERT(op.args.size() == 1, "dvm_alloc_arr expects 1 argument (size)");
			CORE_ASSERT(op.dest.has_value(), "dvm_alloc_arr must have a destination");

			auto count      = ctx->forceToPlace(op.args.front(), "alloc_count");
			auto table_type = extractPointeeTypeName(op.dest->getType());
			ctx->pushInstruction({ OpKind::dynTableReAlloc,
			                       op.dest->asArgument(),
			                       vm::opargs::Type(table_type),
			                       count.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmReallocArr: {
			CORE_ASSERT(op.args.size() == 2, "dvm_realloc_arr expects 2 arguments (ptr, size)");

			auto table_ptr  = ctx->forceToPlace(op.args.at(0), "realloc_ptr");
			auto count      = ctx->forceToPlace(op.args.at(1), "realloc_count");
			auto table_type = extractPointeeTypeName(table_ptr.getType());
			ctx->pushInstruction({ OpKind::dynTableReAlloc,
			                       table_ptr.asArgument(),
			                       vm::opargs::Type(table_type),
			                       count.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmFreeArr: {
			// `dvm_free_arr(p: manyptr T)`. Frees the dynamic table under `p`.
			CORE_ASSERT(op.args.size() == 1, "dvm_free_arr expects 1 argument (ptr)");

			auto table_ptr = ctx->forceToPlace(op.args.front(), "free_arr_ptr");
			ctx->pushInstruction({ OpKind::free, table_ptr.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmAlloc: {
			// `dvm_alloc() -> ptr T`. Allocates storage for a single `T`, the pointee type is read
			// off the destination `ptr T`.
			CORE_ASSERT(op.args.empty(), "dvm_alloc expects no arguments");
			CORE_ASSERT(op.dest.has_value(), "dvm_alloc must have a destination");

			auto pointee_type = extractPointeeTypeName(op.dest->getType());
			ctx->pushInstruction(
				{ OpKind::alloc, op.dest->asArgument(), vm::opargs::Type(pointee_type) }
			);
			break;
		}
		case lir::BuiltinFunctionKind::DvmFree: {
			// `dvm_free(p: ptr T)`. Frees the single object under `p`.
			CORE_ASSERT(op.args.size() == 1, "dvm_free expects 1 argument (ptr)");

			auto ptr = ctx->forceToPlace(op.args.front(), "free_ptr");
			ctx->pushInstruction({ OpKind::free, ptr.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::BoxAlloc: {
			// `box_alloc(value: T) -> box T`. Allocate memory of size `T` and move the value into it.
			CORE_ASSERT(op.args.size() == 1, "box_alloc expects 1 argument (value)");
			CORE_ASSERT(op.dest.has_value(), "box_alloc must have a destination");

			auto& value = op.args.at(0);
			ctx->pushInstruction({ OpKind::alloc,
			                       op.dest->asArgument(),
			                       vm::opargs::Type(typeName(value.getType())) });
			auto value_place = ctx->forceToPlace(value, "box_value");
			ctx->pushInstruction(
				{ OpKind::store, op.dest->asArgument(), value_place.asAnyArgument() }
			);
			break;
		}
		case lir::BuiltinFunctionKind::BoxFree: {
			// `box_free(b: box T)`. Free the memory owned by the box.
			CORE_ASSERT(op.args.size() == 1, "box_free expects 1 argument (ptr)");

			auto box_ptr = ctx->forceToPlace(op.args.front(), "box_ptr");
			ctx->pushInstruction({ OpKind::free, box_ptr.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmPtrParts: {
			// `dvm_ptr_parts(p: ptr T) -> u64[2]`. A DVM pointer is a (block, offset) pair, so
			// both halves are written into the destination array: the id of the block `p`
			// points into, then the offset within it.
			CORE_ASSERT(op.args.size() == 1, "dvm_ptr_parts expects 1 argument (ptr)");
			CORE_ASSERT(op.dest.has_value(), "dvm_ptr_parts must have a destination");

			auto ptr = ctx->forceToPlace(op.args.front(), "ptr_parts_ptr");

			const auto     word_type  = DVMImmediate::u64(::u64(0)).type;
			const DVMPlace id_tmp     = ctx->pushTempLocal(word_type, "ptr_parts_id");
			const DVMPlace offset_tmp = ctx->pushTempLocal(word_type, "ptr_parts_offset");
			ctx->pushInstruction({ OpKind::cast, id_tmp, offset_tmp, ptr.asArgument() });

			const auto& ptr_to_word_type = ctx->program_context.getOrInsertPointerType(word_type);

			// Both halves go through a pointer to the element, which keeps the store working
			// whether the destination array is a direct local or is reached through a pointer.
			const std::array<DVMPlace, 2> halves{ id_tmp, offset_tmp };
			for (::u64 index = 0; index < halves.size(); index++) {
				const auto     index_imm = DVMImmediate::u64(index);
				const DVMPlace index_tmp = ctx->pushTempLocal(index_imm.type, "ptr_parts_index");
				ctx->pushInstruction({ OpKind::mov, index_tmp, index_imm });

				const DVMPlace element_ptr
					= ctx->pushTempLocal(ptr_to_word_type, "ptr_parts_element");
				ctx->pushInstruction({ OpKind::fixedSizeTableLea,
				                       element_ptr,
				                       op.dest->asArgument(),
				                       index_tmp.asArgument() });
				ctx->pushInstruction(
					{ OpKind::store, element_ptr.asArgument(), halves.at(index).asAnyArgument() }
				);
			}
			break;
		}
		case lir::BuiltinFunctionKind::DvmIsNullptr: {
			// `dvm_is_nullptr(p: ptr T) -> bool`. `cmpNull` leaves its answer in the frame flag,
			// which is materialized into a boolean the same way a comparison is. The pointee
			// plays no role, so a `manyptr` - a pointer to a dynamic table - works here too.
			CORE_ASSERT(op.args.size() == 1, "dvm_is_nullptr expects 1 argument (ptr)");
			CORE_ASSERT(op.dest.has_value(), "dvm_is_nullptr must have a destination");

			auto ptr = ctx->forceToPlace(op.args.front(), "is_nullptr_ptr");

			const auto     bool_type = DVMImmediate::boolean(false).type;
			const DVMPlace result
				= op.dest->isDirect() ? *op.dest : ctx->pushTempLocal(bool_type, "is_nullptr_tmp");

			ctx->pushInstruction({ OpKind::cmpNull, ptr.asArgument() });
			ctx->pushInstruction({ OpKind::mov, result, DVMImmediate::boolean(false) });
			ctx->pushInstruction({ OpKind::cmov, result, DVMImmediate::boolean(true) });

			if (not op.dest->isDirect()) ctx->maybeStoreResult(op.dest, { result });
			break;
		}
		case lir::BuiltinFunctionKind::DvmNullptr: {
			// `dvm_nullptr() -> ptr T`. A pointer into no block.
			CORE_ASSERT(op.args.empty(), "dvm_nullptr expects no arguments");
			CORE_ASSERT(op.dest.has_value(), "dvm_nullptr must have a destination");

			ctx->pushInstruction({ OpKind::setNull, op.dest->asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::ListFree: {
			// `list_free(l: ref [T])`. Lists are not supported in DVM code generation yet.
			throw base::NotYetImplemented("Lists are not supported in DVM code generation yet.");
		}
		}
	}
}
