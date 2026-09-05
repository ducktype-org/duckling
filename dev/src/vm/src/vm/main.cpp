#include "cli.hpp"
#include "server.hpp"

#include <clah/clah.hpp>
#include <clah/clah_class.hpp>
#include <clah/param_builder.hpp>
#include <clah/value_parser.hpp>
#include <init/init.hpp>
#include <logger/logger.hpp>
#include <printer/stream_printer.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/supervisor/supervisor.hpp>
#include <vm/debugger/UI/CLI/cli.hpp>
#include <vm/debugger/UI/debug_adapter/debug_adapter.hpp>

#include <exception>
#include <expected>

void showVersion() {
	std::cout << "VM version 0.0.\n";
	std::cout << "Configuration: \n";
	std::cout << vm::getInstructionConfig() << '\n';
}

clah::Clah getVmClah() {
	return clah::Clah("VM", "The Duckling Virtual Machine.")
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Shows version and config")
	             .build())
	    .setPreHandler([](const clah::ParsingResult& options) {
			if (options.isFlag("version")) {
				showVersion();
				throw clah::exceptions::SuccessExitException(options);
			}
		})
#ifdef BUILD_TYPE_DEV_DEBUG
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('d')
	             .addLongName("debug-logs")
	             .addShortDesc("Enables DVM debug logs.")
	             .build())
	    .setPreHandler([](const clah::ParsingResult& options) {
			if (options.isFlag("debug-logs")) {
				std::cerr << "Debug logs enabled.\n";
				logger::enable_dev_logs = true;
				logger::enableDevCategory(logger::DevLogCategories::DVM);
				logger::enableDevCategory(logger::DevLogCategories::DVMDetails);
				logger::setDevLogOutputStreamCurrentDate();
			}
		})
#endif
	    .addSubcommand(clah::Clah("server", "Launch DVM as a http server.")
	                       .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	                                .addShortName('p')
	                                .addLongName("port")
	                                .addShortDesc("Port to listen on.")
	                                .required()
	                                .build())
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   vm::Supervisor::get();
							   auto port = options.getValue<i64>("port").value();
							   server(i32(port));
							   return 0;
						   }))
	    .addSubcommand(
			clah::Clah("run", "Starts VM in CLI mode")
				.setDefaultValueParser(clah::FileParser::make("dbc file", std::regex(".*\\.dbc")))
				.addCustomVerification(
					[](const clah::ParsingResult& parsed) -> clah::VerificationResult {
						if (parsed.getExtraParameterCount() == 0) {
							if (!parsed.isFlag("debug"))
								return std::unexpected<std::string>("Need at least one file");
						} else if (parsed.getExtraParameterCount() != 1 && parsed.isFlag("debug"))
							// @TODO: #3020 Add support for multi-file debugging
							return std::unexpected<std::string>(
								"Debugger currently supports only one file"
							);

						if (parsed.isFlag("debug") && parsed.isFlag("fast-mode"))
							return std::unexpected<std::string>(
								"Debugger does not support fast-mode"
							);

						// @TODO: #3077 Support --ffi-lib in the debugger load path.
						if (parsed.isFlag("debug")
		                    && !parsed.getValue<std::vector<std::string>>("ffi-lib")
		                            .copyValueOr({})
		                            .empty())
							return std::unexpected<std::string>(
								"Debugger does not support --ffi-lib yet"
							);

						return {};
					}
				)
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("fast-mode")
	                     .addShortDesc(
							 "Fast mode for the VM, which does not perform certain runtime checks."
						 )
	                     .build())
#ifndef ENABLE_JIT
				// JIT and debugger are conflicting due to common usage of LowVMProgramCopy,
	            // with both JIT and debugger assuming exclusive control of program copy opcode
	            // modification.
				.add(clah::ParamBuilder::ofFlag()
	                     .addShortName('d')
	                     .addLongName("debug")
	                     .addShortDesc("Start the VM CLI debugger")
	                     .build())
#endif  // ENABLE_JIT
				.add(clah::ParamBuilder::ofValue(
						 clah::StringListParser::make("args", clah::StringParser::make())
				)
	                     .addShortDesc(
							 R"(Program arguments. To pass arguments such as "hello -n 5", enter them as a comma-separated list: "hello,-n,5".)"
						 )
	                     .addShortName('c')
	                     .addLongName("args")
	                     .build())
				.add(clah::ParamBuilder::ofValue(
						 clah::StringListParser::make("libs", clah::StringParser::make())
				)
	                     .addShortDesc(
							 R"(Shared libraries for `ffi function` symbol resolution, as a comma-separated list. A bare name (e.g. "libm.so.6") is searched in the system library paths, a path is loaded as given.)"
						 )
	                     .addShortName('l')
	                     .addLongName("ffi-lib")
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					vm::Supervisor::get();

					std::vector<fs::File> source_files;
					source_files.reserve(options.getExtraParameterCount());
					for (usize i = 0; i < options.getExtraParameterCount(); i++)
						source_files.push_back(*options.getExtra<fs::File>(i));

					std::vector<std::string> args
						= options.getValue<std::vector<std::string>>("args").copyValueOr({});

					std::vector<std::string> ffi_libs
						= options.getValue<std::vector<std::string>>("ffi-lib").copyValueOr({});

					vm::api::ProcessConfig process_options{};
					if (options.isFlag("fast-mode"))
						process_options.mode = vm::api::ProcessMode::Fast;

					if (options.isFlag("debug")) {
						auto debugger = vm::debugger::cli::CLIDebugger();

						auto result = source_files.size() ? debugger.load(source_files[0])
			                                              : debugger.loadDefault();
						if (!result) {
							std::string error_string;
							variant_match(result.error()) {
								variant_case(vm::api::ApiError, error) {
									error_string = vm::api::errorToString(error);
								}
								variant_case(std::string, error) { error_string = error; }
							}

							printer::StreamPrinter::print({
								{ "[ERROR] ", printer::Color::Red },
								{ "Loading file failed with message:\n", printer::Color::Default },
								{ error_string, printer::Color::Default },
								{ "\nAborting\n", printer::Color::Default },
							});

							return 1;
						}

						debugger.setProgramArguments(args);

						return debugger.run();
					} else
						return cli(source_files, args, process_options, ffi_libs);
				})
		)
#ifndef ENABLE_JIT
	    .addSubcommand(clah::Clah("debug_adapter", "Start the VM debug adapter.")
	                       .setHandler([](const clah::ParsingResult&) -> int {
							   vm::Supervisor::get();
							   vm::debugger::debug_adapter::DebugAdapter::get().run();
							   return 0;
						   }))
#endif  // ENABLE_JIT
		;
}

int main(int argc, const char** argv) {
	init::InitObject _;
	auto             clah = getVmClah();

	// NOLINTNEXTLINE(concurrency-mt-unsafe) - runs before any thread is spawned
	if (const char* unbuffered = std::getenv("DUCK_VM_UNBUFFERED")) {
		if (std::string_view(unbuffered) == "1") std::cout << std::unitbuf;
	}

	try {
		return clah.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const base::Exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "DVM Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	} catch (const std::exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	} catch (...) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception not inheriting from std::exception was caught.\n",
		      printer::Color::Default },
		});
		return 1;
	}
}
