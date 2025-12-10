#include <vm_tester_utils.hpp>

#include "tester/tester.hpp"

#include "vm/api/data/api_error.hpp"
#include <vm/api/vm.hpp>
#include <vm/bytecode/extern_c_function.hpp>

namespace simple {
	DEF_VM_EXT_C_FUNC(i64, "i64", add, (i64, "i64", a), (i64, "i64", b)) { return a + b; }
}

namespace cpp_vector {
	std::vector<i64> vec;

	DEF_VM_EXT_C_FUNC(std::vector<i64>*, "opaque_ptr", vecSpawn) { return &vec; }

	DEF_VM_EXT_C_FUNC(
		std::vector<i64>*,
		"opaque_ptr",
		vecPushBack,
		(std::vector<i64>*, "opaque_ptr", vec),
		(i64, "i64", value)
	) {
		vec->push_back(value);
		return vec;
	}

	DEF_VM_EXT_C_FUNC(u64, "i64", vecSize, (std::vector<i64>*, "opaque_ptr", vec)) {
		return vec->size();
	}
}

namespace global_opaque {
	std::vector<i64> vec;

	DEF_VM_EXT_C_FUNC(
		void,
		"void",
		vecPushBack,
		(std::vector<i64>*, "opaque_ptr", vec),
		(i64, "i64", value)
	) {
		vec->push_back(value);
	}

	DEF_VM_EXT_C_FUNC(u64, "i64", vecSize, (std::vector<i64>*, "opaque_ptr", vec)) {
		return vec->size();
	}
}

class VmExternCppTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmExternCppTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simple);
		TESTER_ADD_TEST(cppVectorInVm);
		TESTER_ADD_TEST(globalOpaques);
	}

private:
	void simple() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			ASSERT_TRUE(
				vm::api::loadCode(
					pid,
					{
						.functions            = {},
						.types                = {},
						.global_data          = {},
						.external_c_functions = { VM_INSTANCE_EXT_C_FUNC(add, simple::add, pid) },
					}
				)
					.has_value()
			);
			ASSERT_TRUE(vm::api::loadFiles(pid, { fs::File(path("extern_test.dbc")) }).has_value());
			return pid;
		};

		runTestOnVm(get_ext_func_program(), { "1 2" }, { "3" });
		runTestOnVm(get_ext_func_program(), { "5 3" }, { "8" });
	}

	void cppVectorInVm() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			ASSERT_TRUE(
				vm::api::loadCode(
					pid,
					{ .functions            = {},
			          .types                = {},
			          .global_data          = {},
			          .external_c_functions = {
						  VM_INSTANCE_EXT_C_FUNC(vecSpawn, cpp_vector::vecSpawn, pid),
						  VM_INSTANCE_EXT_C_FUNC(vecPushBack, cpp_vector::vecPushBack, pid),
						  VM_INSTANCE_EXT_C_FUNC(vecSize, cpp_vector::vecSize, pid),
					  } }
				).has_value()
			);
			ASSERT_TRUE(
				vm::api::loadFiles(pid, { fs::File(path("cpp_vector_in_vm.dbc")) }).has_value()
			);
			return pid;
		};
		runTestOnVm(get_ext_func_program(), { "123" }, { "1" });
		ASSERT_EQUAL_PRINT(cpp_vector::vec.size(), 1);
		ASSERT_EQUAL(cpp_vector::vec[0], 123);
	}

	void globalOpaques() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			ASSERT_TRUE(
				vm::api::loadCode(
					pid,
					{ .functions            = {},
			          .types                = {},
			          .global_data          = {},
			          .external_c_functions = {
						  VM_INSTANCE_EXT_C_FUNC(vecPushBack, cpp_vector::vecPushBack, pid),
						  VM_INSTANCE_EXT_C_FUNC(vecSize, cpp_vector::vecSize, pid),
					  } }
				).has_value()
			);
			ASSERT_TRUE(
				vm::api::loadFiles(pid, { fs::File(path("global_opaque.dbc")) }).has_value()
			);
			return pid;
		};

		vm::PID pid = get_ext_func_program();

		// Prepare the initializing argument.
		auto vm_value_response = vm::api::getVmValue(pid, "opaque_ptr");
		ASSERT_TRUE(vm_value_response.has_value());
		auto  vm_value   = std::move(vm_value_response->vm_value);
		auto* vector_ptr = &global_opaque::vec;
		vm_value->writeBytes(vector_ptr);

		// Initialize the global vector pointer
		ASSERT_TRUE(vm::api::runFunction(pid, "initialize_vector", { vm_value.refMut() }).has_value());
		ASSERT_TRUE(vm::api::join(pid));
		
		vm_value->freeData();

		runTestOnVm(pid, { "1 2 3" }, { "3" });
		ASSERT_EQUAL_PRINT(cpp_vector::vec.size(), 4	);
		ASSERT_EQUAL(cpp_vector::vec[0], 1);
		ASSERT_EQUAL(cpp_vector::vec[1], 2);
		ASSERT_EQUAL(cpp_vector::vec[2], 3);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/extern_c/");
