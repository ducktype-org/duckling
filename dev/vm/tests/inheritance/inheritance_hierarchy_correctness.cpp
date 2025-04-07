#include <vm_tester_utils.hpp>

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
		runTestOnVm("inheritance_metadata.dbc", "", "0");

		// Invalid
		using namespace vm::validator;
		auto filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{ "invalid_hierarchy/instance_data_in_interface.dbc",
		      InstanceDataInInterface::ERR_MSG },
			{ "invalid_hierarchy/missing_ancestor_field.dbc", MissingAncestorField::ERR_MSG },
			{ "invalid_hierarchy/missing_vt_pointer.dbc", MissingVtablePointer::ERR_MSG },
			{ "invalid_hierarchy/extends_plain.dbc", InvalidExtends::ERR_MSG },
			{ "invalid_hierarchy/extends_interface.dbc", InvalidExtends::ERR_MSG },
			{ "invalid_hierarchy/extends_final.dbc", InvalidExtends::ERR_MSG },
			{ "invalid_hierarchy/implements_plain.dbc", InvalidImplements::ERR_MSG },
			{ "invalid_hierarchy/implements_class.dbc", InvalidImplements::ERR_MSG },
		});

		for (auto& [filename, error]: filename_and_error) loadInvalidDbc(filename, { error });
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
