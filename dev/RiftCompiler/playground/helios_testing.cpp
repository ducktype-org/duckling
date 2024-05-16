#include "base/exceptions.hpp"
#include "base/string_id.hpp"
#include "helios/scopes/scopes.hpp"
#include "lexer/lexer.hpp"
#include "pst_parser/parser.hpp"
#include "query_framework/dep_graph.hpp"
#include <frontend/module_tree/queries.hpp>
#include <helios/symbols/symbols.hpp>
#include <clap/clap.hpp>
#include <iostream>
#include <query_framework/query_entry_point.hpp>

int main(int argc, const char* argv[]) {
	// @TODO: add to helios init
	lexer::init();
	pst::init();
	// @FUTURE: record all inits somewhere..

	// parser::init();


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


	using namespace compiler;

	auto root       = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);
	auto root_scope = query::entryPoint<helios::QueryRootScopeOf>(root);


	auto symbols_in_root = query::entryPoint<helios::QuerySymbolsInScope>(root_scope);

	std::cerr << "Symbol count: " << symbols_in_root.size() << "\n";

	query::debugPrintDependencyGraph();

	std::cerr << "Some lookup:\n";

	auto&& lookup_result_0 = query::entryPoint<helios::QueryLookupInScope>(
		{ root_scope, base::StrId("Inner_X"), true }
	);

	std::cerr << "found Inner_X times: " << lookup_result_0.symbolCount() << "\n";

	// auto&& lookup_result_1
	// 	= query::entryPoint<helios::QueryLookupInScope>({ root_scope, base::StrId("H2"), true }
	//     );
	// std::cerr << "found H2 times: " << lookup_result_1.leaves.size() << "\n";


	// auto&& lookup_result_2
	// 	= query::entryPoint<helios::QueryLookupInScope>({ root_scope, base::StrId("NN"), true }
	//     );

	// auto NN_symbol = lookup_result_2.getAsSingle();

	// RIFT_ASSERT(NN_symbol.size() > 0, "idk what");

	// auto&& lookup_result_NN_A = query::entryPoint<helios::QueryLookupInSymbol>(
	// 	{ NN_symbol.back(), base::StrId("A"), true }
	// );

	// std::cerr << "Found NN.A: " << lookup_result_NN_A.symbolCount() << "\n";

	query::debugPrintDependencyGraph();
	query::debugPrintDependencyGraphForDrawing();
}
