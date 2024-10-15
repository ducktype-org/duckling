#include <lexer/lexer.hpp>
#include <pst_parser/parser.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <clap/clap.hpp>
#include <iostream>
#include <query_framework/query_entry_point.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>

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

	auto top_level = query::entryPoint<helios::QueryTopLevelEntities>(root);


	for (auto& fun: top_level.functions) {
		auto& mir_fun = query::entryPoint<compiler::mir::LowerToMirFunction>({ fun });
		auto& lir_fun = query::entryPoint<compiler::lir::LowerToLirFunction>({ mir_fun });

		lir_fun.debugPrint(std::cerr);
		std::cerr << "\n";
	}
}
