#include <vm_tester_utils.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/loader/errors.hpp>

class BCVerificationTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCVerificationTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(jumpSkipBlock);
		TESTER_ADD_TEST(manyJumps);
		TESTER_ADD_TEST(initDeinit);

		// @note: This tests breaks the current implementation of jump verification, but will be
		// used soon when better verification is implemented.
		//  TESTER_ADD_TEST(validJumpOutOfBlock);

		// Function verification
		TESTER_ADD_TEST(multipleFunctions);

		// Jump verfification
		TESTER_ADD_TEST(jumpBetween);
		TESTER_ADD_TEST(jumpIntoBlock);
		TESTER_ADD_TEST(jumpOutOfBlock);

		// Local variable verification
		TESTER_ADD_TEST(noInit);
		TESTER_ADD_TEST(beforeInit);
		TESTER_ADD_TEST(afterDeinit);
		TESTER_ADD_TEST(invalidName);
		
		// @note: Not implemented yet
		// TESTER_ADD_TEST(derefAfterDeinit);
		// TESTER_ADD_TEST(derefAfterDeinitAndInit);
		// TESTER_ADD_TEST(derefWrongType);
		// TESTER_ADD_TEST(refOnPrimitive);

		// Type varification
		TESTER_ADD_TEST(wrongTypeMov);
		TESTER_ADD_TEST(wrongTypeSize);
		TESTER_ADD_TEST(globalWrongTypeMov);
		TESTER_ADD_TEST(globalWrongTypeSize);
		TESTER_ADD_TEST(usingPointerAsPrimitive);
	}

private:
	// False positives
	void jumpSkipBlock() { loadValidDbc("right/jump_skip_block.dbc"); }

	void validJumpOutOfBlock() { loadValidDbc("right/valid_jump_out_of_block.dbc"); }

	void initDeinit() { loadValidDbc("right/init_deinit.dbc"); }

	void manyJumps() { loadValidDbc("right/many_jumps.dbc"); }

	// Function verification
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
				vm::code::builders::StackStructureMismatchError::ERR_MSG,
				vm::code::builders::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	void jumpIntoBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_into_block.dbc",
			{
				vm::code::builders::StackStructureMismatchError::ERR_MSG,
				vm::code::builders::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	void jumpOutOfBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_out_of_block.dbc",
			{
				vm::code::builders::StackStructureMismatchError::ERR_MSG,
				vm::code::builders::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	// Deinitialized value verification
	void noInit() {
		loadInvalidDbc(
			"wrong/init_deinit/no_init.dbc",
			{
				vm::code::builders::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void beforeInit() {
		loadInvalidDbc(
			"wrong/init_deinit/before_init.dbc",
			{
				vm::code::builders::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void afterDeinit() {
		loadInvalidDbc(
			"wrong/init_deinit/after_deinit.dbc",
			{
				vm::code::builders::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void invalidName() {
		loadInvalidDbc(
			"wrong/init_deinit/after_deinit.dbc",
			{
				vm::code::builders::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

// @TODO Use these tests when typecheck gets implemented,
// but swap those deleted loader errors for (then freshly added by you) builder ones.
#if 0
	// Pointer verification
	void derefAfterDeinit() {
		loadInvalidDbc(
			"wrong/pointers/deref_after_deinit.dbc",
			{
				vm::loader::UseAfterDeinit::ERR_MSG,
			}
		);
	}

	void derefAfterDeinitAndInit() {
		loadInvalidDbc(
			"wrong/pointers/deref_after_deinit_and_init.dbc",
			{
				vm::loader::UseAfterDeinit::ERR_MSG,
			}
		);
	}

	void derefWrongType() {
		loadInvalidDbc(
			"wrong/pointers/deref_wrong_type.dbc",
			{
				vm::loader::DeferenceTypeMismatchError::ERR_MSG,
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
#endif

	// Type verification
	void wrongTypeMov() {
		loadInvalidDbc(
			"wrong/types/wrong_type_mov.dbc",
			{
				vm::code::builders::ArgumentMismatchError::ERR_MSG,
			}
		);
	}

	void wrongTypeSize() {
		loadInvalidDbc(
			"wrong/types/wrong_type_size.dbc",
			{
				vm::code::builders::InvalidArgumentSizeError::ERR_MSG,
			}
		);
	}

	void globalWrongTypeMov() {
		loadInvalidDbc(
			"wrong/types/global_wrong_type_mov.dbc",
			{
				vm::code::builders::ArgumentMismatchError::ERR_MSG,
			}
		);
	}

	void globalWrongTypeSize() {
		loadInvalidDbc(
			"wrong/types/global_wrong_type_size.dbc",
			{
				vm::code::builders::InvalidArgumentSizeError::ERR_MSG,
			}
		);
	}

	void usingPointerAsPrimitive() {
		loadInvalidDbc(
			"wrong/types/using_pointer_as_primitive.dbc",
			{
				vm::code::builders::InvalidArgumentTypeError::ERR_MSG,
			}
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/loader/verification/");
