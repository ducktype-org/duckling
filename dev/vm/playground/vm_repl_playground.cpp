#include "filesystem/file.hpp"

#include <base/exceptions.hpp>

#include <vm/api/vm.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
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
executes it. init_lptr_any x, i64; mov_l64_g64 	x, global; output_l64		x; deinit;
}
*/


class DuckVMRepl {
public:
	DuckVMRepl() {
		auto process_pid_response = vm::api::spawn();
		CORE_ASSERT(process_pid_response.has_value(), "Error: Failed to spawn a process");
		pid = process_pid_response->pid;
		CORE_ASSERT(vm::api::attach(pid, std::cin, std::cout), "Error: Attach failed\n");
	}

	void run() {
		std::cout << "++++++++++++++++++++++++\n+ DuckREPL has "
					 "started +\n++++++++++++++++++++++++\n";

		std::string line;
		while (true) {
			std::cout << "\n>>> ";
			if (!std::getline(std::cin, line)) {
				std::cout << "Exiting REPL (EOF reached) or error.\n";
				break;
			}
			std::string stripped_line = strip(line);
			if (line == "") {
				continue;
			} else if (stripped_line == "exit" || stripped_line == "q") {
				std::cout << "Exiting REPL\n";
				break;
			} else if (line == "{") {
				processCodeInjection();
			} else if (strip(stripped_line) == "!{") {
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
	static constexpr std::string_view FORMAT_TEMP_FILE_NAME_PREFIX = "temp_repl_file_{}.dbc";
	static constexpr std::string_view FORMAT_EXEC_INSTRUCTION_FUNC
		= "type fun: step_{0} {{}} void\n"
		  "function step_{0} {{\n"
		  "		{1}\n"
		  "		ret;\n"
		  "}}";

	static constexpr std::string_view FORMAT_GLOBAL_OUTPUT_FUNC
		= "type fun: step_{0} {{}} void\n"
		  "function step_{0} {{\n"
		  "		init_lany_type x, i64;\n"
		  "		mov_l64_g64 x, {1};\n"
		  "		output_l64 x;\n"
		  "		deinit;\n"
		  "		ret;\n"
		  "}}";
	static constexpr std::string_view FORMAT_GLOBAL_INIT    = "global_data {} {};\n";
	static constexpr std::string_view FORMAT_STEP_FUNC_NAME = "step_{}";

	// ============== HELPERS ==============
	[[nodiscard]] std::string getCurrentTempFilePath() const {
		return std::format(FORMAT_TEMP_FILE_NAME_PREFIX, load_counter);
	}

	[[nodiscard]] std::string strip(std::string string) const {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	[[nodiscard]] std::string lstrip(std::string string) const {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		return string;
	}

	[[nodiscard]] std::string rstrip(std::string string) const {
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	std::string loadCodeLinesUntil(const std::string& until) {
		std::string function_code = "";
		std::string line;

		i64 brace_count = 1;
		while (brace_count > 0) {
			std::getline(std::cin, line);
			if (line == "q")
				return "";
			else if (line == until && brace_count == 1) {
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

	CallInfo parseFunctionCallLine(const std::string& line) const {
		u64 paren_open  = line.find('(');
		u64 paren_close = line.rfind(')');

		if (paren_open == std::string::npos || paren_close == std::string::npos
		    || paren_close <= paren_open) {
			std::cerr << "Error: Invalid function call -> ')' appeared before '('\n";
			return {};
		}

		std::string function_name = strip(line.substr(0, paren_open));
		std::string args_str      = line.substr(paren_open + 1, paren_close - paren_open - 1);

		u64              start = 0;
		std::vector<i64> arguments;
		while (start < args_str.length()) {
			u64 end = args_str.find(',', start);

			// Last argument.
			if (end == std::string::npos) end = args_str.length();

			std::string arg = strip(args_str.substr(start, end - start));
			try {
				if (!arg.empty()) arguments.push_back(std::stoi(arg));
			} catch (const std::exception& e) {
				std::cerr << "Error: Invalid argument '" << arg << "' - must be integer\n";
				return {};
			}
			start = end + 1;
		}

		return { .func_name = function_name, .func_args = arguments };
	}

	void saveToTempFile(const std::string& content) {
		std::ofstream file(getCurrentTempFilePath());
		if (!file) {
			std::cerr << "Error: Could not create a temp file: " << getCurrentTempFilePath()
					  << "\n";
			return;
		}
		file << content;
		file.close();
	}

	// ============== VM API Functions ==============
	bool loadOnVm(const std::string& code) {
		bool bad = false;
		saveToTempFile(code);
		fs::FilePath file(getCurrentTempFilePath());
		auto         load_files_response = vm::api::loadFiles(pid, { file });
		if (!load_files_response.has_value()) {
			auto err     = load_files_response.error();
			auto core_op = std::get<vm::api::CoreOperationError>(err);
			auto err_str = std::get<vm::api::LoadProgramError>(core_op).why;
			std::cout << "Error: Failed to load a file\n";
			bad = true;
		}

		std::error_code err_code;
		std::filesystem::remove(getCurrentTempFilePath(), err_code);
		if (err_code) {
			std::cout << "Error: Failed to remove a temporary file " << getCurrentTempFilePath()
					  << ": " << err_code.message() << '\n';
		}
		load_counter++;
		return !bad;
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

	void loadAndRun(const std::string& code) {
		if (loadOnVm(code)) {
			runOnVm(std::format(FORMAT_STEP_FUNC_NAME, step_counter));
			step_counter++;
		}
	}

	// ============== Process User Requests ==============
	void processCodeInjection() {
		std::string function_code = loadCodeLinesUntil("}");
		if (function_code == "") return;
		loadOnVm(function_code);
	}

	void processGlobalInitialization(std::string& line) {
		std::string global_name = strip(std::string(line).substr(1, line.find(' ')));
		u64         comma_pos   = line.find(' ');
		std::string global_type = strip(line.substr(comma_pos + 1));

		std::string func_code = std::format(FORMAT_GLOBAL_INIT, global_name, global_type);
		loadOnVm(func_code);
	}

	void processGlobalOutput(std::string& line) {
		std::string global_name = strip(line.substr(1));
		std::string func_code   = std::vformat(
            FORMAT_GLOBAL_OUTPUT_FUNC, std::make_format_args(step_counter, global_name)
        );
		loadAndRun(func_code);
	}

	void processExecuteInstruction(std::string& line) {
		std::string opcode    = strip(line.substr(1));
		std::string func_code = std::vformat(
			FORMAT_EXEC_INSTRUCTION_FUNC, std::make_format_args(step_counter, opcode)
		);
		loadAndRun(func_code);
	}

	void processExecuteInstructionList() {
		std::string opcodes = loadCodeLinesUntil("}");
		if (opcodes == "") return;
		std::string func_code = std::vformat(
			FORMAT_EXEC_INSTRUCTION_FUNC, std::make_format_args(step_counter, opcodes)
		);
		loadAndRun(func_code);
	}

	void processFunctionCall(const std::string& line) {
		CallInfo call_info    = parseFunctionCallLine(line);
		i64      return_value = runOnVm(call_info.func_name, call_info.func_args);
		std::cout << "\n" << call_info.func_name << " -> " << return_value << '\n';
	}
};

int main() {
	init::InitObject _;
	DuckVMRepl       repl;
	repl.run();
	return 0;
}
