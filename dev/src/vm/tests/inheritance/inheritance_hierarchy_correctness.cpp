// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/collections/optional.hpp>

#include <vm/bytecode/validator/errors.hpp>

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
		runTestOnVm("inheritance_metadata.dbc", {}, {}, {}, 0);

		using namespace vm::code;
		auto invalid_filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{
				"invalid_hierarchy/extends_cycle.dbc",
				CycleInHierarchyError::ERR_MSG,
			},
			{
				"invalid_hierarchy/extends_interface.dbc",
				InvalidExtendsError::ERR_MSG,
			},
			{
				"invalid_hierarchy/extends_plain.dbc",
				InvalidExtendsError::ERR_MSG,
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
				"invalid_hierarchy/implements_plain.dbc",
				InvalidImplementsError::ERR_MSG,
			},
		});

		for (auto& [filename, error]: invalid_filename_and_error)
			loadInvalidDbc(filename, { error });
	}

	void virtualMethodImplementationCorrectness() {
		using namespace vm::code;
		auto invalid_filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{
				"vmethod_implementation/implement_non_existing_method.dbc",
				InvalidVirtualMethodImplementationError::ERR_MSG,
			},
			{
				"vmethod_implementation/method_signature_mismatch_1.dbc",
				MethodTypeError::ERR_MSG,
			},
			{
				"vmethod_implementation/method_signature_mismatch_2.dbc",
				MethodTypeError::ERR_MSG,
			},
			{
				"vmethod_implementation/no_self_ptr.dbc",
				MethodFirstArgumentError::ERR_MSG,
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
				"vmethod_implementation/unimplemented_virtual.dbc",
				UnimplementedVirtualMethodError::ERR_MSG,
			},
		});

		for (auto& [filename, error]: invalid_filename_and_error)
			loadInvalidDbc(filename, { error });

		auto valid_filenames = std::to_array<std::string>({
			"vmethod_implementation/correct_interface_override.dbc",
			"vmethod_implementation/unimplemented_abstract.dbc",
			"vmethod_implementation/unimplemented_virtual_hierarchy_correct_1.dbc",
			"vmethod_implementation/unimplemented_virtual_hierarchy_correct_2.dbc",
		});
		for (auto& filename: valid_filenames) loadValidDbc(filename);
	}

	void duplicateCorrectness() {
		using namespace vm::code;
		auto invalid_filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{
				"duplicates/duplicated_field.dbc",
				DuplicatedFieldError::ERR_MSG,
			},
			{
				"duplicates/duplicated_implements.dbc",
				DuplicatedImplementsError::ERR_MSG,
			},
			{
				"duplicates/duplicated_inherited_field.dbc",
				DuplicatedFieldError::ERR_MSG,
			},
			{
				"duplicates/duplicated_inherited_vmethod_class.dbc",
				DuplicatedVirtualMethodError::ERR_MSG,
			},
			{
				"duplicates/duplicated_inherited_vmethod_iface.dbc",
				DuplicatedVirtualMethodError::ERR_MSG,
			},
			{
				"duplicates/duplicated_vmethod_class.dbc",
				DuplicatedVirtualMethodError::ERR_MSG,
			},
			{
				"duplicates/duplicated_vmethod_iface.dbc",
				DuplicatedVirtualMethodError::ERR_MSG,
			},
			{
				"duplicates/duplicated_vmethod_implementation_class.dbc",
				DuplicatedVirtualMethodImplementationError::ERR_MSG,
			},
			{
				"duplicates/duplicated_vmethod_implementation_iface.dbc",
				DuplicatedVirtualMethodImplementationError::ERR_MSG,
			},
		});
		for (auto& [filename, error]: invalid_filename_and_error)
			loadInvalidDbc(filename, { error });
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/inheritance/");
