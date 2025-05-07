#include "base/exceptions.hpp"
#include "base/int_conv.hpp"

#include "vm/api/vm.hpp"
#include "lang_definitions/key_spec_op.hpp"
#include "lexer/classifications.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>

/*
REPL - Quick overview:
>>> exit                        -> exits repl.
>>> {                           -> starts reading code to inject.
...                             -> type valid DBC here.
...
... }                           -> close code injection mode by closing the brace.
>>> [FUNC-NAME]([ARG0], [ARG1]) -> calls a function with specified parameters.
>>> foo(1, 2, 3)                -> currently only i64 arguments are supported.
*/

class DuckRepl {
public:
	DuckRepl() {
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
			std::cout << ">>> ";
			if (!std::getline(std::cin, line)) {
				std::cout << "Exiting REPL (EOF reached) or error.\n";
				break;
			}

			if (line == "exit") {
				std::cout << "Exiting REPL\n";
				break;
			} else if (line == "{")
				processCodeInjection();
			else if (line.find('(') != std::string::npos && line.find(')') != std::string::npos
			         && line.find('(') < line.find(')')) {
				processFunctionCall(line);
			} else {
				std::cout << "Invalid input: \"" << line << "\"\n";
			}
		}
	}

private:
	vm::PID pid{};

	static constexpr const std::string_view TEMP_FILE_NAME = "temp_repl_file.dbc";

	void saveToTempFile(const std::string& content) {
		std::ofstream file(TEMP_FILE_NAME.data());
		if (!file) {
			std::cerr << "Error: Could not create a temp file: " << TEMP_FILE_NAME << "\n";
			return;
		}
		file << content;
		// file.write(content.c_str(), base::safeIntConv<std::streamsize>(content.size()));
		file.flush();
		file.close();
	}

	void processCodeInjection() {
		std::cout << "Processing Code Injection\n";
		std::string function_code = "";
		std::string line;

		i64 brace_count = 1;
		while (brace_count > 0 && std::getline(std::cin, line)) {
			if (line == "}" && brace_count == 1) {
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
		saveToTempFile(function_code);
        
		// std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		fs::FilePath file(TEMP_FILE_NAME.data());
		auto         load_files_response = vm::api::loadFiles(pid, { file });
		if (!load_files_response.has_value()) {
			auto err     = load_files_response.error();
			auto core_op = std::get<vm::api::CoreOperationError>(err);
			auto err_str = std::get<vm::api::LoadProgramError>(core_op).why;
			std::cout << "Error: Failed to load a file " << err_str << '\n';
		} else {
			std::cout << "Function loaded successfully\n";
		}

		std::error_code err_code;
		std::filesystem::remove(TEMP_FILE_NAME.data(), err_code);
		if (err_code) {
			std::cout << "Error: Failed to remove a temporary file " << TEMP_FILE_NAME << ": "
					  << err_code.message() << '\n';
		}
	}

	void strip(std::string& string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
	}

	void processFunctionCall(const std::string& line) {
		std::cout << "Processing function call\n";
		std::cout << line << '\n';

		size_t paren_open  = line.find('(');
		size_t paren_close = line.rfind(')');
		std::cout << paren_open << " " << paren_close << '\n';

		if (paren_open == std::string::npos || paren_close == std::string::npos
		    || paren_close <= paren_open) {
			std::cerr << "Error: Invalid function call -> ')' appeared before '('\n";
			return;
		}

		std::string function_name = line.substr(0, paren_open);
		strip(function_name);

		std::string args_str = line.substr(paren_open + 1, paren_close - paren_open - 1);
		std::cout << "Function name: \"" << function_name << "\"\n";
		std::cout << "Function args: \"" << args_str << "\"\n";

		std::vector<std::string> arg_strings;
		size_t                   start = 0;
		size_t                   end   = args_str.find(',');

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

		std::cout << "Parsed args: \n";
		for (const auto& arg: arg_strings) std::cout << "\"" << arg << "\"" << '\n';
		std::cout << "==================\n";

		// Convert to i64.
		std::vector<i64> arguments;
		arguments.reserve(arg_strings.size());
		for (const auto& arg_str: arg_strings) {
			try {
				i64 arg = std::stoi(arg_str);
				arguments.push_back(arg);
			} catch (const std::exception& e) {
				std::cerr << "Error: Invalid argument '" << arg_str << "' - must be integer\n";
				return;
			}
		}

		std::cout << "Function '" << function_name << "' called with " << arguments.size()
				  << " arguments\n";
		CORE_ASSERT(
			vm::api::runFunction(pid, function_name, arguments).has_value(),
			"Failed to runFunction\n"
		);
		CORE_ASSERT(vm::api::join(pid).has_value(), "Error: Join failed\n");

		auto exit_code_response = vm::api::getExitCode(pid);
		CORE_ASSERT(exit_code_response.has_value(), "Error: Empty exit_code\n");
		std::cout << "-> " << *exit_code_response << '\n';
	}
};

int main() {
	init::InitObject _;
	
	DuckRepl repl;
	repl.run();
	return 0;
}
