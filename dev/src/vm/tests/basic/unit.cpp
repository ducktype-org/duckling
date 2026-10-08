// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		TESTER_ADD_TEST(referenceOnColdBranchTest);
		TESTER_ADD_TEST(localSlotAddressingTest);
		TESTER_ADD_TEST(localSlotAddressingInCalleeTest);
		TESTER_ADD_TEST(globalsInitializationTest);
		TESTER_ADD_TEST(globalDestructorTest);
		TESTER_ADD_TEST(globalNoConstructorTest);
		TESTER_ADD_TEST(globalNoDestructorTest);
		TESTER_ADD_TEST(invalidGlobalConstructorSignatureTest);
		TESTER_ADD_TEST(verySimpleUnsignedTest);
		TESTER_ADD_TEST(verySimpleBooleanTest);
		TESTER_ADD_TEST(literalsTest);
		TESTER_ADD_TEST(checkLiteralErrorHandling);
		TESTER_ADD_TEST(checkZeroDivision);
		TESTER_ADD_TEST(invalidPrimitiveTypes);
		TESTER_ADD_TEST(checkCastingInstructions);
		TESTER_ADD_TEST(testSyncRun);
		TESTER_ADD_TEST(joinReturnsExitValue);
		TESTER_ADD_TEST(secondJoinIsRefused);
		TESTER_ADD_TEST(deinitNeedsJoinedThreads);
		TESTER_ADD_TEST(deinitJoinsProgramStartedThreads);
		TESTER_ADD_TEST(rerunJoinsProgramStartedThreads);
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

	/**
	 * @brief A variable whose address is only taken on a path that is not executed never gets a
	 * block, and the process must still validate its memory cleanly.
	 */
	void referenceOnColdBranchTest() { runTestOnVm("reference_on_cold_branch.dbc", "", "42"); }

	void localSlotAddressingTest() { runTestOnVm("local_slot_addressing.dbc", "", "7724"); }

	void localSlotAddressingInCalleeTest() {
		runTestOnVm("local_slot_addressing_in_callee.dbc", "", "42422");
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

	void invalidGlobalConstructorSignatureTest() {
		loadInvalidDbc(
			"global_invalid_constructor_signature.dbc",
			{ vm::code::InvalidConstructorDestructorSignature::ERR_MSG }
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
		ASSERT_TRUE(validation_result.value().valid());
	}

	/**
	 * @brief `api::run` is asynchronous, so a test which wants to observe a finished process has
	 * to wait for it. Fails the test when nothing terminal shows up in time.
	 *
	 * @return The terminal status of the process.
	 */
	vm::api::ProcStatus awaitTerminalStatus(vm::PID pid) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		while (true) {
			const auto polled = vm::api::getExecutionStatus(pid);
			if (!polled.has_value()) fail("`getExecutionStatus` failed while awaiting the status");

			if (vm::api::isStatusTerminal(polled.value())) return polled.value();
			if (std::chrono::steady_clock::now() > deadline)
				fail("The process did not reach a terminal status in time");

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	/**
	 * A join takes the execution thread away, so there is nothing left for a second one to do. The
	 * exit value was already handed to the first caller.
	 */
	void secondJoinIsRefused() {
		vm::PID pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path("return_1337.dbc")) }));
		ASSERT_HAS_VALUE(vm::api::run(pid));

		ASSERT_HAS_VALUE(vm::api::join(pid));

		const auto second_join = vm::api::join(pid);
		ASSERT_NO_VALUE(second_join);
		ASSERT_TRUE(std::holds_alternative<vm::api::JoinError>(second_join.error()));

		ASSERT_HAS_VALUE(vm::api::deinitAndValidate(pid));
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
		const vm::api::ProcStatus status = awaitTerminalStatus(pid);
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionCompleted>(status));

		// The run completed, but nobody joined its execution thread yet.
		const auto too_early = vm::api::deinitAndValidate(pid);
		ASSERT_NO_VALUE(too_early);
		ASSERT_TRUE(std::holds_alternative<vm::api::StateError>(too_early.error()));

		ASSERT_HAS_VALUE(vm::api::join(pid));

		const auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value().memory_valid);
		// A program which started no threads of its own has nothing to report.
		ASSERT_TRUE(validation_result.value().unjoined_program_threads.empty());
	}

	/**
	 * A thread the program started itself belongs to the DVM - the API caller never learns its ID,
	 * so it must not be forced to join it. The deinit joins such a thread instead of refusing.
	 */
	void deinitJoinsProgramStartedThreads() {
		vm::PID pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path("unjoined_thread.dbc")) }));
		ASSERT_HAS_VALUE(vm::api::run(pid));

		// Waits for the worker as well, since the process is `Completed` only once no thread of
		// it is active any more.
		const vm::api::ProcStatus status = awaitTerminalStatus(pid);
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionCompleted>(status));

		// Only the main thread was started through the API, so only it has to be joined.
		ASSERT_HAS_VALUE(vm::api::join(pid));

		const auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value().memory_valid);
		// The teardown does not refuse over the worker, but it does report that the program left
		// it unjoined.
		const auto& unjoined = validation_result.value().unjoined_program_threads;
		ASSERT_EQUAL(unjoined.size(), 1);
		ASSERT_TRUE(unjoined.at(0) != vm::api::MAIN_THREAD_ID);
	}

	/**
	 * The same has to happen before a re-run: a VMThread may only be spawned again once its
	 * previous execution thread was joined, and the program's threads are nobody else's to join.
	 */
	void rerunJoinsProgramStartedThreads() {
		vm::PID pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path("unjoined_thread.dbc")) }));

		for (usize run = 0; run < 3; run++) {
			ASSERT_HAS_VALUE(vm::api::run(pid));
			ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionCompleted>(awaitTerminalStatus(pid)
			));
			ASSERT_HAS_VALUE(vm::api::join(pid));
		}

		const auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value().memory_valid);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/basic/");
