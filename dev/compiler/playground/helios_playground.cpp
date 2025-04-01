#include <clap/clap.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <init/init.hpp>
#include <lexer/lexer.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>  // For logger only, @TODO relax it #404

#include <base/defer.hpp>

#include <iostream>

void printContextErrors() {
	if (query::Context::logger.messageCount() > 0) {
		std::cerr << "Compilation errors logged in context: \n";
		query::Context::logger.dumpLog(true, std::cerr);
	}
}

int notMain(int argc, const char* const* argv) {
	init::InitObject _;


	auto clap
		= clap::Clap().addHelpFlag().add(clap::ParamBuilder::ofValue(clap::FileParser::make("Path"))
	                                         .addShortName('p')
	                                         .addShortDesc("Path to Duckling source root")
	                                         .required()
	                                         .build());

	clap::ParsingResult options;

	try {
		options = clap.parse(usize(argc), argv);
	} catch (clap::exceptions::HelpException& e) {
		std::cerr << clap::HelpMessageGenerator::generate(clap, e.parsing_result) << '\n';
		return 1;
	} catch (clap::exceptions::ClapException& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}

	auto path_to_compile = options.getValue<fs::FilePath>('p').value();

	using namespace compiler;

	auto root = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);

	defer(printContextErrors());

	auto top_level = query::entryPoint<helios::QueryTopLevelEntities>(root);
	std::cerr << top_level->debugPrint();

	return 0;
}

int main(int argc, const char* argv[]) {
	// note: we need to catch exception here,
	// because otherwise stack unwinding might not happen,
	// and defers might not be called.
	try {
		return notMain(argc, argv);
	} catch (std::exception& e) { std::cerr << "exception was thrown: " << e.what() << '\n'; }
}
