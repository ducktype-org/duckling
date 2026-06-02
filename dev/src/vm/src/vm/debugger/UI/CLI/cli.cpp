#include "cli.hpp"

#include <token_source/source.hpp>

namespace {
	std::string strip(std::string& string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

}

namespace vm::debugger::cli {
	namespace idv = interpreted_data_variant;

	CLIDebugger::CLIDebugger(const std::vector<std::string>& main_args):
		  status_change_listener([&](const api::ProcStatus& status) {
			  variant_match(status) {
				  variant_case(api::ExecutionCompleted, completed) {
					  std::lock_guard lk(output_mutex);
					  std::cout << "VM completed execution.\n";
					  for (auto val: completed.exit_value) {
						  if_opt_some(val->readData(), data) {
							  variant_match(data) {
								  variant_case(idv::Primitive, primitive) {
									  std::cout << "VM returned: " << primitive.value << "\n";
								  }
							  }
						  }
					  }
				  }
				  variant_default {
					  std::visit(
						  [&](auto&& arg) {
							  using T = std::decay_t<decltype(arg)>;
							  std::lock_guard lk(output_mutex);
							  std::cout << "New status: " << TypeParseTraits<T>::NAME.data()
										<< "\n";
						  },
						  status
					  );
				  }
			  }
		  }),
		  error_listener([&](const std::string& err) {
			  std::lock_guard lk(output_mutex);
			  std::cout << "Error: " << err << "\n";
		  }),
		  debugger(main_args) {
		debugger.attachOnStatusChangedListener(status_change_listener);
		debugger.attachOnErrorListener(error_listener);
	}

	CLIDebugger::CLIDebugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		  CLIDebugger(main_args) {
		load_result = debugger.loadFile(filepath);
	}

	int CLIDebugger::run() {
		if (!load_result) {
			std::cout << "Failed to load file\n";

			variant_match(load_result.error()) {
				variant_case(api::LoadProgramError, load_error) {
					std::cout << "Error: " << load_error.why << "\n";
				}
				variant_default {
					std::visit(
						[&](auto&& arg) {
							using T = std::decay_t<decltype(arg)>;
							std::cout << "Error: " << TypeParseTraits<T>::NAME.data() << "\n";
						},
						load_result.error()
					);
				}
			}

			return -1;
		}

		std::cout << "++++++++++++++++++++++++++++\n"
					 "+   Debugger has started   +\n"
					 "++++++++++++++++++++++++++++\n";

		std::string line;
		std::string stripped_line;

		while (std::getline(std::cin, line)) {
			stripped_line = strip(line);

			// @TODO: #2774 Streamline the process of adding commands

			if (stripped_line == "quit" || stripped_line == "q" || stripped_line == "exit") {
				std::lock_guard lk(output_mutex);
				std::cout << "Exiting debugger...\n\n";
				return 0;
			}

			if (stripped_line == "run" || stripped_line == "r") {
				debugger.runMain().transform_error([&](const api::ApiError& api_error) {
					std::lock_guard lk(output_mutex);
					std::cout << "Run failed...\n";
					return api_error;
				});
			} else if (stripped_line == "pause" || stripped_line == "p") {
				auto response
					= debugger.pause().transform_error([&](const api::ApiError& api_error) {
						  std::lock_guard lk(output_mutex);
						  std::cout << "Pause failed...\n";
						  return api_error;
					  });
			} else if (stripped_line == "continue" || stripped_line == "c"
			           || stripped_line == "resume") {
				debugger.resume().transform_error([&](const api::ApiError& api_error) {
					std::lock_guard lk(output_mutex);
					std::cout << "Resume failed...\n";
					return api_error;
				});
			} else if (stripped_line == "help" || stripped_line == "h") {
				help();
			} else if (stripped_line == "status" || stripped_line == "s") {
				status();
			} else if (stripped_line == "position" || stripped_line == "pos") {
				auto response = debugger.getCurrentPosition();
				position();
			}
		}

		return -1;
	}

	void CLIDebugger::help() {
		std::lock_guard lk(output_mutex);
		// @TODO: #2774 Streamline the process of generating help message
		std::cout << "Commands:\n"
					 "  (q)uit      - exit the debugger\n"
					 "  (h)elp      - write this message\n"

					 "  (r)un       - run main function\n"
					 "  (p)ause     - pause running VM\n"
					 "  (c)ontinue  - resume VM execution\n"

					 "  (s)tatus    - write current VM status\n"
					 "  (pos)ition  - write current position\n"
				  << "\n";
	}

	void CLIDebugger::status() {
		auto response = debugger.getStatus();
		std::visit(
			[&](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				std::lock_guard lk(output_mutex);
				std::cout << "Current status: " << TypeParseTraits<T>::NAME.data() << "\n";
			},
			response
		);
	}

	void CLIDebugger::position() {
		auto response = debugger.getCurrentPosition();
		if (response.has_value()) {
			auto            pos = response.value();
			std::lock_guard lk(output_mutex);
			std::cout << "In instruction " << pos.instr_number << " of function "
					  << pos.function_name.strView() << "\n";
			if (pos.source_position.has_value()) {
				auto src   = pos.source_position.value();
				auto start = src.getStartLineColumn();
				std::cout << src.getSource()->getFile().getFilePath().strView() << ":"
						  << start.first << ":" << start.second << "\nline " << start.first << ": "
						  << src.content() << "\n";
			}
		}
	}
}
