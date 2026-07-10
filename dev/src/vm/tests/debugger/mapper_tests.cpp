#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/debugger/mapper.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#define ASSERT_FALSE(actual) ASSERT_TRUE(!(actual))

class VmDebugTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(mappingTest);
	}


private:

	/**
	 * @brief Checks mapping.
	 */
	void mappingTest() {
		// Normally debug symbols contain absolute paths but it's impossible to use them simply in tests, so let's unify relative ones
		auto original_path = std::filesystem::current_path();
		std::filesystem::current_path(path(""));

		vm::debugger::Mapper mapper;

		ASSERT_NO_VALUE(mapper.mainFile());
		ASSERT_FALSE(mapper.containsFile("abc"));

		ASSERT_HAS_VALUE(mapper.loadMapping(fs::File(path("package_dvm.di.json"))));

		auto simple = fs::FilePath("simple.dmf");
		ASSERT_TRUE(mapper.containsFile(simple));

		auto assert_mapping = [&](usize line, base::StrID function, usize index, bool bidirectional = false) {
			{
				auto map_response = mapper.mapSourcePositionToCodePosition(simple, line);

				ASSERT_HAS_VALUE(map_response);
				auto code_position = map_response.value();

				ASSERT_EQUAL_PRINT(code_position.first, function);
				ASSERT_EQUAL_PRINT(code_position.second, index);
			}
			if (bidirectional) {
				auto map_response = mapper.mapCodePositionToSourcePosition(function, index);

				ASSERT_HAS_VALUE(map_response);
				auto source_position = map_response.value();

				ASSERT_EQUAL_PRINT(source_position.getStartLineColumn().first, line);
			}
		};

		auto assert_no_maping = [&](usize line) {
			ASSERT_NO_VALUE(mapper.mapSourcePositionToCodePosition(simple, line));
		};

		auto main = base::StrID("main");
		auto addAndPrint = base::StrID("_Q_M6simpleG11addAndPrintFi64i64i64E1a1bE");

		assert_mapping(15, main, 0);
		assert_mapping(16, main, 1, true);
		assert_mapping(17, main, 59, true);
		assert_mapping(6, addAndPrint, 1, true);
		assert_no_maping(4);
		assert_no_maping(20);

		std::filesystem::current_path(original_path);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
