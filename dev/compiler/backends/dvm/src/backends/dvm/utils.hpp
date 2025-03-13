#pragma once

#include <backends/dvm/instructions.hpp>
#include <vm/code_data/opcode_args.hpp>

#define NOIMPL_CASE(tp, reason)                                                          \
	variant_case(tp, _) {                                                                \
		throw base::NotYetImplemented(                                                   \
			base::strConcat("Unsupported type ", base::typeName<tp>(), " for: ", reason) \
		);                                                                               \
	}

namespace compiler::backend_vm::utils {
	/**
	 * @brief Tests whether two opcode arguments are equal.
	 */
	bool areArgsEqual(const vm::opargs::OpCodeArg& arg0, const vm::opargs::OpCodeArg& arg1);

	/**
	 * @brief Tests whether two opcode instructions are equal.
	 */
	bool areInstrEqual(const VmInstruction& instr0, const VmInstruction& instr1);
}
