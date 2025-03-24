#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <variant>
#include <vm_tester_utils.hpp>
#include <vm/preprocessor/validator/errors.hpp>

class BCValidationTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCValidationTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(noMain); }

private:
	void noMain() {
		parseInvalidDbc(
			"no_main.dbc",
			{
				vm::validator::NoMainError::ERR_MSG,
			}
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/validation/");
