#include <clap/clap.hpp>
#include <iostream>
#include <query_framework/query_entry_point.hpp>
#include <frontend/module_tree/queries.hpp>

int main(int argc, const char* argv[]) {
	// clang-format off
	auto clap
		= clap::Clap()
			.addHelpFlag()
			.addPositional(clap::FileParser::make());
			// .add(clap::ParamBuilder::ofValue(clap::IntParser::make())
	        //                  .required()
	        //                  .addShortName('n')
	        //                  .addLongName("times")
	        //                  .addShortDesc("How many times to print each content")
	        //                  .build());

	// clang-format on

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


	auto path_to_compile = options.getPositional<fs::FilePath>(0);

	std::cerr << "Path to compile:: " << path_to_compile.strView() << "\n";

	using namespace compiler;

	std::cerr << "Getting module tree... ";
	auto module_tree = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);
	std::cerr << "Done.\n";
}
