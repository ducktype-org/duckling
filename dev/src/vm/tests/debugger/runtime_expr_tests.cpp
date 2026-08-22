#include "runtime_expr_dbc/runtime_expr_flow_simulator.hpp"

#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

#include <array>
#include <ranges>
#include <string_view>
#include <vector>

class VmRuntimeExprTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRuntimeExprTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(test1RuntimeExpr);
		TESTER_ADD_TEST(test2RuntimeExpr);
		TESTER_ADD_TEST(test3RuntimeExpr);
		TESTER_ADD_TEST(test4RuntimeExpr);
		TESTER_ADD_TEST(test5RuntimeExpr);
		TESTER_ADD_TEST(test6RuntimeExpr);
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
		const fs::File get_values_expr(path("runtime_expr_dbc/test_1/get_values.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("main"), 5)
			.putBreakpoint(base::StrID("main"), 7)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.evalExprNormal(get_values_expr, { 0, 0 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprNormal(get_values_expr, { 0, 4 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 7)
			.evalExprNormal(get_values_expr, { 2, 4 })
			.finishAndAssertExitValue(2'137);
	}

	void test2RuntimeExpr() {
		const fs::File   main_file(path("runtime_expr_dbc/test_2/main.dbc"));
		const std::array expressions = {
			fs::File(path("runtime_expr_dbc/test_2/expr/multiply.dbc")),
			fs::File(path("runtime_expr_dbc/test_2/expr/add.dbc")),
			fs::File(path("runtime_expr_dbc/test_2/expr/subtract.dbc")),
			fs::File(path("runtime_expr_dbc/test_2/expr/divide.dbc")),
		};
		const std::array expected_results = {
			std::vector<u64>{ 128, 64, 32, 16 },
			std::vector<u64>{ 66, 34, 18, 10 },
			std::vector<u64>{ 62, 30, 14, 6 },
			std::vector<u64>{ 32, 16, 8, 4 },
		};

		static_assert(expected_results.size() == expressions.size(), "match those");

		auto simulator = createSimulator(main_file);
		simulator.putBreakpoint(base::StrID("main"), 16)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 16);

		using namespace std::views;
		for (const auto& [expr, res]: zip(expressions, expected_results))
			simulator.evalExprNormal(expr, res);

		simulator.finishAndAssertExitValue(0);
	}

	void test3RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_3/main.dbc"));
		const fs::File call_foo_expr(path("runtime_expr_dbc/test_3/call_foo.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprNormal(call_foo_expr, { 7 })
			.finishAndAssertExitValue(0);
	}

	void test4RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_4/main.dbc"));
		const fs::File modify_value_expr(path("runtime_expr_dbc/test_4/modify_value.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 2)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 2)
			.evalExprNormal(modify_value_expr, { 42 })
			.finishAndAssertExitValue(42);
	}

	void test5RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_5/main.dbc"));
		const fs::File access_frame_2_expr(path("runtime_expr_dbc/test_5/access_frame_2.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("foo"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("foo"), 4)
			.evalExprNormal(access_frame_2_expr, { 10 })
			.finishAndAssertExitValue(7);
	}

	void test6RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_6/main.dbc"));
		const fs::File compare_frames_expr(path("runtime_expr_dbc/test_6/compare_frames.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("foo"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("foo"), 4)
			.evalExprNormal(compare_frames_expr, { 0 })
			.finishAndAssertExitValue(7);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
