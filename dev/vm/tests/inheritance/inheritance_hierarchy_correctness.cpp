#include <vm_tester_utils.hpp>

#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/api/api.hpp>
#include <vm/bytecode/builders/errors.hpp>

#include <array>

class VmInheritanceHierarchyCorrectnessTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceHierarchyCorrectnessTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(hierarchyCorrectness);
		TESTER_ADD_TEST(virtualMethodImplementationCorrectness);
		TESTER_ADD_TEST(duplicateCorrectness);
	}

private:
	void hierarchyCorrectness() {
		// runTestOnVm("inheritance_metadata.dbc", {}, {}, {}, 0);

		// Invalid
		using namespace vm::code::builders;
		auto invalid_filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{
				"invalid_hierarchy/extends_plain.dbc",
				InvalidExtends::ERR_MSG,
			},
			{
				"invalid_hierarchy/extends_interface.dbc",
				InvalidExtends::ERR_MSG,
			},
			{
				"invalid_hierarchy/extends_cycle.dbc",
				CycleInHierarchyError::ERR_MSG,
			},

			{
				"invalid_hierarchy/implements_plain.dbc",
				InvalidImplementsError::ERR_MSG,
			},
			{
				"invalid_hierarchy/implements_class.dbc",
				InvalidImplementsError::ERR_MSG,
			},
			{
				"invalid_hierarchy/implements_cycle.dbc",
				CycleInHierarchyError::ERR_MSG,
			},

			{
				"invalid_hierarchy/missing_ancestor_field.dbc",
				MissingAncestorFieldError::ERR_MSG,
			},
			{
				"invalid_hierarchy/missing_ancestor_vmethod.dbc",
				MissingAncestorVirtualMethodError::ERR_MSG,
			},

			{
				"invalid_hierarchy/different_order_ancestor_field.dbc",
				MissingAncestorFieldError::ERR_MSG,
			},
			{
				"invalid_hierarchy/different_order_ancestor_vmethod.dbc",
				MissingAncestorVirtualMethodError::ERR_MSG,
			},

		});

		for (auto& [filename, error]: invalid_filename_and_error) {
			std::cerr << "New tests: " << filename << '\n';
			loadInvalidDbc(filename, { error }, true);
		}
	}

	void virtualMethodImplementationCorrectness() {
		// TODO: Test if argument in method implementations.
		using namespace vm::code::builders;
		auto invalid_filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{
				"vmethod_implementation/unimplemented_virtual.dbc",
				UnimplementedVirtualMethodError::ERR_MSG,
			},
			{
				"vmethod_implementation/unimplemented_virtual_hierarchy_1.dbc",
				UnimplementedVirtualMethodError::ERR_MSG,
			},
			{
				"vmethod_implementation/unimplemented_virtual_hierarchy_2.dbc",
				UnimplementedVirtualMethodError::ERR_MSG,
			},
			{
				"vmethod_implementation/unimplemented_virtual_hierarchy_3.dbc",
				UnimplementedVirtualMethodError::ERR_MSG,
			},
			{
				"vmethod_implementation/implement_non_existing_method.dbc",
				InvalidVirtualMethodImplementationError::ERR_MSG,  // TODO: Use base concat for
		                                                           // better errors?
			},
		});

		for (auto& [filename, error]: invalid_filename_and_error) {
			std::cerr << "NEW TEST: " << filename << '\n';
			loadInvalidDbc(filename, { error }, true);
		}

		auto valid_filenames = std::to_array<std::string>({
			"vmethod_implementation/unimplemented_abstract.dbc",
			"vmethod_implementation/unimplemented_virtual_hierarchy_correct_1.dbc",
			"vmethod_implementation/unimplemented_virtual_hierarchy_correct_2.dbc",
		});
		for (auto& filename: valid_filenames) {
			std::cerr << "NEW TEST: " << filename << '\n';
			loadValidDbc(filename, true);
		}
	}

	void duplicateCorrectness() {
		using namespace vm::code::builders;
		auto invalid_filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{
				"duplicates/duplicated_field.dbc",
				DuplicatedFieldError::ERR_MSG,
			},
			{
				"duplicates/duplicated_vmethod.dbc",
				DuplicatedVirtualMethodError::ERR_MSG,
			},
			{
				"duplicates/duplicated_implementation.dbc",
				DuplicatedVirtualMethodImplementationError::ERR_MSG,
			},
			// TODO: This is something to think about.d
		    // {
		    // 	"duplicates/repeating_methods_in_interfaces.dbc",
		    // 	DuplicatedVirtualMethodError::ERR_MSG,
		    // },
		});
		for (auto& [filename, error]: invalid_filename_and_error)
			loadInvalidDbc(filename, { error });
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
