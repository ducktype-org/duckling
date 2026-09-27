#include "runtime_expr_dbc/runtime_expr_flow_simulator.hpp"

#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>

#include <string_view>
#include <vector>

class VmRuntimeExprTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRuntimeExprTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testArithmetic);
		TESTER_ADD_TEST(testBasic);
		TESTER_ADD_TEST(testVmValue);
		TESTER_ADD_TEST(testNestedExpressions);
		TESTER_ADD_TEST(testUseCases);
		TESTER_ADD_TEST(testInvalidExpressions);
		TESTER_ADD_TEST(testPointers);
		TESTER_ADD_TEST(testInput);
		TESTER_ADD_TEST(testReturnValues);
		TESTER_ADD_TEST(testRetFromExpr);
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
			.loadRuntimeExpr(get_values_expr)
			.awaitExprResult({ 0, 0 })
			.step(4)
			.loadRuntimeExpr(get_values_expr)
			.awaitExprResult({ 4, 2 })
			.step()
			// `base` exists but is still `u64`.
			.evalExprExpectLoadError(read_local, vm::code::ArgumentMismatchError::ERR_MSG)
			.step()
			// `base` has now been cast to `my_64`.
			.loadRuntimeExpr(read_local)
			.awaitExprResult({ 7 })
			.step(2)
			.loadRuntimeExpr(call_foo_expr)
			.awaitExprResult({ 7 })
			.resume()
			.awaitBreakpoint(base::StrID("foo"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("foo"), std::nullopt },
			})
			.loadRuntimeExpr(access_frame_2_expr)
			.awaitExprResult({ 10 })
			.loadRuntimeExpr(compare_frames_expr)
			.awaitExprResult({ 0 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 14)
			.loadRuntimeExpr(modify_value_expr)
			.awaitExprResult({ 42 })
			.loadRuntimeExpr(get_values_expr)
			.awaitExprResult({ 42, 2 })
			.disableBreakpoint(base::StrID("main"), 14)
			.resume()
			.evalExprExpectLoadError(ret_five, vm::code::EvaluatingExprOnRunningThreadError::ERR_MSG)
			.pause()
			.loadRuntimeExpr(ret_five)
			.awaitExprResult({ 5 })
			.stop()
			.cleanup();
	}

	void testVmValue() {
		const fs::File main_file(path("runtime_expr_dbc/vmvalues/main.dbc"));
		const fs::File create_value(path("runtime_expr_dbc/vmvalues/create_value.dbc"));
		const fs::File read_value(path("runtime_expr_dbc/vmvalues/read_value.dbc"));
		const fs::File ret_five(path("runtime_expr_dbc/basic/ret_five.dbc"));

		auto simulator = createSimulator(main_file);
		simulator.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.loadRuntimeExpr(create_value)
			.awaitExprResult({ 42 })
			.loadRuntimeExpr(read_value)
			.awaitExprResult({ 42 });

		// The value must survive many unrelated evaluations.
		for (u64 i = 0; i < 1'000; i++)
			simulator.loadRuntimeExpr(ret_five)
				.awaitExprResult({ 5 });

		simulator.loadRuntimeExpr(read_value)
			.awaitExprResult({ 42 })
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Arithmetic expressions evaluated repeatedly at main.
	void testArithmetic() {
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
			.loadRuntimeExpr(multiply)
			.awaitExprResult({ 16, 32, 64, 128 })
			.loadRuntimeExpr(add)
			.awaitExprResult({ 10, 18, 34, 66 })
			.loadRuntimeExpr(subtract)
			.awaitExprResult({ 6, 14, 30, 62 })
			.loadRuntimeExpr(divide)
			.awaitExprResult({ 4, 8, 16, 32 })
			.resume()
			.awaitBreakpoint(base::StrID("main"), 20)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.loadRuntimeExpr(multiply)
			.awaitExprResult({ 32, 64, 128, 256 })
			.loadRuntimeExpr(add)
			.awaitExprResult({ 12, 20, 36, 68 })
			.loadRuntimeExpr(subtract)
			.awaitExprResult({ 4, 12, 28, 60 })
			.loadRuntimeExpr(divide)
			.awaitExprResult({ 2, 4, 8, 16 })
			.finishAndAssertExitValue(0)
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
			.loadRuntimeExpr(timeout_input)
			.evalExprExpectTimeout()
			.provideInput("42\n")
			.awaitExprResult({ 42 })
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Runtime expressions nested inside runtime expressions: nested evaluation while the outer
	// pauses, LIFO unwinding, expression-frame inspection, and unwinding with anchors disabled.
	void testNestedExpressions() {
		const fs::File main_file(path("runtime_expr_dbc/nested/main.dbc"));
		const fs::File change_l1_local(path("runtime_expr_dbc/nested/change_l1_local.dbc"));
		const fs::File level1_expr(path("runtime_expr_dbc/nested/level1.dbc"));
		const fs::File level2_expr(path("runtime_expr_dbc/nested/level2.dbc"));
		const fs::File level3_expr(path("runtime_expr_dbc/nested/level3.dbc"));
		const fs::File probe_expr(path("runtime_expr_dbc/nested/probe.dbc"));

		const vm::test::FlowSimulator::FrameVars level1_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("l1_local"), base::StrID("i64") },
		};
		const vm::test::FlowSimulator::FrameVars level2_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("l2_local"), base::StrID("i64") },
		};
		const vm::test::FlowSimulator::FrameVars probe_vars{
			{ base::StrID("ret0"), base::StrID("i64") },
			{ base::StrID("local"), base::StrID("i64") },
		};

		const std::vector<vm::test::FlowSimulator::FrameExpectation> main_stack{
			{ base::StrID("vm_start_function"), startFunctionVars() },
			{ base::StrID("main"), std::nullopt },
		};

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.putBreakpoint(base::StrID("pause_here"), 0)
			.putBreakpoint(base::StrID("pause_here_too"), 0)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack(main_stack)

			// Two levels: a nested expression runs while `level1` is paused and mutates its frame.
			.evalExprExpectBreakpoint(level1_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.enforceFrameVarValue(2, base::StrID("l1_local"), 10)
			.loadRuntimeExpr(change_l1_local)
			.awaitExprResult({ 106 })
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.enforceFrameVarValue(2, base::StrID("l1_local"), 100)
			.resume()
			.awaitExprResult({ 204 })
			.enforceCallStack(main_stack)

			// Three levels: the first level's frame is inspected, then completions arrive LIFO.
			.evalExprExpectBreakpoint(level1_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.enforceFrameVarValue(2, base::StrID("l1_local"), 10)
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
			.loadRuntimeExpr(level3_expr)
			.awaitExprResult({ 46 })
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
			.awaitExprResult({ 70 })
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("level1"), level1_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.resume()
			.awaitExprResult({ 24 })
			.enforceCallStack(main_stack)

			// The expression's own frame is inspectable while it is paused.
			.evalExprExpectBreakpoint(probe_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("probe"), probe_vars },
				{ base::StrID("pause_here"), vm::test::FlowSimulator::FrameVars{} },
			})
			.enforceFrameVarValue(2, base::StrID("local"), 123)
			.resume()
			.awaitExprResult({ 123 })
			.enforceCallStack(main_stack)

			// Disabling the anchors does not stop nested expressions from unwinding.
			.evalExprExpectBreakpoint(level1_expr)
			.awaitBreakpoint(base::StrID("pause_here"), 0)
			.evalExprExpectBreakpoint(level2_expr)
			.awaitBreakpoint(base::StrID("pause_here_too"), 0)
			.disableBreakpoint(base::StrID("pause_here"), 0)
			.disableBreakpoint(base::StrID("pause_here_too"), 0)
			.loadRuntimeExpr(level3_expr)
			.awaitExprResult({ 46 })
			.resume()
			.awaitExprResult({ 70 })
			.resume()
			.awaitExprResult({ 24 })
			.enforceCallStack(main_stack)
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	// Practical use: a second expression writes into a spinning expression's frame to stop it.
	void testUseCases() {
		const fs::File main_file(path("runtime_expr_dbc/use_cases/main.dbc"));
		const fs::File spin_expr(path("runtime_expr_dbc/use_cases/spin.dbc"));
		const fs::File break_loop_expr(path("runtime_expr_dbc/use_cases/break_loop.dbc"));

		createSimulator(main_file)
			.putBreakpoint(base::StrID("main"), 4)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 4)
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.loadRuntimeExpr(spin_expr)
			.evalExprExpectTimeout()
			.pause()
			.step(20)
			.loadRuntimeExpr(break_loop_expr)
			.awaitExprResult({ 1 })
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
				{ base::StrID("spin"), std::nullopt },
			})
			.resume()
			.awaitExprResult({ 0 })
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
			.loadRuntimeExpr(multi)
			.awaitExprResult({ 11, 22, 33, 44 })
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.loadRuntimeExpr(make_pair)
			.awaitExprResultValues({ vm::test::ExpectedValue::structure({
				{ base::StrID("a"), 1 },
				{ base::StrID("b"), 2 },
			}) })
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.loadRuntimeExpr(make_variant)
			.awaitExprResultValues({ vm::test::ExpectedValue::variant(0, 5) })
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.loadRuntimeExpr(make_table)
			.awaitExprResultValues({ vm::test::ExpectedValue::table({ 10, 20, 30 }) })
			.enforceCallStack({
				{ base::StrID("vm_start_function"), startFunctionVars() },
				{ base::StrID("main"), std::nullopt },
			})
			.finishAndAssertExitValue(2'137)
			.cleanup();
	}

	/// A user step over `ret_from_expr` must not re-enter the breakpoint, while resuming from it
	/// must still pause afterwards.
	void testRetFromExpr() {
		const fs::File main_file(path("runtime_expr_dbc/ret_from_expr/main.dbc"));
		const fs::File stepped_expr(path("runtime_expr_dbc/ret_from_expr/stepped.dbc"));

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
			.awaitExprResult({ 42 })
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
			.awaitExprResult({ 42 })
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
			.evalExprExpectLoadError(
				expr("init_vmval_imm_in_expr.dbc"), vm::code::ForbiddenOpcodePresent::ERR_MSG
			)
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
			"runtime_expr_dbc/invalid_exprs/init_vmval_in_normal.dbc",
			{ vm::code::ForbiddenOpcodePresent::ERR_MSG, "initFromVMValue_pany_idvmval" }
		);
		loadInvalidDbc(
			"runtime_expr_dbc/invalid_exprs/init_vmval_imm_in_normal.dbc",
			{ vm::code::ForbiddenOpcodePresent::ERR_MSG, "initFromVMValue_pany_immvmval" }
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
			.loadRuntimeExpr(ptr_to_main)
			.awaitExprResultValues({ vm::test::ExpectedValue::nonNullPtr() })
			.loadRuntimeExpr(set_through_ptr)
			.awaitExprResult({ 99 })
			.enforceFrameVarValue(1, base::StrID("a"), 99)
			.finishAndAssertExitValue(2'137)
			.cleanup();

		fresh_main()
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.loadRuntimeExpr(ptr_to_global)
			.awaitExprResultValues({ vm::test::ExpectedValue::nonNullPtr() })
			.loadRuntimeExpr(set_through_ptr)
			.awaitExprResult({ 99 })
			.loadRuntimeExpr(read_global)
			.awaitExprResult({ 99 })
			.finishAndAssertExitValue(2'137)
			.cleanup();

		fresh_main()
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.loadRuntimeExpr(ptr_to_local)
			.awaitExprResultValues({ vm::test::ExpectedValue::nonNullPtr() })
			.evalExprExpectPanic(load_local, "Data was freed")
			.cleanup();

		// Heap allocation followed by an explicit `free`: the data is released (no leak).
		fresh_main()
			.putBreakpoint(base::StrID("main"), 5)
			.runMain()
			.awaitBreakpoint(base::StrID("main"), 5)
			.loadRuntimeExpr(freed)
			.awaitExprResult({ 7 })
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
