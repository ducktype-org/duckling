#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

class VmRuntimeExprTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRuntimeExprTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(test1RuntimeExpr);
		TESTER_ADD_TEST(test2RuntimeExpr);
	}

private:
	void test1RuntimeExpr() {
		constexpr std::string_view sum_a_b_expr    = "runtime_expr_dbc/test_1/sum_a_b.dbc";
		constexpr std::string_view print_ret_expr  = "runtime_expr_dbc/test_1/print_ret.dbc";
		constexpr std::string_view modify_ret_expr = "runtime_expr_dbc/test_1/modify_ret.dbc";

		auto pid = spawnAndLoad("runtime_expr_dbc/test_1/main.dbc");
		ASSERT_HAS_VALUE(vm::api::setBreakpoint(pid, base::StrID("main"), 5, true));
		ASSERT_HAS_VALUE(vm::api::setBreakpoint(pid, base::StrID("main"), 9, true));

		ASSERT_HAS_VALUE(vm::api::run(pid));

		// First breakpoint at instruction 5 (after init a, b, c; mov a, 0; mov b, 0)
		auto execution_position = vm::api::waitForBreakpoint(pid);
		ASSERT_HAS_VALUE(execution_position);
		ASSERT_EQUAL_PRINT(5, execution_position->instr_number);

		auto tid = vm::api::ThreadID(0);

		// eval sum_a_b.dbc (a=0, b=0 -> 0)
		executeRuntimeExprAndAssertResult(pid, tid, sum_a_b_expr, { 0 });

		// eval print_ret.dbc (ret0 uninitialized/0)
		executeRuntimeExprAndAssertResult(pid, tid, print_ret_expr, { 0 });

		// eval modify_ret.dbc (sets ret0 to 2137, returns previous ret0 value 0)
		executeRuntimeExprAndAssertResult(pid, tid, modify_ret_expr, { 0 });

		// Resume to next breakpoint
		ASSERT_HAS_VALUE(vm::api::resume(pid));

		// Second breakpoint at instruction 9 (after a=4, b=2, c=0, ret0=69)
		execution_position = vm::api::waitForBreakpoint(pid);
		ASSERT_HAS_VALUE(execution_position);
		ASSERT_EQUAL_PRINT(9, execution_position->instr_number);

		// eval sum_a_b.dbc (a=4, b=2 -> 6)
		executeRuntimeExprAndAssertResult(pid, tid, sum_a_b_expr, { 6 });

		// eval print_ret.dbc (ret0 was set to 69)
		executeRuntimeExprAndAssertResult(pid, tid, print_ret_expr, { 69 });

		// eval modify_ret.dbc (reads 69, sets ret0 to 2137)
		executeRuntimeExprAndAssertResult(pid, tid, modify_ret_expr, { 69 });

		// Resume and finish
		ASSERT_HAS_VALUE(vm::api::resume(pid));
		ASSERT_HAS_VALUE(vm::api::join(pid));

		// Exit value should be 2137 since modify_ret modified ret0 of main
		auto exit_code_response = vm::api::getExitValue(pid);
		ASSERT_HAS_VALUE(exit_code_response);
		auto& exit_value_vec = std::get<std::vector<Ref<vm::IVMValue>>>(exit_code_response.value());
		ASSERT_EQUAL(exit_value_vec.size(), 1);
		ASSERT_EQUAL_PRINT(exit_value_vec.at(0)->readBytes<i64>(), 2'137);
	}

	void test2RuntimeExpr() {
		constexpr std::string_view call_foo_unused_args_expr
			= "runtime_expr_dbc/test_2/expr/call_foo_unused_args.dbc";
		constexpr std::string_view modify_unused_arg_1_expr
			= "runtime_expr_dbc/test_2/expr/modify_unused_arg_1.dbc";
		constexpr std::string_view print_foo_ret0_expr
			= "runtime_expr_dbc/test_2/expr/print_foo_ret0.dbc";

		auto pid = spawnAndLoad("runtime_expr_dbc/test_2/main.dbc");
		// Breakpoint before first call foo (at instruction 7: mov foo_arg0, 1)
		ASSERT_HAS_VALUE(vm::api::setBreakpoint(pid, base::StrID("main"), 7, true));
		// Breakpoint after first call foo (at instruction 10: add acc, foo_ret0)
		ASSERT_HAS_VALUE(vm::api::setBreakpoint(pid, base::StrID("main"), 10, true));

		ASSERT_HAS_VALUE(vm::api::run(pid));

		auto tid = vm::api::ThreadID(0);

		// First breakpoint at instruction 7 (unused_arg0=67, unused_arg1=42)
		auto execution_position = vm::api::waitForBreakpoint(pid);
		ASSERT_HAS_VALUE(execution_position);
		ASSERT_EQUAL_PRINT(7, execution_position->instr_number);

		// eval call_foo_unused_args.dbc (calls foo with unused_arg0=67 and unused_arg1=42 ->
		// returns 67+42=109)
		executeRuntimeExprAndAssertResult(pid, tid, call_foo_unused_args_expr, { 109 });

		// eval modify_unused_arg_1.dbc (reads 42, sets unused_arg1 = 42 * 2 = 84)
		executeRuntimeExprAndAssertResult(pid, tid, modify_unused_arg_1_expr, { 42 });

		// eval call_foo_unused_args.dbc again (calls foo with unused_arg0=67 and unused_arg1=84 ->
		// returns 67+84=151)
		executeRuntimeExprAndAssertResult(pid, tid, call_foo_unused_args_expr, { 151 });

		// Resume to second breakpoint (instruction 10)
		ASSERT_HAS_VALUE(vm::api::resume(pid));

		execution_position = vm::api::waitForBreakpoint(pid);
		ASSERT_HAS_VALUE(execution_position);
		ASSERT_EQUAL_PRINT(10, execution_position->instr_number);

		// eval print_foo_ret0.dbc (reads acc, which is currently 0 before add acc, foo_ret0)
		executeRuntimeExprAndAssertResult(pid, tid, print_foo_ret0_expr, { 0 });

		// Resume to completion
		ASSERT_HAS_VALUE(vm::api::resume(pid));
		ASSERT_HAS_VALUE(vm::api::join(pid));
	}

	vm::PID spawnAndLoad(std::string_view path_name) {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (spawnAndLoad)");
		auto pid = process_pid_response.value().pid;

		fs::File file(path(std::string(path_name)));
		auto     loaded_file_response = vm::api::loadFiles(pid, { file });
		assertTrue(loaded_file_response.has_value(), "Load failed (spawnAndLoad)");
		return pid;
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
