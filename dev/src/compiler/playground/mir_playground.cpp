#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>

#include <clah/clah.hpp>
#include <init/init.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>

void printContextErrors() {
	if (query::Context::logger.messageCount() > 0) {
		std::cerr << "Compilation errors logged in context: \n";
		query::Context::logger.dumpLog(true, std::cerr);
	}
}

int notMain(int argc, const char* const* argv) {
	init::InitObject _;

	auto clah = clah::Clah("mir_playground")
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

	defer(printContextErrors());

	auto path_to_compile = options.getValue<fs::File>('p').value();

	using namespace compiler;

	auto root = frontend::createModuleTree(path_to_compile);

	auto top_level = query::entryPoint<helios::QueryModuleHOUT>(root);

	for (auto& glob_data: top_level.glob_data) {
		if (std::holds_alternative<helios::HOUTGlobalConst>(glob_data.value)) continue;
		CRef mir_fun
			= &query::entryPoint<compiler::mir::LowerGlobalDataToMirCtor>({ glob_data })->value();

		mir_fun->debugPrint(std::cerr);
		std::cerr << "\n";
	}


	for (auto& fun: top_level.functions) {
		CRef mir_fun = &query::entryPoint<compiler::mir::LowerToMirFunction>({ fun })->value();

		mir_fun->debugPrint(std::cerr);
		std::cerr << "\n";
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
