#include <backends/llvm/llvm_backend.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <utility>

class LLVMBackendTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LLVMBackendTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(returnVoidTest);
		TESTER_ADD_TEST(simpleTypesVariables);
		TESTER_ADD_TEST(booleansTest);
		TESTER_ADD_TEST(arithmeticTest);
		TESTER_ADD_TEST(comparisonTest);
		TESTER_ADD_TEST(functionCalls);
		TESTER_ADD_TEST(parseFromIRCodeTest);
		TESTER_ADD_TEST(doesNotParseIncorrectIRCode);
		TESTER_ADD_TEST(globalVariablesTest);
	}

private:
	auto getLLVMModuleFromPath(std::string module_path) {
		using namespace compiler;

		backend_llvm::Module             llvm_module(base::StrID("test_module"));
		std::vector<CRef<lir::Function>> ctors;

		query::utils::withContextDo([&](query::Context& ctx) {
			auto module    = frontend::createModuleTree(fs::File(path(module_path)));
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);

			for (auto& hout_glob: top_level->glob_data) {
				lir::LirGlobal lir_glob = lir::LirGlobal::fromHOUT(ctx, hout_glob);
				llvm_module.addGlobalToModule(lir_glob);
				variant_match(hout_glob.value) {
					variant_case(helios::HOUTGlobalVariable, var) {
						CRef mir_func
							= &ctx.query<mir::LowerGlobalDataToMirCtor>({ hout_glob })->value();
						mir_func->debugPrint(std::cerr);
						std::cerr << "\n\n\n";
						auto lir_func = ctx.query<lir::LowerToLirFunction>({ mir_func });
						lir_func->debugPrint(ctx, std::cerr);
						std::cerr << "\n\n\n";
						ctors.push_back(lir_func);
						llvm_module.addFunctionToModule(ctx, lir_func);
					}
					variant_case(helios::HOUTGlobalConst, cnst) {
						fail(base::strConcat(
							"Creating ctors for constant variables is not implemented yet. "
							"Global constant: ",
							hout_glob.original_name.strView()
						));
					}
					variant_default {
						fail(base::strConcat(
							"Unexpected global data type of: ", hout_glob.original_name
						));
					}
				}
			}

			if (!ctors.empty()) {
				// Add module ctors
				auto module_ctor = lir::createFunctionInvoker(
					ctx,
					ctors,
					compiler::helios::mangler::getSpecialMangledName<
						compiler::helios::mangler::ManglingSymbolKind::ModuleConstructor>(
						ctx,
						compiler::helios::mangler::special_symbol_keys::LirModuleID{
							frontend::moduleName(module) }
					)
				);
				llvm_module.addFunctionToModuleCtors(ctx, CRef<lir::Function>(&module_ctor));

				// Add module dtors (for now empty)
				// @TODO: add a legit dtors
				auto module_dtor = lir::createFunctionInvoker(
					ctx,
					{},
					compiler::helios::mangler::getSpecialMangledName<
						compiler::helios::mangler::ManglingSymbolKind::ModuleDestructor>(
						ctx,
						compiler::helios::mangler::special_symbol_keys::LirModuleID{
							frontend::moduleName(module) }
					)
				);
				llvm_module.addFunctionToModuleDtors(ctx, CRef<lir::Function>(&module_dtor));
			}

			for (auto& fun: top_level->functions) {
				CRef mir_fun = &ctx.query<compiler::mir::LowerToMirFunction>({ fun })->value();
				auto lir_fun = ctx.query<compiler::lir::LowerToLirFunction>({ mir_fun });
				llvm_module.addFunctionToModule(ctx, lir_fun);
			}
		});

		// debug print for coverage only:
		llvm_module.debugPrint();

		// verify integrity, then return for further checks.
		assertTrue(llvm_module.verify().isOk(), "LLVM module verification failed");
		return llvm_module;
	}

	void runTestForModule(
		std::string module_path, i32 expected_function_count = 1, i32 expected_prototype_count = -1
	) {
		if (expected_prototype_count == -1) expected_prototype_count = expected_function_count;
		auto llvm_module = getLLVMModuleFromPath(std::move(module_path));
		ASSERT_EQUAL_PRINT(llvm_module.getFunctionCount(false), expected_function_count);
		ASSERT_EQUAL_PRINT(llvm_module.getFunctionCount(), expected_prototype_count);
	}

	void returnVoidTest() { runTestForModule("modules/simple"); }

	void simpleTypesVariables() {
		// Currently this test is for coverage mainly, but it will be
		// replaced with something more meaningful in the future.

		runTestForModule("modules/variables");
	}

	void booleansTest() { runTestForModule("modules/booleans", 2); }

	void arithmeticTest() { runTestForModule("modules/arithmetic"); }

	void comparisonTest() { runTestForModule("modules/comparison", 1, 2); }

	void functionCalls() {
		runTestForModule("modules/calls_simple", 3);
		runTestForModule("modules/calls", 3, 5);
	}

	void parseFromIRCodeTest() {
		auto llvm_module = compiler::backend_llvm::Module::fromIRCode(
			"define void @test() {\n"
			"entry:\n"
			"  ret void\n"
			"}\n"
		);
		llvm_module.debugPrint();

		assertTrue(llvm_module.verify().isOk(), "LLVM module verification failed");
	}

	void doesNotParseIncorrectIRCode() {
		assertThrows<base::Panic>(
			[&]() {
				auto llvm_module = compiler::backend_llvm::Module::fromIRCode(
					"define void @test() {\n"
					"entry:\n"
					"  re void\n"
					"}\n"
				);
			},
			"LLVM incorrect code didn't throw"
		);
	}

	void globalVariablesTest() { runTestForModule("modules/global-variables", 5, 5); }

	void externTest() { runTestForModule("modules/extern", 1, 2); }
};


TESTER_COMMON_MAIN("/src/compiler/core/backends/llvm/tests/")
