#pragma once

#include <helios/ctv/ctv.hpp>
#include <helios/helios_errors.hpp>

#include <vm/bytecode/bytecode.hpp>

#include <expected>
#include <string>

namespace compiler::helios {
	/**
	 * @brief Executes a function with given arguments on a DVM and returns the calculated result.
	 *
	 * The VM is initialized on the first call and reused for subsequent evaluations.
	 *
	 * @param func_name The name of the function to call.
	 * @param code The bytecode containing the function to execute.
	 * @param args A vector of CTVs to be passed as arguments.
	 * @param return_type The expected return type of the function.
	 * @return The resulting CTV on success, or an error.
	 */
	std::expected<CompileTimeValue, errors::Failed> executeInVm(
		const std::string&                   func_name,
		const vm::code::CodeCollection&      code,
		const std::vector<CompileTimeValue>& args,
		const tsh::SymbolType<>&             return_type
	);

}
