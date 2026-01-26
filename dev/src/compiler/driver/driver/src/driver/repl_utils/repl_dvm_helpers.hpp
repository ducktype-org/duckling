#pragma once

#include <helios/hout/hout.hpp>

#include <query_frameworkcontext/context.hpp>

#include <vm/core/process/interface_types.hpp>

#include <expected>
#include <string>

namespace compiler::repl {

	/**
	 * @brief Compile HOUT unit to DVM bytecode and load it into a running DVM process
	 */
	std::expected<void, std::string> compileAndLoad(
		query::Context& ctx, const helios::HOUTUnit& hout_unit, vm::PID pid
	);

	/**
	 * @brief Execute a previously loaded function on the DVM and capture its return value
	 *
	 * Runs any previously loaded function on the DVM, waits for completion, and extracts
	 * the return value as a formatted string. Primarily used for executing REPL expression
	 * wrappers, but can execute any loaded function with a supported return type.
	 *
	 * @param pid Process ID of the target DVM instance
	 * @param func_name Mangled name of the function to execute
	 * @param return_type Return type of the function, used to format the output
	 * @return Formatted output string on success, error message on failure
	 */
	std::expected<std::string, std::string> executeFunctionAndCaptureResult(
		vm::PID pid, std::string_view func_name, const tsh::SymbolType<>& return_type
	);

}  // namespace compiler::repl
