#include <tester/tester.hpp>

#include <query_framework/utils/with_context_do.hpp>
#include <query_framework/query_impl.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <backends/llvm/llvm_backend.hpp>

class LLVMBackendTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LLVMBackendTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(returnVoidTest);
		TESTER_ADD_TEST(simpleTypesVariables);
	}

private:
	void returnVoidTest() {
		using namespace compiler;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto module
				= ctx.query<frontend::QueryModuleTree>(fs::FilePath(path("modules/simple")));
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);

			ASSERT_TRUE(top_level.functions.size() == 1);

			auto mir_fun = ctx.query<compiler::mir::LowerToMirFunction>({ top_level.functions[0] });
			auto lir_fun = ctx.query<compiler::lir::LowerToLirFunction>({ mir_fun });

			auto llvm_module = backend_llvm::lirFunctionToModule(lir_fun);
			assertTrue(llvm_module.verify(), "LLVM module verification failed");

			// debug print for coverage only:
			llvm_module.debugPrint();
		});
	}

	void simpleTypesVariables() {
		using namespace compiler;
		query::utils::withContextDo([&](query::Context& ctx) {
			// Currently this test is for coverage mainly, but it will be
			// replaced with something more meaningful in the future.
			auto module
				= ctx.query<frontend::QueryModuleTree>(fs::FilePath(path("modules/variables")));
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);

			ASSERT_TRUE(top_level.functions.size() == 1);

			auto mir_fun = ctx.query<compiler::mir::LowerToMirFunction>({ top_level.functions[0] });
			auto lir_fun = ctx.query<compiler::lir::LowerToLirFunction>({ mir_fun });

			auto llvm_module = backend_llvm::lirFunctionToModule(lir_fun);
			assertTrue(llvm_module.verify(), "LLVM module verification failed");

			llvm_module.debugPrint();
		});
	}
};


TESTER_COMMON_MAIN("/compiler/backends/llvm/tests/")
