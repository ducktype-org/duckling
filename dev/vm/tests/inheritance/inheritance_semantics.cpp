#include "vm_tester_utils.hpp"

#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/api/api.hpp>
#include <vm/preprocessor/validator/errors.hpp>

class VmInheritanceHierarchyCorrectnessTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceHierarchyCorrectnessTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(hierarchyCorrectness); }


private:
	void hierarchyCorrectness() {
		// Invalid
		using namespace vm::validator;
		auto filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{ "invalid_instantiation.dbc", UninstantiableValue::ERR_MSG },
			{ "invalid_upcast.dbc", InvalidUpcast::ERR_MSG },
		});

		for (auto& [filename, error]: filename_and_error) loadInvalidDbc(filename, { error });
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
