#pragma once

#include <vm/api/vm.hpp>

#include <string>
#include <vector>

/*
REPL - Quick overview:
--------------------------------------------------
>>> exit / q                        -> exits repl.
--------------------------------------------------
>>> {                           	-> starts reading code to inject.
                                    -> type valid DBC here.
q									-> typing 'q' as line aborts the code injection mode and goes
back to the repl loop. }   								-> close code injection mode by closing the
brace.
>>>
--------------------------------------------------
>>> [FUNC-NAME]([ARG0], [ARG1], ...) 	-> calls a function with specified parameters.
>>> foo(1, 2, 3)                		-> currently only i64 arguments are supported.
--------------------------------------------------
>>> #[global_name] [type]		->  initializes a global value with the specified name.
--------------------------------------------------
>>> $[global_name]				-> outputs a value of a global with the specified name.
--------------------------------------------------
>>> ![opcode] 					-> loads a void function with just the specified opcode and executes
it.
--------------------------------------------------
>>> !{ 							-> loads a void function which performs specified operations and
executes it.
                                -> type valid dbc here.
q								-> typing 'q' as line aborts the code injection mode and goes back
to the repl loop.
}
*/
class DuckVMRepl {
public:
	static DuckVMRepl get();
	DuckVMRepl(DuckVMRepl&&)                 = delete;
	DuckVMRepl& operator=(DuckVMRepl&&)      = delete;
	DuckVMRepl(const DuckVMRepl&)            = delete;
	DuckVMRepl& operator=(const DuckVMRepl&) = delete;

	void run();

private:
	struct CallInfo {
		std::string      func_name;
		std::vector<i64> func_args;
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
	i64  runOnVm(const std::string& func_name, const std::vector<i64>& func_args = {});
	void loadAndRun(const std::string& code);

	// Process User Requests
	void processCodeInjection();
	void processGlobalInitialization(std::string& line);
	void processGlobalOutput(std::string& line);
	void processExecuteInstruction(std::string& line);
	void processExecuteInstructionList();
	void processFunctionCall(const std::string& line);
};
