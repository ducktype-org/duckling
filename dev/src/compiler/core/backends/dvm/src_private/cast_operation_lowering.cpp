#include "cast_operation_lowering.hpp"

#include "function_lowering_context.hpp"
#include "program_lowering_context.hpp"

namespace compiler::backend_vm::internal {

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
			variant_default { CORE_PANIC("Unsupported cast source layout in DVM lowering"); }
		}
		CORE_UNREACHABLE();
	}

	void CastOperationLowerer::lowerCastOperation(
		const CastOperation&     cast_operation,
		std::deque<DVMValue>&    args,
		base::Optional<DVMValue> maybe_output,
		FunctionLoweringContext& function_context
	) {
		// Operation in form a = OP b (like mov)
		CORE_ASSERT(args.size() == 1, "Invalid cast operation argument count");
		CORE_ASSERT(maybe_output.has_value(), "Cast operations must have an output destination");
		auto output    = maybe_output.value();
		auto operation = getOpKindFromLIRLayouts(cast_operation.cast_params);

		// The cast operations are only supported between local stack values.
		// So if we have a non-local source (like immediate value or global),
		// we first move it to a temporary local, perform the cast there,

		// If the destination is non-local, we put the result in a temporary local
		// and then move the result to the final destination.
		// ---- Resolve source ----
		DVMValue                 src_arg = args[0];
		base::Optional<DVMLocal> src_temp;

		if (not src_arg.is<DVMLocal>()) {
			auto source_type = function_context.program_context.lowerAndKeepTslType(
				cast_operation.cast_params.source_layout
			);

			src_temp = function_context.pushTempLocal(source_type, "cast_src_tmp");
			src_arg  = DVMValue(src_temp.value());
		}

		// ---- Resolve destination ----
		DVMValue                 dst_arg = output;
		base::Optional<DVMLocal> dst_temp;

		if (not dst_arg.is<DVMLocal>()) {
			auto target_type = function_context.program_context.lowerAndKeepTslType(
				cast_operation.cast_params.target_layout
			);

			dst_temp = function_context.pushTempLocal(target_type, "cast_dst_tmp");
			dst_arg  = DVMValue(dst_temp.value());
		}

		// ---- Move the source to temp if needed ----
		if (src_temp.has_value())
			function_context.pushInstruction({ OpKind::mov, src_arg, args[0] });

		// ---- Perform cast (local → local) ----
		function_context.pushInstruction({ operation, dst_arg, src_arg });

		// ---- Move to final destination if needed ----
		if (dst_temp.has_value())
			function_context.pushInstruction({ OpKind::mov, output, dst_temp->asArgument() });

		// ---- Cleanup ----
		if (src_temp.has_value())
			function_context.pushInstruction({ vm::code::instructions::Op_deinit() });
		if (dst_temp.has_value())
			function_context.pushInstruction({ vm::code::instructions::Op_deinit() });
	}
}
