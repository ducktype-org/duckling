#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/process/interface_types.hpp>

#include <chrono>
#include <thread>

class VmUnitTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmUnitTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(globalInitializationTest);
		TESTER_ADD_TEST(globalsTest);
		TESTER_ADD_TEST(commandLineArguments);
		TESTER_ADD_TEST(jump);
		TESTER_ADD_TEST(return1337);
		TESTER_ADD_TEST(initPrimitivesWithZero);
		TESTER_ADD_TEST(check8BitsInstructions);
		TESTER_ADD_TEST(check16BitsInstructions);
		TESTER_ADD_TEST(check32BitsInstructions);
		TESTER_ADD_TEST(check64BitsInstructions);
		TESTER_ADD_TEST(checkMultipleRetVals);
		TESTER_ADD_TEST(checkVoidTypeValid);
		TESTER_ADD_TEST(pointerTest);
		TESTER_ADD_TEST(globalsInitializationTest);
		TESTER_ADD_TEST(globalDestructorTest);
		TESTER_ADD_TEST(globalNoConstructorTest);
		TESTER_ADD_TEST(globalNoDestructorTest);
		TESTER_ADD_TEST(verySimpleUnsignedTest);
		TESTER_ADD_TEST(verySimpleBooleanTest);
		TESTER_ADD_TEST(literalsTest);
		TESTER_ADD_TEST(checkLiteralErrorHandling);
		TESTER_ADD_TEST(checkZeroDivision);
		TESTER_ADD_TEST(invalidPrimitiveTypes);
		TESTER_ADD_TEST(checkCastingInstructions);
		TESTER_ADD_TEST(testSyncRun);
		TESTER_ADD_TEST(joinReturnsExitValue);
		TESTER_ADD_TEST(deinitNeedsJoinedThreads);
		TESTER_ADD_TEST(structureOperations);
		TESTER_ADD_TEST(fixedSizeTableOperations);
		TESTER_ADD_TEST(nestedAggregateTypesCorrectness);
		TESTER_ADD_TEST(globalInitialValueTest);
	}

private:
	void checkCastingInstructions() { runTestOnVm("casting.dbc", "", "11111111111", {}); }

	void jump() { runTestOnVm("jump.dbc", "", "5", {}); }

	void return1337() { runTestOnVm("return_1337.dbc", {}, {}, {}, 1'337); }

	void initPrimitivesWithZero() { runTestOnVm("init_primitives_with_zero.dbc", "", "0", {}); }

	void check8BitsInstructions() { runTestOnVm("8bits.dbc", "", "1", {}); }

	void check16BitsInstructions() { runTestOnVm("16bits.dbc", "", "1", {}); }

	void check32BitsInstructions() { runTestOnVm("32bits.dbc", "", "11", {}); }

	void check64BitsInstructions() { runTestOnVm("64bits.dbc", "", "11", {}); }

	void checkMultipleRetVals() { runTestOnVm("multiple_retvals.dbc", "", "21373315", {}); }

	void checkVoidTypeValid() { runTestOnVm("valid_void_type.dbc", "", "2", {}); }

	void pointerTest() {
		for (auto filename:
		     { "pointer_to_local.dbc", "pointer_copy.dbc", "pointer_to_passed_blocks.dbc" }) {
			runTestOnVm(filename, "", "42");
		}
		runTestOnVm("pointer_to_global.dbc", {}, "429913371337", {}, 1'337);
	}

	void commandLineArguments() {
		runTestOnVm("command_line_args.dbc", "", "10", { "1", "2", "3", "4" }, 0);
	}

	void globalsTest() { runTestOnVm("globals.dbc", {}, "5", {}, 5); }

	void globalInitializationTest() { runTestOnVm("global_initialization.dbc", {}, {}, {}, 5); }

	void globalsInitializationTest() { runTestOnVm("globals_initialization.dbc", {}, {}, {}, 7); }

	void globalDestructorTest() { runTestOnVm("global_destructor.dbc", {}, {}, {}, 5); }

	void globalNoConstructorTest() {
		loadInvalidDbc(
			"global_no_constructor.dbc",
			{
				vm::code::MissingGlobalCtorDtorError::ERR_MSG,
			}
		);
	}

	void globalNoDestructorTest() {
		loadInvalidDbc(
			"global_no_destructor.dbc",
			{
				vm::code::MissingGlobalCtorDtorError::ERR_MSG,
			}
		);
	}

	void literalsTest() {
		runTestOnVm("literals_test_32.dbc", "", "1", {});
		runTestOnVm("literals_test_64.dbc", "", "1", {});
	}

	void verySimpleUnsignedTest() { runTestOnVm("very_simple_unsigned.dbc", "", "1235", {}); }

	void verySimpleBooleanTest() { runTestOnVm("very_simple_boolean.dbc", "", "1", {}); }

	void checkZeroDivision() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("zero_division_i64.dbc", "", "0"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("zero_division_i32.dbc", "", "0"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
	}

	void checkLiteralErrorHandling() {
		loadInvalidDbc(
			"invalid_literal.dbc",
			{
				"Numeric literal overflows a 32-bit signed integer",
				"Numeric literal underflows a 32-bit signed integer",
				"Numeric literal overflows a 32-bit unsigned integer",
				"Numeric literal overflows a 64-bit signed integer",
				"Floating-point literals must be in decimal base for",
			}
		);
	}

	void invalidPrimitiveTypes() {
		loadInvalidDbc(
			"size_zero_primitive.dbc",
			{
				vm::code::InvalidPrimitiveSizeError::ERR_MSG,
			}
		);
	}

	void structureOperations() { runTestOnVm("structure_operations.dbc", "", "506", {}); }

	void fixedSizeTableOperations() {
		runTestOnVm("fixed_size_table_operations.dbc", "", "123", {});
	}

	void nestedAggregateTypesCorrectness() {
		runTestOnVm("nested_aggregate.dbc", "", "133707770999", {});
	}

	void globalInitialValueTest() {
		runTestOnVm("global_initial_value.dbc", "", "10\n-10\n/1\n-1\no", {});
	}

	void testSyncRun() {
		vm::PID pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path("simple_function.dbc")) }));
		runFunctionSynchronouslyAsTest(pid, "foo", {}, "", "120", 123);
		ASSERT_HAS_VALUE(vm::api::deinitAndValidate(pid));
	}

	/**
	 * @brief `api::join` is the endpoint that reports the exit value of a finished run, so a plain
	 * `run` + `join` has to hand back what `main` returned. Joining a process that never ran has no
	 * exit value to report and must fail instead.
	 */
	void joinReturnsExitValue() {
		vm::PID pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path("return_1337.dbc")) }));

		ASSERT_NO_VALUE(vm::api::join(pid));

		ASSERT_HAS_VALUE(vm::api::run(pid));

		auto exit_value = vm::api::join(pid);
		ASSERT_HAS_VALUE(exit_value);
		ASSERT_TRUE(std::holds_alternative<std::vector<Ref<vm::IVMValue>>>(exit_value.value()));
		const auto& exit_values = std::get<std::vector<Ref<vm::IVMValue>>>(exit_value.value());
		ASSERT_EQUAL(exit_values.size(), 1);
		ASSERT_EQUAL_PRINT(exit_values.at(0)->readBytes<i64>(), 1'337);

		auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value());
	}

	/**
	 * Deinit of a finished but unjoined run has to be refused, and the very same deinit has to work
	 * right after the join.
	 */
	void deinitNeedsJoinedThreads() {
		vm::PID pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path("return_1337.dbc")) }));
		ASSERT_HAS_VALUE(vm::api::run(pid));

		// `run` is asynchronous, so we wait for the program to really finish. Only then is a
		// refusal about the missing join and not about the process still executing.
		vm::api::ProcStatus status = vm::api::NotStarted{};
		for (usize tries = 0; tries < 1'000 && !vm::api::isStatusTerminal(status); tries++) {
			const auto polled = vm::api::getExecutionStatus(pid);
			ASSERT_HAS_VALUE(polled);
			status = polled.value();
			if (!vm::api::isStatusTerminal(status))
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionCompleted>(status));

		// The run completed, but nobody reaped its execution thread yet.
		const auto too_early = vm::api::deinitAndValidate(pid);
		ASSERT_NO_VALUE(too_early);
		ASSERT_TRUE(std::holds_alternative<vm::api::StateError>(too_early.error()));

		ASSERT_HAS_VALUE(vm::api::join(pid));

		const auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value());
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/basic/");
