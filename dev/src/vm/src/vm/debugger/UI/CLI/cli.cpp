#include "cli.hpp"

namespace {
	std::string strip(std::string& string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}
}

namespace vm::debugger::cli {
	namespace idv = vm::interpreted_data_variant;

	CLIDebugger::CLIDebugger(const std::vector<std::string>& main_args):
		  status_change_listener([this](const api::ProcStatus& status) {
			  std::visit(
				  [&](auto&& arg) {
					  using T = std::decay_t<decltype(arg)>;
					  std::cout << "New status: " << TypeParseTraits<T>::NAME.data() << std::endl;
				  },
				  status
			  );

		  }),
		  error_listener([this](const std::string& err){
				std::lock_guard lk(output_mutex);
				std::cout << "Error: " << err << std::endl; 
		  }),
		  exit_value_listener([this](const api::ExitValue& exit_val){
			std::lock_guard lk(output_mutex);
			for (auto val: exit_val) {
				if_opt_some (val->readData(), data) {
					variant_match (data) {
						variant_case (idv::Primitive, primitive) {
							std::cout << "VM returned: " << primitive.value << std::endl;
						}
					}
				}
			}
		  }),
		  debugger(main_args) {
		debugger.attachOnVMChangesStatusListener(status_change_listener);
		debugger.attachOnErrorListener(error_listener);
		debugger.attachOnVMCompletesExecutionListener(exit_value_listener);
	}
	
	CLIDebugger::CLIDebugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		CLIDebugger(main_args) {
			debugger.loadFile(filepath);
		};

	int CLIDebugger::run() {
		std::cout << "++++++++++++++++++++++++++++\n"
					 "+   Debugger has started   +\n"
					 "++++++++++++++++++++++++++++\n";

		std::string line;
		std::string stripped_line;

		while (true) {
			std::getline(std::cin, line);
			stripped_line = strip(line);

			if (stripped_line == "quit" || stripped_line == "q" || stripped_line == "exit") return 0;

			if (stripped_line == "run" || stripped_line == "r") {
				debugger.runMain();
			} else if (stripped_line == "pause" || stripped_line == "p") {
				debugger.pause();
			} else if (stripped_line == "continue" || stripped_line == "c" || stripped_line == "resume") {
				debugger.resume();
			} else if (stripped_line == "help" || stripped_line == "h") {
				help();
			} else if (stripped_line == "status" || stripped_line == "s") {
				status();
			}
		}

	}

	void CLIDebugger::help() {
		std::lock_guard lk(output_mutex);
		std::cout << "Commands:\n"
			"  (q)uit      - exit the debugger\n"
			"  (h)elp      - write this message\n"
			
			"  (r)un       - run main function\n"
			"  (p)ause     - pause running VM\n"
			"  (c)ontinue  - resume VM execution\n"

			"  (s)tatus    - write current VM status\n" << std::endl;
	}

	void CLIDebugger::status() {
		auto response = debugger.getStatus();
		std::visit(
			[&](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				std::lock_guard lk(output_mutex);
				std::cout << "Current status: " << TypeParseTraits<T>::NAME.data() << std::endl;
			},
			response
		);
	}
}