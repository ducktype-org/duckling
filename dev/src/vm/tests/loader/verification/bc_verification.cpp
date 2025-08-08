#include <vm_tester_utils.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/loader/errors.hpp>

class BCVerificationTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCVerificationTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(jumpSkipBlock);
		TESTER_ADD_TEST(manyJumps);
		TESTER_ADD_TEST(initDeinit);
		TESTER_ADD_TEST(twoInits);

		// Function verification
		TESTER_ADD_TEST(multipleFunctions);
		TESTER_ADD_TEST(useArgumentAfterCall);

		// Jump verification
		TESTER_ADD_TEST(jumpBetween);
		TESTER_ADD_TEST(jumpIntoBlock);
		TESTER_ADD_TEST(jumpOutOfBlock);
		TESTER_ADD_TEST(duplicatedDataFields);
		TESTER_ADD_TEST(unknownMethod);
		TESTER_ADD_TEST(jumpWithInplaceCast);
		TESTER_ADD_TEST(validJumpOutOfBlock);

		// Local variable verification
		TESTER_ADD_TEST(noInit);
		TESTER_ADD_TEST(beforeInit);
		TESTER_ADD_TEST(afterDeinit);
		TESTER_ADD_TEST(invalidName);
		TESTER_ADD_TEST(repeatedName);

		// @note: Not implemented yet
		// TESTER_ADD_TEST(derefAfterDeinit);
		// TESTER_ADD_TEST(derefAfterDeinitAndInit);
		TESTER_ADD_TEST(derefWrongType);
		TESTER_ADD_TEST(refOnPrimitive);

		// Type verification
		TESTER_ADD_TEST(wrongTypeMov);
		TESTER_ADD_TEST(wrongTypeSize);
		TESTER_ADD_TEST(globalWrongTypeMov);
		TESTER_ADD_TEST(globalWrongTypeSize);
		TESTER_ADD_TEST(usingPointerAsPrimitive);
		TESTER_ADD_TEST(variantWrongType);
		TESTER_ADD_TEST(fixedSizeTableWrongType);
		TESTER_ADD_TEST(inplaceCasts);
	}

private:
	// False positives
	void jumpSkipBlock() { loadValidDbc("right/jump_skip_block.dbc"); }

	void validJumpOutOfBlock() { loadValidDbc("right/valid_jump_out_of_block.dbc"); }

	void initDeinit() { loadValidDbc("right/init_deinit.dbc"); }

	void manyJumps() { loadValidDbc("right/many_jumps.dbc"); }

	void twoInits() { loadValidDbc("right/two_inits.dbc"); }

	// Function verification
	void multipleFunctions() {
		loadInvalidDbc(
			"wrong/functions/multiple_functions.dbc",
			{
				vm::loader::DuplicatedFunctionError::ERR_MSG,
			}
		);
	}

	// Function verification
	void useArgumentAfterCall() {
		loadInvalidDbc(
			"wrong/functions/use_argument_after_call.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	// Jump verification
	void jumpBetween() {
		loadInvalidDbc(
			"wrong/jumps/jump_between.dbc",
			{
				vm::code::StackStructureMismatchError::ERR_MSG,
				vm::code::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	void jumpIntoBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_into_block.dbc",
			{
				vm::code::StackStructureMismatchError::ERR_MSG,
				vm::code::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	void jumpOutOfBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_out_of_block.dbc",
			{
				vm::code::StackStructureMismatchError::ERR_MSG,
				vm::code::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	void jumpWithInplaceCast() {
		loadInvalidDbc(
			"wrong/jumps/jump_with_inplace_cast.dbc",
			{
				vm::code::StackStructureMismatchError::ERR_MSG,
				vm::code::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	// Deinitialized value verification
	void noInit() {
		loadInvalidDbc(
			"wrong/init_deinit/no_init.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void beforeInit() {
		loadInvalidDbc(
			"wrong/init_deinit/before_init.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void afterDeinit() {
		loadInvalidDbc(
			"wrong/init_deinit/after_deinit.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void invalidName() {
		loadInvalidDbc(
			"wrong/init_deinit/invalid_name.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void repeatedName() {
		loadInvalidDbc(
			"wrong/init_deinit/repeated_name.dbc",
			{
				vm::code::DuplicatedLocalNameError::ERR_MSG,
			}
		);
	}

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
#endif

	void derefWrongType() {
		loadInvalidDbc(
			"wrong/pointers/deref_wrong_type.dbc",
			{
				vm::code::PointerTypeMismatchError::ERR_MSG,
			}
		);
	}

	void refOnPrimitive() {
		loadInvalidDbc(
			"wrong/pointers/ref_on_primitive.dbc",
			{
				vm::code::InvalidArgumentTypeError::ERR_MSG,
			}
		);
	}

	// Type verification
	void wrongTypeMov() {
		loadInvalidDbc(
			"wrong/types/wrong_type_mov.dbc",
			{
				vm::code::ArgumentMismatchError::ERR_MSG,
			}
		);
	}

	void wrongTypeSize() {
		loadInvalidDbc(
			"wrong/types/wrong_type_size.dbc",
			{
				vm::code::InvalidArgumentSizeError::ERR_MSG,
			}
		);
	}

	void globalWrongTypeMov() {
		loadInvalidDbc(
			"wrong/types/global_wrong_type_mov.dbc",
			{
				vm::code::ArgumentMismatchError::ERR_MSG,
			}
		);
	}

	void globalWrongTypeSize() {
		loadInvalidDbc(
			"wrong/types/global_wrong_type_size.dbc",
			{
				vm::code::InvalidArgumentSizeError::ERR_MSG,
			}
		);
	}

	void usingPointerAsPrimitive() {
		loadInvalidDbc(
			"wrong/types/using_pointer_as_primitive.dbc",
			{
				vm::code::InvalidArgumentTypeError::ERR_MSG,
			}
		);
	}

	void duplicatedDataFields() {
		loadInvalidDbc(
			"wrong/data_type/duplicated_fields.dbc",
			{
				vm::code::DuplicatedFieldError::ERR_MSG,
			}
		);
	}

	void variantWrongType() {
		loadInvalidDbc(
			"wrong/types/variant_wrong_type.dbc",
			{
				vm::code::VariantTypeMismatchError::ERR_MSG,
			}
		);
	}

	void unknownMethod() {
		loadInvalidDbc(
			"wrong/unknown_method.dbc",
			{
				vm::code::InvalidVirtualMethodImplementationError::ERR_MSG,
			}
		);
	}

	void fixedSizeTableWrongType() {
		loadInvalidDbc(
			"wrong/types/fixed_size_table_wrong_type.dbc",
			{
				vm::code::FixedSizeTableTypeMismatchError::ERR_MSG,
			}
		);
	}

	void inplaceCasts() {
		runTestOnVm("right/inplace_cast.dbc", {}, "42");
		loadInvalidDbc(
			"wrong/types/inplace_cast_size_mismatch.dbc",
			{
				vm::code::CastSizeMismatchError::ERR_MSG,
			}
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/verification/");
