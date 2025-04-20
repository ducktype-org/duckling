#include <vm_tester_utils.hpp>

#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/api/api.hpp>
// @TODOB
// #include <vm/preprocessor/validator/errors.hpp>

class VmInheritanceSemanticsTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceSemanticsTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(semantics); }


private:
	void semantics() {
		runTestOnVm("downcast.dbc", "", "10");
		runTestOnVm("valid_upcast.dbc", "", "0");

        // @TODOB
		// // Invalid
		// using namespace vm::validator;
		// auto filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
		// 	{ "invalid_instantiation.dbc", UninstantiableValue::ERR_MSG },
		// 	{ "invalid_upcast.dbc", InvalidUpcast::ERR_MSG },
		// });

		// for (auto& [filename, error]: filename_and_error) loadInvalidDbc(filename, { error });
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
