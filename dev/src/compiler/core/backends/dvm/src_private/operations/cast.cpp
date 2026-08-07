#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"
#include "instruction_lowerer.hpp"

#include <tsl/type_layout.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/opcode_args.hpp>

namespace compiler::backend_vm::internal {
	namespace {

		vm::code::builders::OpKind getOpKindFromLIRLayouts(
			Ref<FunctionLoweringContext>        ctx,
			Ref<ProgramLoweringContext>         program_context,
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
							using tsl::PointerTypeLayout::PointerKind::CPointer;
							using tsl::PointerTypeLayout::PointerKind::ManyPointer;
							using tsl::PointerTypeLayout::PointerKind::SinglePointer;

							// The first element of a dynamic table is where the table's payload
							// starts, so a ManyPointer is narrowed to a SinglePointer to it before
							// anything else looks at the address.
							auto lea_first_element
								= [&](const vm::opargs::OpCodeArg& source) -> DVMPlace {
								const vm::code::TypeOfData& vm_element_type
									= **program_context->lowerAndKeepTslType(
										pointer_layout.getPointee()
									);
								const vm::code::TypeOfData& ptr_to_element_type
									= program_context->getOrInsertPointerType(vm_element_type);

								const auto     zero_index = DVMImmediate::u64(u64(0));
								const DVMPlace index_tmp
									= ctx->pushTempLocal(zero_index.type, "cast_index_tmp");
								ctx->pushInstruction({ OpKind::mov, index_tmp, zero_index });

								const DVMPlace element_ptr_tmp
									= ctx->pushTempLocal(ptr_to_element_type, "cast_elem_ptr_tmp");
								ctx->pushInstruction(
									{ OpKind::dynTableLea, element_ptr_tmp, source, index_tmp }
								);
								return element_ptr_tmp;
							};

							// CPointer -> CPointer
							if (pointer_layout.getPointerKind() == CPointer
							    && target_pointer_layout.getPointerKind() == CPointer) {
								// Reinterpreting one native address as another is exactly what a C
								// cast does; nothing about the address itself changes.
								return OpKind::movCast;
							} else if ((pointer_layout.getPointerKind() == SinglePointer
							            || pointer_layout.getPointerKind() == ManyPointer)
							           && target_pointer_layout.getPointerKind() == CPointer) {
								// Pointer -> CPointer
								// `cast_pcptr_pptr` demands that both sides agree on the pointee,
								// so a ManyPointer (a pointer to a dynamic table) is first
								// narrowed to a pointer to its first element.
								if (pointer_layout.getPointerKind() == ManyPointer) {
									CORE_ASSERT(
										out_arguments.size() == 1,
										"Expected the cast source as the only pending argument"
									);
									out_arguments[0] = lea_first_element(out_arguments[0]);
								}
								return OpKind::cast;
							} else if (pointer_layout.getPointerKind() == ManyPointer
							           && target_pointer_layout.getPointerKind() == SinglePointer) {
								// ManyPointer -> SinglePointer

								// tmp = 0
								// out = dynTableLea source, tmp
								const auto     zero_index = DVMImmediate::u64(u64(0));
								const DVMPlace index_tmp
									= ctx->pushTempLocal(zero_index.type, "cast_index_tmp");
								ctx->pushInstruction({ OpKind::mov, index_tmp, zero_index });
								out_arguments.push_back(index_tmp);
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
	}

	void InstructionLowerer::lower(const CastOperation& op) {
		auto target_type = **ctx->program_context.lowerAndKeepTslType(op.cast_params.target_layout);

		// If the source is an immediate, we place it in a local and perform a cast on it.
		std::vector<vm::opargs::OpCodeArg> arguments{ ctx->forceToPlace(op.src, "cast_src_tmp") };

		// Operation in form a = OP b (like mov)
		auto operation
			= getOpKindFromLIRLayouts(ctx, &ctx->program_context, op.cast_params, arguments);
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
