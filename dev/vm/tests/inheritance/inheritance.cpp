#include <base/optional.hpp>
#include <base/variant.hpp>
#include <variant>
#include <vm/api/api.hpp>
#include <tester/tester.hpp>

class VmInheritanceTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(metadataLoading); }


private:
	vm::TypeCRef getType(vm::PID pid, const std::string& name) {
		auto response = vm::api::getType(pid, name);
		assertTrue(response.has_value(), "Type query failed");
		return response.value();
	}

	void checkPod(vm::TypeCRef type) {
		assertFalse(type->getVTable().has_value(), "Plain data should be plain");
	}

	void checkI1(vm::TypeCRef type, vm::TypeCRef method_type) {
		if_opt_some(type->getVTable(), vtable) {
			assertTrue(vtable.implements.empty(), "I1 should not implement anything");
			assertTrue(vtable.virtual_methods.size() == 2, "I1 should have two virtual methods");
			auto foo_type = vtable.virtual_methods[base::StrID("foo")];
			auto bar_type = vtable.virtual_methods[base::StrID("bar")];

			assertTrue(foo_type == bar_type, "I2's methods should have the same type");
			assertTrue(foo_type == method_type, "I1's method have the wrong type");

			assertTrue(
				std::holds_alternative<vm::VTable::Interface>(vtable.kind),
				"I2 should be an interface"
			);

			return;
		}

		fail("I1 should not be plain");
	}

	void checkI2(vm::TypeCRef type) {
		if_opt_some(type->getVTable(), vtable) {
			assertTrue(
				std::holds_alternative<vm::VTable::Interface>(vtable.kind),
				"I2 should be an interface"
			);
			assertTrue(vtable.implements.empty(), "I2 should not implement anything");
			assertTrue(vtable.virtual_methods.empty(), "I2 should not have any virtual methods");

			return;
		}

		fail("I1 should not be plain");
	}

	void checkParent(vm::TypeCRef type, vm::TypeCRef method_type) {
		if_opt_some(type->getVTable(), vtable) {
			variant_match(vtable.kind) {
				variant_case(vm::VTable::Class, clazz) {
					assertFalse(clazz.extends.has_value(), "Parent should not extend anything");
				}
				variant_default { fail("Parent should be a class"); }
			}
			assertTrue(vtable.implements.empty(), "Parent should not implement anything");
			assertTrue(
				vtable.virtual_methods.size() == 1, "Parent should implement one virtual method"
			);

			auto lorem = vtable.virtual_methods[base::StrID("lorem")];
			assertTrue(lorem == method_type, "Invalid Parent method type");

			return;
		}

		fail("Parent should not be plain");
	}

	void checkChild(
		vm::TypeCRef type, vm::TypeCRef super_type, const std::vector<vm::TypeCRef>& interfaces
	) {
		if_opt_some(type->getVTable(), vtable) {
			variant_match(vtable.kind) {
				variant_case(vm::VTable::Class, clazz) {
					assertTrue(clazz.extends.has_value(), "Child has no superclass");
					assertTrue(*clazz.extends == super_type, "Child is not Parent's child");
				}
				variant_default { fail("Parent should be a class"); }
			}
			assertTrue(
				std::ranges::equal(
					vtable.implements,
					interfaces,
					// Custom comparison needed since one is nonconst.
					[](vm::TypeCRef a, vm::TypeCRef b) { return a == b; }
				),
				"Child implements wrong interfaces"
			);

			assertTrue(vtable.virtual_methods.empty(), "Child should have no virtual methods");

			return;
		}

		fail("Child should not be plain");
	}

	void metadataLoading() {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (1)");
		auto pid = process_pid_response.expect("Spawn failed (2)").pid;

		fs::FilePath file(path("inheritance_metadata.dbc"));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		assertTrue(loaded_file_response.has_value(), "Load failed (1)");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		auto join_response = vm::api::join(pid);
		assertTrue(join_response.has_value(), "Join failed (1)");

		auto        pod    = getType(pid, "POD");
		auto        i1     = getType(pid, "I1");
		auto        i2     = getType(pid, "I2");
		auto        parent = getType(pid, "Parent");
		auto        child  = getType(pid, "Child");
		std::vector interfaces{ i1, i2 };
		auto        i1_method     = getType(pid, "method_I1_int");
		auto        parent_method = getType(pid, "method_parent_int_int");

		checkPod(pod);
		checkI1(i1, i1_method);
		checkI2(i2);
		checkParent(parent, parent_method);
		checkChild(child, parent, interfaces);
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
