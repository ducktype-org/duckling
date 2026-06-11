#include <psackling/compiler.hpp>

#include <filesystem/file.hpp>
#include <system_command/system_command.hpp>
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

	static u64 runExecutable(const fs::File& executable) {
		system_command::SystemCommand cmd(executable.getFilePath().string());
		return u64(cmd.execute(system_command::SystemCommand::ExitCodeHandling::Ignore));
	}

	/// Changing one function body recompiles exactly that function; identical source
	/// recompiles nothing.
	void incrementalBodyChange() {
		const std::string v1 = R"(
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper() + 1; }
		)";
		const std::string v2 = R"(
			fun helper() -> i64 = { return 20; }
			fun main() -> i64 = { return helper() + 1; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		pg::CompileStats initial = compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(2), initial.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), initial.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(11), runExecutable(executable));

		pg::CompileStats unchanged = compiler.update(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(0), unchanged.recompiled_functions);

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(21), runExecutable(executable));
	}

	/// Changing a global constant recompiles every function (coarse globals dependency).
	void incrementalGlobalsChange() {
		const std::string v1 = R"(
			const X: i64 = 5;
			fun helper() -> i64 = { return X; }
			fun main() -> i64 = { return helper() + X; }
		)";
		const std::string v2 = R"(
			const X: i64 = 7;
			fun helper() -> i64 = { return X; }
			fun main() -> i64 = { return helper() + X; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_globals.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(2), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(14), runExecutable(executable));
	}

	/// Changing a global variable initializer re-emits the globals object and (via the
	/// coarse globals hash) recompiles every function.
	void incrementalGlobalVarChange() {
		const std::string v1 = R"(
			var g: i64 = 5;
			fun main() -> i64 = { return g; }
		)";
		const std::string v2 = R"(
			var g: i64 = 9;
			fun main() -> i64 = { return g; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_gvar.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(5), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));
	}

	/// Adding a function changes the function set, which falls back to a full rebuild.
	void incrementalFunctionSetChange() {
		const std::string v1 = R"(
			fun main() -> i64 = { return 1; }
		)";
		const std::string v2 = R"(
			fun extra() -> i64 = { return 41; }
			fun main() -> i64 = { return extra() + 1; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_set.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(1), runExecutable(executable));

		pg::CompileStats rebuilt = compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(2), rebuilt.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), rebuilt.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(42), runExecutable(executable));
	}

	/// Incremental updates also work in --sequential mode (hash diff without PSAC).
	void incrementalSequentialMode() {
		const std::string v1 = R"(
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper(); }
		)";
		const std::string v2 = R"(
			fun helper() -> i64 = { return 30; }
			fun main() -> i64 = { return helper(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_seq.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string(),
		                                          .sequential  = true });

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(30), runExecutable(executable));
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

		PS_TEST(
			TimesEquals,
			R"(
				fun main() -> i64 = {
					var x: i64 = 5;
					x *= 3;
					return x;
				}
			)",
			15
		);

		PS_TEST(
			DivideEquals,
			R"(
				fun main() -> i64 = {
					var x: i64 = 20;
					x /= 4;
					return x;
				}
			)",
			5
		);

		PS_TEST(
			ModuloEquals,
			R"(
				fun main() -> i64 = {
					var x: i64 = 17;
					x %= 5;
					return x;
				}
			)",
			2
		);

		PS_TEST(
			ModuloBinary,
			R"(
				fun main() -> i64 = {
					return 17 % 5;
				}
			)",
			2
		);

		PS_TEST(
			CompoundOpsCombined,
			R"(
				fun main() -> i64 = {
					var x: i64 = 2;
					x *= 5;
					x += 7;
					x /= 2;
					x -= 1;
					x %= 6;
					return x;
				}
			)",
			1
		);  // 2*5=10, +7=17, /2=8, -1=7, %6=1

		PS_TEST(
			FieldCompoundOps,
			R"(
				class Box {
					var v: i64;
				}
				fun main() -> i64 = {
					var b: Box;
					b.v = 7;
					b.v *= 3;
					b.v %= 4;
					return b.v;
				}
			)",
			1
		);  // 7*3=21, 21%4=1

		// --- Functions and calls in expressions ---

		PS_TEST(
			CallingFunction,
			R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = { return add(2, 5); }
			)",
			7
		);

		PS_TEST(
			CallNoArgs,
			R"(
				fun five() -> i64 = { return 5; }
				fun main() -> i64 = { return five(); }
			)",
			5
		);

		PS_TEST(
			NestedCalls,
			R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = { return add(add(1, 2), add(3, 4)); }
			)",
			10
		);

		PS_TEST(
			CallInArithmetic,
			R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = { return add(2, 3) * 2; }
			)",
			10
		);

		PS_TEST(
			CallWithVariableArgs,
			R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = {
					var x: i64 = 3;
					return add(x, 4);
				}
			)",
			7
		);

		PS_TEST(
			ForwardCall,
			R"(
				fun main() -> i64 = { return helper(); }
				fun helper() -> i64 = { return 9; }
			)",
			9
		);

		PS_TEST(
			Recursion,
			R"(
				fun factorial(n: i64) -> i64 = {
					if (n < 2) {
						return 1;
					}
					return n * factorial(n - 1);
				}
				fun main() -> i64 = { return factorial(5); }
			)",
			120
		);

		PS_TEST(
			FibRecursion,
			R"(
				fun fib(n: i64) -> i64 = {
					if (n < 2) {
						return n;
					}
					return fib(n - 1) + fib(n - 2);
				}
				fun main() -> i64 = { return fib(10); }
			)",
			55
		);

		PS_TEST(
			VoidFunctionCall,
			R"(
				fun nothing() = {
					var x: i64 = 1;
					x = 2;
				}
				fun main() -> i64 = {
					nothing();
					return 7;
				}
			)",
			7
		);

		PS_TEST(
			VoidExplicitReturn,
			R"(
				fun nothing() = { return; }
				fun main() -> i64 = {
					nothing();
					return 3;
				}
			)",
			3
		);

		PS_TEST(
			BoolParamAndReturn,
			R"(
				fun isPositive(x: i64) -> bool = { return 0 < x; }
				fun main() -> i64 = {
					if (isPositive(5)) {
						return 1;
					}
					return 0;
				}
			)",
			1
		);

		// --- Constant values ---

		PS_TEST(
			GlobalConst,
			R"(
				const X: i64 = 42;
				fun main() -> i64 = { return X; }
			)",
			42
		);

		PS_TEST(
			GlobalConstExprInit,
			R"(
				const X: i64 = 2 * 3 + 4;
				fun main() -> i64 = { return X; }
			)",
			10
		);

		PS_TEST(
			ConstFromConst,
			R"(
				const A: i64 = 5;
				const B: i64 = A + 2;
				fun main() -> i64 = { return B; }
			)",
			7
		);

		PS_TEST(
			LocalConst,
			R"(
				fun main() -> i64 = {
					const Y: i64 = 7;
					return Y;
				}
			)",
			7
		);

		PS_TEST(
			ConstInArithmetic,
			R"(
				const X: i64 = 10;
				fun main() -> i64 = { return X * 2 + 1; }
			)",
			21
		);

		PS_TEST(
			BoolConst,
			R"(
				const T: bool = true;
				fun main() -> i64 = {
					if (T) {
						return 3;
					}
					return 0;
				}
			)",
			3
		);

		// --- Booleans ---

		PS_TEST(
			BoolVar,
			R"(
				fun main() -> i64 = {
					var b: bool = true;
					if (b) {
						return 1;
					}
					return 0;
				}
			)",
			1
		);

		PS_TEST(
			BoolVarFromComparison,
			R"(
				fun main() -> i64 = {
					var b: bool = 1 < 2;
					if (b) {
						return 5;
					}
					return 0;
				}
			)",
			5
		);

		PS_TEST(
			BoolAssignment,
			R"(
				fun main() -> i64 = {
					var b: bool = false;
					b = true;
					if (b) {
						return 4;
					}
					return 0;
				}
			)",
			4
		);

		PS_TEST(
			BoolAnd,
			R"(
				fun main() -> i64 = {
					if (true and false) {
						return 1;
					}
					return 2;
				}
			)",
			2
		);

		PS_TEST(
			BoolOr,
			R"(
				fun main() -> i64 = {
					if (false or true) {
						return 3;
					}
					return 0;
				}
			)",
			3
		);

		PS_TEST(
			BoolAndVars,
			R"(
				fun main() -> i64 = {
					var a: bool = true;
					var b: bool = false;
					if (a and b) {
						return 1;
					}
					if (a or b) {
						return 2;
					}
					return 0;
				}
			)",
			2
		);

		// --- Namespaces ---

		PS_TEST(
			NamespaceFunction,
			R"(
				namespace math {
					fun add(a: i64, b: i64) -> i64 = { return a + b; }
				}
				fun main() -> i64 = { return math.add(2, 3); }
			)",
			5
		);

		PS_TEST(
			NestedNamespaces,
			R"(
				namespace a {
					namespace b {
						fun f() -> i64 = { return 6; }
					}
				}
				fun main() -> i64 = { return a.b.f(); }
			)",
			6
		);

		PS_TEST(
			NamespaceConst,
			R"(
				namespace m {
					const PI: i64 = 3;
				}
				fun main() -> i64 = { return m.PI; }
			)",
			3
		);

		PS_TEST(
			NamespaceShadowsGlobalName,
			R"(
				fun f() -> i64 = { return 1; }
				namespace n {
					fun f() -> i64 = { return 2; }
				}
				fun main() -> i64 = { return f() + n.f(); }
			)",
			3
		);

		PS_TEST(
			SiblingCallInNamespace,
			R"(
				namespace n {
					fun a() -> i64 = { return 1; }
					fun b() -> i64 = { return a() + 1; }
				}
				fun main() -> i64 = { return n.b(); }
			)",
			2
		);

		PS_TEST(
			ManyFunctionsParallel,
			R"(
				fun f1() -> i64 = { return 1; }
				fun f2() -> i64 = { return f1() + 1; }
				fun f3() -> i64 = { return f2() + 1; }
				fun f4() -> i64 = { return f3() + 1; }
				fun f5() -> i64 = { return f4() + 1; }
				fun f6() -> i64 = { return f5() + 1; }
				fun f7() -> i64 = { return f6() + 1; }
				fun f8() -> i64 = { return f7() + 1; }
				namespace deep {
					fun g(x: i64) -> i64 = { return x * 2; }
				}
				fun main() -> i64 = { return deep.g(f8()) + f4(); }
			)",
			20
		);  // f8 = 8, g(8) = 16, f4 = 4 -> 20

		PS_TEST(
			NamespaceConstUsedInFunction,
			R"(
				namespace m {
					const K: i64 = 5;
					fun get() -> i64 = { return K; }
				}
				fun main() -> i64 = { return m.get(); }
			)",
			5
		);

		// --- Global variables ---

		PS_TEST(
			GlobalVarRead,
			R"(
				var g: i64 = 7;
				fun main() -> i64 = { return g; }
			)",
			7
		);

		PS_TEST(
			GlobalVarZeroInit,
			R"(
				var g: i64;
				fun main() -> i64 = { return g; }
			)",
			0
		);

		PS_TEST(
			GlobalVarWrite,
			R"(
				var g: i64 = 1;
				fun main() -> i64 = {
					g = 9;
					return g;
				}
			)",
			9
		);

		PS_TEST(
			GlobalVarConstExprInit,
			R"(
				var g: i64 = 2 * 3 + 4;
				fun main() -> i64 = { return g; }
			)",
			10
		);

		PS_TEST(
			GlobalVarFromConst,
			R"(
				const A: i64 = 6;
				var g: i64 = A;
				fun main() -> i64 = { return g; }
			)",
			6
		);

		PS_TEST(
			GlobalVarSharedAcrossFunctions,
			R"(
				var counter: i64 = 0;
				fun bump() = { counter += 1; }
				fun main() -> i64 = {
					bump();
					bump();
					bump();
					return counter;
				}
			)",
			3
		);

		PS_TEST(
			GlobalVarCompoundOps,
			R"(
				var g: i64 = 5;
				fun main() -> i64 = {
					g *= 4;
					g -= 3;
					g %= 7;
					return g;
				}
			)",
			3
		);  // 5*4=20, -3=17, %7=3

		PS_TEST(
			GlobalBoolVar,
			R"(
				var flag: bool = true;
				fun main() -> i64 = {
					if (flag) {
						flag = false;
					}
					if (flag) {
						return 1;
					}
					return 2;
				}
			)",
			2
		);

		PS_TEST(
			NamespaceGlobalVar,
			R"(
				namespace n {
					var x: i64 = 5;
					fun bump() = { x += 1; }
				}
				fun main() -> i64 = {
					n.bump();
					return n.x;
				}
			)",
			6
		);

		PS_TEST(
			GlobalVarInLoop,
			R"(
				var sum: i64 = 0;
				fun addTo(v: i64) = { sum += v; }
				fun main() -> i64 = {
					var i: i64 = 1;
					while (i < 6) {
						addTo(i);
						i += 1;
					}
					return sum;
				}
			)",
			15
		);  // 1+2+3+4+5

		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalBodyChange),
			"IncrementalBodyChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalGlobalsChange),
			"IncrementalGlobalsChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalGlobalVarChange),
			"IncrementalGlobalVarChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalFunctionSetChange),
			"IncrementalFunctionSetChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalSequentialMode),
			"IncrementalSequentialMode",
			false
		);
	}
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/psac_compiler/tests");
