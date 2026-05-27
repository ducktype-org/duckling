#include <psackling/compiler.hpp>

#include "filesystem/file.hpp"
#include "system_command/system_command.hpp"
#include <tester/tester.hpp>

#include <deque>

#define PS_TEST_DISPATCH(_1, _2, _3, _4, _5, NAME, ...) NAME

#define PS_TEST(...) \
	PS_TEST_DISPATCH(__VA_ARGS__, PS_TEST_5, PS_TEST_4, PS_TEST_3, PS_TEST_2)(__VA_ARGS__)
#define PS_TEST_2(NAME, CODE)                  PS_TEST_3(NAME, CODE, 0)
#define PS_TEST_3(NAME, CODE, EXITCODE)        PS_TEST_4(NAME, CODE, EXITCODE, std::nullopt)
#define PS_TEST_4(NAME, CODE, EXITCODE, INPUT) PS_TEST_5(NAME, CODE, EXITCODE, INPUT, std::nullopt)
#define PS_TEST_5(NAME, CODE, EXITCODE, INPUT, OUPUT)                            \
	do {                                                                         \
		test_units.push_back(                                                    \
			TestUnit{ .name               = #NAME,                               \
		              .code               = std::string(CODE),                   \
		              .expected_exit_code = EXITCODE,                            \
		              .input              = INPUT,                               \
		              .expected_output    = OUPUT }                              \
		);                                                                       \
		addTest(static_cast<TestType>(&PsacklingerTest::theTest), #NAME, false); \
	} while (0)

class PsacklingerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PsacklingerTest

	struct TestUnit {
		std::string                 name;
		std::string                 code;
		u64                         expected_exit_code;
		base::Optional<std::string> input;
		base::Optional<std::string> expected_output;
	};

	std::deque<TestUnit> test_units;
	usize                id = 0;

	void theTest() {
		const TestUnit& data = test_units[id++];
		std::cout << "Running test: " << data.name << '\n';
		fs::File f                 = fs::FileManager::createRandomVirtualFile(data.code);
		fs::File random_output_dir = fs::FileManager::createRandomTempDirectory();
		fs::File executable        = random_output_dir.createSubFile("", "psacklinged.out");
		pg::compile(f, executable.getFilePath().string());
		system_command::SystemCommand cmd(executable.getFilePath().string());
		auto exit_code = cmd.execute(system_command::SystemCommand::ExitCodeHandling::Ignore);
		ASSERT_EQUAL_PRINT(data.expected_exit_code, exit_code);
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		PS_TEST(Return42, "fun main() -> i64 = { return 42; }", 42);

		PS_TEST(
			BasicArithmetic,
			R"(
				fun main() -> i64 = { return 2 + 5 * 3 - 4 / 2; }
			)",
			15
		);

		PS_TEST(
			CallingFunction,
			R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = { return add(2, 5); }
			)",
			7
		);
	}
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/psac_compiler/tests");
