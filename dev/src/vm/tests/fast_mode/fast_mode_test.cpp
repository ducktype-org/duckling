#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include "vm/api/data/api_error.hpp"
#include "vm/api/data/process_info.hpp"
#include "vm/api/vm.hpp"
#include <vm/bytecode/validator/errors.hpp>

class VmFunctionsTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFunctionsTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testSimple); }

private:
	vm::PID spawnFastAndLoad(const std::string& dbc_filename) {
		auto pid = initProcess(
			{
				.mode = vm::api::ProcessMode::Fast,
			}
		);
		auto file        = fs::File(path(dbc_filename));
		auto load_result = vm::api::loadFiles(pid, { file });
		if (!load_result.has_value()) {
			std::cerr << std::format("Error loading file: {}\n", errorToString(load_result.error()));
		}
		ASSERT_TRUE(load_result.has_value());
		return pid;
	}

	void testFast(
		const std::string& file_name, std::string input, std::string output, i64 exit_code
	) {
		auto pid        = spawnFastAndLoad(file_name);
		auto run_result = vm::api::run(pid, {});
		ASSERT_TRUE(run_result.has_value());
		auto join_result = vm::api::join(pid);
		ASSERT_TRUE(join_result.has_value());
		auto exit_value = vm::api::getExitValue(pid);
		if (!exit_value.has_value())
			std::cerr << vm::api::errorToString(exit_value.error()) << '\n';
		ASSERT_TRUE(exit_value.has_value());
		variant_match(exit_value.value()) {
			variant_case(i64, value) { ASSERT_EQUAL_PRINT(value, exit_code); }
			variant_default { fail("Unexpected exit value type"); }
		}

	}

	void testSimple() { testFast("simple.dbc", "", "", 42); }

	void testFib() { testFast("simple.dbc", "5", "3", 0); }
};

TESTER_COMMON_MAIN("/src/vm/tests/fast_mode/");
