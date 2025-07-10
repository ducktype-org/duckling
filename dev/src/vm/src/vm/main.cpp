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
	return clap::Clap("dvm", "The Duckling Virtual Machine.")
	    .addHelpFlag()
	    .addGlobalParameter(
			clap::ParamBuilder::ofFlag()
				.addShortName('v')
				.addLongName("version")
				.addShortDesc("Shows version and config")
				.build()
		)
	    .addGlobalParameter(
			clap::ParamBuilder::ofFlag()
				.addLongName("stdlib")
				.addShortDesc("When passed, loads standard library")
				.build()
		)
	    .setPreHandler([](const clap::ParsingResult& options) {
			if (options.isFlag("version")) {
				showVersion();
				throw clap::exceptions::VersionException(options);
			}
		})
	    .addSubcommand(
			clap::Command("server", "Launch DVM as a http server.")
				.add(
					clap::ParamBuilder::ofValue(clap::IntParser::make())
						.addShortName('p')
						.addLongName("port")
						.addShortDesc("Port to listen on.")
						.required()
						.build()
				)
				.setHandler([](const clap::ParsingResult& options) -> int {
					vm::Supervisor::get();
					auto port = options.getValue<i64>("port").value();
					server(i32(port));
					return 0;
				})
		)
	    .addSubcommand(
			clap::Command("run", "Run a .qbc file.")
				.addPositional(clap::FileParser::make())
				.add(
					// TODOP: Maybe this file should be positional?
					clap::ParamBuilder::ofValue(clap::FileParser::make())
						.addShortName('f')
						.addLongName("file")
						.addShortDesc("Path to the .dbc file to execute.")
						.optional()
						.build()
				)
				.setHandler([](const clap::ParsingResult& options) {
					vm::Supervisor::get();
					if (auto file = options.getValue<fs::FilePath>("file"))
						return cli(file.value(), options.isFlag("stdlib"));
					return cli(options.isFlag("stdlib"));
				})
		)
	    .addSubcommand(
			clap::Command("repl", "Start the VM in REPL mode.")
				.addPositional(clap::FileParser::make())
				// TODOP: Maybe REPL should have a file flag as well?
				.setHandler([](const clap::ParsingResult& _) -> int {
					vm::Supervisor::get();
					DuckVMRepl::get().run();
					return 0;
				})
		);
}

int main(int argc, const char** argv) {
	init::InitObject _;
	auto             clap = getVmClap();

	try {
		return clap.execute(argc, argv);
	} catch (const clap::exceptions::VersionException& e) {
		showVersion();
	} catch (const clap::exceptions::HelpException& e) {
		// printHelp(); // TODOP
		std::cerr << "Help flag passed\n";
	} catch (const clap::exceptions::ClapException& e) {
		printer::StreamPrinter::print(
			{
				{ "duckling: ", printer::Color::DEFAULT },
				{ "error: ", printer::Color::RED },
				{ e.what(), printer::Color::DEFAULT },
			}
		);
		return 1;
	} catch (const std::exception& e) { std::cerr << "Non Clap exception caught.\n"; }
}
