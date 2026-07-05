#include "../dvm_value.hpp"
#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"
#include "instruction_lowerer.hpp"

namespace compiler::backend_vm::internal {
	using namespace vm::code;

	void InstructionLowerer::lower(const VariantConstructOperation& op) {
		auto lowered_alternative
			= ctx->program_context.lowerAndKeepTslType(op.variant_params.alternative_layout);
		// Information-less alternatives (the `()` of an optional) have no VM type; they are
		// represented by the same one-byte marker primitive the variant type declares.
		const TypeOfData alternative_type
			= lowered_alternative.has_value()
		        ? **lowered_alternative
		        : *ctx->program_context.keepVMType(
					  vm::code::PrimitiveType(base::StrID("unit_marker"), Bytes{ 1 })
				  );
		const auto alternative_type_arg = vm::opargs::Type(typeName(alternative_type));

		// Activate the alternative, then store the payload through a pointer to the
		// variant's data.
		ctx->pushInstruction({ OpKind::variantSetInner, op.dest.asArgument(), alternative_type_arg }
		);

		// An absent payload means the alternative carries no information (an optional's
		// `none` state) — activating the alternative is all there is to do.
		if (!op.payload.has_value()) return;

		const TypeOfData& ptr_type = ctx->program_context.getOrInsertPointerType(alternative_type);
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
		const TypeOfData& alternative_type
			= **ctx->program_context.lowerAndKeepTslType(op.variant_params.alternative_layout);

		// The VM writes the payload address into `dest`, or null on alternative mismatch.
		ctx->pushInstruction({ OpKind::variantGetInner,
		                       op.dest.asArgument(),
		                       op.variant.asArgument(),
		                       vm::opargs::Type(typeName(alternative_type)) });
	}
}
