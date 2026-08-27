#include "../dvm_value.hpp"
#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"
#include "instruction_lowerer.hpp"

namespace compiler::backend_vm::internal {
	using namespace vm::code;

	namespace {
		/**
		 * @brief The strategy of lowering the variant alternative type to DVM type.
		 *
		 * We can't use the lowerAndKeepTslType directly, as it lowers the unit type
		 * to empty optional result. We want a different behaviour for variant alterantive
		 * type, we want unit type to lower to some opaque type.
		 */
		const TypeOfData& alternativeTypeOf(
			Ref<FunctionLoweringContext> ctx, const lir::VariantParameters& params
		) {
			return *ctx->programCtx()
			            .lowerAndKeepTslType(params.alternative_layout)
			            .copyValueOr(&ctx->programCtx().getUnitType());
		}
	}

	void InstructionLowerer::lower(const VariantConstructOperation& op) {
		const TypeOfData& alternative_type     = alternativeTypeOf(ctx, op.variant_params);
		const auto        alternative_type_arg = vm::opargs::Type(typeName(alternative_type));

		// Activate the alternative, then store the payload through a pointer to the
		// variant's data.
		ctx->pushInstruction({ OpKind::variantSetInner, op.dest.asArgument(), alternative_type_arg }
		);

		// An alternative carrying no information (e.g. `()`) is fully described by being active,
		// so there is no payload to write.
		if (op.payload.empty()) return;

		const TypeOfData& ptr_type    = ctx->programCtx().getOrInsertPointerType(alternative_type);
		DVMPlace          payload_ptr = ctx->pushTempLocal(ptr_type, "variant_data_ptr");
		ctx->pushInstruction({ OpKind::variantGetInner,
		                       payload_ptr.asArgument(),
		                       op.dest.asArgument(),
		                       alternative_type_arg });

		ctx->maybeStoreResult(
			payload_ptr.withAccessKind(DVMPlace::AccessKind::Pointer), op.payload.value()
		);
	}

	void InstructionLowerer::lower(const VariantTryProjectOperation& op) {
		CORE_ASSERT(op.dest.isDirect(), "Non direct dest is not supported.");

		const TypeOfData& alternative_type = alternativeTypeOf(ctx, op.variant_params);

		// `op.variant` is a reference to the variant, so `asArgument` yields a pointer place and
		// the loader picks the `variantGetInner_pptr_pptr_type` form, which recovers the variant
		// type from the pointer's inner type.
		// The VM writes the payload address into `dest`, or null on alternative mismatch.
		ctx->pushInstruction({ OpKind::variantGetInner,
		                       op.dest.asArgument(),
		                       op.variant.asArgument(),
		                       vm::opargs::Type(typeName(alternative_type)) });
	}
}
