#include <vm_tester_utils.hpp>

#include <base/variant.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>

class VmVariantTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmVariantTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(verySimpleVariant);
		TESTER_ADD_TEST(simpleVariant0);
		TESTER_ADD_TEST(simpleVariant1);
		TESTER_ADD_TEST(simpleVariant2);
		TESTER_ADD_TEST(blocksDontDisappearTest);
		TESTER_ADD_TEST(nestedVariantTest);
		TESTER_ADD_TEST(variantInsideStruct);
		TESTER_ADD_TEST(emptyVariant);
		TESTER_ADD_TEST(nonInstantiableVariant);
	}

private:
	void verySimpleVariant() { runTestOnVm("very_simple_variant.dbc", "42", "42"); }

	void simpleVariant0() { runTestOnVm("simple_variant.dbc", "0", "13"); }

	void simpleVariant1() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("simple_variant.dbc", "1", "13"), "Copying to/from null pointer"
		);
	}

	void simpleVariant2() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("simple_variant.dbc", "2", "13"), "Data was freed"
		);
	}

	void blocksDontDisappearTest() { runTestOnVm("variant_blocks_dont_disappear.dbc", "5", "5"); }

	void nestedVariantTest() {
		runTestOnVm("nested.dbc", "15", "15");
		assertExecutionPanickedWith(
			runTestOnVmGetResult("nested_failing.dbc", "15", "15"), "Data was freed"
		);
	}

	void variantInsideStruct() { runTestOnVm("inside_struct.dbc"); }

	void nonInstantiableVariant() {
		loadInvalidDbc("non_instantiable.dbc", { vm::code::UninstantiableValueError::ERR_MSG });
	}

	void emptyVariant() {
		loadInvalidDbc("empty_variant.dbc", { vm::code::EmptyVariantError::ERR_MSG });
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/variant/");
