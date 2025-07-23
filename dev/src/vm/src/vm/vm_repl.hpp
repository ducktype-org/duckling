#pragma once

#include <vm/api/vm.hpp>

#include <string>
#include <vector>

/**
 * @brief REPL interface for the VM.
 *
 * Provides an interactive shell that allows for:
 * - Loading and executing DBC code
 * - Managing global variables
 * - Executing individual instructions
 * - Calling functions with arguments
 *
 * The REPL supports several commands:
 * =========================================
 * >>> {			-> Direct code injection
 *					-> Type valid DBC here.
 * q				-> Type 'q' to abort the code injection mode.
 * }				-> Finish code injection by closing the curly brace.
 * =========================================
 * >>> !{			-> Instruction injection and immediate run.
 *					-> Type valid bytecode instructions here.
 * q				-> Type 'q' to abort the code injection mode.
 * }				-> Finish code injection by closing the curly brace.
 * =========================================
 * >>> !{instruction} 		-> Execute single instruction.
 * =========================================
 * >>> {func_name}({arg0}, {arg1}, ...) -> Call a specified function with arguments.
 * =========================================
 * >>> #{name} {type} 		-> Initialize a global variable with specified name and type.
 * =========================================
 * >>> ${name} 				-> Output a global variable with a specified name.
 * =========================================
 * @todo: Currently REPL only supports passing and returning arguments of type i64.
 * This should change after: https://github.com/ducktype-org/duckling/issues/776
 */
class DuckVMRepl {
public:
	static DuckVMRepl get();
	DuckVMRepl(DuckVMRepl&&)                 = delete;
	DuckVMRepl& operator=(DuckVMRepl&&)      = delete;
	DuckVMRepl(const DuckVMRepl&)            = delete;
	DuckVMRepl& operator=(const DuckVMRepl&) = delete;
	/**
	 * @brief Starts the REPL loop
	 *
	 * Reads commands from standard input and executes them until exit command is received or EOF is
	 * reached.
	 */
	void run();

private:
	struct CallInfo {
		std::string              func_name;
		vm::FunctionRunArguments func_args;
	};

	vm::PID pid{};
	u64     step_counter = 0;

	static constexpr std::string_view FORMAT_TEMP_FILE_NAME_PREFIX = "temp_repl_file_{}.dbc";
	static constexpr std::string_view FORMAT_EXEC_INSTRUCTION_FUNC
		= "type fun: step_{0} {{}} void\n"
		  "function step_{0} {{\n"
		  "     {1}\n"
		  "     ret;\n"
		  "}}";

	static constexpr std::string_view FORMAT_GLOBAL_OUTPUT_FUNC
		= "type fun: step_{0} {{}} void\n"
		  "function step_{0} {{\n"
		  "     init_lany_type x, i64;\n"
		  "     mov_l64_g64 x, {1};\n"
		  "     output_l64 x;\n"
		  "     deinit;\n"
		  "     ret;\n"
		  "}}";
	static constexpr std::string_view FORMAT_GLOBAL_INIT    = "global_data {} {};\n";
	static constexpr std::string_view FORMAT_STEP_FUNC_NAME = "step_{}";

	DuckVMRepl();
	// Helper methods
	[[nodiscard]] static std::string strip(std::string string);
	[[nodiscard]] static std::string lstrip(std::string string);
	[[nodiscard]] static std::string rstrip(std::string string);
	std::string                      loadCodeLinesUntil(const std::string& until);
	[[nodiscard]] CallInfo           parseFunctionCallLine(const std::string& line) const;

	// VM API Functions
	bool loadOnVm(const std::string& code);
	i64  runOnVm(const std::string& func_name, const vm::FunctionRunArguments& func_args = {});
	void loadAndRun(const std::string& code);

	// Process User Requests
	void processCodeInjection();
	void processGlobalInitialization(std::string& line);
	void processGlobalOutput(std::string& line);
	void processExecuteInstruction(std::string& line);
	void processExecuteInstructionList();
	void processFunctionCall(const std::string& line);
};
