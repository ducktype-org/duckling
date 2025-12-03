#include "program_lowering_context.hpp"

using namespace compiler::backend_vm::internal;

vm::code::TypeOfData ProgramLoweringContext::lowerTslType(CRef<tsl::TypeLayout> layout) {
	variant_match(layout->getVariant()) {
		variant_case_novalue(tsl::EmptyTypeLayout) {
			return vm::code::PrimitiveType(base::StrID("void"), 1);
		}
		variant_case_novalue(tsl::IntegralTypeLayout) {
			auto bits = usize(layout->getSize());
			if (bits == 1) bits = 8;  // Boolean case.
			if (bits % 8 != 0) CORE_PANIC("Integral type size not divisible by 8");
			usize       bytes = bits / 8;
			std::string name  = "i" + std::to_string(bits);

			return vm::code::PrimitiveType(base::StrID(name.c_str()), bytes);
		}
		variant_case_novalue(tsl::FloatTypeLayout) {
			auto bits = usize(layout->getSize());
			CORE_ASSERT(
				bits == 16 || bits == 32 || bits == 64 || bits == 80, "Invalid size of float: ", bits
			);
			usize       bytes = bits / 8;
			std::string name  = "f" + std::to_string(bits);

			return vm::code::PrimitiveType(base::StrID(name.c_str()), bytes);
		}
		variant_default {
			CORE_PANIC(base::strConcat("Type not handled yet: ", layout->toStringIdentification()));
		}
	}
	CORE_UNREACHABLE();
}

const DVMGlobal& ProgramLoweringContext::getDVMGlobal(const lir::LIRGlobal& lir_global) {
	if (!lir_global_to_dvm.contains(lir_global)) {
		auto var_type = lowerTslType(lir_global.layout);
		lir_global_to_dvm.put(
			lir_global, DVMGlobal{ .name = lir_global.mangled_name, .type = var_type }
		);
	}

	return lir_global_to_dvm.at(lir_global);
}
