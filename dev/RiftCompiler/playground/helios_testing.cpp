#include <frontend/module_tree/queries.hpp>
#include <helios/symbols/symbols.hpp>
#include <clap/clap.hpp>
#include <iostream>
#include <query_framework/query_entry_point.hpp>


int main(int argc, const char* argv[]) {
	auto clap
		= clap::Clap().addHelpFlag().add(clap::ParamBuilder::ofValue(clap::FileParser::make("Path"))
	                                         .addShortName('p')
	                                         .addShortDesc("Path to Rift source root")
	                                         .required()
	                                         .build());

	clap::ParsingResult options;
	try {
		options = clap.parse(argc, argv);
	} catch (clap::exceptions::HelpException& e) {
		std::cerr << clap::HelpMessageGenerator::generate(clap, e.parsing_result) << '\n';
		return 1;
	} catch (clap::exceptions::ClapException& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}


	auto path_to_compile = options.getValue<fs::FilePath>('p').value();

	std::cerr << "path_to_compile: " << path_to_compile.strView() << "\n";


	using namespace compiler::frontend;

	auto root = query::queryEntryPoint<QueryModuleTree>(path_to_compile);

	// @TODO: add helios here...

}
