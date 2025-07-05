#include <clap/clap.hpp>
#include <diagnostic/highlight_positions.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <init/init.hpp>
#include <lexer/lexer.hpp>
#include <pst_parser/pst_query/code_dependency.hpp>
#include <pst_parser/pst_query/pst_access_side_input.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>

#include <base/defer.hpp>

#include <iostream>

void printContextErrors() {
	if (query::Context::logger.messageCount() > 0) {
		std::cerr << "Compilation errors logged in context: \n";
		query::Context::logger.dumpLog(true, std::cerr);
	}
}

void printQueryDeps(const std::vector<query::internal::NodeID>& deps) {
	std::cerr << "Dependencies:\n";
	for (auto& i: deps)
		std::cerr << "    > query: " << i.q_id.getData().name << ",  key: " << i.hash.val << "\n";
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

	for (auto& i: top_level->functions) {
		if (i.original_name == base::StrID("main")) {
			auto positions
				= pst::queryPositionDependencies<helios::QueryCodeOFFun>(i.original_symbol);

			printer::PrinterOStream str;
			dia::printHighlightedPositions(str, positions);

			printer::StreamPrinter p;
			p.print(str.getContents());
		}
	}
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
