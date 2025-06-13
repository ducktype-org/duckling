#include "lir/lir_structure/lir_structure.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <clap/clap.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <init/init.hpp>
#include <lexer/lexer.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <base/variant.hpp>

#include <iostream>

int main(int argc, const char* argv[]) {
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

	auto top_level = query::entryPoint<helios::QueryTopLevelEntities>(root);

	auto llvm_module = compiler::backend_llvm::Module(base::StrID("test_module"));

	for (auto& hout_glob: top_level->glob_data) {
		query::utils::withContextDo([&](query::Context& ctx) {
			lir::LirGlobal lir_glob = lir::LirGlobal::fromHout(ctx, hout_glob);
			llvm_module.addGlobalToModule(lir_glob);

			variant_match(hout_glob.value) {
				variant_case(helios::HOUTGlobalVariable, var) {
					CRef mir_func
						= &ctx.query<mir::LowerGlobalDataToMirCtor>({ hout_glob })->value();
					auto lir_func = ctx.query<lir::LowerToLirFunction>({ mir_func });
					lir_func->debugPrint(ctx, std::cerr);
					std::cerr << "\n\n\n";

					llvm_module.addFunctionToModuleCtors(ctx, lir_func);
				}
				variant_case(helios::HOUTGlobalConst, cnst) {
					//@TODO: create global constant ctors if nessesary
				}
			}

			auto v = llvm_module.verify();

			if (v.isOk())
				std::cerr << "LLVM verification passed\n\n";
			else
				std::cerr << "LLVM verification failed\n\n";
		});
	}

	for (auto& fun: top_level->functions) {
		CRef mir_fun = &query::entryPoint<compiler::mir::LowerToMirFunction>({ fun })->value();

		mir_fun->debugPrint(std::cerr);
		std::cerr << "\n\n\n";

		auto lir_fun = query::entryPoint<compiler::lir::LowerToLirFunction>({ mir_fun });

		query::utils::withContextDo([&](query::Context& ctx) {
			lir_fun->debugPrint(ctx, std::cerr);
		});
		std::cerr << "\n\n\n";

		query::utils::withContextDo([&](query::Context& ctx) {
			llvm_module.addFunctionToModule(ctx, lir_fun);
		});
		auto v = llvm_module.verify();

		if (v.isOk())
			std::cerr << "LLVM verification passed\n\n";
		else
			std::cerr << "LLVM verification failed\n\n";
	}

	llvm_module.debugPrint();
}
