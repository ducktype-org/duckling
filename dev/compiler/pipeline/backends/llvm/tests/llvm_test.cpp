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
	}

private:
	auto getLLVMModuleFromPath(std::string module_path) {
		using namespace compiler;

		backend_llvm::Module llvm_module(base::StrID("test_module"));

		query::utils::withContextDo([&](query::Context& ctx) {
			auto module    = ctx.query<frontend::QueryModuleTree>(fs::FilePath(path(module_path)));
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);

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
};


tester_common_main("/compiler/pipeline/backends/llvm/tests/")
