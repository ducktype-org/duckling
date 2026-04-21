#include "instruction_lowerer.hpp"

namespace {
	vm::code::builders::OpKind getOpKindFromLIRLayouts(const lir::CastParameters& cast_params) {
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

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const CastOperation& op) {
		// Operation in form a = OP b (like mov)
		auto operation   = getOpKindFromLIRLayouts(op.cast_params);
		auto target_type = ctx.program_context.lowerAndKeepTslType(op.cast_params.target_layout);

		// The cast operations are only supported between local stack values.
		// So if we have a non-local source (like immediate value or global),
		// we first move it to a temporary local and perform the cast there.

		// If the destination is non-local, we put the result in a temporary local
		// and then move the result to the final destination.
		DVMLocal src_arg = ctx.forceToLocal(op.src, "cast_src_tmp");

		if (op.dest.isDirect() && op.dest.is<DVMLocal>()) {
			// If output is a direct (not a local storing a pointer to the output place) local,
			// we optimize the cast to work directly on the local.
			auto dst_local = op.dest.get<DVMLocal>();
			ctx.pushInstruction({ operation, dst_local, src_arg });
		} else {
			// Otherwise, if the output place is not direct or a global we have to create a
			// temporary to perform the operation on.
			DVMLocal dst_temp = ctx.pushTempLocal(target_type, "cast_dst_tmp");

			ctx.pushInstruction({ operation, dst_temp, src_arg });
			ctx.storeResult(op.dest, { dst_temp, DVMPlace::AccessKind::Direct });
		}
	}
}
