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
		test_units.push_back(TestUnit{ .name               = #NAME,              \
		                               .code               = std::string(CODE),  \
		                               .expected_exit_code = EXITCODE,           \
		                               .input              = INPUT,              \
		                               .expected_output    = OUPUT });              \
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
			IfThenReturn,
			R"(
				fun main() -> i64 = {
					if (true) {
						return 42;
					}
					return 0;
				}
			)",
			42
		);

		PS_TEST(
			IfFallthrough,
			R"(
				fun main() -> i64 = {
					if (false) {
						return 99;
					}
					return 7;
				}
			)",
			7
		);

		PS_TEST(
			IfElseTakesElse,
			R"(
				fun main() -> i64 = {
					if (false) {
						return 1;
					} else {
						return 2;
					}
				}
			)",
			2
		);

		PS_TEST(
			NestedIf,
			R"(
				fun main() -> i64 = {
					if (true) {
						if (true) {
							return 11;
						}
						return 22;
					}
					return 33;
				}
			)",
			11
		);

		PS_TEST(
			WhileNeverEnters,
			R"(
				fun main() -> i64 = {
					while (false) {
						return 99;
					}
					return 7;
				}
			)",
			7
		);

		PS_TEST(
			WhileBodyExecutes,
			R"(
				fun main() -> i64 = {
					while (true) {
						return 13;
					}
					return 0;
				}
			)",
			13
		);

		PS_TEST(
			ClassDeclares,
			R"(
				class Point {
					var x: i64;
					var y: i64;
				}
				fun main() -> i64 = {
					var p: Point;
					return 0;
				}
			)",
			0
		);

		PS_TEST(
			FieldAccess,
			R"(
				class T {
				    a: i64;
				}
				class U {
					t: T;	
				}

				fun main() -> i64 = {
					var u: U;
					return u.t.a;
				}
			)",
			0
		);

		PS_TEST(
			AssignToLocal,
			R"(
				fun main() -> i64 = {
					var x: i64 = 5;
					x = 10;
					return x;
				}
			)",
			10
		);

		PS_TEST(
			AssignWithoutInit,
			R"(
				fun main() -> i64 = {
					var x: i64;
					x = 42;
					return x;
				}
			)",
			42
		);

		PS_TEST(
			AssignArithmetic,
			R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					x = 2 * 3 + 4;
					return x;
				}
			)",
			10
		);

		PS_TEST(
			AssignSelfReference,
			R"(
				fun main() -> i64 = {
					var x: i64 = 5;
					x = x + 1;
					return x;
				}
			)",
			6
		);

		PS_TEST(
			AssignSequence,
			R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					x = 1;
					x = 2;
					x = 3;
					return x;
				}
			)",
			3
		);

		PS_TEST(
			AssignInsideIf,
			R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					if (1 < 2) {
						x = 100;
					}
					return x;
				}
			)",
			100
		);

		PS_TEST(
			AssignInIfElse,
			R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					if (5 < 2) {
						x = 1;
					} else {
						x = 2;
					}
					return x;
				}
			)",
			2
		);

		PS_TEST(
			PlusEquals,
			R"(
				fun main() -> i64 = {
					var x: i64 = 5;
					x += 3;
					return x;
				}
			)",
			8
		);

		PS_TEST(
			MinusEquals,
			R"(
				fun main() -> i64 = {
					var x: i64 = 10;
					x -= 4;
					return x;
				}
			)",
			6
		);

		PS_TEST(
			PlusEqualsInLoop,
			R"(
				fun main() -> i64 = {
					var sum: i64 = 0;
					var i: i64 = 1;
					while (i < 11) {
						sum += i;
						i += 1;
					}
					return sum;
				}
			)",
			55
		);

		PS_TEST(
			WhileCounter,
			R"(
				fun main() -> i64 = {
					var i: i64 = 0;
					while (i < 10) {
						i = i + 1;
					}
					return i;
				}
			)",
			10
		);

		PS_TEST(
			WhileSum,
			R"(
				fun main() -> i64 = {
					var sum: i64 = 0;
					var i: i64 = 1;
					while (i < 11) {
						sum = sum + i;
						i = i + 1;
					}
					return sum;
				}
			)",
			55
		);

		PS_TEST(
			WhileCountdown,
			R"(
				fun main() -> i64 = {
					var i: i64 = 10;
					while (0 < i) {
						i = i - 1;
					}
					return i;
				}
			)",
			0
		);

		PS_TEST(
			FieldWriteRead,
			R"(
				class Box {
					var v: i64;
				}
				fun main() -> i64 = {
					var b: Box;
					b.v = 42;
					return b.v;
				}
			)",
			42
		);

		PS_TEST(
			FieldMultiple,
			R"(
				class Point {
					var x: i64;
					var y: i64;
				}
				fun main() -> i64 = {
					var p: Point;
					p.x = 3;
					p.y = 4;
					return p.x + p.y;
				}
			)",
			7
		);

		PS_TEST(
			FieldOrderingMatters,
			R"(
				class Triple {
					var a: i64;
					var b: i64;
					var c: i64;
				}
				fun main() -> i64 = {
					var t: Triple;
					t.a = 1;
					t.b = 2;
					t.c = 3;
					return t.a * 100 + t.b * 10 + t.c;
				}
			)",
			123
		);

		PS_TEST(
			FieldCopyToField,
			R"(
				class Pair {
					var a: i64;
					var b: i64;
				}
				fun main() -> i64 = {
					var p: Pair;
					p.a = 5;
					p.b = p.a;
					return p.b;
				}
			)",
			5
		);

		PS_TEST(
			FieldPlusEquals,
			R"(
				class Box {
					var v: i64;
				}
				fun main() -> i64 = {
					var b: Box;
					b.v = 10;
					b.v += 5;
					return b.v;
				}
			)",
			15
		);

		PS_TEST(
			FieldInWhileLoop,
			R"(
				class Counter {
					var n: i64;
				}
				fun main() -> i64 = {
					var c: Counter;
					c.n = 0;
					var i: i64 = 0;
					while (i < 7) {
						c.n = c.n + i;
						i = i + 1;
					}
					return c.n;
				}
			)",
			21
		);  // 0+1+2+3+4+5+6 = 21
	}

	// PS_TEST(
	// 	CallingFunction,
	// 	R"(
	// 		fun add(a: i64, b: i64) -> i64 = { return a + b; }
	// 		fun main() -> i64 = { return add(2, 5); }
	// 	)",
	// 	7
	// );
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/psac_compiler/tests");
