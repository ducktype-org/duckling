#include "../dvm_value.hpp"
#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"
#include "instruction_lowerer.hpp"

namespace compiler::backend_vm::internal {
	using namespace vm::code;

	void InstructionLowerer::lower(const VariantConstructOperation& op) {
		const TypeOfData& alternative_type
			= **ctx->program_context.lowerAndKeepTslType(op.variant_params.alternative_layout);
		const auto alternative_type_arg = vm::opargs::Type(typeName(alternative_type));

		// Activate the alternative, then store the payload through a pointer to the
		// variant's data.
		ctx->pushInstruction({ OpKind::variantSetInner, op.dest.asArgument(), alternative_type_arg }
		);

		const TypeOfData& ptr_type = ctx->program_context.getOrInsertPointerType(alternative_type);
		DVMPlace          payload_ptr = ctx->pushTempLocal(ptr_type, "variant_data_ptr");
		ctx->pushInstruction({ OpKind::variantGetInner,
		                       payload_ptr.asArgument(),
		                       op.dest.asArgument(),
		                       alternative_type_arg });

		ctx->maybeStoreResult(payload_ptr.withAccessKind(DVMPlace::AccessKind::Pointer), op.payload);
	}

	void InstructionLowerer::lower(const VariantTryProjectOperation& op) {
		const TypeOfData& alternative_type
			= **ctx->program_context.lowerAndKeepTslType(op.variant_params.alternative_layout);

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
