#include <vm_tester_utils.hpp>

#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/safe/exceptions.hpp>

class DynamicTableVmTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DynamicTableVmTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(twoDim);
		TESTER_ADD_TEST(dynArrSum);
		TESTER_ADD_TEST(lea);
		TESTER_ADD_TEST(nonInstantiableDynTable);
		TESTER_ADD_TEST(tooLarge);
		TESTER_ADD_TEST(stringOutput);
		TESTER_ADD_TEST(reallocZero);
	}

private:
	void dynArrSum() { runTestOnVm("dyn_arr_sum.dbc", "", "55", {}); }

	void lea() { runTestOnVm("lea.dbc", "", "4", {}); }

	void twoDim() { runTestOnVm("two_dim.dbc", "", "1235", {}); }

	void nonInstantiableDynTable() {
		loadInvalidDbc("non_instantiable.dbc", { vm::code::UninstantiableValueError::ERR_MSG });
	}

	void tooLarge() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("too_large.dbc", "", "9223372036854775808"),
			vm::exceptions::VMMemoryAllocationError::ERR_MSG
		);
	}

	void stringOutput() { runTestOnVm("string_output.dbc", {}, "test\ntest", { "test" }, 0); }

	void reallocZero() { runTestOnVm("realloc_zero.dbc", "", "42", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/dynamic_table/");
