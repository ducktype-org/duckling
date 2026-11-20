// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <string_id/string_id.hpp>

/**
 * @brief What belongs here: the inheritance metadata the loader builds for a valid program, i.e.
 * the class and interface hierarchy and the vtables it lowers the bytecode types to.
 *
 * A hierarchy the loader must reject goes to `loader/inheritance/`.
 */
class VmInheritanceLoadingTest: public VmLoaderTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceLoadingTest

public:
	VM_LOADER_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(metadataLoading);
		TESTER_ADD_TEST(inheritedFieldsKeepTheirOffsets);
	}


private:
	vm::TypeCRef getType(vm::PID pid, const std::string& name) {
		auto response = vm::api::getType(pid, name);
		assertSucceeded(response, base::strConcat("Type query for '", name, "'"));
		return response->type;
	}

	void checkPod(vm::TypeCRef type) {
		ASSERT_NO_VALUE(type->getInheritanceMetadata(), "Plain data should be plain");
	}

	void checkI1(
		vm::TypeCRef type, vm::TypeCRef expected_method_type, base::StrID expected_foo_impl_type
	) {
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd->type == type, "Invalid type in inheritance metadata");
			ASSERT_MATCHES_MSG(
				imd->kind, "I1 should be an interface", vm::InheritanceMetadata::Interface
			);

			assertTrue(imd->implements.empty(), "I1 should not implement anything");
			assertTrue(imd->available_methods.size() == 2, "I1 should have two virtual methods");
			assertTrue(imd->vtable.size() == 1, "I1 should have foo in it's vtable");

			auto foo_type = imd->available_methods[base::StrID("foo")];
			auto bar_type = imd->available_methods[base::StrID("bar")];
			auto vt_foo   = imd->vtable[base::StrID("foo")];
			assertTrue(foo_type == bar_type, "I2's methods should have the same type");
			assertTrue(foo_type == expected_method_type, "I1's method have the wrong type");
			assertTrue(vt_foo == expected_foo_impl_type, "I1's vtable contains wrong type");

			return;
		}

		fail("I1 should not be plain");
	}

	void checkI2(vm::TypeCRef type) {
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd->type == type, "Invalid type in inheritance metadata");
			ASSERT_MATCHES_MSG(
				imd->kind, "I2 should be an interface", vm::InheritanceMetadata::Interface
			);
			assertTrue(imd->implements.empty(), "I2 should not implement anything");
			assertTrue(imd->available_methods.empty(), "I2 should not have any virtual methods");
			assertTrue(imd->vtable.empty(), "I2 should not implement any virtual methods");

			return;
		}

		fail("I2 should not be plain");
	}

	void checkParent(vm::TypeCRef type, vm::TypeCRef method_type, base::StrID method_impl_type) {
		std::cerr << "Check parent\n";
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd->type == type, "Invalid type in inheritance metadata");
			variant_match(imd->kind) {
				variant_case(vm::InheritanceMetadata::Class, clazz) {
					assertFalse(clazz.is_abstract, "Parent should be a concrete class");
					ASSERT_NO_VALUE(clazz.extends, "Parent should not extend anything");
				}
				variant_default { fail("Parent should be a class"); }
			}

			assertTrue(imd->implements.empty(), "Parent should not implement anything");
			assertTrue(
				imd->available_methods.size() == 1, "Parent should declare one virtual method"
			);
			assertTrue(imd->vtable.size() == 1, "Parents' vtable should contain one method");

			auto get_age_type        = imd->available_methods[base::StrID("getAge")];
			auto vtable_get_age_type = imd->vtable[base::StrID("getAge")];
			assertTrue(get_age_type == method_type, "Invalid Parent method type");
			assertTrue(vtable_get_age_type == method_impl_type, "Invalid Parent method type");

			return;
		}

		fail("Parent should not be plain");
	}

	void checkChild(
		vm::TypeCRef                            type,
		vm::TypeCRef                            super_type,
		const std::unordered_set<vm::TypeCRef>& interfaces,
		base::StrID                             expected_i1_foo_impl,
		vm::TypeCRef                            expected_child_method,
		base::StrID                             expected_child_get_age_impl,
		base::StrID                             expected_child_i1_impl,
		base::StrID                             expected_child_cry_impl
	) {
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd->type == type, "Invalid type in inheritance metadata");
			variant_match(imd->kind) {
				variant_case(vm::InheritanceMetadata::Class, clazz) {
					assertFalse(clazz.is_abstract, "Child should be a concrete class");
					ASSERT_HAS_VALUE(clazz.extends, "Child should have a superclass");
					assertTrue(*clazz.extends == super_type, "Child should be a Parent's child");
				}
				variant_default { fail("Child should be a class"); }
			}
			assertTrue(
				imd->implements == interfaces, base::strConcat("Child implements wrong interfaces")
			);

			assertTrue(imd->available_methods.size() == 4, "Child should have 4 available methods");
			assertTrue(imd->vtable.size() == 4, "Child should have 4 method in its vtable");

			auto cry_type = imd->available_methods[base::StrID("cry")];
			assertTrue(cry_type == expected_child_method, "Invalid Child vmethod type: cry()");

			auto vt_foo     = imd->vtable[base::StrID("foo")];
			auto vt_bar     = imd->vtable[base::StrID("bar")];
			auto vt_get_age = imd->vtable[base::StrID("getAge")];
			auto vt_cry     = imd->vtable[base::StrID("cry")];

			assertTrue(vt_foo == expected_i1_foo_impl, "Invalid method implementation in vt: foo()");
			assertTrue(
				vt_bar == expected_child_i1_impl, "Invalid method implementation in vt: bar()"
			);
			assertTrue(
				vt_get_age == expected_child_get_age_impl,
				"Invalid method implementation in vt: getAge()"
			);
			assertTrue(
				vt_cry == expected_child_cry_impl, "Invalid method implementation in vt: cry()"
			);

			return;
		}

		fail("Child should not be plain");
	}

	void checkPietMondrian(vm::TypeCRef type) {
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd->type == type, "Invalid type in inheritance metadata");
			variant_match(imd->kind) {
				variant_case(vm::InheritanceMetadata::Class, clazz) {
					assertTrue(clazz.is_abstract, "Piet mondrian was an *abstract* art pioneer");
					ASSERT_NO_VALUE(clazz.extends, "PietMondrian should not extend anything");
				}
				variant_default { fail("PietMondrian should be a class"); }
			}

			assertTrue(imd->implements.empty(), "PietMondrian should implement no interfaces");
			assertTrue(
				imd->available_methods.empty(), "PietMondrian should have no virtual methods"
			);
			assertTrue(imd->vtable.empty(), "PietMondrian's  vtable should be empty");

			return;
		}

		fail("PietMondrian should not be plain");
	}

	void metadataLoading() {
		const vm::PID pid = spawnAndLoad("../../common/programs/inheritance_metadata.dbc");

		auto               pod           = getType(pid, "POD");
		auto               i1            = getType(pid, "I1");
		auto               i2            = getType(pid, "I2");
		auto               parent        = getType(pid, "Parent");
		auto               child         = getType(pid, "Child");
		auto               piet_mondrian = getType(pid, "PietMondrian");
		std::unordered_set interfaces{ i1, i2 };

		auto i1_method           = getType(pid, "method_I1_int");
		auto i1_foo_impl         = base::StrID("I1_foo_impl");
		auto parent_method       = getType(pid, "method_parent_int");
		auto parent_get_age_impl = base::StrID("Parent_getAge_impl");
		auto child_method        = getType(pid, "method_child_int");
		auto child_get_age_impl  = base::StrID("Child_getAge_impl");
		auto child_i1_impl       = base::StrID("Child_I1_impl");
		auto child_cry_impl      = base::StrID("Child_cry_impl");

		checkPod(pod);
		checkI1(i1, i1_method, i1_foo_impl);
		checkI2(i2);
		checkParent(parent, parent_method, parent_get_age_impl);
		checkChild(
			child,
			parent,
			interfaces,
			i1_foo_impl,
			child_method,
			child_get_age_impl,
			child_i1_impl,
			child_cry_impl
		);
		checkPietMondrian(piet_mondrian);
	}

	/**
	 * @brief A subclass does not repeat the fields of its superclass. It gets them from the
	 * superclass at the same offsets, so a pointer to it can be upcast without moving any field.
	 */
	void inheritedFieldsKeepTheirOffsets() {
		const vm::PID pid    = spawnAndLoad("inherited_fields.dbc");
		auto          widget = getType(pid, "Widget");
		auto          button = getType(pid, "Button");

		for (const base::StrID field: { base::StrID("pos_x"), base::StrID("pos_y") }) {
			auto in_widget = widget->getFieldOffsetByName(field);
			auto in_button = button->getFieldOffsetByName(field);
			ASSERT_HAS_VALUE(in_widget, "Widget should declare `", field.str(), "`");
			ASSERT_HAS_VALUE(in_button, "Button should inherit `", field.str(), "` from Widget");
			assertTrue(
				*in_widget == *in_button,
				base::strConcat(
					"`", field.str(), "` should have the same offset in Button and Widget"
				)
			);
		}

		ASSERT_HAS_VALUE(button->getFieldOffsetByName(base::StrID("n_clicked")));
		ASSERT_NO_VALUE(
			widget->getFieldOffsetByName(base::StrID("n_clicked")),
			"Widget should not get the fields of its subclass"
		);
		assertTrue(
			button->getFields().value()->size() == widget->getFields().value()->size() + 1,
			"Button should have the fields of Widget plus its own one"
		);
		assertSucceeded(vm::api::kill(pid), "kill");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/lowering/");
