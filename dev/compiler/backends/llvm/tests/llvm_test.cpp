#include <backends/llvm/llvm_backend.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <base/exceptions.hpp>

class LLVMBackendTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LLVMBackendTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(returnVoidTest);
		TESTER_ADD_TEST(simpleTypesVariables);
		TESTER_ADD_TEST(booleansTest);
		TESTER_ADD_TEST(arithmeticTest);
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

		ASSERT_TRUE(llvm_module.verify().isOk());
		return llvm_module;
	}

	void runTestForModuleWithSingleFunction(std::string module_path) {
		using namespace compiler;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto module    = ctx.query<frontend::QueryModuleTree>(fs::FilePath(path(module_path)));
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);

			ASSERT_TRUE(top_level->functions.size() == 1);

			CRef mir_fun
				= &ctx.query<compiler::mir::LowerToMirFunction>({ top_level->functions[0] })->value();
			auto lir_fun = ctx.query<compiler::lir::LowerToLirFunction>({ mir_fun });

			auto llvm_module = backend_llvm::Module(base::StrID("test_module"));
			llvm_module.addFunctionToModule(ctx, lir_fun);

			// debug print for coverage only:
			llvm_module.debugPrint();

			// this is where the main part ot test is:
			assertTrue(llvm_module.verify().isOk(), "LLVM module verification failed");
		});
	}

	void returnVoidTest() { runTestForModuleWithSingleFunction("modules/simple"); }

	void simpleTypesVariables() {
		// Currently this test is for coverage mainly, but it will be
		// replaced with something more meaningful in the future.

		runTestForModuleWithSingleFunction("modules/variables");
	}

	void booleansTest() {
		auto llvm_module = getLLVMModuleFromPath("modules/booleans");
		ASSERT_EQUAL(llvm_module.getFunctionCount(), 2);
		ASSERT_EQUAL(llvm_module.getFunctionCount(false), 2);
	}

	void arithmeticTest() { runTestForModuleWithSingleFunction("modules/arithmetic"); }

	void functionCalls() {
		{
			auto llvm_module = getLLVMModuleFromPath("modules/calls_simple");
			ASSERT_EQUAL(llvm_module.getFunctionCount(), 3);
			ASSERT_EQUAL(llvm_module.getFunctionCount(false), 3);
		}
		{
			auto llvm_module = getLLVMModuleFromPath("modules/calls");
			ASSERT_EQUAL(llvm_module.getFunctionCount(false), 3);
			ASSERT_EQUAL(llvm_module.getFunctionCount(), 5);
		}
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


TESTER_COMMON_MAIN("/compiler/backends/llvm/tests/")
