#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/bytecode/validator/errors.hpp>

class VmFunctionsTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFunctionsTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testSimple); }

private:
	void spawnFastAndLoad(const std::string& dbc_filename) {
		auto pid = initProcess(
			{
				.mode = vm::api::ProcessMode::Fast,
			}
		);
		auto file        = fs::File(path(dbc_filename));
		auto load_result = vm::api::loadFiles(pid, { file });
        if(!load_result.has_value()) {
            std::cerr << std::format("Error loading file: {}\n", errorToString(load_result.error()));
        }
		ASSERT_TRUE(load_result.has_value());
	}

	void testSimple() { spawnFastAndLoad("simple.dbc"); }
};

TESTER_COMMON_MAIN("/src/vm/tests/fast_mode/");
