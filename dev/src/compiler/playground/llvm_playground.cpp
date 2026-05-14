#include <backends/llvm/llvm_backend.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <clah/clah.hpp>
#include <init/init.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <iostream>

int main(int argc, const char* argv[]) {
	init::InitObject _;

	auto clah = clah::Clah("llvm_playground")
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

	auto& top_level = query::entryPoint<helios::QueryTopLevelEntities>(root)->valueOrPanic();

	auto llvm_module = compiler::backend_llvm::Module(base::StrID("test_module"));

	std::vector<CRef<lir::Function>> ctors;

	for (auto& hout_glob: top_level.glob_data) {
		query::utils::withContextDo([&](query::Context& ctx) {
			lir::LIRGlobal lir_glob = lir::LIRGlobal::fromHOUT(ctx, *hout_glob);
			llvm_module.addGlobalToModule(lir_glob);

			variant_match(hout_glob->value) {
				variant_case(helios::HOUTGlobalVariable, var) {
					CRef mir_func
						= &ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_glob })->valueOrThrow();

					mir_func->debugPrint(std::cerr);
					std::cerr << "\n\n\n";

					auto lir_func = ctx.query<lir::LowerToLIRFunction>({ mir_func });

					lir_func->debugPrint(ctx, std::cerr);
					std::cerr << "\n\n\n";

					ctors.push_back(lir_func);

					llvm_module.addFunctionToModule(ctx, lir_func);
				}
				variant_case(helios::HOUTGlobalConst, cnst) {
					// @future #1554 -- const ctors will probably be added here
				}
			}

			auto v = llvm_module.verify();

			if (v.isOk())
				std::cerr << "LLVM verification passed\n\n";
			else
				std::cerr << "LLVM verification failed\n\n";
		});
	}

	// Add module ctors and dtors to module CTOR and DTOR functions
	if (!ctors.empty()) {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto module_ctor = lir::createFunctionInvoker(
				ctx,
				ctors,
				compiler::helios::mangler::getSpecialMangledName<
					compiler::helios::mangler::ManglingSymbolKind::ModuleConstructor>(
					ctx,
					compiler::helios::mangler::special_symbol_keys::LIRModuleID{
						frontend::moduleName(root) }
				)
			);

			module_ctor.debugPrint(ctx, std::cerr);

			llvm_module.addFunctionToModuleCtors(ctx, CRef<lir::Function>(&module_ctor));
			auto v = llvm_module.verify();

			if (v.isOk())
				std::cerr << "LLVM verification passed\n\n";
			else
				std::cerr << "LLVM verification failed\n\n";

			// @TODO: add legit dtors
			auto module_dtor = lir::createFunctionInvoker(
				ctx,
				{},
				compiler::helios::mangler::getSpecialMangledName<
					compiler::helios::mangler::ManglingSymbolKind::ModuleDestructor>(
					ctx,
					compiler::helios::mangler::special_symbol_keys::LIRModuleID{
						frontend::moduleName(root) }
				)
			);

			module_dtor.debugPrint(ctx, std::cerr);

			llvm_module.addFunctionToModuleDtors(ctx, CRef<lir::Function>(&module_dtor));
			v = llvm_module.verify();

			if (v.isOk())
				std::cerr << "LLVM verification passed\n\n";
			else
				std::cerr << "LLVM verification failed\n\n";
		});
	}

	for (auto& fun: top_level.functions) {
		CRef mir_fun
			= &query::entryPoint<compiler::mir::LowerToMIRFunction>({ fun })->valueOrThrow();

		mir_fun->debugPrint(std::cerr);
		std::cerr << "\n\n\n";

		auto lir_fun = query::entryPoint<compiler::lir::LowerToLIRFunction>({ mir_fun });

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
