#include "runtime_expr_dbc/runtime_expr_flow_simulator.hpp"

#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

#include <string_view>
#include <vector>

class VmRuntimeExprTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRuntimeExprTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(test1RuntimeExpr);
		TESTER_ADD_TEST(test2RuntimeExpr);
	}

private:
	vm::test::FlowSimulator createSimulator(const fs::File& file) {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed");
		auto pid = process_pid_response.value().pid;

		auto loaded_file_response = vm::api::loadFiles(pid, { file });
		assertTrue(loaded_file_response.has_value(), "Load failed");
		return { [this](bool cond, std::string_view err) { assertTrue(cond, err); }, pid };
	}

	void test1RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_1/main.dbc"));
		const fs::File sum_a_b_expr(path("runtime_expr_dbc/test_1/sum_a_b.dbc"));
		const fs::File print_ret_expr(path("runtime_expr_dbc/test_1/print_ret.dbc"));
		const fs::File modify_ret_expr(path("runtime_expr_dbc/test_1/modify_ret.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 5)
			.putBreakpoint(base::StrID("main"), 9)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprNormal(sum_a_b_expr, { 0 })
			.evalExprNormal(print_ret_expr, { 0 })
			.evalExprNormal(modify_ret_expr, { 0 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 9)
			.evalExprNormal(sum_a_b_expr, { 6 })
			.evalExprNormal(print_ret_expr, { 69 })
			.evalExprNormal(modify_ret_expr, { 69 })
			.finishAndAssertExitValue(2'137);
	}

	void test2RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_2/main.dbc"));
		const fs::File call_foo_unused_args_expr(
			path("runtime_expr_dbc/test_2/expr/call_foo_unused_args.dbc")
		);
		const fs::File modify_unused_arg_1_expr(
			path("runtime_expr_dbc/test_2/expr/modify_unused_arg_1.dbc")
		);
		const fs::File print_foo_ret0_expr(path("runtime_expr_dbc/test_2/expr/print_foo_ret0.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 7)
			.putBreakpoint(base::StrID("main"), 10)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 7)
			.evalExprNormal(call_foo_unused_args_expr, { 109 })
			.evalExprNormal(modify_unused_arg_1_expr, { 42 })
			.evalExprNormal(call_foo_unused_args_expr, { 151 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 10)
			.evalExprNormal(print_foo_ret0_expr, { 0 })
			.finishAndAssertExitValue(0);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
