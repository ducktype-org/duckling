#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/bytecode/validator/errors.hpp>

class VmUnitTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmUnitTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(jump);
		TESTER_ADD_TEST(return1337);
		TESTER_ADD_TEST(initPrimitivesWithZero);
		TESTER_ADD_TEST(check32BitsInstructions);
		TESTER_ADD_TEST(pointerTest);
		TESTER_ADD_TEST(commandLineArguments);
		TESTER_ADD_TEST(globalsTest);
		TESTER_ADD_TEST(globalInitializationTest);
		TESTER_ADD_TEST(globalsInitializationTest);
		TESTER_ADD_TEST(globalDestructorTest);
		TESTER_ADD_TEST(globalNoConstructorTest);
		TESTER_ADD_TEST(globalNoDestructorTest);
		TESTER_ADD_TEST(verySimpleUnsignedTest);
		TESTER_ADD_TEST(verySimpleBooleanTest);
		TESTER_ADD_TEST(floatOperationTest);
		TESTER_ADD_TEST(literalsTest);
		TESTER_ADD_TEST(checkLiteralErrorHandling);
	}

private:
	void jump() { runTestOnVm("jump.dbc", "", "5", {}); }

	void return1337() { runTestOnVm("return_1337.dbc", {}, {}, {}, 1'337); }

	void initPrimitivesWithZero() { runTestOnVm("init_primitives_with_zero.dbc", "", "0", {}); }

	void check32BitsInstructions() { runTestOnVm("32bits.dbc", "", "4", {}); }

	void pointerTest() {
		for (auto filename:
		     { "pointer_to_local.dbc", "pointer_copy.dbc", "pointer_to_passed_blocks.dbc" }) {
			runTestOnVm(filename, "", "42");
		}
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

	void floatOperationTest() {
		runTestOnVm("floating_point_arithmetic_32.dbc", "", "1056964608", {});
		runTestOnVm("floating_point_arithmetic_64.dbc", "", "4602678819172646912", {});
	}

	void literalsTest() {
		runTestOnVm("literals_test_32.dbc", "", "3", {});
		runTestOnVm("literals_test_64.dbc", "", "3", {});
	}

	void verySimpleUnsignedTest() { runTestOnVm("very_simple_unsigned.dbc", "", "2137", {}); }

	void verySimpleBooleanTest() { runTestOnVm("very_simple_boolean.dbc", "", "1", {}); }

	void checkZeroDivision() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("zero_division_i64.dbc", "", "0"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
		assertExecutionPanickedWith(
			runTestOnVmGetResult("zero_division_i32.dbc", "", "0"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
	}

	void checkLiteralErrorHandling() {
		loadInvalidDbc("invalid_type_specifier.dbc", { "Invalid literal: Unknown type specifier" });
		loadInvalidDbc("invalid_literal.dbc", { "Invalid literal: Number not read fully for" });
		loadInvalidDbc("invalid_literal_value.dbc", { "Invalid literal: Not a valid number for" });
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/basic/");
