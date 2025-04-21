#include <backends/dvm/backend.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <base/exceptions.hpp>
#include <base/str_utils.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <utility>

class DVMBackendTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DVMBackendTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(functionCallsTest);
	}

protected:
	void testWithLir(query::Context& ctx, CRef<compiler::lir::Function> lir_function);

private:
	auto getModuleFromPath(std::string module_path) {
		using namespace compiler;

		std::vector<CRef<lir::Function>> funcs;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto module    = ctx.query<frontend::QueryModuleTree>(fs::FilePath(path(module_path)));
			auto top_level = ctx.query<helios::QueryTopLevelEntities>(module);
			for (auto& fun: top_level->functions) {
				auto mir_fun = ctx.query<compiler::mir::LowerToMirFunction>({ fun });
				auto lir_fun = ctx.query<compiler::lir::LowerToLirFunction>({ mir_fun });
				funcs.emplace_back(lir_fun);
			}
		});
		return backend_vm::Module(
			base::StrID(base::strSplit(module_path, "/").back().data()), funcs
		);
	}

	void runTest(
		std::string                        module_path,
		const base::Optional<std::string>& input  = {},
		const base::Optional<std::string>& output = {}
	) {
		using namespace compiler;
		auto module = getModuleFromPath(std::move(module_path));
		auto code   = module.build();

		for (auto& type: code.types) vm::code::serialize(type, std::cerr);
		for (auto& func: code.functions) vm::code::serialize(func, std::cerr);

		auto process_pid_response = vm::api::spawn();
		ASSERT_TRUE(process_pid_response);
		auto pid = process_pid_response->pid;

		ASSERT_TRUE(vm::api::loadCode(pid, code));
		ASSERT_TRUE(vm::api::run(pid));

		if_opt_some(input, in_str) {
			auto output_response = vm::api::output(pid);
			ASSERT_TRUE(output_response);
			ASSERT_EQUAL(in_str, output_response->output);
		}

		if_opt_some(output, out_str) {
			auto output_response = vm::api::output(pid);
			ASSERT_TRUE(output_response);
			ASSERT_EQUAL(out_str, output_response->output);
		}

		auto join_response = vm::api::join(pid);
		ASSERT_TRUE(join_response);
	}

	void simpleTest() { runTest("modules/simple"); }

	void functionCallsTest() { runTest("modules/function_calls"); }
};


TESTER_COMMON_MAIN("/compiler/backends/dvm/tests/")
