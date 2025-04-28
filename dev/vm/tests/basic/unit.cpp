#include <tester/tester.hpp>
#include <vm_tester_utils.hpp>

class VmUnitTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmUnitTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(jump);
		TESTER_ADD_TEST(return1337);
		TESTER_ADD_TEST(initPrimitivesWithZero);
		TESTER_ADD_TEST(check32BitsInstructions);
		TESTER_ADD_TEST(types);
		TESTER_ADD_TEST(pointerToLocal);
		TESTER_ADD_TEST(pointerCopy);
		TESTER_ADD_TEST(commandLineArguments);
		TESTER_ADD_TEST(globalsTest);
	}

private:
	void jump() { runTestOnVm("jump.dbc", "", "5", {}); }

	void return1337() { runTestOnVm("return_1337.dbc", {}, {}, {}, 1'337); }

	void initPrimitivesWithZero() { runTestOnVm("init_primitives_with_zero.dbc", "", "0", {}); }

	void check32BitsInstructions() { runTestOnVm("32bits.dbc", "", "4", {}); }

	void types() { runTestOnVm("types.dbc", "3 4", "7", {}); }

	void pointerToLocal() { runTestOnVm("pointer_to_local.dbc", "", "42", {}); }

	void pointerCopy() { runTestOnVm("pointer_copy.dbc", "", "42", {}); }

	void commandLineArguments() {
		runTestOnVm("command_line_args.dbc", "", "10", { "1", "2", "3", "4" });
	}

	void globalsTest() { runTestOnVm("globals.dbc", "42", {}, {}, 5); }
};

TESTER_COMMON_MAIN("/vm/tests/basic/");
