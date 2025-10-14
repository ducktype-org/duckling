#include <vm_tester_utils.hpp>

#include "vm/api/vm.hpp"
#include <vm/bytecode/extern_cpp_function.hpp>

namespace n {
	DEF_VM_EXT_CPP_FUNC(i64, "i64", add, (i64, "i64", a), (i64, "i64", b)) { return a + b; }
}

class VmExternCppTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmExternCppTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(simple); }

private:
	void simple() {
		auto get_ext_func_program = [this]() {
			auto pid = initProcess();
			ASSERT_TRUE(
				vm::api::loadCode(
					pid,
					{ .functions     = {},
			          .types         = {},
			          .global_data   = {},
			          .cpp_functions = { VM_INSTANCE_EXT_CPP_FUNC(n::add, pid) } }
				).has_value()
			);
			ASSERT_TRUE(vm::api::loadFiles(pid, { fs::File(path("extern_test.dbc")) }).has_value());
			return pid;
		};

		runTestOnVm(get_ext_func_program(), { "1 2" }, { "3" });
		runTestOnVm(get_ext_func_program(), { "5 3" }, { "8" });
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/extern_cpp/");
