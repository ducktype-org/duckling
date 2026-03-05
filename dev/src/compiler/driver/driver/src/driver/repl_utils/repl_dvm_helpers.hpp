#pragma once

#include <helios/hout/hout.hpp>

#include <query_framework/context/context.hpp>

#include <vm/core/process/interface_types.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace compiler::repl {

	/**
	 * @brief Compile a HOUT unit to DVM bytecode and load it into a running DVM process.
	 *
	 * @param ctx          Query context for compilation.
	 * @param hout_unit    The HOUT unit to compile.
	 * @param module_name  Used for identification and symbol resolution.
	 * @param pid          Process ID of the target DVM instance.
	 * @return Success or error message on failure.
	 */
	std::expected<void, std::string> compileAndLoad(
		query::Context&         ctx,
		const helios::HOUTUnit& hout_unit,
		std::string_view        module_name,
		vm::PID                 pid
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
