#include "cli.hpp"
#include "server.hpp"
#include "vm_repl.hpp"

#include <clap/clap.hpp>
#include <init/init.hpp>
#include <printer/stream_printer.hpp>

#include <vm/core/supervisor/supervisor.hpp>
#include <vm/core/thread/low_program/instruction.hpp>

#include <exception>

void showVersion() {
	std::cout << "VM version 0.0.\n";
	std::cout << "Configuration: \n";
	std::cout << vm::getInstructionConfig() << '\n';
}

clap::Clap getVmClap() {
	return clap::Clap("VM", "The Duckling Virtual Machine.")
	    .add(clap::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Shows version and config")
	             .build())
	    .add(clap::ParamBuilder::ofFlag()
	             .addLongName("stdlib")
	             .addShortDesc("When passed, loads standard library")
	             .build())
	    .setPreHandler([](const clap::ParsingResult& options) {
			if (options.isFlag("version")) showVersion();
		})
	    .addSubcommand(clap::Clap("server", "Launch DVM as a http server.")
	                       .add(clap::ParamBuilder::ofValue(clap::IntParser::make())
	                                .addShortName('p')
	                                .addLongName("port")
	                                .addShortDesc("Port to listen on.")
	                                .required()
	                                .build())
	                       .setHandler([](const clap::ParsingResult& options) -> int {
							   vm::Supervisor::get();
							   auto port = options.getValue<i64>("port").value();
							   server(i32(port));
							   return 0;
						   }))
	    .addSubcommand(clap::Clap("run", "Run a .qbc file.")
	                       .add(clap::ParamBuilder::ofValue(clap::FileParser::make())
	                                .addShortName('f')
	                                .addLongName("file")
	                                .addShortDesc("Path to the .dbc file to execute.")
	                                .optional()
	                                .build())
	                       .setHandler([](const clap::ParsingResult& options) {
							   vm::Supervisor::get();
							   if (auto file = options.getValue<fs::File>("file"))
								   return cli(file.value(), options.isFlag("stdlib"));
							   return cli(options.isFlag("stdlib"));
						   }))
	    .addSubcommand(clap::Clap("repl", "Start the VM in REPL mode.")
	                       .setHandler([](const clap::ParsingResult&) -> int {
							   vm::Supervisor::get();
							   DuckVMRepl::get().run();
							   return 0;
						   }));
}

int main(int argc, const char** argv) {
	init::InitObject _;
	auto             clap = getVmClap();

	try {
		return clap.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const base::Exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::RED },
			{ "DVM Exception was caught with message:\n", printer::Color::DEFAULT },
			{ e.what(), printer::Color::DEFAULT },
			{ "\nAborting\n", printer::Color::DEFAULT },
		});
		return 1;
	} catch (const std::exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::RED },
			{ "Unexpected Exception was caught with message:\n", printer::Color::DEFAULT },
			{ e.what(), printer::Color::DEFAULT },
			{ "\nAborting\n", printer::Color::DEFAULT },
		});
		return 1;
	} catch (...) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::RED },
			{ "Unexpected Exception not inheriting from std::exception was caught.\n",
		      printer::Color::DEFAULT },
		});
		return 1;
	}
}
