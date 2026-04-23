#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>

#include <clah/clah.hpp>
#include <init/init.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <iostream>

int main(int argc, const char* argv[]) {
	init::InitObject _;

	auto clah = clah::Clah("lir_playground")
	                .add(clah::ParamBuilder::ofValue(clah::FileParser::make("Path"))
	                         .addShortName('p')
	                         .addShortDesc("Path to Duckling source root")
	                         .required()
	                         .build());

	clah::ParsingResult options;

	try {
		options = clah.parse(base::safeIntConv<usize>(argc), argv);
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

	auto& top_level = query::entryPoint<helios::QueryTopLevelEntities>(root)->valueOrPanic();

	for (const auto& hout_glob: top_level.glob_data) {
		query::utils::withContextDo([&](query::Context& ctx) {
			variant_match(hout_glob->value) {
				variant_case(helios::HOUTGlobalVariable, var) {
					CRef mir_func
						= &ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_glob })->valueOrThrow();
					auto lir_func = ctx.query<lir::LowerToLIRFunction>({ mir_func });
					lir_func->debugPrint(ctx, std::cerr);
					std::cerr << "\n";
				}
				variant_case(helios::HOUTGlobalConst, cnst) {
					// @future #1554 -- const ctors will probably be added here
				}
			}
		});
	}

	for (auto& fun: top_level.functions) {
		query::utils::withContextDo([&](query::Context& ctx) {
			CRef mir_fun = &ctx.query<compiler::mir::LowerToMIRFunction>({ fun })->valueOrThrow();
			auto lir_fun = ctx.query<compiler::lir::LowerToLIRFunction>({ mir_fun });
			lir_fun->debugPrint(ctx, std::cerr);
		});
		std::cerr << "\n";
	}
}
