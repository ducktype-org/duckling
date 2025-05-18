#include "cli.hpp"
#include "server.hpp"
#include "vm_repl.hpp"

#include <clap/clap.hpp>
#include <init/init.hpp>
#include <printer/stream_printer.hpp>

#include <vm/core/supervisor/supervisor.hpp>
#include <vm/core/thread/low_program/instruction.hpp>

void showVersion() {
	std::cout << "VM version 0.0.\n";
	std::cout << "Configuration: \n";
	std::cout << vm::getInstructionConfig() << '\n';
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
	                         .build())
	                .add(clap::ParamBuilder::ofFlag()
	                         .conditional(
								 [](const clap::ParsingResult& result) {
									 return !(
										 result.isFlag('r')
										 && (result.isParam('f') || result.isParam('s'))
									 );
								 },
								 "REPL cannot be used with -s/--server or -f/--file flags"
							 )
	                         .addShortName('r')
	                         .addLongName("repl")
	                         .addShortDesc("Executes the VM in REPL mode")
	                         .build())
	                .add(clap::ParamBuilder::ofFlag()
	                         .addLongName("stdlib")
	                         .addShortDesc("When passed, loads standard library")
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
	else if (result.isFlag('r'))
		DuckVMRepl::get().run();
	else if (auto port = result.getValue<i64>("server"))
		server(i32(port.value()));
	else if (auto file = result.getValue<fs::FilePath>("file"))
		cli(file.value(), result.isFlag("stdlib"));
	else
		cli(result.isFlag("stdlib"));
}
