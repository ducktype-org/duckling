#include "runtime_expr_dbc/runtime_expr_flow_simulator.hpp"

#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>

#include <array>
#include <chrono>
#include <ranges>
#include <string_view>
#include <thread>
#include <vector>

class VmRuntimeExprTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRuntimeExprTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(test1RuntimeExpr);
		TESTER_ADD_TEST(test2RuntimeExpr);
		TESTER_ADD_TEST(test3RuntimeExpr);
		TESTER_ADD_TEST(test4RuntimeExpr);
		TESTER_ADD_TEST(test5RuntimeExpr);
		TESTER_ADD_TEST(test6RuntimeExpr);
		TESTER_ADD_TEST(test7RuntimeExpr);
		TESTER_ADD_TEST(test8RuntimeExpr);
		TESTER_ADD_TEST(test9RuntimeExpr);
		TESTER_ADD_TEST(test10RuntimeExpr);
		TESTER_ADD_TEST(test11RuntimeExpr);
		TESTER_ADD_TEST(test12RuntimeExpr);
		TESTER_ADD_TEST(test13RuntimeExpr);
		TESTER_ADD_TEST(test14RuntimeExpr);
		TESTER_ADD_TEST(test15RuntimeExpr);
		TESTER_ADD_TEST(test16RuntimeExpr);
		TESTER_ADD_TEST(test17RuntimeExpr);
		TESTER_ADD_TEST(test18RuntimeExpr);
		TESTER_ADD_TEST(test19RuntimeExpr);
		TESTER_ADD_TEST(test20RuntimeExpr);
		TESTER_ADD_TEST(test21RuntimeExpr);
		TESTER_ADD_TEST(test22RuntimeExpr);
		TESTER_ADD_TEST(test23RuntimeExpr);
	}

private:
	vm::test::FlowSimulator createSimulator(const fs::File& file) {
		return createSimulator(std::vector<fs::File>{ file });
	}

	vm::test::FlowSimulator createSimulator(const std::vector<fs::File>& files) {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed");
		auto pid = process_pid_response.value().pid;

		auto loaded_file_response = vm::api::loadFiles(pid, files);
		if (!loaded_file_response)
			assertTrue(false, vm::api::errorToString(loaded_file_response.error()));
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
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(get_values_expr, { 0, 0 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.resume()
			.awaitBreakpoint(base::StrID("main"), 5)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(get_values_expr, { 4, 0 })
			.awaitBreakpoint(base::StrID("main"), 5)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.resume()
			.awaitBreakpoint(base::StrID("main"), 7)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(get_values_expr, { 4, 2 })
			.awaitBreakpoint(base::StrID("main"), 7)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
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
			std::vector<u64>{ 16, 32, 64, 128 },
			std::vector<u64>{ 10, 18, 34, 66 },
			std::vector<u64>{ 6, 14, 30, 62 },
			std::vector<u64>{ 4, 8, 16, 32 },
		};

		static_assert(expected_results.size() == expressions.size(), "match those");

		auto simulator = createSimulator(main_file);
		simulator.putBreakpoint(base::StrID("main"), 16)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 16)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			});

		using namespace std::views;
		for (const auto& [expr, res]: zip(expressions, expected_results))
			simulator.evalExprNormal(expr, res)
				.awaitBreakpoint(base::StrID("main"), 16)
				.enforceCallStack({
					base::StrID("vm_start_function"),
					base::StrID("main"),
				});

		simulator.finishAndAssertExitValue(0);
		simulator.cleanup();
	}

	void test3RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_3/main.dbc"));
		const fs::File call_foo_expr(path("runtime_expr_dbc/test_3/call_foo.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(call_foo_expr, { 7 })
			.awaitBreakpoint(base::StrID("main"), 5)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(0)
			.cleanup();
	}

	void test4RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_4/main.dbc"));
		const fs::File modify_value_expr(path("runtime_expr_dbc/test_4/modify_value.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 2)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(modify_value_expr, { 42 })
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(42)
			.cleanup();
	}

	void test5RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_5/main.dbc"));
		const fs::File access_frame_2_expr(path("runtime_expr_dbc/test_5/access_frame_2.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("foo"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("foo"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("foo"),
			})
			.evalExprNormal(access_frame_2_expr, { 10 })
			.awaitBreakpoint(base::StrID("foo"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("foo"),
			})
			.finishAndAssertExitValue(7)
			.cleanup();
	}

	void test6RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_6/main.dbc"));
		const fs::File compare_frames_expr(path("runtime_expr_dbc/test_6/compare_frames.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("foo"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("foo"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("foo"),
			})
			.evalExprNormal(compare_frames_expr, { 0 })
			.awaitBreakpoint(base::StrID("foo"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("foo"),
			})
			.finishAndAssertExitValue(7)
			.cleanup();
	}

	void test7RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_7/main.dbc"));
		const fs::File create_value_expr(path("runtime_expr_dbc/test_7/create_value.dbc"));
		const fs::File use_value_expr(path("runtime_expr_dbc/test_7/use_value.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 2)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(create_value_expr, { 42 })
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(use_value_expr, { 42 })
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(10)
			.cleanup();
	}

	void test8RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_8/main.dbc"));
		const fs::File expr(path("runtime_expr_dbc/test_8/expr.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("foo"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("foo"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("foo"),
			})
			.evalExprExpectBreakpoint(expr)
			.awaitBreakpoint(base::StrID("foo"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("foo"),
				base::StrID("call_foo"),
				base::StrID("foo"),
			})
			.resume()
			.awaitExprCompletion({ 13 })
			.awaitBreakpoint(base::StrID("foo"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("foo"),
			})
			.finishAndAssertExitValue(2)
			.cleanup();
	}

	void test9RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_9/main.dbc"));
		const fs::File frame_global_expr(path("runtime_expr_dbc/test_9/frame_global.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectLoadError(frame_global_expr, vm::code::UnknownLocalNameError::ERR_MSG)
			.cleanup();
	}

	void test10RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_10/main.dbc"));
		const fs::File frame_oob_expr(path("runtime_expr_dbc/test_10/frame_oob.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectLoadError(frame_oob_expr, vm::code::UnknownLocalNameError::ERR_MSG)
			.cleanup();
	}

	void test11RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_11/main.dbc"));
		const fs::File tail_rec_expr(path("runtime_expr_dbc/test_11/tail_rec.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectLoadError(tail_rec_expr, vm::code::ForbiddenOpcodePresent::ERR_MSG)
			.cleanup();
	}

	void test12RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_11/main.dbc"));
		const fs::File exit_expr(path("runtime_expr_dbc/test_12/exit.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectLoadError(exit_expr, vm::code::ForbiddenOpcodePresent::ERR_MSG)
			.cleanup();
	}

	void test13RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_11/main.dbc"));
		const fs::File ret_expr(path("runtime_expr_dbc/test_13/ret.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectLoadError(ret_expr, vm::code::ForbiddenOpcodePresent::ERR_MSG)
			.cleanup();
	}

	void test14RuntimeExpr() {
		loadInvalidDbc(
			"runtime_expr_dbc/test_14/ret_from_expr.dbc",
			{ vm::code::ForbiddenOpcodePresent::ERR_MSG }
		);
	}

	void test15RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_15/main.dbc"));
		const fs::File input_expr(path("runtime_expr_dbc/test_15/input.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprProvideInputAfter(input_expr, "42\n", 10, { 42 })
			.cleanup();
	}

	void test16RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_16/main.dbc"));
		const fs::File sleep_expr(path("runtime_expr_dbc/test_16/sleep.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectTimeout(sleep_expr)
			.provideInput("42\n")
			.awaitExprCompletion({ 42 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	void test17RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_17/main.dbc"));
		const fs::File outer_expr(path("runtime_expr_dbc/test_17/expr_outer.dbc"));
		const fs::File inner_expr(path("runtime_expr_dbc/test_17/expr_inner.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectBreakpoint(outer_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("outer"),
				base::StrID("pause_here"),
			})
			.evalExprNormal(inner_expr, { 106 })
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("outer"),
				base::StrID("pause_here"),
			})
			.resume()
			.awaitExprCompletion({ 204 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	void test18RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_18/main.dbc"));
		const fs::File level1_expr(path("runtime_expr_dbc/test_18/expr_level1.dbc"));
		const fs::File level2_expr(path("runtime_expr_dbc/test_18/expr_level2.dbc"));
		const fs::File level3_expr(path("runtime_expr_dbc/test_18/expr_level3.dbc"));
		const fs::File pause_too_file(path("runtime_expr_dbc/test_18/pause_here_too.dbc"));

		createSimulator({ main_file, pause_too_file })
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.putBreakpoint(base::StrID("pause_here_too"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectBreakpoint(level1_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("level1"),
				base::StrID("pause_here"),
			})
			.evalExprExpectBreakpoint(level2_expr)
			.awaitBreakpoint(base::StrID("pause_here_too"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("level1"),
				base::StrID("pause_here"),
				base::StrID("level2"),
				base::StrID("pause_here_too"),
			})
			.evalExprNormal(level3_expr, { 46 })
			.awaitBreakpoint(base::StrID("pause_here_too"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("level1"),
				base::StrID("pause_here"),
				base::StrID("level2"),
				base::StrID("pause_here_too"),
			})
			.resume()
			// `level2`: paused `l1_local` (10) + overwritten `l2_local` (30) + `l2_local` (30).
			.awaitExprCompletion({ 70 })
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("level1"),
				base::StrID("pause_here"),
			})
			.resume()
			.awaitExprCompletion({ 24 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	void test19RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_19/main.dbc"));
		const fs::File add_five_expr(path("runtime_expr_dbc/test_19/add_five.dbc"));

		auto simulator = createSimulator(main_file);
		simulator.runMain();
		simulator.evalExprExpectEvalError(
			add_five_expr, vm::code::EvaluatingExprOnRunningThreadError::ERR_MSG
		);

		// Give the thread time to enter `main`'s infinite loop. Without this the pause request can
		// be serviced while the thread is still in the synthetic start function.
		std::this_thread::sleep_for(std::chrono::milliseconds(2));

		simulator.pause(base::StrID("main"), 4)
			.resume()
			.evalExprExpectEvalError(
				add_five_expr, vm::code::EvaluatingExprOnRunningThreadError::ERR_MSG
			)
			.stop()
			.cleanup();
	}

	void test20RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_20/main.dbc"));
		const fs::File spin_expr(path("runtime_expr_dbc/test_20/spin.dbc"));
		const fs::File break_loop_expr(path("runtime_expr_dbc/test_20/break_loop.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectTimeout(spin_expr)
			.pause()
			.evalExprNormal(break_loop_expr, { 1 })
			.awaitExprCompletion({ 0 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	void test21RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_21/main.dbc"));
		const fs::File reads_local_expr(path("runtime_expr_dbc/test_21/reads_local.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 0)
			.putBreakpoint(base::StrID("main"), 2)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectLoadError(reads_local_expr, vm::code::UnknownLocalNameError::ERR_MSG)
			.resume()
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(reads_local_expr, { 7 })
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	void test22RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_22/main.dbc"));
		const fs::File create_value_expr(path("runtime_expr_dbc/test_22/create_value.dbc"));
		const fs::File use_value_expr(path("runtime_expr_dbc/test_22/use_value.dbc"));
		const fs::File simple_expr(path("runtime_expr_dbc/test_22/simple.dbc"));

		auto simulator = createSimulator(main_file);
		simulator.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(create_value_expr, { 42 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprExpectBreakpoint(use_value_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
				base::StrID("use_value"),
				base::StrID("pause_here"),
			});

		for (u64 i = 0; i < 1'000; i++)
			simulator.evalExprNormal(simple_expr, { 7 })
				.awaitBreakpoint(base::StrID("pause_here"), 0)
				.enforceCallStack({
					base::StrID("vm_start_function"),
					base::StrID("main"),
					base::StrID("use_value"),
					base::StrID("pause_here"),
				});

		simulator.resume()
			.awaitExprCompletion({ 42 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	void test23RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_23/main.dbc"));
		const fs::File multi_expr(path("runtime_expr_dbc/test_23/multi.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.evalExprNormal(multi_expr, { 11, 22, 33, 44 })
			.awaitBreakpoint(base::StrID("main"), 5)
			.enforceCallStack({
				base::StrID("vm_start_function"),
				base::StrID("main"),
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
