#include "cli.hpp"

#include <clah/clah.hpp>
#include <diagnostic/highlight_positions.hpp>
#include <token_source/source.hpp>

namespace {
	std::string strip(std::string& string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	template<typename T>
	std::string typeToString(const T& status) {
		return std::visit(
			[&](auto&& arg) {
				using TT = std::decay_t<decltype(arg)>;
				return TypeParseTraits<TT>::NAME.data();
			},
			status
		);
	}

	void printProcStatus(printer::PrinterOStream& os, const vm::api::ProcStatus& status) {
		os.add(printer::PrinterContent(typeToString(status)));
		if (std::holds_alternative<vm::api::ExecutionCompleted>(status)) {
			for (auto val: std::get<vm::api::ExecutionCompleted>(status).exit_value) {
				if_opt_some(val->readData(), data) {
					variant_match(data) {
						variant_case(vm::interpreted_data_variant::Primitive, primitive) {
							os << " (return value = " << std::to_string(primitive.value) << ")";
						}
					}
				}
			}
		}
	}
}

namespace vm::debugger::cli {
	namespace idv = interpreted_data_variant;

	CLIDebugger::CLIDebugger(const std::vector<std::string>& main_args):
		  status_change_listener([&](const api::ProcStatus& status) {
			  printer::PrinterOStream out;
			  out << "New status: ";
			  printProcStatus(out, status);
			  printNL(out);
		  }),
		  error_listener([&](const std::string& err) { printNL("Error: ", err); }),
		  debugger(main_args) {
		debugger.attachOnStatusChangedListener(status_change_listener);
		debugger.attachOnErrorListener(error_listener);
	}

	CLIDebugger::CLIDebugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		  CLIDebugger(main_args) {
		load_result   = debugger.loadFile(filepath);
		selected_file = filepath;
	}

	int CLIDebugger::run() {
		if (!load_result) {
			printNL("Failed to load file");

			variant_match(load_result.error()) {
				variant_case(api::LoadProgramError, load_error) {
					printNL("Error: ", load_error.why);
				}
				variant_default {
					std::visit(
						[&](auto&& arg) {
							using T = std::decay_t<decltype(arg)>;
							printNL("Error: ", TypeParseTraits<T>::NAME.data());
						},
						load_result.error()
					);
				}
			}

			return -1;
		}

		bool running = true;

		clah::Clah cmds
			= clah::Clah("debug", "Debugger CLI Command Parser")
		          .addSubcommand(clah::Clah("exit", "exits the debugger")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 running = false;
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("run", "runs main function")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 auto response = debugger.runMain();
										 if (!response) printNL("Run failed!");
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("pause", "pauses running VM")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 auto response = debugger.pause();
										 if (!response) printNL("Pause failed!");
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("continue", "resumes VM execution")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 auto response = debugger.resume();
										 if (!response) printNL("Continue failed!");
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("status", "writes current VM status")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 auto status = debugger.getStatus();
										 printNL("Current status: ", typeToString(status));
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("position", "writes current position")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 auto response = debugger.getCurrentPosition();
										 if (response) {
											 auto pos = response.value();
											 printNL(
												 "Function `",
												 pos.function_name.strView(),
												 "` instruction ",
												 pos.instr_number
											 );
											 if_opt_some(pos.source_position, sp) {
												 printer::PrinterOStream out;
												 dia::printHighlightedPositions(out, { sp }, 1);
												 print(out);
											 }
										 } else {
											 printNL("Faild to obtain position!");
										 }
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("step", "executes one step")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 debugger.step();
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("break", "sets or unsets the breakpoint")
		                             .addPositional(clah::CategoryParser::make(
										 "option", std::vector<std::string>{ "set", "del" }
									 ))
		                             .addPositional(clah::IntParser::make("line"))
		                             .setHandler([&](const clah::ParsingResult& options) -> int {
										 auto option = options.getPositional<std::string>(0);
										 auto line   = options.getPositional<usize>(1);

										 if (!selected_file) {
											 printNL("No selected file");
											 return 0;
										 }

										 bool enable = option == "set";
										 auto result = debugger.setBreakpoint(
											 selected_file.value(), line, enable
										 );

										 if (result) {
											 printNL(
												 "Breakpoint in ",
												 selected_file->name(),
												 " line ",
												 line,
												 " ",
												 (enable ? "set." : "unset.")
											 );
										 } else {
											 printNL("Modyfing breakpoint failed!");
										 }

										 return 0;
									 }));

		printNL(
			"++++++++++++++++++++++++++++\n"
			"+   Debugger has started   +\n"
			"++++++++++++++++++++++++++++"
		);

		for (std::string line; running && std::getline(std::cin, line); cmds.execute(strip(line)));

		printNL("Exiting debugger.");
		return 0;
	}

	void CLIDebugger::print(const printer::PrinterOStream& content) {
		std::lock_guard lk(output_mutex);
		printer::StreamPrinter::print(content.getContents(), std::cout);
	}

	void CLIDebugger::printNL(const printer::PrinterOStream& content) {
		std::lock_guard lk(output_mutex);
		printer::StreamPrinter::print(content.getContents(), std::cout);
		printer::StreamPrinter::newline(1, std::cout);
	}
}
