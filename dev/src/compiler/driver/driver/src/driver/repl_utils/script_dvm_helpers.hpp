#pragma once

#include <vm/bytecode/bytecode.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace compiler::repl {
	/**
	 * Metadata for orchestrating execution of a compiled statement wrapper.
	 * Holds function_name and result_type, which makeDvmScriptMainFunction needs to emit
	 * correct call instructions (Op_init_lany_type for non-void returns).
	 */
	struct ScriptExecutableCall final {
		std::string function_name;
		base::StrID result_type_name;
	};

	/**
	 * Extract wrapper metadata from compiled statement chunk.
	 * Each statement wrapper compiles to a CodeCollection with exactly one function.
	 * Returns the wrapper's name + return type (needed for DVM main orchestrator generation).
	 *
	 * Why extract from DVM bytecode instead of HOUT:
	 * - HOUT types are semantic symbols (SymbolType<>), DVM types are string identifiers
	 * - During lowering, type names may be transformed to match DVM conventions
	 * - We need the type as represented in DVM (what Op_init_lany_type expects), not HOUT
	 */
	std::expected<ScriptExecutableCall, std::string> getDvmExecutableCallMetadata(
		const vm::code::CodeCollection& chunk, std::string_view wrapper_func_name
	);

	/**
	 * Build synthetic DVM entrypoint for script execution.
	 *
	 * Script statements are compiled into wrapper functions first. This function emits
	 * a single "main" that calls wrappers in the same order as they appeared in source,
	 * preserving script side-effect order.
	 *
	 * For non-void wrapper calls, the VM call validator requires a return-value slot to
	 * exist on stack before Op_call_func. We therefore emit:
	 * - Op_init_lany_type(temp, wrapper_result_type)
	 * - Op_call_func(wrapper)
	 * - Op_deinit()
	 *
	 * The synthetic main is required because vm::api::run executes "main", and bytecode
	 * validation requires that "main" has i64 return type. We finalize by writing 0 to
	 * ret_val and returning.
	 *
	 * Program execution path is vm::api::run -> VMProcess::doRequest(request::Run)
	 * -> runFunction("main", ...), so scripts must synthesize a callable "main" entry.
	 * The bytecode validator also enforces that "main" returns i64.
	 */
	vm::code::Function makeDvmScriptMainFunction(const std::vector<ScriptExecutableCall>& calls);

}  // namespace compiler::repl
