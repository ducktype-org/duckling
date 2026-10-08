// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/pst_query/code_dependency.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>

#include <base/extend_cpp/defer.hpp>

#include <clah/clah.hpp>
#include <diagnostic/highlight_positions.hpp>
#include <init/init.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>

#include <iostream>

void printContextErrors() {
	// Context errors are now handled via the interactive logger.
}

void printQueryDeps(const std::vector<query::internal::NodeID>& deps) {
	std::cerr << "Dependencies:\n";
	for (auto& i: deps)
		std::cerr << "    > query: " << i.q_id.getData().name << ",  key: " << i.hash.val << "\n";
}

int notMain(int argc, const char* const* argv) {
	init::InitObject _;


	auto clah = clah::Clah("main_code_dependency_playground")
	                .add(clah::ParamBuilder::ofValue(clah::FileParser::make("Path"))
	                         .addShortName('p')
	                         .addShortDesc("Path to Duckling source root")
	                         .required()
	                         .build());

	clah::ParsingResult options;

	try {
		options = clah.parse(usize(argc), argv);
	} catch (clah::exceptions::HelpException& e) {
		std::cerr << clah::HelpMessageGenerator::generate(clah, e.parsing_result) << '\n';
		return 1;
	} catch (clah::exceptions::ClahException& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}

	auto path_to_compile = options.getValue<fs::File>('p').value();

	using namespace compiler;

	auto root = frontend::createModuleTreeWithRandomPackageID(path_to_compile);

	defer(printContextErrors());

	auto& top_level = query::entryPoint<helios::QueryTopLevelEntities>(root)->valueOrPanic();

	for (auto& i: top_level.functions) {
		if (i->declaration->original_name == base::StrID("main")) {
			auto positions = pst::queryPositionDependencies<helios::QueryCodeOfFun>(
				i->declaration->original_symbol
			);

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
