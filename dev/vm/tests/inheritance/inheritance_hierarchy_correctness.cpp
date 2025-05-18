#include <vm_tester_utils.hpp>

#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/api/api.hpp>
#include <vm/bytecode/builders/errors.hpp>

class VmInheritanceHierarchyCorrectnessTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceHierarchyCorrectnessTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(hierarchyCorrectness); }


private:
	void hierarchyCorrectness() {
		runTestOnVm("inheritance_metadata.dbc", {}, {}, {}, 0);

		// Invalid
		using namespace vm::code::builders;
		auto filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{ "invalid_hierarchy/missing_ancestor_field.dbc", MissingAncestorFieldError::ERR_MSG },
			{ "invalid_hierarchy/different_order_ancestor_field.dbc", MissingAncestorFieldError::ERR_MSG },
			{ "invalid_hierarchy/extends_plain.dbc", InvalidExtends::ERR_MSG },
			{ "invalid_hierarchy/extends_interface.dbc", InvalidExtends::ERR_MSG },
			{ "invalid_hierarchy/implements_plain.dbc", InvalidImplementsError::ERR_MSG },
			{ "invalid_hierarchy/implements_class.dbc", InvalidImplementsError::ERR_MSG },
			{ "invalid_hierarchy/extends_cycle.dbc", CycleInHierarchyError::ERR_MSG },
			{ "invalid_hierarchy/implements_cycle.dbc", CycleInHierarchyError::ERR_MSG },
		});

		for (auto& [filename, error]: filename_and_error) loadInvalidDbc(filename, { error });
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
