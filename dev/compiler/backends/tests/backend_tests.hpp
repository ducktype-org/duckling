/**
 * @brief This file serves as a common base for backend tests.
 */

#include <query_framework/query_int.hpp>
#include <tester/tester.hpp>

#include <query_framework/utils/with_context_do.hpp>
#include <query_framework/query_impl.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>

class TESTER_CLASS: public tester::TestSuite {
public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
#ifdef DVM_BACKEND_TEST
		TESTER_ADD_TEST(returnVoidTest);
		TESTER_ADD_TEST(simpleTypesVariables);
		TESTER_ADD_TEST(booleanLiteralsTests);
		TESTER_ADD_TEST(arithmeticTest);
#elif defined(LLVM_BACKEND_TEST)
		TESTER_ADD_TEST(returnVoidTest);
		TESTER_ADD_TEST(simpleTypesVariables);
		// TESTER_ADD_TEST(booleanLiteralsTests);
		TESTER_ADD_TEST(arithmeticTest);
#else
	#error "Unknown backend test"
#endif
	}

protected:
	void testWithLir(query::Context& ctx, CRef<compiler::lir::Function> lir_function);

private:
	void runTestForModuleWithSingleFunction(std::string module_path) {
		using namespace compiler;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto module    = ctx.query<frontend::QueryModuleTree>(fs::FilePath(path(module_path)));
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);

			ASSERT_TRUE(top_level.functions.size() == 1);

			auto mir_fun = ctx.query<compiler::mir::LowerToMirFunction>({ top_level.functions[0] });
			auto lir_fun = ctx.query<compiler::lir::LowerToLirFunction>({ mir_fun });
			testWithLir(ctx, lir_fun);
		});
	}

	void returnVoidTest() { runTestForModuleWithSingleFunction("modules/simple"); }

	void simpleTypesVariables() {
		// Currently this test is for coverage mainly, but it will be
		// replaced with something more meaningful in the future.

		runTestForModuleWithSingleFunction("modules/variables");
	}

	void booleanLiteralsTests() { runTestForModuleWithSingleFunction("modules/boolean_literals"); }

	void arithmeticTest() { runTestForModuleWithSingleFunction("modules/arithmetic"); }
};


TESTER_COMMON_MAIN("/compiler/backends/tests/")
