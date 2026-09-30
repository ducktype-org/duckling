#include "cli.hpp"

#include "common.hpp"

#include <diagnostic/highlight_positions.hpp>
#include <token_source/source.hpp>

namespace vm::debugger::cli {
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

		// @TODO: #3179 Add vm run -d flag and/or debugger command for explicite mapping loading
		clah::Clah cmds
			= clah::Clah("debug", "Debugger CLI Command Parser")
		          .addSubcommand(clah::Clah("exit", "exits the debugger")
		                             .setHandler([&](const clah::ParsingResult&) -> int {
										 exitMainLoop();
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
										 printNL("Current status: ", common::typeToString(status));
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
		          .addSubcommand(clah::Clah("select", "file targeted by operations such as `break`")
		                             .addPositional(clah::FileParser::make("file"), "File to select")
		                             .setHandler([&](const clah::ParsingResult& options) -> int {
										 auto file = options.getPositional<fs::File>(0);

										 if (!debugger.isFileAvailable(file)) {
											 printError("No such file in compiled or loaded code");
											 return 0;
										 }

										 selected_file = file;
										 printNL("Selected ", selected_file->name());
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


		events::Listener<std::string> error_listener([&](const std::string& err) {
			printError(err);
		});

		events::Listener<std::string> output_listener([&](const std::string& str) {
			print({ { str, printer::Color::BrightCyan } });
		});

		debugger.attachOnErrorListener(error_listener);
		debugger.attachOnOutputListener(output_listener);

		mainLoop(cmds);

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

	void CLIDebugger::printNL(const printer::PrinterContentsSeq& content) {
		std::stringstream stream;
		printer::StreamPrinter::print(content, stream);
		printer::StreamPrinter::newline(1, stream);
		print(stream.str());
	}

	void CLIDebugger::printError(const printer::PrinterContentsSeq& content) {
		std::stringstream stream;
		printer::StreamPrinter::print({ { "[Debug error]: ", printer::Color::BrightRed } }, stream);
		printer::StreamPrinter::print(content, stream);
		printer::StreamPrinter::newline(1, stream);
		print(stream.str());
	}
}
