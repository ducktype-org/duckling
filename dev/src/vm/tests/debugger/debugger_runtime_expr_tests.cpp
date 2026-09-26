#include "runtime_expr_dbc/runtime_expr_flow_simulator.hpp"

#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>

#include <array>
#include <ranges>
#include <string_view>
#include <vector>

class VmRuntimeExprTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRuntimeExprTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(test2RuntimeExpr);
		TESTER_ADD_TEST(testBasic);
		TESTER_ADD_TEST(testVmValueReuse);
		TESTER_ADD_TEST(test22RuntimeExpr);
		TESTER_ADD_TEST(test8RuntimeExpr);
		TESTER_ADD_TEST(testInvalidExpressions);
		TESTER_ADD_TEST(testPointers);
		TESTER_ADD_TEST(testInput);
		TESTER_ADD_TEST(test11RuntimeExpr);
		TESTER_ADD_TEST(test12RuntimeExpr);
		TESTER_ADD_TEST(test14RuntimeExpr);
		TESTER_ADD_TEST(test16RuntimeExpr);
		TESTER_ADD_TEST(testReturnValues);
		TESTER_ADD_TEST(test18RuntimeExpr);
		TESTER_ADD_TEST(test20RuntimeExpr);
		TESTER_ADD_TEST(test27RuntimeExpr);
	}

private:
	/// The local variables always materialized by the synthetic `vm_start_function`. Once the
	/// thread is paused inside a call, only these five slots are still owned by the frame: the
	/// two argument slots (`argc`, `argv`) have been handed over to `main`.
	static const vm::test::FlowSimulator::FrameVars& startFunctionVars() {
		static const vm::test::FlowSimulator::FrameVars vars{
			{ base::StrID("ret_value"), base::StrID("i64") },
			{ base::StrID("argv_internal"), base::StrID("ptr_argv") },
			{ base::StrID("argc_internal"), base::StrID("i64") },
			{ base::StrID("ix"), base::StrID("i64") },
			{ base::StrID("main_ret_val"), base::StrID("i64") },
		};
		return vars;
	}

	vm::test::FlowSimulator createSimulator(const fs::File& file) {
		return createSimulator(std::vector<fs::File>{ file });
	}

	vm::test::FlowSimulator createSimulator(const std::vector<fs::File>& files) {
		auto process_pid_response = vm::api::spawn();
		if (!process_pid_response)
			assertTrue(
				false,
				base::strConcat(
					"Spawn failed: ", vm::api::errorToString(process_pid_response.error())
				)
			);
		auto pid = process_pid_response.value().pid;

		auto loaded_file_response = vm::api::loadFiles(pid, files);
		if (!loaded_file_response)
			assertTrue(false, vm::api::errorToString(loaded_file_response.error()));
		return { [this](bool cond, std::string_view err) { assertTrue(cond, err); }, pid };
	}

	void testBasic() {
		const fs::File main_file(path("runtime_expr_dbc/basic/main.dbc"));
		const fs::File get_values_expr(path("runtime_expr_dbc/basic/get_values.dbc"));
		const fs::File call_foo_expr(path("runtime_expr_dbc/basic/call_foo.dbc"));
		const fs::File modify_value_expr(path("runtime_expr_dbc/basic/modify_value.dbc"));
		const fs::File access_frame_2_expr(path("runtime_expr_dbc/basic/access_frame_2.dbc"));
		const fs::File compare_frames_expr(path("runtime_expr_dbc/basic/compare_frames.dbc"));
		const fs::File ret_five(path("runtime_expr_dbc/basic/ret_five.dbc"));
		const fs::File read_local(path("runtime_expr_dbc/basic/read_local.dbc"));

		auto simulator = createSimulator(main_file);
		simulator.putBreakpoint(base::StrID("main"), 0)
			.putBreakpoint(base::StrID("foo"), 4)
			.putBreakpoint(base::StrID("main"), 14)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			// `base` has not been initialized yet.
			.evalExprExpectLoadError(read_local, vm::code::UnknownLocalNameError::ERR_MSG)
			.step(2)
			.evalExprNormal(get_values_expr, { 0, 0 })
			.step(4)
			.evalExprNormal(get_values_expr, { 4, 2 })
			.step()
			// `base` exists but is still `u64`.
			.evalExprExpectLoadError(read_local, vm::code::ArgumentMismatchError::ERR_MSG)
			.step()
			// `base` has now been cast to `my_64`.
			.evalExprNormal(read_local, { 7 })
			.step(2)
			.evalExprNormal(call_foo_expr, { 7 })
			.resume()
			.awaitBreakpoint(base::StrID("foo"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("foo"), std::nullopt },
			})
			.evalExprNormal(access_frame_2_expr, { 10 })
			.evalExprNormal(compare_frames_expr, { 0 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 14)
			.evalExprNormal(modify_value_expr, { 42 })
			.awaitBreakpoint(base::StrID("main"), 14)
			.evalExprNormal(get_values_expr, { 42, 2 })
			.awaitBreakpoint(base::StrID("main"), 14)
			.disableBreakpoint(base::StrID("main"), 14)
			.resume()
			.evalExprExpectEvalError(ret_five, vm::code::EvaluatingExprOnRunningThreadError::ERR_MSG)
			.sleep(20)
			.pause(base::StrID("main"), 14)
			.resume()
			.evalExprExpectEvalError(ret_five, vm::code::EvaluatingExprOnRunningThreadError::ERR_MSG)
			.stop()
			.cleanup();
	}

	void testVmValueReuse() {
		const fs::File main_file(path("runtime_expr_dbc/vmvalue_reuse/main.dbc"));
		const fs::File create_value(path("runtime_expr_dbc/vmvalue_reuse/create_value.dbc"));
		const fs::File use_value(path("runtime_expr_dbc/vmvalue_reuse/use_value.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 2)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 2)
			.evalExprNormal(create_value, { 42 })
			.awaitBreakpoint(base::StrID("main"), 2)
			.evalExprNormal(use_value, { 42 })
			.awaitBreakpoint(base::StrID("main"), 2)
			.finishAndAssertExitValue(10)
			.cleanup();
	}

	// Arithmetic expressions evaluated repeatedly at main.
	void test2RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/arithmetic/main.dbc"));
		const fs::File multiply(path("runtime_expr_dbc/arithmetic/expr/multiply.dbc"));
		const fs::File add(path("runtime_expr_dbc/arithmetic/expr/add.dbc"));
		const fs::File subtract(path("runtime_expr_dbc/arithmetic/expr/subtract.dbc"));
		const fs::File divide(path("runtime_expr_dbc/arithmetic/expr/divide.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 16)
			.putBreakpoint(base::StrID("main"), 20)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 16)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprNormal(multiply, { 16, 32, 64, 128 })
			.evalExprNormal(add, { 10, 18, 34, 66 })
			.evalExprNormal(subtract, { 6, 14, 30, 62 })
			.evalExprNormal(divide, { 4, 8, 16, 32 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 20)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprNormal(multiply, { 32, 64, 128, 256 })
			.evalExprNormal(add, { 12, 20, 36, 68 })
			.evalExprNormal(subtract, { 4, 12, 28, 60 })
			.evalExprNormal(divide, { 2, 4, 8, 16 })
			.finishAndAssertExitValue(0)
			.cleanup();
	}

	// Expression hits a breakpoint, resumes, then completes.
	void test8RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_8/main.dbc"));
		const fs::File expr(path("runtime_expr_dbc/test_8/expr.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("foo"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("foo"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("foo"), std::nullopt },
			})
			.evalExprExpectBreakpoint(expr)
			.awaitBreakpoint(base::StrID("foo"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("foo"), std::nullopt },
				{ base::StrID("call_foo"), std::nullopt },
				{ base::StrID("foo"), std::nullopt },
			})
			.resume()
			.awaitExprCompletion({ 13 })
			.awaitBreakpoint(base::StrID("foo"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("foo"), std::nullopt },
			})
			.finishAndAssertExitValue(2)
			.cleanup();
	}

	void testInput() {
		const fs::File main_file(path("runtime_expr_dbc/input/main.dbc"));
		const fs::File immediate_input(path("runtime_expr_dbc/input/immediate.dbc"));
		const fs::File timeout_input(path("runtime_expr_dbc/input/timeout.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprProvideInputAfter(immediate_input, "42\n", 10, { 42 })
			.cleanup();

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectTimeout(timeout_input)
			.provideInput("42\n")
			.awaitExprCompletion({ 42 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Inner expression evaluated while outer paused.
	void test11RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_11/main.dbc"));
		const fs::File outer_expr(path("runtime_expr_dbc/test_11/expr_outer.dbc"));
		const fs::File inner_expr(path("runtime_expr_dbc/test_11/expr_inner.dbc"));

		const vm::test::FlowSimulator::FrameVars outer_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("outer_local"), base::StrID("i64") },
		};

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectBreakpoint(outer_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("outer"), outer_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.evalExprNormal(inner_expr, { 106 })
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("outer"), outer_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.resume()
			.awaitExprCompletion({ 204 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Three nested expressions complete in LIFO order.
	void test12RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_12/main.dbc"));
		const fs::File level1_expr(path("runtime_expr_dbc/test_12/expr_level1.dbc"));
		const fs::File level2_expr(path("runtime_expr_dbc/test_12/expr_level2.dbc"));
		const fs::File level3_expr(path("runtime_expr_dbc/test_12/expr_level3.dbc"));
		const fs::File pause_too_file(path("runtime_expr_dbc/test_12/pause_here_too.dbc"));

		const vm::test::FlowSimulator::FrameVars level1_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("l1_local"), base::StrID("i64") },
		};
		const vm::test::FlowSimulator::FrameVars level2_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("l2_local"), base::StrID("i64") },
		};

		createSimulator({ main_file, pause_too_file })
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.putBreakpoint(base::StrID("pause_here_too"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectBreakpoint(level1_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.evalExprExpectBreakpoint(level2_expr)
			.awaitBreakpoint(base::StrID("pause_here_too"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
				{ base::StrID("level2"), level2_vars },
				{ base::StrID("pause_here_too"), vm::test::FlowSimulator::FrameVars{} },
			})
			.evalExprNormal(level3_expr, { 46 })
			.awaitBreakpoint(base::StrID("pause_here_too"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
				{ base::StrID("level2"), level2_vars },
				{ base::StrID("pause_here_too"), vm::test::FlowSimulator::FrameVars{} },
			})
			.resume()
			// `level2`: paused `l1_local` (10) + overwritten `l2_local` (30) + `l2_local` (30).
			.awaitExprCompletion({ 70 })
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.resume()
			.awaitExprCompletion({ 24 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Break-loop expression stops a running spin expression.
	void test14RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_14/main.dbc"));
		const fs::File spin_expr(path("runtime_expr_dbc/test_14/spin.dbc"));
		const fs::File break_loop_expr(path("runtime_expr_dbc/test_14/break_loop.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectTimeout(spin_expr)
			.pause()
			.evalExprNormal(break_loop_expr, { 1 })
			.sleep(20)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("spin"), std::nullopt },
			})
			.resume()
			.awaitExprCompletion({ 0 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// VMValues remain valid (no memory relocation).
	void test16RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_16/main.dbc"));
		const fs::File create_value_expr(path("runtime_expr_dbc/test_16/create_value.dbc"));
		const fs::File use_value_expr(path("runtime_expr_dbc/test_16/use_value.dbc"));
		const fs::File simple_expr(path("runtime_expr_dbc/test_16/simple.dbc"));

		const vm::test::FlowSimulator::FrameVars use_value_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
		};

		auto simulator = createSimulator(main_file);
		simulator.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprNormal(create_value_expr, { 42 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectBreakpoint(use_value_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("use_value"), use_value_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			});

		for (u64 i = 0; i < 1'000; i++)
			simulator.evalExprNormal(simple_expr, { 7 })
				.awaitBreakpoint(base::StrID("pause_here"), 0)
				.enforceCallStack({
					{ base::StrID("vm_start_function"), startFunctionVars() },
					{ base::StrID("main"), std::nullopt },
					{ base::StrID("use_value"), use_value_vars },
					{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
				});

		simulator.resume()
			.awaitExprCompletion({ 42 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	void testReturnValues() {
		const fs::File main_file(path("runtime_expr_dbc/return_values/main.dbc"));
		const fs::File multi(path("runtime_expr_dbc/return_values/multi.dbc"));
		const fs::File make_pair(path("runtime_expr_dbc/return_values/make_pair.dbc"));
		const fs::File make_variant(path("runtime_expr_dbc/return_values/make_variant.dbc"));
		const fs::File make_table(path("runtime_expr_dbc/return_values/make_table.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 2)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprNormal(multi, { 11, 22, 33, 44 })
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectValues(
				make_pair,
				{ vm::test::ExpectedValue::structure({
					{ base::StrID("a"), 1 },
					{ base::StrID("b"), 2 },
				}) }
			)
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectValues(make_variant, { vm::test::ExpectedValue::variant(0, 5) })
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectValues(make_table, { vm::test::ExpectedValue::table({ 10, 20, 30 }) })
			.awaitBreakpoint(base::StrID("main"), 2)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Nested expressions unwind with breakpoints disabled - expressions still stop.
	void test18RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_18/main.dbc"));
		const fs::File level1_expr(path("runtime_expr_dbc/test_18/expr_level1.dbc"));
		const fs::File level2_expr(path("runtime_expr_dbc/test_18/expr_level2.dbc"));
		const fs::File level3_expr(path("runtime_expr_dbc/test_18/expr_level3.dbc"));
		const fs::File pause_too_file(path("runtime_expr_dbc/test_18/pause_here_too.dbc"));

		const vm::test::FlowSimulator::FrameVars level1_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("l1_local"), base::StrID("i64") },
		};
		const vm::test::FlowSimulator::FrameVars level2_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("l2_local"), base::StrID("i64") },
		};

		createSimulator({ main_file, pause_too_file })
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.putBreakpoint(base::StrID("pause_here_too"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectBreakpoint(level1_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.evalExprExpectBreakpoint(level2_expr)
			.awaitBreakpoint(base::StrID("pause_here_too"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
				{ base::StrID("level2"), level2_vars },
				{ base::StrID("pause_here_too"), vm::test::FlowSimulator::FrameVars{} },
			})
			.disableBreakpoint(base::StrID("pause_here"), 0)
			.disableBreakpoint(base::StrID("pause_here_too"), 0)
			.evalExprNormal(level3_expr, { 46 })
			.awaitBreakpoint(base::StrID("pause_here_too"), 0)
			.resume()
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.resume()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.awaitExprCompletion({ 70 })
			.awaitExprCompletion({ 24 })
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Breakpoint can be put in expression
	void test20RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_20/main.dbc"));
		const fs::File level1_expr(path("runtime_expr_dbc/test_20/expr_level1.dbc"));

		const vm::test::FlowSimulator::FrameVars level1_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("l1_local"), base::StrID("i64") },
		};

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.evalExprExpectBreakpoint(level1_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.resume()
			.awaitExprCompletion({ 24 })
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// We can view and access locals of the expression.
	void test22RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_22/main.dbc"));
		const fs::File probe_expr(path("runtime_expr_dbc/test_22/probe.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 2)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 2)
			.evalExprExpectBreakpoint(probe_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceFrameVarValue(2, base::StrID("local"), 123)
			.cleanup();
	}

	/// A user step over `ret_from_expr` must not re-enter the breakpoint, while resuming from it
	/// must still pause afterwards.
	void test27RuntimeExpr() {
		const fs::File main_file(path("runtime_expr_dbc/test_27/main.dbc"));
		const fs::File stepped_expr(path("runtime_expr_dbc/test_27/stepped.dbc"));

		// Stepping over `ret_from_expr` must not re-enter the breakpoint.
		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.evalExprExpectBreakpoint(stepped_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.step()
			.awaitBreakpoint(base::StrID("stepped"), 1)
			.step()
			.awaitBreakpoint(base::StrID("stepped"), 2)
			.step()
			.awaitBreakpoint(base::StrID("main"), 4)
			.awaitExprCompletion({ 42 })
			.finishAndAssertExitValue(2'137)
			.cleanup();

		// Resuming from `ret_from_expr` must still pause afterwards. `main:4` carries no
		// breakpoint, so the post-evaluation pause is the only thing that can stop the thread.
		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 3)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 3)
			.step()
			.awaitBreakpoint(base::StrID("main"), 4)
			.evalExprExpectBreakpoint(stepped_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.step()
			.awaitBreakpoint(base::StrID("stepped"), 1)
			.step()
			.awaitBreakpoint(base::StrID("stepped"), 2)
			.resume()
			.awaitBreakpoint(base::StrID("main"), 4)
			.awaitExprCompletion({ 42 })
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Every invalid runtime expression is rejected.
	void testInvalidExpressions() {
		const fs::File main_file(path("runtime_expr_dbc/invalid_exprs/main.dbc"));

		auto expr = [this](const char* name) {
			return fs::File(path(std::string("runtime_expr_dbc/invalid_exprs/") + name));
		};

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 2)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 2)
			.evalExprExpectLoadError(
				expr("frame_global.dbc"), vm::code::UnknownLocalNameError::ERR_MSG
			)
			.evalExprExpectLoadError(expr("frame_oob.dbc"), vm::code::UnknownLocalNameError::ERR_MSG)
			.evalExprExpectLoadError(
				expr("cast_prev.dbc"), vm::code::CannotCastPreviousFrameVariableError::ERR_MSG
			)
			.evalExprExpectLoadError(expr("tail_rec.dbc"), vm::code::ForbiddenOpcodePresent::ERR_MSG)
			.evalExprExpectLoadError(expr("exit.dbc"), vm::code::ForbiddenOpcodePresent::ERR_MSG)
			.evalExprExpectLoadError(expr("ret.dbc"), vm::code::ForbiddenOpcodePresent::ERR_MSG)
			.evalExprExpectLoadError(
				expr("sig_param.dbc"), vm::code::InvalidRuntimeExprSignature::ERR_MSG
			)
			.evalExprExpectLoadError(
				expr("bad_vmval_id.dbc"), vm::code::InvalidVMValueIDError::ERR_MSG
			)
			.evalExprExpectLoadError(expr("no_function.dbc"), "No function defined in the file")
			.evalExprExpectLoadError(expr("two_functions.dbc"), "More than one function in the file")
			.evalExprExpectLoadError(expr("global.dbc"), "There is a global declaration in the file")
			.evalExprExpectLoadError(expr("type.dbc"), "There is a type declaration in the file")
			.cleanup();

		// Normal-mode (program) functions reject the expression-only opcodes / places.
		loadInvalidDbc(
			"runtime_expr_dbc/invalid_exprs/ret_from_expr_in_normal.dbc",
			{ vm::code::ForbiddenOpcodePresent::ERR_MSG }
		);
		loadInvalidDbc(
			"runtime_expr_dbc/invalid_exprs/init_vmval_in_normal.dbc",
			{ vm::code::ForbiddenOpcodePresent::ERR_MSG, "init_pany_vmval" }
		);
		loadInvalidDbc(
			"runtime_expr_dbc/invalid_exprs/exit_in_normal.dbc",
			{ vm::code::ForbiddenOpcodePresent::ERR_MSG, "exit" }
		);
		loadInvalidDbc(
			"runtime_expr_dbc/invalid_exprs/frame_in_normal.dbc",
			{ vm::code::FrameSpecifierWithoutRuntimeThread::ERR_MSG }
		);
	}

	// Frame, global, dangling pointers; alloc, free, leak.
	void testPointers() {
		const fs::File main_file(path("runtime_expr_dbc/pointers/main.dbc"));
		const fs::File ptr_to_main(path("runtime_expr_dbc/pointers/ptr_to_main.dbc"));
		const fs::File ptr_to_global(path("runtime_expr_dbc/pointers/ptr_to_global.dbc"));
		const fs::File ptr_to_local(path("runtime_expr_dbc/pointers/ptr_to_local.dbc"));
		const fs::File set_through_ptr(path("runtime_expr_dbc/pointers/set_through_ptr.dbc"));
		const fs::File read_global(path("runtime_expr_dbc/pointers/read_global.dbc"));
		const fs::File load_local(path("runtime_expr_dbc/pointers/load_local.dbc"));
		const fs::File freed(path("runtime_expr_dbc/pointers/freed.dbc"));
		const fs::File leaky(path("runtime_expr_dbc/pointers/leaky.dbc"));

		auto fresh_main = [&] { return createSimulator(main_file); };

		fresh_main()
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.enforceFrameVarValue(1, base::StrID("a"), 4)
			.evalExprExpectValues(ptr_to_main, { vm::test::ExpectedValue::nonNullPtr() })
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprNormal(set_through_ptr, { 99 })
			.awaitBreakpoint(base::StrID("main"), 5)
			.enforceFrameVarValue(1, base::StrID("a"), 99)
			.finishAndAssertExitValue(2'137)
			.cleanup();

		fresh_main()
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprExpectValues(ptr_to_global, { vm::test::ExpectedValue::nonNullPtr() })
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprNormal(set_through_ptr, { 99 })
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprNormal(read_global, { 99 })
			.awaitBreakpoint(base::StrID("main"), 5)
			.finishAndAssertExitValue(2'137)
			.cleanup();

		fresh_main()
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprExpectValues(ptr_to_local, { vm::test::ExpectedValue::nonNullPtr() })
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprExpectPanic(load_local, "Data was freed")
			.cleanup();

		// Heap allocation followed by an explicit `free`: the data is released (no leak).
		fresh_main()
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprNormal(freed, { 7 })
			.awaitBreakpoint(base::StrID("main"), 5)
			.finishAndAssertExitValue(2'137)
			.cleanup();

		//  Heap allocation without a `free`: the process panics on the leaked block.
		fresh_main()
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.evalExprExpectPanic(leaky, vm::exceptions::VMFoundMemoryLeakException::ERR_MSG)
			.cleanup();
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
