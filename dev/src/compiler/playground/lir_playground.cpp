#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_querries.hpp>

#include <base/int_conv.hpp>
#include <base/variant.hpp>

#include <clah/clah.hpp>
#include <init/init.hpp>
#include <lexer/lexer.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

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

	auto root = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);

	auto top_level = query::entryPoint<helios::QueryTopLevelEntities>(root);

	for (const auto& hout_glob: top_level->glob_data) {
		query::utils::withContextDo([&](query::Context& ctx) {
			variant_match(hout_glob.value) {
				variant_case(helios::HOUTGlobalVariable, var) {
					CRef mir_func
						= &ctx.query<mir::LowerGlobalDataToMirCtor>({ hout_glob })->value();
					auto lir_func = ctx.query<lir::LowerToLirFunction>({ mir_func });
					lir_func->debugPrint(ctx, std::cerr);
					std::cerr << "\n";
				}
				variant_case(helios::HOUTGlobalConst, cnst) {
					//@TODO: create global constant ctors if nessesary
					std::cerr << "skiping generation of ctor for global constant: "
							  << hout_glob.original_name.strView() << "\n";
				}
			}
		});
	}

	for (auto& fun: top_level->functions) {
		query::utils::withContextDo([&](query::Context& ctx) {
			CRef mir_fun = &ctx.query<compiler::mir::LowerToMirFunction>({ fun })->value();
			auto lir_fun = ctx.query<compiler::lir::LowerToLirFunction>({ mir_fun });
			lir_fun->debugPrint(ctx, std::cerr);
		});
		std::cerr << "\n";
	}
}
