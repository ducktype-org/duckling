#include "tester/tester.hpp"
#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <variant>
#include <vm_tester_utils.hpp>
#include <vm/preprocessor/validator/errors.hpp>

class BCValidationTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCValidationTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(noMain);
		TESTER_ADD_TEST(jumpBetween);
		TESTER_ADD_TEST(jumpIntoBlock);
		TESTER_ADD_TEST(jumpOutOfBlock);
		TESTER_ADD_TEST(jumpSkipBlock);
		TESTER_ADD_TEST(manyJumps);
	}

private:
	void noMain() {
		loadInvalidDbc(
			"wrong/functions/no_main.dbc",
			{
				vm::validator::NoMainError::ERR_MSG,
			}
		);
	}

	void jumpBetween() {
		loadInvalidDbc(
			"wrong/jumps/jump_between.dbc",
			{
				vm::validator::JumpStackStructureMismatch::ERR_MSG,
			}
		);
	}

	void jumpIntoBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_into_block.dbc",
			{
				vm::validator::JumpStackStructureMismatch::ERR_MSG,
			}
		);
	}

	void jumpOutOfBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_out_of_block.dbc",
			{
				vm::validator::JumpStackStructureMismatch::ERR_MSG,
			}
		);
	}

	void jumpSkipBlock() { loadValidDbc("right/jump_skip_block.dbc"); }

	void manyJumps() { loadValidDbc("right/many_jumps.dbc"); }
};

TESTER_COMMON_MAIN("/vm/tests/verification/");
