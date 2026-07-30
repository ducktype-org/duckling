#pragma once

#include <base/types/ints.hpp>

#include <filesystem/file.hpp>

#include <atomic>
#include <string>
#include <vector>

/// Shared end-to-end test corpus of the Psackling-family playground compilers
/// (psacklinger and querylinger). Both test suites instantiate every case, so the
/// two compilers are proven to implement identical language behavior.
namespace psack_corpus {
	struct SingleFileCase {
		const char* name               = nullptr;
		const char* code               = nullptr;
		u64         expected_exit_code = 0;
	};

	struct ProgramFile {
		const char* relative_path = nullptr;
		const char* code          = nullptr;
	};

	/// A multi-file program (the first file is the root) or an error case: when
	/// `error_substring` is non-null, compilation must throw a std::runtime_error
	/// whose message contains it.
	struct MultiFileCase {
		const char*              name = nullptr;
		std::vector<ProgramFile> files;
		u64                      expected_exit_code = 0;
		const char*              error_substring    = nullptr;
	};

	/// Creates a fresh virtual directory with the given program files. Relative paths
	/// may contain '/' — intermediate folders are created. Returns the first file
	/// (the program root).
	inline fs::File makeProgramDir(const std::vector<ProgramFile>& files) {
		static std::atomic<usize> counter{ 0 };
		// Joins happen below the FilePath level: FilePath::join classifies bare
		// relative segments by CWD, which misfires when the tests run under /tmp.
		const std::filesystem::path dir = fs::FilePath::getDefaultVirtualDirectoryPath().getPath()
		                                / ("psack_multifile_" + std::to_string(counter++));
		fs::FileManager::createVirtualFolder(fs::FilePath{ dir });

		base::Optional<fs::File> root;
		for (const ProgramFile& program_file: files) {
			const std::string_view relative = program_file.relative_path;
			// Create intermediate folders for nested module paths.
			std::string           prefix;
			std::filesystem::path parent = dir;
			for (char c: relative) {
				if (c != '/') {
					prefix += c;
					continue;
				}
				parent /= prefix;
				const fs::FilePath parent_path{ parent };
				if (!parent_path.exists()) fs::FileManager::createVirtualFolder(parent_path);
				prefix.clear();
			}
			fs::File file = fs::FileManager::createVirtualFile(
				fs::FilePath{ dir / std::filesystem::path(relative) }, program_file.code
			);
			if (!root.has_value()) root = file;
		}
		return root.value();
	}

	inline const std::vector<SingleFileCase>& singleFileCases() {
		static const std::vector<SingleFileCase> cases = {
			{ "Return42", "fun main() -> i64 = { return 42; }", 42 },
			{ "BasicArithmetic",
			  R"(
				fun main() -> i64 = { return 2 + 5 * 3 - 4 / 2; }
			)",
			  15 },
			{ "IfThenReturn",
			  R"(
				fun main() -> i64 = {
					if (true) {
						return 42;
					}
					return 0;
				}
			)",
			  42 },
			{ "IfFallthrough",
			  R"(
				fun main() -> i64 = {
					if (false) {
						return 99;
					}
					return 7;
				}
			)",
			  7 },
			{ "IfElseTakesElse",
			  R"(
				fun main() -> i64 = {
					if (false) {
						return 1;
					} else {
						return 2;
					}
				}
			)",
			  2 },
			{ "NestedIf",
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
			  11 },
			{ "WhileNeverEnters",
			  R"(
				fun main() -> i64 = {
					while (false) {
						return 99;
					}
					return 7;
				}
			)",
			  7 },
			{ "WhileBodyExecutes",
			  R"(
				fun main() -> i64 = {
					while (true) {
						return 13;
					}
					return 0;
				}
			)",
			  13 },
			{ "ClassDeclares",
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
			  0 },
			{ "FieldAccess",
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
			  0 },
			{ "AssignToLocal",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 5;
					x = 10;
					return x;
				}
			)",
			  10 },
			{ "AssignWithoutInit",
			  R"(
				fun main() -> i64 = {
					var x: i64;
					x = 42;
					return x;
				}
			)",
			  42 },
			{ "AssignArithmetic",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					x = 2 * 3 + 4;
					return x;
				}
			)",
			  10 },
			{ "AssignSelfReference",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 5;
					x = x + 1;
					return x;
				}
			)",
			  6 },
			{ "AssignSequence",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					x = 1;
					x = 2;
					x = 3;
					return x;
				}
			)",
			  3 },
			{ "AssignInsideIf",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					if (1 < 2) {
						x = 100;
					}
					return x;
				}
			)",
			  100 },
			{ "AssignInIfElse",
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
			  2 },
			{ "PlusEquals",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 5;
					x += 3;
					return x;
				}
			)",
			  8 },
			{ "MinusEquals",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 10;
					x -= 4;
					return x;
				}
			)",
			  6 },
			{ "PlusEqualsInLoop",
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
			  55 },
			{ "WhileCounter",
			  R"(
				fun main() -> i64 = {
					var i: i64 = 0;
					while (i < 10) {
						i = i + 1;
					}
					return i;
				}
			)",
			  10 },
			{ "WhileSum",
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
			  55 },
			{ "WhileCountdown",
			  R"(
				fun main() -> i64 = {
					var i: i64 = 10;
					while (0 < i) {
						i = i - 1;
					}
					return i;
				}
			)",
			  0 },
			{ "FieldWriteRead",
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
			  42 },
			{ "FieldMultiple",
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
			  7 },
			{ "FieldOrderingMatters",
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
			  123 },
			{ "FieldCopyToField",
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
			  5 },
			{ "FieldPlusEquals",
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
			  15 },
			{ "FieldInWhileLoop",
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
			  21 },
			{ "TimesEquals",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 5;
					x *= 3;
					return x;
				}
			)",
			  15 },
			{ "DivideEquals",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 20;
					x /= 4;
					return x;
				}
			)",
			  5 },
			{ "ModuloEquals",
			  R"(
				fun main() -> i64 = {
					var x: i64 = 17;
					x %= 5;
					return x;
				}
			)",
			  2 },
			{ "ModuloBinary",
			  R"(
				fun main() -> i64 = {
					return 17 % 5;
				}
			)",
			  2 },
			{ "CompoundOpsCombined",
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
			  1 },
			{ "FieldCompoundOps",
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
			  1 },
			{ "CallingFunction",
			  R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = { return add(2, 5); }
			)",
			  7 },
			{ "CallNoArgs",
			  R"(
				fun five() -> i64 = { return 5; }
				fun main() -> i64 = { return five(); }
			)",
			  5 },
			{ "NestedCalls",
			  R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = { return add(add(1, 2), add(3, 4)); }
			)",
			  10 },
			{ "CallInArithmetic",
			  R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = { return add(2, 3) * 2; }
			)",
			  10 },
			{ "CallWithVariableArgs",
			  R"(
				fun add(a: i64, b: i64) -> i64 = { return a + b; }
				fun main() -> i64 = {
					var x: i64 = 3;
					return add(x, 4);
				}
			)",
			  7 },
			{ "ForwardCall",
			  R"(
				fun main() -> i64 = { return helper(); }
				fun helper() -> i64 = { return 9; }
			)",
			  9 },
			{ "Recursion",
			  R"(
				fun factorial(n: i64) -> i64 = {
					if (n < 2) {
						return 1;
					}
					return n * factorial(n - 1);
				}
				fun main() -> i64 = { return factorial(5); }
			)",
			  120 },
			{ "FibRecursion",
			  R"(
				fun fib(n: i64) -> i64 = {
					if (n < 2) {
						return n;
					}
					return fib(n - 1) + fib(n - 2);
				}
				fun main() -> i64 = { return fib(10); }
			)",
			  55 },
			{ "VoidFunctionCall",
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
			  7 },
			{ "VoidExplicitReturn",
			  R"(
				fun nothing() = { return; }
				fun main() -> i64 = {
					nothing();
					return 3;
				}
			)",
			  3 },
			{ "BoolParamAndReturn",
			  R"(
				fun isPositive(x: i64) -> bool = { return 0 < x; }
				fun main() -> i64 = {
					if (isPositive(5)) {
						return 1;
					}
					return 0;
				}
			)",
			  1 },
			{ "GlobalConst",
			  R"(
				const X: i64 = 42;
				fun main() -> i64 = { return X; }
			)",
			  42 },
			{ "GlobalConstExprInit",
			  R"(
				const X: i64 = 2 * 3 + 4;
				fun main() -> i64 = { return X; }
			)",
			  10 },
			{ "ConstFromConst",
			  R"(
				const A: i64 = 5;
				const B: i64 = A + 2;
				fun main() -> i64 = { return B; }
			)",
			  7 },
			{ "LocalConst",
			  R"(
				fun main() -> i64 = {
					const Y: i64 = 7;
					return Y;
				}
			)",
			  7 },
			{ "ConstInArithmetic",
			  R"(
				const X: i64 = 10;
				fun main() -> i64 = { return X * 2 + 1; }
			)",
			  21 },
			{ "BoolConst",
			  R"(
				const T: bool = true;
				fun main() -> i64 = {
					if (T) {
						return 3;
					}
					return 0;
				}
			)",
			  3 },
			{ "BoolVar",
			  R"(
				fun main() -> i64 = {
					var b: bool = true;
					if (b) {
						return 1;
					}
					return 0;
				}
			)",
			  1 },
			{ "BoolVarFromComparison",
			  R"(
				fun main() -> i64 = {
					var b: bool = 1 < 2;
					if (b) {
						return 5;
					}
					return 0;
				}
			)",
			  5 },
			{ "BoolAssignment",
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
			  4 },
			{ "BoolAnd",
			  R"(
				fun main() -> i64 = {
					if (true and false) {
						return 1;
					}
					return 2;
				}
			)",
			  2 },
			{ "BoolOr",
			  R"(
				fun main() -> i64 = {
					if (false or true) {
						return 3;
					}
					return 0;
				}
			)",
			  3 },
			{ "BoolAndVars",
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
			  2 },
			{ "NamespaceFunction",
			  R"(
				namespace math {
					fun add(a: i64, b: i64) -> i64 = { return a + b; }
				}
				fun main() -> i64 = { return math.add(2, 3); }
			)",
			  5 },
			{ "NestedNamespaces",
			  R"(
				namespace a {
					namespace b {
						fun f() -> i64 = { return 6; }
					}
				}
				fun main() -> i64 = { return a.b.f(); }
			)",
			  6 },
			{ "NamespaceConst",
			  R"(
				namespace m {
					const PI: i64 = 3;
				}
				fun main() -> i64 = { return m.PI; }
			)",
			  3 },
			{ "NamespaceShadowsGlobalName",
			  R"(
				fun f() -> i64 = { return 1; }
				namespace n {
					fun f() -> i64 = { return 2; }
				}
				fun main() -> i64 = { return f() + n.f(); }
			)",
			  3 },
			{ "SiblingCallInNamespace",
			  R"(
				namespace n {
					fun a() -> i64 = { return 1; }
					fun b() -> i64 = { return a() + 1; }
				}
				fun main() -> i64 = { return n.b(); }
			)",
			  2 },
			{ "ManyFunctionsParallel",
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
			  20 },
			{ "NamespaceConstUsedInFunction",
			  R"(
				namespace m {
					const K: i64 = 5;
					fun get() -> i64 = { return K; }
				}
				fun main() -> i64 = { return m.get(); }
			)",
			  5 },
			{ "ConstructorCall",
			  R"(
				class Point {
					x: i64 = 0;
					y: i64 = 0;
				}
				fun main() -> i64 = {
					var p: Point = Point();
					p.x = 3;
					return p.x + p.y;
				}
			)",
			  3 },
			{ "BuiltinOutputI64",
			  R"(
				fun main() -> i64 = {
					builtin_output_i64(42i64);
					return 7;
				}
			)",
			  7 },
			{ "GlobalVarCallInitializer",
			  R"(
				fun five() -> i64 = { return 5; }
				var g: i64 = five() * 2i64;
				fun main() -> i64 = { return g; }
			)",
			  10 },
			{ "GlobalVarChainedInitializers",
			  R"(
				var a: i64 = 3;
				var b: i64 = a * a;
				fun main() -> i64 = { return b + a; }
			)",
			  12 },
			{ "GlobalVarRead",
			  R"(
				var g: i64 = 7;
				fun main() -> i64 = { return g; }
			)",
			  7 },
			{ "GlobalVarZeroInit",
			  R"(
				var g: i64;
				fun main() -> i64 = { return g; }
			)",
			  0 },
			{ "GlobalVarWrite",
			  R"(
				var g: i64 = 1;
				fun main() -> i64 = {
					g = 9;
					return g;
				}
			)",
			  9 },
			{ "GlobalVarConstExprInit",
			  R"(
				var g: i64 = 2 * 3 + 4;
				fun main() -> i64 = { return g; }
			)",
			  10 },
			{ "GlobalVarFromConst",
			  R"(
				const A: i64 = 6;
				var g: i64 = A;
				fun main() -> i64 = { return g; }
			)",
			  6 },
			{ "GlobalVarSharedAcrossFunctions",
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
			  3 },
			{ "GlobalVarCompoundOps",
			  R"(
				var g: i64 = 5;
				fun main() -> i64 = {
					g *= 4;
					g -= 3;
					g %= 7;
					return g;
				}
			)",
			  3 },
			{ "GlobalBoolVar",
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
			  2 },
			{ "NamespaceGlobalVar",
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
			  6 },
			{ "GlobalVarInLoop",
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
			  15 },
			{ "CharVarCompare",
			  R"(
				fun main() -> i64 = {
					var c: char = 'a';
					if (c == 'a') {
						return 1;
					}
					return 0;
				}
			)",
			  1 },
			{ "CharOrdering",
			  R"(
				fun main() -> i64 = {
					if ('a' < 'b') {
						if ('z' > 'y') {
							return 2;
						}
					}
					return 0;
				}
			)",
			  2 },
			{ "CharComparisonChain",
			  R"(
				fun main() -> i64 = {
					if ('a' < 'b' < 'c') {
						return 3;
					}
					return 0;
				}
			)",
			  3 },
			{ "CharEscapes",
			  R"(
				fun main() -> i64 = {
					if ('\n' == '\n') {
						if ('\n' != 'n') {
							if ('\0' < '\t') {
								return 4;
							}
						}
					}
					return 0;
				}
			)",
			  4 },
			{ "CharParamReturn",
			  R"(
				fun pick(flag: bool, a: char, b: char) -> char = {
					if (flag) {
						return a;
					}
					return b;
				}
				fun main() -> i64 = {
					if (pick(true, 'x', 'y') == 'x') {
						if (pick(false, 'x', 'y') == 'y') {
							return 5;
						}
					}
					return 0;
				}
			)",
			  5 },
			{ "CharConst",
			  R"(
				const NL: char = '\n';
				fun main() -> i64 = {
					if (NL == '\n') {
						return 6;
					}
					return 0;
				}
			)",
			  6 },
			{ "CharAssign",
			  R"(
				fun main() -> i64 = {
					var c: char = 'a';
					c = 'b';
					if (c == 'b') {
						return 7;
					}
					return 0;
				}
			)",
			  7 },
			{ "GlobalCharVar",
			  R"(
				var g: char = 'q';
				fun main() -> i64 = {
					if (g == 'q') {
						g = 'r';
						if (g == 'r') {
							return 8;
						}
					}
					return 0;
				}
			)",
			  8 },
			{ "GlobalCharZeroInit",
			  R"(
				var g: char;
				fun main() -> i64 = {
					if (g == '\0') {
						return 9;
					}
					return 0;
				}
			)",
			  9 },
			{ "CharField",
			  R"(
				class Cell {
					c: char;
				}
				fun main() -> i64 = {
					var cell: Cell;
					if (cell.c == '\0') {
						cell.c = 'k';
						if (cell.c == 'k') {
							return 10;
						}
					}
					return 0;
				}
			)",
			  10 },
			{ "BuiltinOutputChar",
			  R"(
				fun main() -> i64 = {
					builtin_output_char('H');
					builtin_output_char('i');
					return builtin_output_char('\n');
				}
			)",
			  1 },
			// C-style arrays: fixed-size `T[N]`, indexing, VLAs, arrays in classes
			// and globals.
			{ "ArrayIndexAssign",
			  R"(
				fun main() -> i64 = {
					var a: i64[10];
					a[2] = 5;
					return a[2];
				}
			)",
			  5 },
			{ "ArraySumLoop",
			  R"(
				fun main() -> i64 = {
					var a: i64[10];
					var i: i64 = 0;
					while (i < 10) {
						a[i] = i;
						i += 1;
					}
					var sum: i64 = 0;
					i = 0;
					while (i < 10) {
						sum += a[i];
						i += 1;
					}
					return sum;
				}
			)",
			  45 },
			{ "Array2D",
			  R"(
				fun main() -> i64 = {
					var m: i64[3][4];
					var i: i64 = 0;
					var j: i64 = 0;
					while (i < 3) {
						j = 0;
						while (j < 4) {
							m[i][j] = i * 4 + j;
							j += 1;
						}
						i += 1;
					}
					var sum: i64 = 0;
					i = 0;
					while (i < 3) {
						j = 0;
						while (j < 4) {
							sum += m[i][j];
							j += 1;
						}
						i += 1;
					}
					return sum;
				}
			)",
			  66 },
			{ "ArrayConstSize",
			  R"(
				const N: i64 = 6;
				fun main() -> i64 = {
					var a: i64[N];
					var i: i64 = 0;
					while (i < N) {
						a[i] = i + 1;
						i += 1;
					}
					return a[0] + a[5];
				}
			)",
			  7 },
			{ "ArrayVla",
			  R"(
				fun sumUpTo(n: i64) -> i64 = {
					var a: i64[n];
					var i: i64 = 0;
					while (i < n) {
						a[i] = i;
						i += 1;
					}
					var sum: i64 = 0;
					i = 0;
					while (i < n) {
						sum += a[i];
						i += 1;
					}
					return sum;
				}
				fun main() -> i64 = {
					return sumUpTo(5);
				}
			)",
			  10 },
			{ "ArrayVlaMultiDim",
			  R"(
				fun fill(n: i64) -> i64 = {
					var a: i64[n][3];
					var i: i64 = 0;
					var j: i64 = 0;
					while (i < n) {
						j = 0;
						while (j < 3) {
							a[i][j] = i * 3 + j;
							j += 1;
						}
						i += 1;
					}
					var sum: i64 = 0;
					i = 0;
					while (i < n) {
						j = 0;
						while (j < 3) {
							sum += a[i][j];
							j += 1;
						}
						i += 1;
					}
					return sum;
				}
				fun main() -> i64 = {
					return fill(2);
				}
			)",
			  15 },
			{ "GlobalArray",
			  R"(
				var g: i64[4];
				fun bump(idx: i64, v: i64) = {
					g[idx] = v;
				}
				fun main() -> i64 = {
					bump(0, 30);
					bump(3, 12);
					return g[0] + g[3];
				}
			)",
			  42 },
			{ "ArrayOfClass",
			  R"(
				class Point {
					x: i64;
					y: i64;
				}
				fun main() -> i64 = {
					var pts: Point[3];
					pts[1].x = 7;
					pts[2].y = 4;
					return pts[1].x + pts[2].y;
				}
			)",
			  11 },
			{ "ClassArrayField",
			  R"(
				class Buf {
					data: i64[4];
					len: i64;
				}
				fun main() -> i64 = {
					var b: Buf;
					b.data[2] = 9;
					b.len = 3;
					return b.data[2] + b.len;
				}
			)",
			  12 },
			{ "CharArray",
			  R"(
				fun main() -> i64 = {
					var s: char[3];
					s[0] = 'H';
					s[1] = 'i';
					if (s[0] == 'H') {
						if (s[1] == 'i') {
							return 3;
						}
					}
					return 0;
				}
			)",
			  3 },
		};
		return cases;
	}

	inline const std::vector<MultiFileCase>& multiFileCases() {
		static const std::vector<MultiFileCase> cases = {
			{ "ImportAliasCall",
			  { { "root.dmf",
			      "import math as M;\n"
			      "fun main() -> i64 = { return M.add(2, 3); }\n" },
			    { "math.dmf", "fun add(a: i64, b: i64) -> i64 = { return a + b; }\n" } },
			  5,
			  nullptr },
			{ "ImportDefaultAlias",
			  { { "root.dmf",
			      "import math;\n"
			      "fun main() -> i64 = { return math.add(4, 3); }\n" },
			    { "math.dmf", "fun add(a: i64, b: i64) -> i64 = { return a + b; }\n" } },
			  7,
			  nullptr },
			{ "ImportStar",
			  { { "root.dmf",
			      "import math.*;\n"
			      "fun main() -> i64 = { return add(K, 3); }\n" },
			    { "math.dmf",
			      "const K: i64 = 10;\n"
			      "fun add(a: i64, b: i64) -> i64 = { return a + b; }\n" } },
			  13,
			  nullptr },
			{ "ImportNestedPath",
			  { { "root.dmf",
			      "import a.b as AB;\n"
			      "fun main() -> i64 = { return AB.f(); }\n" },
			    { "a/b.dmf", "fun f() -> i64 = { return 6; }\n" } },
			  6,
			  nullptr },
			{ "ImportTransitive",
			  { { "root.dmf",
			      "import util as U;\n"
			      "fun main() -> i64 = { return U.twicePlusOne(5); }\n" },
			    { "util.dmf",
			      "import math as M;\n"
			      "fun twicePlusOne(x: i64) -> i64 = { return M.twice(x) + 1; }\n" },
			    { "math.dmf", "fun twice(x: i64) -> i64 = { return x * 2; }\n" } },
			  11,
			  nullptr },
			// Two importers of the same module share one load and one set of symbols.
			{ "ImportDiamond",
			  { { "root.dmf",
			      "import a as A;\n"
			      "import b as B;\n"
			      "fun main() -> i64 = { return A.f() + B.g(); }\n" },
			    { "a.dmf",
			      "import shared as S;\n"
			      "fun f() -> i64 = { return S.base() + 1; }\n" },
			    { "b.dmf",
			      "import shared as S;\n"
			      "fun g() -> i64 = { return S.base() + 2; }\n" },
			    { "shared.dmf", "fun base() -> i64 = { return 10; }\n" } },
			  23,
			  nullptr },
			{ "ImportedGlobals",
			  { { "root.dmf",
			      "import counter as C;\n"
			      "fun main() -> i64 = {\n"
			      "    C.bump();\n"
			      "    C.bump();\n"
			      "    return C.value;\n"
			      "}\n" },
			    { "counter.dmf",
			      "var value: i64 = 5;\n"
			      "fun bump() = { value += 1; }\n" } },
			  7,
			  nullptr },
			{ "ImportCycleFails",
			  { { "root.dmf",
			      "import a as A;\n"
			      "fun main() -> i64 = { return 0; }\n" },
			    { "a.dmf", "import b as B;\nfun f() -> i64 = { return 1; }\n" },
			    { "b.dmf", "import a as A;\nfun g() -> i64 = { return 2; }\n" } },
			  0,
			  "cycle" },
			{ "ImportStarCollisionFails",
			  { { "root.dmf",
			      "import a.*;\n"
			      "import b.*;\n"
			      "fun main() -> i64 = { return f(); }\n" },
			    { "a.dmf", "fun f() -> i64 = { return 1; }\n" },
			    { "b.dmf", "fun f() -> i64 = { return 2; }\n" } },
			  0,
			  "already declared" },
			// Chars compare but don't participate in arithmetic, and never mix with i64.
			{ "CharArithmeticFails",
			  { { "root.dmf",
			      "fun main() -> i64 = {\n"
			      "    var c: char = 'a' + 'b';\n"
			      "    return 0;\n"
			      "}\n" } },
			  0,
			  "i64 operands" },
			{ "CharMixedCompareFails",
			  { { "root.dmf",
			      "fun main() -> i64 = {\n"
			      "    if ('a' == 1) {\n"
			      "        return 1;\n"
			      "    }\n"
			      "    return 0;\n"
			      "}\n" } },
			  0,
			  "operand pairs" },
			// Arrays follow C rules: no pass/return by value, no whole-array
			// assignment, sizes positive, non-local sizes compile-time constant.
			{ "ArrayParamFails",
			  { { "root.dmf",
			      "fun f(a: i64[3]) -> i64 = { return a[0]; }\n"
			      "fun main() -> i64 = { return 0; }\n" } },
			  0,
			  "not supported as parameter or return types" },
			{ "ArrayReturnFails",
			  { { "root.dmf",
			      "fun f() -> i64[3] = { return 0; }\n"
			      "fun main() -> i64 = { return 0; }\n" } },
			  0,
			  "not supported as parameter or return types" },
			{ "ArrayZeroSizeFails",
			  { { "root.dmf",
			      "fun main() -> i64 = {\n"
			      "    var a: i64[0];\n"
			      "    return 0;\n"
			      "}\n" } },
			  0,
			  "must be positive" },
			{ "ArrayGlobalRuntimeSizeFails",
			  { { "root.dmf",
			      "var size: i64 = 4;\n"
			      "var g: i64[size];\n"
			      "fun main() -> i64 = { return 0; }\n" } },
			  0,
			  "compile-time constant" },
			{ "ArrayVlaInnerDimFails",
			  { { "root.dmf",
			      "fun f(n: i64) -> i64 = {\n"
			      "    var a: i64[3][n];\n"
			      "    return 0;\n"
			      "}\n"
			      "fun main() -> i64 = { return f(2); }\n" } },
			  0,
			  "compile-time constant" },
			{ "ArrayWholeAssignFails",
			  { { "root.dmf",
			      "fun main() -> i64 = {\n"
			      "    var a: i64[3];\n"
			      "    var b: i64[3];\n"
			      "    a = b;\n"
			      "    return 0;\n"
			      "}\n" } },
			  0,
			  "cannot be assigned as a whole" },
			{ "SubscriptNonArrayFails",
			  { { "root.dmf",
			      "fun main() -> i64 = {\n"
			      "    var x: i64 = 1;\n"
			      "    return x[0];\n"
			      "}\n" } },
			  0,
			  "Subscript on non-array type" },
		};
		return cases;
	}
}
