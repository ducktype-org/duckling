#include "cli.hpp"
#include "config.hpp"
#include "server.hpp"

#include <clap/clap.hpp>
#include <init/init.hpp>
#include <printer/stream_printer.hpp>

#include <vm/core/supervisor/supervisor.hpp>

void showVersion() {
	std::cout << std::boolalpha;
	std::cout << "VM version 0.0.\n";
	std::cout << "Configuration: \n";
	std::cout << "IGNORE_EXECUTION_STRATEGY: " << IGNORE_EXECUTION_STRATEGY << "\n";
	std::cout << "USE_COMPUTED_GOTO: " << USE_COMPUTED_GOTO_VALUE << "\n";
}

int main(int argc, const char** argv) {
	init::InitObject _;
	auto             clap = clap::Clap()
	                .addHelpFlag()
	                .add(clap::ParamBuilder::ofValue(clap::IntParser::make("port"))
	                         .addShortName('s')
	                         .addLongName("server")
	                         .addShortDesc("Launch VM as a http server")
	                         .build())
	                .add(clap::ParamBuilder::ofValue(clap::FileParser::make(std::regex(".*\\.dbc")))
	                         .conditional(
								 [](const clap::ParsingResult& result) {
									 return !(result.isParam('f') && result.isParam('s'));
								 },
								 "File cannot be passed with -s/--server flag"
							 )
	                         .addShortName('f')
	                         .addLongName("file")
	                         .addShortDesc("Launch given file (only if not -s/--server)")
	                         .build())
	                .add(clap::ParamBuilder::ofFlag()
	                         .addShortName('v')
	                         .addLongName("version")
	                         .addShortDesc("Shows version and config")
	                         .build());

	clap::ParsingResult result;

	try {
		result = clap.parse(usize(argc), argv);
	} catch (clap::exceptions::ClapException& e) {
		printer::StreamPrinter::print({
			{ "duckling: ", printer::Color::DEFAULT },
			{ "error: ", printer::Color::RED },
			{ e.what(), printer::Color::DEFAULT },
		});
		return 1;
	} catch (clap::exceptions::HelpException& e) {
		std::string help_message = clap::HelpMessageGenerator::generate(clap, e.parsing_result);
		std::cout << help_message << '\n';
		return 0;
	}

	// Instantiate supervisor
	vm::Supervisor::get();

	if (result.isFlag('v'))
		showVersion();
	else if (auto port = result.getValue<i64>("server"))
		server(i32(port.value()));
	else if (auto file = result.getValue<fs::FilePath>("file"))
		// TODO: This is just temporary, change that to take in real command line arguments.
		cli(file.value(), { "1", "2" });
	else
		cli();
}
