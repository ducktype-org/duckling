#include <lexer/lexer.hpp>
#include <pst_parser/parser.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <clap/clap.hpp>
#include <iostream>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp> // For logger only, @TODO relax it #404
#include <base/defer.hpp>

int main(int argc, const char* argv[]) {
	// @TODO: add to helios init
	lexer::init();
	pst::init();

	// @FUTURE: record all inits somewhere..

	auto clap
		= clap::Clap().addHelpFlag().add(clap::ParamBuilder::ofValue(clap::FileParser::make("Path"))
	                                         .addShortName('p')
	                                         .addShortDesc("Path to Duckling source root")
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

	using namespace compiler;

	auto root = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);

	defer({
		// defer, so it runs, even if QueryTopLevelEntities panics/throws
		if (query::Context::logger.messageCount() > 0) {
			std::cerr << "Compilation errors logged in context: \n";
			query::Context::logger.dumpLog(true, std::cerr);
		}
	});
	
	auto top_level = query::entryPoint<helios::QueryTopLevelEntities>(root);

	std::cerr << top_level.debugPrint();
}
