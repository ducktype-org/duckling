#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/loader/compiler/safe/safe_compiler.hpp>
#include <vm/loader/loader.hpp>

#include <chrono>
#include <thread>

class VmDebugTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(lowVMProgramCopyReplaceOpcode); }


private:
	/**
	 * @brief Checks if editing opcodes works correctly.
	 */
	void lowVMProgramCopyReplaceOpcode() {}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
