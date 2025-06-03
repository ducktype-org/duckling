#include <backends/dvm/backend.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <vm_tester_utils.hpp>

#include <base/exceptions.hpp>
#include <base/str_utils.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <utility>

class DVMBackendTest final: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DVMBackendTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(functionCallsTest);
		TESTER_ADD_TEST(builtinFuncsTest);
	}

protected:
	void testWithLir(query::Context& ctx, CRef<compiler::lir::Function> lir_function);

private:
	auto getModuleFromPath(std::string module_path) {
		using namespace compiler;

		std::vector<CRef<lir::Function>> funcs;
		base::StrID                      module_name;
		vm::code::CodeCollection         code;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto module    = ctx.query<frontend::QueryModuleTree>(fs::FilePath(path(module_path)));
			module_name    = moduleName(module);
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);
			for (auto& fun: top_level->functions) {
				auto mir_fun = ctx.query<compiler::mir::LowerToMirFunction>({ fun });
				auto lir_fun = ctx.query<compiler::lir::LowerToLirFunction>(
					{ &mir_fun->expect("Couldn\'t compile") }
				);
				funcs.emplace_back(lir_fun);
			}
			backend_vm::Module m{ ctx, module_name, funcs };
			code = m.build();
		});
		return code;
	}

	void runTest(
		std::string                        module_path,
		const base::Optional<std::string>& input      = {},
		const base::Optional<std::string>& output     = {},
		const std::vector<std::string>&    args       = {},
		i64                                exit_code  = 0,
		bool                               add_stdlib = true
	) {
		using namespace compiler;
		auto code = getModuleFromPath(std::move(module_path));

		for (auto& type: code.types) vm::code::serialize(type, std::cerr);
		for (auto& func: code.functions) vm::code::serialize(func, std::cerr);
		runTestOnVm(code, input, output, args, exit_code, add_stdlib);
	}

	void simpleTest() { runTest("modules/simple", {}, {}, {}, 42); }

	void functionCallsTest() { runTest("modules/function_calls", {}, {}, {}, 4); }

	void builtinFuncsTest() { runTest("modules/builtin_funcs", "9", "81\n82\n", {}, 82); }
};


tester_common_main("/compiler/pipeline/backends/dvm/tests/")
