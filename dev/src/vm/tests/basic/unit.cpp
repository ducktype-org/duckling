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
		TESTER_ADD_TEST(globalInitialValueTest);
		TESTER_ADD_TEST(globalsInitialValueTest);
		TESTER_ADD_TEST(verySimpleUnsignedTest);
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
		runTestOnVm("command_line_args.dbc", "", "10", { "1", "2", "3", "4" });
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

	void globalInitialValueTest() { runTestOnVm("global_initial_value.dbc", {}, {}, {}, 5); }

	void globalsInitialValueTest() { runTestOnVm("globals_initial_value.dbc", {}, {}, {}, 5); }

	void verySimpleUnsignedTest() { runTestOnVm("very_simple_unsigned.dbc", "", "2137", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/basic/");
