#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

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
};

TESTER_COMMON_MAIN("/src/vm/tests/basic/");
