#include "runtime_expr_flow_simulator.hpp"

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
	vm::test::FlowSimulator createSimulator(std::string_view path_name) {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed");
		auto pid = process_pid_response.value().pid;

		fs::File file(path(std::string(path_name)));
		auto     loaded_file_response = vm::api::loadFiles(pid, { file });
		assertTrue(loaded_file_response.has_value(), "Load failed");
		return vm::test::FlowSimulator(
			[this](bool cond, std::string_view err) { assertTrue(cond, err); },
			[this](std::string_view p) { return fs::FilePath(path(std::string(p))); },
			pid
		);
	}

	void test1RuntimeExpr() {
		constexpr std::string_view sum_a_b_expr    = "runtime_expr_dbc/test_1/sum_a_b.dbc";
		constexpr std::string_view print_ret_expr  = "runtime_expr_dbc/test_1/print_ret.dbc";
		constexpr std::string_view modify_ret_expr = "runtime_expr_dbc/test_1/modify_ret.dbc";

		createSimulator("runtime_expr_dbc/test_1/main.dbc")
			.putBreakpoint(base::StrID("main"), 5)
			.putBreakpoint(base::StrID("main"), 9)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExpr(sum_a_b_expr, { 0 })
			.evalExpr(print_ret_expr, { 0 })
			.evalExpr(modify_ret_expr, { 0 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 9)
			.evalExpr(sum_a_b_expr, { 6 })
			.evalExpr(print_ret_expr, { 69 })
			.evalExpr(modify_ret_expr, { 69 })
			.finishAndAssertExitValue(2'137);
	}

	void test2RuntimeExpr() {
		constexpr std::string_view call_foo_unused_args_expr
			= "runtime_expr_dbc/test_2/expr/call_foo_unused_args.dbc";
		constexpr std::string_view modify_unused_arg_1_expr
			= "runtime_expr_dbc/test_2/expr/modify_unused_arg_1.dbc";
		constexpr std::string_view print_foo_ret0_expr
			= "runtime_expr_dbc/test_2/expr/print_foo_ret0.dbc";

		createSimulator("runtime_expr_dbc/test_2/main.dbc")
			.putBreakpoint(base::StrID("main"), 7)
			.putBreakpoint(base::StrID("main"), 10)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 7)
			.evalExpr(call_foo_unused_args_expr, { 109 })
			.evalExpr(modify_unused_arg_1_expr, { 42 })
			.evalExpr(call_foo_unused_args_expr, { 151 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 10)
			.evalExpr(print_foo_ret0_expr, { 0 })
			.finishAndAssertExitValue(0);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
