#pragma once

#include <helios/hout/hout.hpp>

#include <query_framework/context.hpp>

#include <vm/core/process/interface_types.hpp>

#include <expected>
#include <string>

namespace compiler::repl {

	/**
	 * @brief Output from evaluating a REPL expression on the DVM
	 */
	struct ExpressionResult final {
		std::string result_string;
	};

	/**
	 * @brief Compile HOUT unit to DVM bytecode and load it into a running DVM process
	 */
	std::expected<void, std::string> compileAndLoad(
		query::Context& ctx, const helios::HOUTUnit& hout_unit, vm::PID pid
	);

	/**
	 * @brief Execute a wrapped REPL expression on the DVM and capture its return value
	 *
	 * Runs a previously loaded function (typically a REPL expression wrapper) on the DVM,
	 * waits for completion, and extracts the return value as a formatted string based on
	 * the expression's type.
	 *
	 * @param pid Process ID of the target DVM instance
	 * @param func_name Mangled name of the function to execute
	 * @param return_type Return type of the expression, used to format the output
	 * @return ExpressionResult with formatted output on success, error message on failure
	 */
	std::expected<ExpressionResult, std::string> executeExpression(
		vm::PID pid, const std::string& func_name, const tsh::SymbolType<>& return_type
	);

}  // namespace compiler::repl
