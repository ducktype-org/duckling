#include <vm_tester_utils.hpp>

#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/api/api.hpp>

#include <variant>

class VmInheritanceLoadingTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceLoadingTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(metadataLoading); }


private:
	vm::TypeCRef getType(vm::PID pid, const std::string& name) {
		auto response = vm::api::getType(pid, name);
		assertTrue(response.has_value(), "Type query failed for: " + name);
		return response.value();
	}

	void checkPod(vm::TypeCRef type) {
		assertFalse(type->getInheritanceMetadata().has_value(), "Plain data should be plain");
	}

	void checkI1(vm::TypeCRef type, vm::TypeCRef method_type, vm::TypeCRef method_impl_type) {
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd.type == type, "Invalid type in inheritance metadata");
			assertTrue(
				std::holds_alternative<vm::InheritanceMetadata::Interface>(imd.kind),
				"I1 should be an interface"
			);

			assertTrue(imd.implements.empty(), "I1 should not implement anything");
			assertTrue(imd.virtual_methods.size() == 2, "I1 should have two virtual methods");
			assertTrue(
				imd.vmethods_implementations.size() == 1, "I1 should implement virtual methods"
			);

			auto foo_type      = imd.virtual_methods[base::StrID("foo")];
			auto bar_type      = imd.virtual_methods[base::StrID("bar")];
			auto foo_impl_type = imd.vmethods_implementations[base::StrID("foo")];
			assertTrue(foo_type == bar_type, "I2's methods should have the same type");
			assertTrue(foo_type == method_type, "I1's method have the wrong type");
			assertTrue(
				foo_impl_type == method_impl_type, "I1's method implementation have the wrong type"
			);

			return;
		}

		fail("I1 should not be plain");
	}

	void checkI2(vm::TypeCRef type) {
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd.type == type, "Invalid type in inheritance metadata");
			assertTrue(
				std::holds_alternative<vm::InheritanceMetadata::Interface>(imd.kind),
				"I2 should be an interface"
			);
			assertTrue(imd.implements.empty(), "I2 should not implement anything");
			assertTrue(imd.virtual_methods.empty(), "I2 should not have any virtual methods");
			assertTrue(
				imd.vmethods_implementations.empty(), "I2 should not implement any virtual methods"
			);

			return;
		}

		fail("I2 should not be plain");
	}

	void checkParent(vm::TypeCRef type, vm::TypeCRef method_type, vm::TypeCRef method_impl_type) {
		std::cerr << "Check parent\n";
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd.type == type, "Invalid type in inheritance metadata");
			variant_match(imd.kind) {
				variant_case(vm::InheritanceMetadata::Class, clazz) {
					assertFalse(clazz.is_abstract, "Parent should be a concrete class");
					assertFalse(clazz.extends.has_value(), "Parent should not extend anything");
				}
				variant_default { fail("Parent should be a class"); }
			}

			assertTrue(imd.implements.empty(), "Parent should not implement anything");
			assertTrue(imd.virtual_methods.size() == 1, "Parent should declare one virtual method");
			assertTrue(
				imd.vmethods_implementations.size() == 1,
				"Parent should implement one virtual method"
			);

			auto get_age_type      = imd.virtual_methods[base::StrID("getAge")];
			auto get_age_impl_type = imd.vmethods_implementations[base::StrID("getAge")];
			assertTrue(get_age_type == method_type, "Invalid Parent method type");
			assertTrue(get_age_impl_type == method_impl_type, "Invalid Parent method type");

			return;
		}

		fail("Parent should not be plain");
	}

	void checkChild(
		vm::TypeCRef                     type,
		vm::TypeCRef                     super_type,
		const std::vector<vm::TypeCRef>& interfaces,
		vm::TypeCRef                     expected_get_age_type,
		vm::TypeCRef                     expected_foo_bar_type,
		vm::TypeCRef                     expected_get_age_impl_type,
		vm::TypeCRef                     expected_bar_impl_type
	) {
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd.type == type, "Invalid type in inheritance metadata");
			variant_match(imd.kind) {
				variant_case(vm::InheritanceMetadata::Class, clazz) {
					assertFalse(clazz.is_abstract, "Child should be a concrete class");
					assertTrue(clazz.extends.has_value(), "Child should have a superclass");
					assertTrue(*clazz.extends == super_type, "Child should be a Parent's child");
				}
				variant_default { fail("Child should be a class"); }
			}
			assertTrue(
				std::ranges::equal(
					imd.implements,
					interfaces,
					// Custom comparison needed since one is nonconst.
					[](vm::TypeCRef a, vm::TypeCRef b) { return a == b; }
				),
				"Child implements wrong interfaces"
			);

			assertTrue(
				imd.virtual_methods.size() == 3, "Child should implement getAge(), foo() and bar()"
			);
			assertTrue(
				imd.vmethods_implementations.size() == 2,
				"Child should implement two virtual methods: bar(), getAge()"
			);

			auto get_age_type = imd.virtual_methods[base::StrID("getAge")];
			auto foo_type     = imd.virtual_methods[base::StrID("foo")];
			auto bar_type     = imd.virtual_methods[base::StrID("bar")];

			auto get_age_impl_type = imd.vmethods_implementations[base::StrID("getAge")];
			auto bar_impl_type     = imd.vmethods_implementations[base::StrID("bar")];

			assertTrue(
				get_age_type == expected_get_age_type, "Invalid Child vmethod type: getAge()"
			);
			assertTrue(foo_type == expected_foo_bar_type, "Invalid Child vmethod type: foo()");
			assertTrue(bar_type == expected_foo_bar_type, "Invalid Child vmethod type: bar()");
			assertTrue(
				get_age_impl_type == expected_get_age_impl_type,
				"Invalid Child method type: getAge()"
			);
			assertTrue(bar_impl_type == expected_bar_impl_type, "Invalid Child method type: bar()");
			return;
		}

		fail("Child should not be plain");
	}

	void checkPietMondrian(vm::TypeCRef type) {
		if_opt_some(type->getInheritanceMetadata(), imd) {
			assertTrue(imd.type == type, "Invalid type in inheritance metadata");
			variant_match(imd.kind) {
				variant_case(vm::InheritanceMetadata::Class, clazz) {
					assertTrue(clazz.is_abstract, "Piet mondrian was an *abstract* art pioneer");
					assertFalse(
						clazz.extends.has_value(), "PietMondrian should not extend anything"
					);
				}
				variant_default { fail("PietMondrian should be a class"); }
			}

			assertTrue(imd.implements.empty(), "PietMondrian should implement no interfaces");
			assertTrue(imd.virtual_methods.empty(), "PietMondrian should have no virtual methods");
			assertTrue(
				imd.vmethods_implementations.empty(),
				"PietMondrian should not implement any virtual methods"
			);

			return;
		}

		fail("PietMondrian should not be plain");
	}

	void metadataLoading() {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (1)");
		auto pid = process_pid_response.value().pid;

		fs::FilePath file(path("inheritance_metadata.dbc"));
		assertTrue(vm::api::loadFiles(pid, { file }).has_value(), "Load failed (1)");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		auto join_response = vm::api::join(pid);
		assertTrue(join_response.has_value(), "Join failed (1)");

		auto        pod           = getType(pid, "POD");
		auto        i1            = getType(pid, "I1");
		auto        i2            = getType(pid, "I2");
		auto        parent        = getType(pid, "Parent");
		auto        child         = getType(pid, "Child");
		auto        piet_mondrian = getType(pid, "PietMondrian");
		std::vector interfaces{ i1, i2 };

		auto i1_method           = getType(pid, "method_I1_int");
		auto parent_method       = getType(pid, "method_parent_int");
		auto parent_get_age_impl = getType(pid, "Parent_getAge_impl");
		auto child_get_age_impl  = getType(pid, "Child_getAge_impl");
		auto child_i1_impl       = getType(pid, "Child_I1_impl");
		auto i1_foo_impl         = getType(pid, "I1_foo_impl");

		checkPod(pod);
		checkI1(i1, i1_method, i1_foo_impl);
		checkI2(i2);
		checkParent(parent, parent_method, parent_get_age_impl);
		checkChild(
			child, parent, interfaces, parent_method, i1_method, child_get_age_impl, child_i1_impl
		);
		checkPietMondrian(piet_mondrian);
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
