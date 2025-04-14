#include <vm_tester_utils.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm/loader/errors.hpp>
#include <vm/loader/validator/errors.hpp>

class BCVerificationTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCVerificationTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(jumpSkipBlock);
		TESTER_ADD_TEST(manyJumps);
		TESTER_ADD_TEST(initDeinit);

		// TESTER_ADD_TEST(validJumpOutOfBlock);

		// Function verification
		TESTER_ADD_TEST(noMain);
		TESTER_ADD_TEST(invalidMainRetSize);
		TESTER_ADD_TEST(invalidMainRetType);
		TESTER_ADD_TEST(multipleFunctions);

		// Jump verfification
		TESTER_ADD_TEST(jumpBetween);
		TESTER_ADD_TEST(jumpIntoBlock);
		TESTER_ADD_TEST(jumpOutOfBlock);

		// @note: Not implemented yet
		// TESTER_ADD_TEST(noInit);
		// TESTER_ADD_TEST(beforeInit);
		// TESTER_ADD_TEST(afterDeinit);
		// TESTER_ADD_TEST(invalidOffset);

		// TESTER_ADD_TEST(derefAfterDeinit);
		// TESTER_ADD_TEST(derefAfterDeinitAndInit);
		// TESTER_ADD_TEST(derefWrongType);
		// TESTER_ADD_TEST(refOnPrimitive);

		// TESTER_ADD_TEST(wrongTypeMov);
		// TESTER_ADD_TEST(wrongTypeSize);
	}

private:
	// False positives
	void jumpSkipBlock() { loadValidDbc("right/jump_skip_block.dbc"); }

	void validJumpOutOfBlock() { loadValidDbc("right/vaild_jump_out_of_block.dbc"); }

	void initDeinit() { loadValidDbc("right/init_deinit.dbc"); }

	void manyJumps() { loadValidDbc("right/many_jumps.dbc"); }

	// Function verification
	void noMain() {
		loadInvalidDbc(
			"wrong/functions/no_main.dbc",
			{
				vm::loader::NO_MAIN_ERR,
			}
		);
	}

	void invalidMainRetSize() {
		loadInvalidDbc(
			"wrong/functions/invalid_main_ret_size.dbc",
			{
				vm::loader::WRONG_MAIN_RET_VAL_ERR,
			}
		);
	}

	void invalidMainRetType() {
		loadInvalidDbc(
			"wrong/functions/invalid_main_ret_type.dbc",
			{
				vm::loader::WRONG_MAIN_RET_VAL_ERR,
			}
		);
	}

	void multipleFunctions() {
		loadInvalidDbc(
			"wrong/functions/multiple_functions.dbc",
			{
				vm::loader::DuplicatedFunctionError::ERR_MSG,
			}
		);
	}

	// Jump verification
	void jumpBetween() {
		loadInvalidDbc(
			"wrong/jumps/jump_between.dbc",
			{
				vm::loader::StackStructureMismatchError::ERR_MSG,
				vm::loader::StackStructureMismatchNote::ERR_MSG,
			}
		);
	}

	void jumpIntoBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_into_block.dbc",
			{
				vm::loader::StackStructureMismatchError::ERR_MSG,
				vm::loader::StackStructureMismatchNote::ERR_MSG,
			}
		);
	}

	void jumpOutOfBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_out_of_block.dbc",
			{
				vm::loader::StackStructureMismatchError::ERR_MSG,
				vm::loader::StackStructureMismatchNote::ERR_MSG,
			}
		);
	}

	// Deinitialized valued verification
	void noInit() {
		loadInvalidDbc(
			"wrong/init_deinit/no_init.dbc",
			{
				vm::loader::UninitializedLocalUsedError::ERR_MSG,
			}
		);
	}

	void beforeInit() {
		loadInvalidDbc(
			"wrong/init_deinit/before_init.dbc",
			{
				vm::loader::UninitializedLocalUsedError::ERR_MSG,
			}
		);
	}

	void afterDeinit() {
		loadInvalidDbc(
			"wrong/init_deinit/after_deinit.dbc",
			{
				vm::loader::UninitializedLocalUsedError::ERR_MSG,
			}
		);
	}

	void invalidOffset() {
		loadInvalidDbc(
			"wrong/init_deinit/invalid_offset.dbc",
			{
				vm::loader::WrongOffsetStackDerefError::ERR_MSG,
			}
		);
	}

	// Pointer verification
	void derefAfterDeinit() {
		loadInvalidDbc(
			"wrong/pointers/deref_after_deinit.dbc",
			{
				vm::loader::PointerDereferenceAfterDeinitError::ERR_MSG,
			}
		);
	}

	void derefAfterDeinitAndInit() {
		loadInvalidDbc(
			"wrong/pointers/deref_after_deinit_and_init.dbc",
			{
				vm::loader::PointerDereferenceAfterDeinitError::ERR_MSG,
			}
		);
	}

	void derefWrongType() {
		loadInvalidDbc(
			"wrong/pointers/deref_wrong_type.dbc",
			{
				vm::loader::WrongTypePointerDereferenceError::ERR_MSG,
			}
		);
	}

	void refOnPrimitive() {
		loadInvalidDbc(
			"wrong/pointers/ref_on_primitive.dbc",
			{
				vm::loader::LocalUsedAsPointerError::ERR_MSG,
			}
		);
	}

	// Type verification
	void wrongTypeMov() {
		loadInvalidDbc(
			"wrong/types/wrong_type_mov.dbc",
			{
				vm::loader::OpCodeTypeMismatchError::ERR_MSG,
			}
		);
	}

	void wrongTypeSize() {
		loadInvalidDbc(
			"wrong/types/wrong_type_size.dbc",
			{
				vm::loader::OpCodeTypeMismatchError::ERR_MSG,
			}
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/loader/verification/");
