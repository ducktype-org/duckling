#include "cli.hpp"
#include "server.hpp"
#include "vm_repl.hpp"
#include <vm/debugger/UI/debug_adapter/debug_adapter.hpp>

#include <clah/clah.hpp>
#include <init/init.hpp>
#include <logger/logger.hpp>
#include <printer/stream_printer.hpp>

#include <vm/core/supervisor/supervisor.hpp>
#include <vm/core/thread/low_program/instruction.hpp>

#include <exception>

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
	    .addSubcommand(clah::Clah("run", "Run a .dbc file.")
	                       .addPositional(clah::FileParser::make("file"))
	                       .setDefaultValueParser(clah::StringParser::make("program_argument"))
	                       .setHandler([](const clah::ParsingResult& options) {
							   vm::Supervisor::get();
							   auto                     file = options.getPositional<fs::File>(0);
							   std::vector<std::string> args;
							   args.reserve(options.getExtraParameterCount());
							   for (usize argc = 0; argc < options.getExtraParameterCount(); argc++)
								   args.push_back(*options.getExtra<std::string>(argc));

							   return cli(file, args);
						   }))
		.addSubcommand(clah::Clah("debug_adapter", "Start the VM debug adapter.")
	                       .addPositional(clah::FileParser::make("file"))
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   vm::Supervisor::get();
							   auto                     file = options.getPositional<fs::File>(0);
							   DebugAdapter::get(file).run();
							   return 0;
						   }))
	    .addSubcommand(clah::Clah("repl", "Start the VM in REPL mode.")
	                       .setHandler([](const clah::ParsingResult&) -> int {
							   vm::Supervisor::get();
							   DuckVMRepl::get().run();
							   return 0;
						   }));
}

int main(int argc, const char** argv) {
	init::InitObject _;
	auto             clah = getVmClah();

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
