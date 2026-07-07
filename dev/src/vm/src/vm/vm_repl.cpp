#include "vm_repl.hpp"

#include "core/process/interface_types.hpp"

#include <format>
#include <iostream>
#include <string>

namespace {
	Box<vm::VmValue> getIntVmValue(vm::PID pid, i64 value) {
		auto response = vm::api::getVmValue(pid, "i64");
		if (!response.has_value()) throw ReplFailedToCreateAVmValue();
		auto vm_value = std::move(response->vm_value);
		vm_value->writeBytes<i64>(value);
		return vm_value;
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

	/**
	 * @brief Create a list of references to owned VmValues which can be passed to the VM.
	 */
	vm::FunctionRunArguments createArgumentList(DuckVMRepl::OwnedArgumentList& arguments) {
		return arguments | std::views::transform([](auto& value) { return value.refMut(); })
		     | std::ranges::to<vm::FunctionRunArguments>();
	}

	void freeArguments(DuckVMRepl::OwnedArgumentList& arguments) {
		for (auto& arg: arguments) arg->freeData();
	}
}

DuckVMRepl DuckVMRepl::get() { return {}; }

DuckVMRepl::DuckVMRepl() {
	auto process_pid_response = vm::api::spawn();
	if (!process_pid_response.has_value()) throw ReplFailedToSpawnProcessException();
	pid = process_pid_response->pid;
	if (!vm::api::attach(pid, std::cin, std::cout)) throw ReplFailedToAttachStreamsException();
}

void DuckVMRepl::run() {
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

// ============== HELPERS ==============

std::string DuckVMRepl::loadCodeLinesUntil(const std::string& until) {
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

DuckVMRepl::CallInfo DuckVMRepl::parseFunctionCallLine(const std::string& line) const {
	u64 paren_open  = line.find('(');
	u64 paren_close = line.rfind(')');

	if (paren_open == std::string::npos || paren_close == std::string::npos
	    || paren_close <= paren_open) {
		std::cerr << "Error: Invalid function call -> ')' appeared before '('\n";
		return {};
	}

	std::string function_name = strip(line.substr(0, paren_open));
	std::string args_str      = line.substr(paren_open + 1, paren_close - paren_open - 1);

	u64               start = 0;
	OwnedArgumentList arguments;
	while (start < args_str.length()) {
		u64 end = args_str.find(',', start);

		// Last argument.
		if (end == std::string::npos) end = args_str.length();

		std::string arg = strip(args_str.substr(start, end - start));
		try {
			if (!arg.empty()) arguments.push_back(getIntVmValue(pid, std::stoll(arg)));
		} catch (const std::exception& e) {
			std::cerr << "Error: Invalid argument '" << arg << "' - must be integer\n";
			return {};
		}
		start = end + 1;
	}

	return { .func_name = function_name, .func_args = std::move(arguments) };
}

// ============== VM API Functions ==============
bool DuckVMRepl::loadOnVm(const std::string& code) {
	fs::File file                = fs::FileManager::createRandomTempFile(code);
	auto     load_files_response = vm::api::loadFiles(pid, { file });
	if (!load_files_response.has_value()) {
		auto err     = load_files_response.error();
		auto err_str = std::get<vm::api::LoadProgramError>(err).why;
		std::cout << "Error: Failed to load a file: " << err_str << "\n";
		return false;
	}
	return true;
}

i64 DuckVMRepl::runOnVm(const std::string& func_name, OwnedArgumentList& func_args) {
	if (!vm::api::runFunction(pid, func_name, createArgumentList(func_args)).has_value())
		throw ReplFailedToRunCodeException();
	if (!vm::api::join(pid).has_value()) throw ReplFailedToJoinProcessException();
	freeArguments(func_args);

	auto exit_code_response = vm::api::getExitValue(pid);
	if (!exit_code_response.has_value()) throw ReplEmptyExitCodeException();
	variant_match(exit_code_response.value()) {
		variant_case(i64, exit_value) { return exit_value; }
		variant_case(std::vector<Ref<vm::VmValue>>, values) {
			CORE_ASSERT(values.size() == 1, "REPL expects only one response value");
			if (values.at(0)->type->getName() != base::StrID("i64"))
				throw ReplWrongReturnTypeException();
			return values.at(0)->readBytes<i64>();
		}
	}
	CORE_UNREACHABLE();
}

void DuckVMRepl::loadAndRun(const std::string& code) {
	if (loadOnVm(code)) {
		OwnedArgumentList no_args;
		runOnVm(std::format(FORMAT_STEP_FUNC_NAME, step_counter), no_args);
		step_counter++;
	}
}

// ============== Process User Requests ==============
void DuckVMRepl::processCodeInjection() {
	std::string function_code = loadCodeLinesUntil("}");
	if (function_code == "") return;
	loadOnVm(function_code);
}

void DuckVMRepl::processGlobalInitialization(std::string& line) {
	std::string global_name = strip(std::string(line).substr(1, line.find(' ')));
	u64         comma_pos   = line.find(' ');
	std::string global_type = strip(line.substr(comma_pos + 1));

	std::string func_code = std::format(FORMAT_GLOBAL_INIT, global_name, global_type);
	loadOnVm(func_code);
}

void DuckVMRepl::processGlobalOutput(std::string& line) {
	std::string global_name = strip(line.substr(1));
	std::string func_code
		= std::vformat(FORMAT_GLOBAL_OUTPUT_FUNC, std::make_format_args(step_counter, global_name));
	loadAndRun(func_code);
}

void DuckVMRepl::processExecuteInstruction(std::string& line) {
	std::string opcode = strip(line.substr(1));
	std::string func_code
		= std::vformat(FORMAT_EXEC_INSTRUCTION_FUNC, std::make_format_args(step_counter, opcode));
	loadAndRun(func_code);
}

void DuckVMRepl::processExecuteInstructionList() {
	std::string opcodes = loadCodeLinesUntil("}");
	if (opcodes == "") return;
	std::string func_code
		= std::vformat(FORMAT_EXEC_INSTRUCTION_FUNC, std::make_format_args(step_counter, opcodes));
	loadAndRun(func_code);
}

void DuckVMRepl::processFunctionCall(const std::string& line) {
	CallInfo call_info    = parseFunctionCallLine(line);
	i64      return_value = runOnVm(call_info.func_name, call_info.func_args);
	std::cout << "\n" << call_info.func_name << " -> " << return_value << '\n';
}
