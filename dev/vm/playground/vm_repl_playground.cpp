#include "lang_definitions/key_spec_op.hpp"
#include "lexer/classifications.hpp"

#include "base/exceptions.hpp"
#include "base/int_conv.hpp"

#include "vm/api/vm.hpp"

#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

/*
REPL - Quick overview:
--------------------------------------------------
>>> exit / q                        -> exits repl.
--------------------------------------------------
>>> {                           	-> starts reading code to inject.
                                    -> type valid DBC here.

}   								-> close code injection mode by closing the brace.
>>>
--------------------------------------------------
>>> [FUNC-NAME]([ARG0], [ARG1]) -> calls a function with specified parameters.
>>> foo(1, 2, 3)                -> currently only i64 arguments are supported.
--------------------------------------------------
>>> #[global_name] [type]		->  initializes a global value with the specified name.
--------------------------------------------------
>>> $[global_name]				-> outputs a value of a global with the specified name.
--------------------------------------------------
>>> ![opcode] 					-> loads a void function with just the specified opcode and executes
it.
--------------------------------------------------
>>> ![ 							-> loads a void function which performem specified operations and
executes it. init_lptr_any x, i64; mov_l64_g64 	x, global; output_l64		x; deinit;
]



*/

class DuckRepl {
public:
	DuckRepl() {
		init::InitObject _;
		auto             process_pid_response = vm::api::spawn();
		CORE_ASSERT(process_pid_response.has_value(), "Error: Failed to spawn a process");
		pid = process_pid_response->pid;
		CORE_ASSERT(vm::api::attach(pid, std::cin, std::cout), "Error: Attach failed\n");
	}

	void run() {
		std::cout << "++++++++++++++++++++++++\n+ DuckREPL has "
					 "started +\n++++++++++++++++++++++++\n";

		std::string line;
		while (true) {
			std::cout << ">>> ";
			if (!std::getline(std::cin, line)) {
				std::cout << "Exiting REPL (EOF reached) or error.\n";
				break;
			}
			std::string stripped_line = strip(line);
			if (stripped_line == "exit" || stripped_line == "q") {
				std::cout << "Exiting REPL\n";
				break;
			} else if (line == "{")
				processCodeInjection();
			else if (strip(stripped_line) == "![") {
				processExecuteInstructionList();
			} else if (lstrip(line).starts_with("#")) {
				processGlobalInitialization(line);
			} else if (lstrip(line).starts_with("$")) {
				processGlobalOutput(line);
			} else if (lstrip(line).starts_with("!")) {
				processExecuteInstruction(line);
			} else if (line.find('(') != std::string::npos && line.find(')') != std::string::npos
			           && line.find('(') < line.find(')')) {
				processFunctionCall(line);
			} else {
				std::cout << "Invalid input: \"" << line << "\"\n";
			}
		}
	}

private:
	struct CallInfo {
		std::string      func_name;
		std::vector<i64> func_args;
	};

	vm::PID pid{};
	u64     load_counter = 0;  // Maybe that should be a static var in loadOnVm().
	u64     step_counter = 0;

	static constexpr const std::string_view TEMP_FILE_NAME_PREFIX = "temp_repl_file_";
	static constexpr const std::string_view TEMP_FILE_NAME_SUFFIX = ".dbc";
	static constexpr const std::string_view TEMPLATE_EXEC_INSTRUCTION_FUNC
		= "type fun: step_{0} {{}} void\n"
		  "function step_{0} {{\n"
		  "		{1}\n"
		  "		ret;\n"
		  "}}";

	static constexpr const std::string_view TEMPLATE_GLOBAL_OUTPUT_FUNC
		= "type fun: step_{0} {{}} void\n"
		  "function step_{0} {{\n"
		  "		init_lany_type x, i64;\n"
		  "		mov_l64_g64 x, {1};\n"
		  "		output_l64 x;\n"
		  "		deinit;\n"
		  "		ret;\n"
		  "}}";
	static constexpr const std::string_view TEMPLATE_GLOBAL_INIT_FUNC = "global_data {} {};\n";

	// ============== HELPERS ==============
	std::string getCurrentFilePath() {
		return TEMP_FILE_NAME_PREFIX.data() + std::to_string(load_counter)
		     + TEMP_FILE_NAME_SUFFIX.data();
	}

	std::string strip(std::string string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	std::string lstrip(std::string string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		return string;
	}

	std::string rstrip(std::string string) {
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	std::string loadCodeLinesUntil(const std::string& until) {
		std::string function_code = "";
		std::string line;

		i64 brace_count = 1;
		while (brace_count > 0 && std::getline(std::cin, line)) {
			if (line == until && brace_count == 1) {
				brace_count--;
				break;
			}

			function_code += line + '\n';
			for (char c: line)
				if (c == '{')
					brace_count++;
				else if (c == '}')
					brace_count--;
		}
		return function_code;
	}

	CallInfo parseFunctionCallLine(const std::string& line) {
		u64 paren_open  = line.find('(');
		u64 paren_close = line.rfind(')');

		if (paren_open == std::string::npos || paren_close == std::string::npos
		    || paren_close <= paren_open) {
			std::cerr << "Error: Invalid function call -> ')' appeared before '('\n";
			return {};
		}

		std::string function_name = line.substr(0, paren_open);
		strip(function_name);

		std::string args_str = line.substr(paren_open + 1, paren_close - paren_open - 1);

		std::vector<std::string> arg_strings;
		u64                      start = 0;
		u64                      end   = args_str.find(',');
		while (end != std::string::npos) {
			std::string arg = args_str.substr(start, end - start);
			strip(arg);
			if (!arg.empty()) arg_strings.push_back(arg);
			start = end + 1;
			end   = args_str.find(',', start);
		}

		// TODO: Potential do while to remove that duplicated code.
		std::string last_arg = args_str.substr(start);
		strip(last_arg);
		if (!last_arg.empty()) arg_strings.push_back(last_arg);

		// Convert to i64.
		std::vector<i64> arguments;
		arguments.reserve(arg_strings.size());
		for (const auto& arg_str: arg_strings) {
			try {
				i64 arg = std::stoi(arg_str);
				arguments.push_back(arg);
			} catch (const std::exception& e) {
				std::cerr << "Error: Invalid argument '" << arg_str << "' - must be integer\n";
				return {};
			}
		}

		return { .func_name = function_name, .func_args = arguments };
	}

	void saveToTempFile(const std::string& content) {
		std::ofstream file(getCurrentFilePath());
		if (!file) {
			std::cerr << "Error: Could not create a temp file: " << TEMP_FILE_NAME_PREFIX << "\n";
			return;
		}
		file << content;
		file.close();
	}

	// ============== VM API Functions ==============
	void loadOnVm(const std::string& code) {
		saveToTempFile(code);
		fs::FilePath file(getCurrentFilePath());
		auto         load_files_response = vm::api::loadFiles(pid, { file });
		if (!load_files_response.has_value()) {
			auto err     = load_files_response.error();
			auto core_op = std::get<vm::api::CoreOperationError>(err);
			auto err_str = std::get<vm::api::LoadProgramError>(core_op).why;
			std::cout << "Error: Failed to load a file " << err_str << '\n';
		}

		std::error_code err_code;
		std::filesystem::remove(getCurrentFilePath(), err_code);
		if (err_code) {
			std::cout << "Error: Failed to remove a temporary file " << getCurrentFilePath() << ": "
					  << err_code.message() << '\n';
		}

		load_counter++;
	}

	i64 runOnVm(const std::string& func_name, const std::vector<i64>& func_args = {}) {
		CORE_ASSERT(
			vm::api::runFunction(pid, func_name, func_args).has_value(), "Failed to runFunction\n"
		);
		CORE_ASSERT(vm::api::join(pid).has_value(), "Error: Join failed\n");

		auto exit_code_response = vm::api::getExitCode(pid);
		CORE_ASSERT(exit_code_response.has_value(), "Error: Empty exit_code\n");
		return *exit_code_response;
	}

	// ============== Process User Requests ==============
	void processCodeInjection() {
		std::string function_code = loadCodeLinesUntil("}");
		loadOnVm(function_code);
	}

	void processGlobalInitialization(std::string& line) {
		std::string global_name = strip(std::string(line).substr(1, line.find(' ')));
		std::cerr << global_name << '\n';
		u64         comma_pos   = line.find(' ');
		std::string global_type = strip(line.substr(comma_pos + 1));

		std::string func_code = std::format(TEMPLATE_GLOBAL_INIT_FUNC, global_name, global_type);
		loadOnVm(func_code);
	}

	void processGlobalOutput(std::string& line) {
		std::string global_name = strip(line.substr(1));
		std::string func_code   = std::vformat(
            TEMPLATE_GLOBAL_OUTPUT_FUNC, std::make_format_args(step_counter, global_name)
        );
		loadOnVm(func_code);
		runOnVm(std::format("step_{}", step_counter));
		step_counter++;
	}

	void processExecuteInstruction(std::string& line) {
		std::string opcode    = strip(line.substr(1));
		std::string func_code = std::vformat(
			TEMPLATE_EXEC_INSTRUCTION_FUNC, std::make_format_args(step_counter, opcode)
		);
		loadOnVm(func_code);
		runOnVm("step_[STEP_COUNTER]");
		step_counter++;
	}

	void processExecuteInstructionList() {
		std::string opcodes   = loadCodeLinesUntil("]");
		std::string func_code = std::vformat(
			TEMPLATE_EXEC_INSTRUCTION_FUNC, std::make_format_args(step_counter, opcodes)
		);
		loadOnVm(func_code);
		runOnVm(std::format("step_{}", step_counter));
		step_counter++;
	}

	void processFunctionCall(const std::string& line) {
		CallInfo call_info = parseFunctionCallLine(line);
		i64      exit_code = runOnVm(call_info.func_name, call_info.func_args);
		std::cout << "\n" << call_info.func_name << " -> " << exit_code << '\n';
	}
};

int main() {
	DuckRepl repl;
	repl.run();
	return 0;
}
