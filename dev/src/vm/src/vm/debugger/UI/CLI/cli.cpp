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
		if (v_matches(status, vm::api::ExecutionCompleted)) {
			const auto& exit_value = std::get<vm::api::ExecutionCompleted>(status).exit_value;
			if (v_matches(exit_value, std::vector<Ref<vm::IVMValue>>)) {
				for (auto val: std::get<std::vector<Ref<vm::IVMValue>>>(exit_value)) {
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
}

namespace vm::debugger::cli {
	namespace idv = interpreted_data_variant;

	CLIDebugger::CLIDebugger():
		  status_change_listener([&](const api::ProcStatus& status) {
			  printer::PrinterOStream out;
			  out << "New status: ";
			  printProcStatus(out, status);
			  printNL(out.getContents());
		  }),
		  error_listener([&](const std::string& err) { printError(err); }),
		  output_listener([&](const std::string& str) {
			  print({ { str, printer::Color::BrightCyan } });
		  }) {
		debugger.attachOnStatusChangedListener(status_change_listener);
		debugger.attachOnErrorListener(error_listener);
		debugger.attachOnOutputListener(output_listener);
	}

	std::expected<void, api::ApiError> CLIDebugger::load(const fs::File& file) {
		auto response = debugger.loadFiles({ file });
		if (!response) return std::unexpected(response.error());

		selected_file = file;
		return {};
	}

	std::expected<void, std::variant<api::ApiError, std::string>> CLIDebugger::loadDefault() {
		auto response = debugger.loadDefault();
		if (!response) return std::unexpected(response.error());

		if_opt_some(debugger.getMapper().mainFile(), main_filepath) selected_file
			= fs::File(main_filepath);

		return {};
	}

	void CLIDebugger::setProgramArguments(const ProgramRunArguments& args) {
		debugger.setProgramArguments(args);
	}

	int CLIDebugger::run() {
		if (!load_result) {
			printError("Failed to load file");

			variant_match(load_result.error()) {
				variant_case(api::LoadProgramError, load_error) { printError(load_error.why); }
				variant_default {
					std::visit(
						[&](auto&& arg) {
							using T = std::decay_t<decltype(arg)>;
							printError(TypeParseTraits<T>::NAME.data());
						},
						load_result.error()
					);
				}
			}

			return -1;
		}

		bool running = true;

		// @TODO: #3179 Add vm run -d flag and/or debugger command for explicite mapping loading
		// @TODO: #3180 Add possibility for switching selected file in debugger CLI
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
										 if (!response) printError("Run failed!");
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("pause", "pauses running VM")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 auto response = debugger.pause();
										 if (!response) printError("Pause failed!");
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("continue", "resumes VM execution")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 auto response = debugger.resume();
										 if (!response) printError("Continue failed!");
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
										 if (response)
											 printCodePosition(*response);
										 else
											 printNL("Failed to obtain position!");
										 return 0;
									 }))
		          .addSubcommand(clah::Clah("step", "executes one step")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 auto response = (selected_file
			                                              && debugger.getMapper().containsFile(
															  selected_file->getFilePath()
														  ))
			                                               ? debugger.mappedStep()
			                                               : debugger.step();

										 if (!response) {
											 printNL("Failed to obtain position!");
											 return 0;
										 }

										 match_optional(*response) {
											 opt_some(position) { printCodePosition(position); }
											 opt_none { printNL("Program has finished."); }
										 }

										 return 0;
									 }))
		          .addSubcommand(
					  clah::Clah("break", "sets or unsets the breakpoint")
						  .addPositional(
							  clah::CategoryParser::make(
								  "option", std::vector<std::string>{ "set", "del" }
							  ),
							  "Breakpoint operation. Possible values are: set, del."
						  )
						  .addPositional(clah::IntParser::make("line"), "Source line number.")
						  .setHandler([&](const clah::ParsingResult& options) -> int {
							  auto option = options.getPositional<std::string>(0);
							  auto line   = base::safeIntConv<usize>(options.getPositional<i64>(1));

							  if (!selected_file) {
								  printError("No selected file");
								  return 0;
							  }

							  bool enable = option == "set";
							  auto result
								  = debugger.setBreakpoint(selected_file.value(), line, enable);

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
								  printError("Modyfing breakpoint failed!");
							  }

							  return 0;
						  })
				  );

		specInit();
		printNL(
			"++++++++++++++++++++++++++++\n"
			"+   Debugger has started   +\n"
			"++++++++++++++++++++++++++++"
		);
		for (std::string line; running && getline(line); cmds.execute(strip(line)));
		printNL("Exiting debugger.");
		specExit();

		return 0;
	}

	void CLIDebugger::printCodePosition(const CodePosition& position) {
		printNL(
			"Function `", position.function_name.strView(), "` instruction ", position.instr_number
		);
		if_opt_some(position.source_position, sp) {
			printer::PrinterOStream out;
			dia::printHighlightedPositions(out, { sp }, 1);
			print(out.getContents());
		}
		if_opt_some(position.mapped_position, sp) {
			printer::PrinterOStream out;
			dia::printHighlightedPositions(out, { sp }, 1);
			print(out.getContents());
		}
	}
}
