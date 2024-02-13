#include <iomanip>

#include <clap/clap.hpp>
#include <printer/printer.hpp>

#include <supervisor/supervisor.hpp>
#include "cli.hpp"
#include "server.hpp"

#include <services/executor_f8/op_case_config.hpp>

void showVersion() {
	std::cout << std::boolalpha;
	std::cout << "RiftVM version 0.0.\n";
	std::cout << "Configuration: \n";
	std::cout << "IGNORE_EXECUTION_STRATEGY: " << IGNORE_EXECUTION_STRATEGY << "\n";
	std::cout << "USE_COMPUTED_GOTO: " << USE_COMPUTED_GOTO_VALUE << "\n";
	std::cout << "USE_FLAT_FRAME: " << USE_FLAT_FRAME_VALUE << "\n";
}

int main(int argc, const char** argv) {
	auto clap = clap::Clap()
	                .addHelpFlag()
	                .add(clap::ParamBuilder::ofValue(clap::IntParser::make("port"))
	                         .addShortName('s')
	                         .addLongName("server")
	                         .addShortDesc("Launch RiftVM as a http server")
	                         .build())
	                .add(clap::ParamBuilder::ofValue(clap::FileParser::make(std::regex(".*\\.rbc")))
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
		result = clap.parse(argc, argv);
	} catch (clap::exceptions::ClapException& e) {
		printer::Console console = printer::Console();
		console.add({
			{
				{ "rift: ", printer::Color::DEFAULT },
				{ "error: ", printer::Color::RED },
				{ e.what(), printer::Color::DEFAULT },
			},
			printer::MessageType::ERROR,
			0,
		});
		console.print(std::cerr);
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
		server(port.value());
	else if (auto file = result.getValue<fs::FilePath>("file"))
		cli(file.value());
	else
		cli();
}
