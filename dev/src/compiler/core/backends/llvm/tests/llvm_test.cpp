#include <backends/llvm/llvm_backend.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <base/exceptions.hpp>

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

		backend_llvm::Module              llvm_module(base::StrID("test_module"));
		std::vector<lir::FunctionLiteral> ctors;

		query::utils::withContextDo([&](query::Context& ctx) {
			auto module    = ctx.query<frontend::QueryModuleTree>(fs::FilePath(path(module_path)));
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);

			for (auto& hout_glob: top_level->glob_data) {
				lir::LirGlobal lir_glob = lir::LirGlobal::fromHOUT(ctx, hout_glob);
				llvm_module.addGlobalToModule(lir_glob);
				std::visit(
					[&](auto&& val) {
						using T = std::decay_t<decltype(val)>;
						if constexpr (std::is_same_v<T, helios::HOUTGlobalVariable>) {
							CRef mir_func
								= &ctx.query<mir::LowerGlobalDataToMirCtor>({ hout_glob })->value();
							auto lir_func = ctx.query<lir::LowerToLirFunction>({ mir_func });
							ctors.push_back(lir::getFunctionLiteralfromFunction(*lir_func));
							llvm_module.addFunctionToModule(ctx, lir_func);
						} else if constexpr (std::is_same_v<T, helios::HOUTGlobalConst>) {
							/* TODO: create global constant ctors if necessary */
							fail(base::strConcat(
								"Creating ctors for constant variables is not implemented yet. "
								"Global constant: ",
								hout_glob.original_name.strView()
							));
						}
					},
					hout_glob.value
				);
			}

			if (!ctors.empty()) {
				// Add module ctors
				//@TODO: fix this proper module global ctor mangling
				auto module_ctor = lir::fromFunctionLiterals(
					ctx,
					ctors,
					base::StrID(
						base::strConcat("_MODULE_CTOR_", frontend::moduleName(module).str()).c_str()
					)
				);
				llvm_module.addFunctionToModuleCtors(ctx, CRef<lir::Function>(&module_ctor));

				// Add module dtors (reverse order)
				//@TODO: fix this proper module global dtor mangling
				// Note: we reverse the order of ctors to ensure dtors are called in the reverse order
				//@TODO: add a legit dtors
				std::vector<lir::FunctionLiteral> reversed_ctors(ctors.rbegin(), ctors.rend());
				auto                              module_dtor = lir::fromFunctionLiterals(
                    ctx,
                    reversed_ctors,
                    base::StrID(
                        base::strConcat("_MODULE_DTOR_", frontend::moduleName(module).str()).c_str()
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
};


TESTER_COMMON_MAIN("/src/compiler/core/backends/llvm/tests/")
