#include "../dvm_value.hpp"
#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"
#include "instruction_lowerer.hpp"

#include <tsl/type_layout.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/opcode_args.hpp>

namespace compiler::backend_vm::internal {

	vm::code::builders::OpKind InstructionLowerer::getOpKindFromLIRLayouts(
		Ref<FunctionLoweringContext>        ctx,
		const lir::CastParameters&          cast_params,
		std::vector<vm::opargs::OpCodeArg>& out_arguments
	) {
		const auto  target_layout = cast_params.target_layout;
		const auto  source_layout = cast_params.source_layout;
		const auto& source_type   = cast_params.source_type;
		const auto& target_type   = cast_params.target_type;

		auto is_signed = [](const tsh::SymbolType<>& type) -> bool {
			if (type.getType().getKind() == tsh::Kind::Integral) {
				return (
					tsh::IntegralAbstractType(type.getType()).getSignedness()
					== tsh::IntegralAbstractType::Signedness::Signed
				);
			}
			// Char, bool, etc. are treated as unsigned
			return false;
		};

		variant_match(source_layout->getVariant()) {
			variant_case_novalue(tsl::IntegralTypeLayout) {
				const bool src_signed = is_signed(source_type);
				variant_match(target_layout->getVariant()) {
					variant_case_novalue(tsl::IntegralTypeLayout) {
						// ================== Int -> Int ==================
						if (source_layout->getSize() < target_layout->getSize()) {
							// Extend
							return src_signed ? OpKind::sext : OpKind::zext;
						} else if (source_layout->getSize() > target_layout->getSize()) {
							// Truncate
							return OpKind::trunc;
						} else {
							// No-op cast
							return OpKind::mov;
						}
					}
					variant_case_novalue(tsl::FloatTypeLayout) {
						// ================== Int -> Float ==================
						return src_signed ? OpKind::sitofp : OpKind::uitofp;
					}
					variant_default {
						CORE_PANIC("Unsupported cast from integral-layout to target layout");
					}
				}
			}
			variant_case_novalue(tsl::FloatTypeLayout) {
				variant_match(target_layout->getVariant()) {
					variant_case_novalue(tsl::IntegralTypeLayout) {
						// ================== Float -> Int ==================
						const bool to_signed = is_signed(target_type);
						return to_signed ? OpKind::fptosi : OpKind::fptoui;
					}
					variant_case_novalue(tsl::FloatTypeLayout) {
						// ================== Float -> Float ==================
						if (source_layout->getSize() < target_layout->getSize()) {
							// Extend
							return OpKind::fpext;
						} else if (source_layout->getSize() > target_layout->getSize()) {
							// Truncate
							return OpKind::fptrunc;
						} else {
							// No-op cast
							return OpKind::mov;
						}
					}
					variant_default {
						CORE_PANIC("Unsupported cast from float-layout to target layout");
					}
				}
			}
			variant_case(tsl::PointerTypeLayout, pointer_layout) {
				variant_match(target_layout->getVariant()) {
					variant_case(tsl::PointerTypeLayout, target_pointer_layout) {
						using enum tsl::PointerTypeLayout::PointerKind;
						auto& query_ctx = **ctx->program_context.getActiveContext();

						const bool same_pointee
							= pointer_layout.getPointee(query_ctx)->getSourceType()
						   == target_pointer_layout.getPointee(query_ctx)->getSourceType();

						/**
						 * @brief Takes the native address of `source` (a `ptr T`) into a fresh
						 * `cptr T` temporary, keeping the source pointee.
						 *
						 * Emitted when the target pointee differs.
						 */
						auto cptr_source_type_tmp
							= [&](const vm::opargs::OpCodeArg& source) -> DVMPlace {
							const vm::code::TypeOfData& source_pointer_type
								= ctx->programCtx().getOrInsertPointerType(
									ctx->programCtx().keepTslType(pointer_layout.getPointee(query_ctx
							        )),
									CPointer
								);
							const DVMPlace cptr_source_elem_tmp
								= ctx->pushTempLocal(source_pointer_type, "to_cptr_tmp");
							ctx->pushInstruction({ OpKind::cast, cptr_source_elem_tmp, source });
							return cptr_source_elem_tmp;
						};

						/**
						 * @brief Turns `source` (a `manyptr T`, a handle to a dynamic table) into
						 * a `ptr T` temporary pointing at element 0.
						 */
						auto lea_first_element
							= [&](const vm::opargs::OpCodeArg& source) -> DVMPlace {
							const vm::code::TypeOfData& ptr_to_element_type
								= ctx->programCtx().getOrInsertPointerType(
									ctx->programCtx().keepTslType(pointer_layout.getPointee(query_ctx
							        ))
								);

							auto index_tmp
								= ctx->forceToPlace(DVMValue(DVMImmediate::u64(0)), "index_tmp");

							const DVMPlace element_ptr_tmp
								= ctx->pushTempLocal(ptr_to_element_type, "cast_elem_ptr_tmp");
							ctx->pushInstruction(
								{ OpKind::dynTableLea, element_ptr_tmp, source, index_tmp }
							);
							return element_ptr_tmp;
						};

						if (pointer_layout.getPointerKind() == CPointer
						    && target_pointer_layout.getPointerKind() == CPointer) {
							return OpKind::cast;
						} else if (pointer_layout.getPointerKind() == SinglePointer
						           && target_pointer_layout.getPointerKind() == CPointer) {
							// A differing pointee needs the extra hop through a `cptr` of the
							// source pointee, so that the cast stays cptr-to-cptr.
							if (!same_pointee)
								out_arguments[0] = cptr_source_type_tmp(out_arguments[0]);
							return OpKind::cast;
						} else if (pointer_layout.getPointerKind() == ManyPointer
						           && target_pointer_layout.getPointerKind() == CPointer) {
							// First ManyPointer -> Pointer, then Pointer -> CPointer
							out_arguments[0] = lea_first_element(out_arguments[0]);
							if (!same_pointee)
								out_arguments[0] = cptr_source_type_tmp(out_arguments[0]);
							return OpKind::cast;
						} else if (pointer_layout.getPointerKind() == ManyPointer
						           && target_pointer_layout.getPointerKind() == SinglePointer) {
							// ManyPointer -> SinglePointer

							// tmp = 0
							// out = dynTableLea source, tmp
							const auto zero_index = ctx->forceToPlace(
								DVMValue(DVMImmediate::u64(0)), "cast_index_tmp"
							);
							out_arguments.push_back(zero_index);
							return OpKind::dynTableLea;
						} else {
							CORE_PANIC("Unsupported cast between different pointer kinds");
						}
					}
					variant_default {
						CORE_PANIC("Unsupported cast from pointer-layout to target layout");
					}
				}
			}
			variant_default {
				CORE_PANIC(base::strConcat(
					"Unsupported cast source layout in DVM lowering: ",
					source_layout->toStringIdentification()
				));
			}
		}
		CORE_UNREACHABLE();
	}

	void InstructionLowerer::lower(const CastOperation& op) {
		auto target_type = *ctx->program_context.lowerAndKeepTslType(op.cast_params.target_layout);

		// If the source is an immediate, we place it in a local and perform a cast on it.
		std::vector<vm::opargs::OpCodeArg> arguments{ ctx->forceToPlace(op.src, "cast_src_tmp") };

		// Operation in form a = OP b (like mov)
		auto operation = getOpKindFromLIRLayouts(ctx, op.cast_params, arguments);
		vm::code::builders::InstructionBuilder builder(operation);

		if (op.dest && op.dest->isDirect()) {
			// If output is a direct (not a local storing a pointer to the output place) local,
			// we optimize the cast to work directly on the local.
			builder.pushArg(*op.dest);
			builder.pushArgs(arguments);
			ctx->pushInstruction(builder.build());
		} else {
			// Otherwise, if the output place is not direct, we have to create a
			// temporary to place the cast result and then move it into the output place.
			DVMPlace dst_temp = ctx->pushTempLocal(target_type, "cast_dst_tmp");

			builder.pushArg(dst_temp);
			builder.pushArgs(arguments);
			ctx->pushInstruction(builder.build());

			ctx->maybeStoreResult(op.dest, { dst_temp });
		}
	}
}
