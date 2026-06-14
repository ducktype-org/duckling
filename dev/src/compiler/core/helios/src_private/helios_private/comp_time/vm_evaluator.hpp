#pragma once

#include <ctv/ctv.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <vm/bytecode/bytecode.hpp>

#include <expected>
#include <string>

namespace compiler::helios {
	struct VmEvaluationError final {
		enum class Kind {
			VmInitializationFailed,
			CodeLoadFailed,
			ArgConversionFailed,
			FunctionRunFailed,
			VmJoinFailed,
			GetExitValueFailed,
			ReturnConversionFailed
		};
		Kind        kind;
		std::string message;

		VmEvaluationError(Kind k, std::string msg): kind(k), message(std::move(msg)) {}
	};

	/**
	 * @brief Executes a function with given arguments on a DVM and returns the calculated result.
	 *
	 * @note: The DVM compile-time evaluation process is initialized once upon the first call to
	 * this function and is reused for subsequent evaluations. This is done by a static instance of
	 * VmManager, which spawns the process when first used and kills on exit.
	 *
	 * @param ctx query context needed to perform compile time type operations.
	 * @param func_name The name of the function to call.
	 * @param lir_unit List of LIR functions and other entities to be passed to the VM. Includes the actual
	 * function to call as well as all others called by it.
	 * @param args A vector of CTVs to be passed as arguments.
	 * @param return_type The expected return type of the function, needed to cast the VMs return
	 * value back to the expected "compiler type"
	 * @return The resulting CTV on success, or a VmEvaluationError.
	 */
	std::expected<ctv::CompileTimeValue, VmEvaluationError> executeInVm(
		query::Context&                           ctx,
		const std::string&                        func_name,
		const lir::LIRUnit&   lir_unit,
		const std::vector<ctv::CompileTimeValue>& args,
		const tsh::SymbolType<>&                  return_type
	);

}
