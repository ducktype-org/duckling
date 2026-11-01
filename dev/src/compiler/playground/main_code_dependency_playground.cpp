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
	                         .build())
	                .add(clah::ParamBuilder::ofValue(clah::StringParser::make("Name"))
	                         .addShortName('f')
	                         .addShortDesc("Name of the global function to inspect (default: main)")
	                         .optional()
	                         .build())
	                .add(clah::ParamBuilder::ofValue(clah::StringParser::make("Query"))
	                         .addShortName('q')
	                         .addShortDesc("Inspected query: `code` (default) or `decl`")
	                         .optional()
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
	auto function_name   = base::StrID(options.getValue<std::string>('f').copyValueOr("main"));
	auto query_name      = options.getValue<std::string>('q').copyValueOr("code");

	using namespace compiler;

	if (query_name != "code" && query_name != "decl") {
		std::cerr << "Unknown query `" << query_name << "`, expected `code` or `decl`\n";
		return 1;
	}

	auto root = frontend::createModuleTreeWithRandomPackageID(path_to_compile);

	defer(printContextErrors());

	auto& top_level = query::entryPoint<helios::QueryTopLevelEntities>(root)->valueOrPanic();

	for (auto& i: top_level.functions) {
		if (i->declaration->original_name == function_name) {
			auto symbol = i->declaration->original_symbol;

			std::vector<dia::SourcePosition> positions;
			if (query_name == "decl") {
				std::ignore = query::entryPoint<helios::QueryDeclOfFun>(symbol);
				positions   = pst::queryPositionDependencies<helios::QueryDeclOfFun>(symbol);
			} else {
				std::ignore = query::entryPoint<helios::QueryCodeOfFun>(symbol);
				positions   = pst::queryPositionDependencies<helios::QueryCodeOfFun>(symbol);
			}

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
