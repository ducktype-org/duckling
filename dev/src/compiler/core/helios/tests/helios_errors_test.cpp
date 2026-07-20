
#include <diagnostic_interactive/stable_position.hpp>
#include <driver/test_utils.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries/queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/hout_creation/expressions/errors.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/internal/query_errors.hpp>
#include <query_framework/query_result.hpp>
#include <tester/tester.hpp>

using namespace compiler;

class HeliosErrorsTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosErrorsTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testErrorLogging);
		TESTER_ADD_TEST(testCopyabilityErrors);

		// This test has some strange side effects. Putting it before `testErrorLogging` causes
		// the tests to fail.
		TESTER_ADD_TEST(testDuplicatedDefinitions);

		TESTER_ADD_TEST(testErrorLoggingExpandStatements);
		TESTER_ADD_TEST(testErrorLoggingCyclicErrors);
		TESTER_ADD_TEST(testPointerCastErrors);
		TESTER_ADD_TEST(testBackendDependentAttributeErrors);


		TESTER_ADD_TEST(testErrorBadExpr);
		TESTER_ADD_TEST(testDiagnosticErrorsCorrectness);
	}

protected:
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		auto         init_result
			= compiler::driver::test_utils::initializeCompilerForTests({}, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	/**
	 * @brief Helper function that check for HELIOS compilation
	 * errors in a module with given content, when compiling it to HOUT module.
	 *
	 * It creates a virtual file from the `module_content` argument
	 * and creates a module tree from it every function call.
	 * The `present_phrases` are checked to be present in the logged
	 * error messages in the given order.
	 *
	 * @TODO: #2213 Add PST errors handling here.
	 *
	 * @param module_content The content of the module main source file.
	 * @param logged_msg_count Expected number of logged error messages.
	 * @param present_phrases List of phrases that should be present in the logged errors in order.
	 */
	void checkForErrorOnCompileModule(
		std::string_view                     module_content,
		const std::vector<std::string_view>& present_phrases,
		u64                                  logged_msg_count
	) {
		frontend::ModuleID module_id = frontend::createModuleTreeFromContents(module_content);

		auto result = query::entryPoint<helios::QueryModuleHOUT>(module_id);

		assertTrue(result->hasFailed(), "Expected HOUT query to fail for module content.");
		auto logger = query::Context::dumpToOneLoggerAndClear();

		// @TODO: #2213 we should do something smarted here, and see if the sum of pst and
		// query errors is ok:
		assertTrue(logger->hasErrors() or logged_msg_count == 0, "Expected errors to be logged.");

		std::stringstream logged_messages;
		logger->terminalPrint(logged_messages);
		// std::cerr << "Logged messages:\n" << logged_messages.str() << "\n";
		auto msg_count = logger->messageCount();
		assertEqual(
			msg_count,
			logged_msg_count,
			"Expected logged message count to be " + std::to_string(logged_msg_count) + ", but got "
				+ std::to_string(msg_count)
		);
		size_t      current_pos = 0;
		std::string logged_str  = logged_messages.str();
		for (const auto& phrase: present_phrases) {
			size_t found_pos = logged_str.find(phrase, current_pos);
			assertTrue(
				found_pos != std::string::npos,
				"Expected logged messages to contain phrase in order: " + std::string(phrase)
			);
			if (found_pos != std::string::npos) current_pos = found_pos + phrase.length();
		}
	}

	/**
	 * Generic error logging tests.
	 * Add additional test cases for more specific categories.
	 */
	void testErrorLogging() {
		// ============================ No operator found ============================
		{
			checkForErrorOnCompileModule(
				R"(fun a() = true + false;)",
				{ "Call failed due to ambiguous overload resolution" },
				1
			);
			checkForErrorOnCompileModule(
				R"(fun a() = 'c' + 1i64;)",
				{
					"Call failed due to ambiguous overload resolution",
					"type const Function (const u8, const char) -> (const char).",
					"given argument type `char` cannot be converted to the expected type `const "
					"u8`.",
					"type const Function (const char, const u8) -> (const char).",
					"given argument type `i64` cannot be converted to the expected type `const "
					"u8`.",
				},
				1
			);
			checkForErrorOnCompileModule(
				R"(fun a() = -true;)", { "Call failed because no matching functions were found." }, 1
			);
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					x++;
					return 0;
				}
			)",
				{ "Call failed because no matching functions were found." },
				1
			);
		}

		// ============================ Function calls ============================
		{
			checkForErrorOnCompileModule(
				R"(
				fun a(x: i32) -> i32 = 0;
				fun b() = a(1.0);
			)",
				{ "given argument type `f32` cannot be converted", "Function declared here." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fundecl a(x: i32) -> i32;
				fun b() = a(1.0);
			)",
				{ "given argument type `f32` cannot be converted", "Function declared here." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class Point{x: i64;}
				fun a() = {
					var p = Point(x=1.0);
				}
			)",
				{ "given argument type `f32` cannot be converted" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun a() = {
					b(1,2,3);
				}
			)",
				{ "Call failed because no matching functions were found." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class MyClass {}

				fun MyClass() -> i32 = {
					return 1;
				}

				# Error, we have multiple callable candidates for MyClass,
				# and not all of them are functions (one is a class, which
				# requires further lookup to call its constructor).
				let a: i32 = MyClass();
			)",
				{ "Found a combination of function and non-function callables" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				# Test case: Ambiguous call due to multiple coercion matches
				fun foo(x: i64) -> i32 = {
					return 1;
				}

				fun foo(x: f64) -> i32 = {
					return 2;
				}

				fun foo(x: i32) -> i32 = {
					return 3;
				}

				fun get_i16() -> i16 = {
					return 42i16;
				}

				var AMBIGUOUS_COERCION_CALL: i32 = foo(get_i16());
			)",
				{ "Call failed due to ambiguous overload resolution",
			      "Found coercible candidate",
			      "Candidate failed to match" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun ambiguous() -> i32 = {
					return 1;
				}

				fun ambiguous(x: i64 = 42) -> i32 = {
					return 2;
				}

				# This call should fail: both functions match exactly
				var AMBIGUOUS_CALL: i32 = ambiguous();
			)",
				{ "ambiguous overload resolution", "Found exact candidate." },
				1
			);
		}

		// =========================== Method call errors ===========================
		{
			checkForErrorOnCompileModule(
				R"(
				class MyClass {
					var dummy: i64 = 0; # to avoid ZST

					fun method(x: i64) = {
						return x + 1;
					}
				}
				fun main() = {
					var obj = MyClass();
					return obj.method(1.0);
				}
			)",
				{ "The given argument type `f32` cannot be converted to the expected type `i64`." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class MyClass {
					var dummy: i64 = 0; # to avoid ZST

					fun method(x: i64) = {
						return x + 1;
					}
				}
				fun main() = {
					var obj = MyClass();
					return obj.method();
				}
			)",
				{ "The call is missing a required argument with no default value." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class MyClass {
					var dummy: i64 = 0; # to avoid ZST

					fun method(x: i64) = {
						return x + 1;
					}

					fun method(x: f64) = {
						return x + 1.0;
					}
				}
				fun main() = {
					var obj = MyClass();
					return obj.method("abc");
				}
			)",
				{ "Call failed due to ambiguous overload resolution." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun foo(x: i64) = {
					return x + 1;
				}

				class MyClass {
					a: i64 = 0;

					fun foo(x: i64) = {
						return x + 1;
					}

					fun goo(x: i64) = {
						return foo(x);
					}
				}
				fun main() = {
					var obj = MyClass();
					return obj.goo(1);
				}
			)",
				{ "Found a combination of function and non-function callables in a call "
			      "expression." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class Point {
					x:i64;y:i64;

					fun Point() = {}

					fun foo() = {
						let a = Point(1, 2);
					}
				}
			)",
				{ "Found a combination of function and non-function callables in a call "
			      "expression." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class MyClass {
					x:i64 = 0;

					fun foo() = {
						return self.goo(5);
					}
				}
			)",
				{ "Call failed because no matching functions were found." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class MyClass {
					x:i64 = 0;

					fun foo(a:i64 = 0.5) = {
						return a;
					}
				}
			)",
				{ "Type `f32` cannot be converted to type `i64`." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class MyClass {
					x:i64 = 0;

					MyClass.copy(other: const MyClass) = {
						return MyClass(1);
					}
				}
			)",
				{ "A copy constructor's parameter must be a reference to its own class "
			      "`MyClass`." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class MyClass {
					x:i64 = 0;

					MyClass.copy(other: const ref MyClass, a: i64) = {
						return MyClass(1);
					}
				}
			)",
				{ "A copy constructor must declare exactly one parameter: a reference to the "
			      "object being copied." },
				1
			);
		}

		// ============================ Typecheck errors ============================
		{
			checkForErrorOnCompileModule(
				R"(fun a() -> i64 = 1.0;)", { "Type `f32` cannot be converted to type `i64`." }, 1
			);

			checkForErrorOnCompileModule(
				R"(
				fun example(x: i64) = {
					if (x > 0) {
						return x; # this is i64
					}
					else {
						return 0; # this is i32
					}
				}
			)",
				{ "no explicit return type and inconsistent return statements" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					let x: i64 = 0;
					x = 1;
				}
			)",
				{ "Left side of assignment can't be immutable." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() = {
					var arr: i32[5];
					arr["index"] = 1;
				}
			)",
				{ "Type `const slice char` cannot be converted to type `const i64`." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() = {
					var static_arr: i64[2];
					for (x: f32 in static_arr) {}
				}
			)",
				{ "Cannot coerce collection element type 'i64' to iterator type 'f32'." },
				1
			);

			// @TODO: #1488 Enable this test when fixed.
			// checkForErrorOnCompileModule(
			// 	R"(
			// 	fun main() = {
			// 		var const_dyn_arr: List[const i32];
			// 		# Mutable iterator, but array stores immutable
			// 		for (var x in const_dyn_arr) {}
			// 	}
			// )",
			// 	{ "Cannot coerce collection element type 'const i32' to iterator type 'i32'." },
			// 	1
			// );

			checkForErrorOnCompileModule(
				R"(
				const unitType: type = ();

				fun foo(u: ()) -> () = {
				    return u;
				}

				fun main() -> i64 = {
					foo(unitType);
					return 0;
				}
			)",
				{ "The given argument type `const type` cannot be converted to the expected type "
			      "`()`" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun bad() = {
				    return;
				    return 0;
				}

				fun main() -> i64 = {
					return 0;
				}
			)",
				{ "inconsistent return statements" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
					fun main() = {
						var i: i64 = 0;
						var u: u64 = i;
					}
				)",
				{ "Type `i64` cannot be converted to type `u64`." },
				1
			);
		}

		// ========================== Lexer errors ==========================

		// We don't see errors here, because they are produced by the lexer, not query:
		checkForErrorOnCompileModule(
			R"(
				fun main() -> i64 = {
					var a = 1kg;
					return 0;
				}
			)",
			{},
			0
		);

		// ========================== Parsing errors ==========================

		// We don't see errors here, because they are produced by the parser, not query:
		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					if Loop <= 1 { # no parenthesis around condition
					
					}
				}
			)",
			{},
			0
		);

		// ========================== Comp time errors ==========================
		{
			checkForErrorOnCompileModule(
				R"(
				const a: f64 = 1.0 / 0.0;
				fun main() -> i64 = 0;
			)",
				{ "Division", "zero" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					var x = 1;
					const y = x;
   					return 0;
				}
			)",
				{ "cannot be evaluated at compile-time", "const y = x" },
				1
			);
		}

		// ========================== Default initialization errors ==========================
		{
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					var x: ref i64;
					return 0;
				}
			)",
				{ "Type `ref i64` cannot be default initialized" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					var x: box i64;
					return 0;
				}
			)",
				{ "Type `box i64` cannot be default initialized" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class Inner { non_defaultable: ref i64; }
				class Outer { inner: Inner; }

				fun main() -> i64 = {
					var o: Outer;
					return 0;
				}
			)",
				{ "Type `Class Outer` cannot be default initialized" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class Inner { non_defaultable: ref i64; }

				fun main() -> i64 = {
				var arr: Inner[2];
					return 0;
				}
			)",
				{ "Type `Class Inner[2]` cannot be default initialized" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() = {
					var l: List;
					l[0] = 123;
				}
			)",
				{ "Type `List` cannot be default initialized" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class Inner { non_defaultable: ref i64; }

				fun main() -> i64 = {
					var tup: (Inner, i64);
					return 0;
				}
			)",
				{ "Type `Tuple(Class Inner, i64)` cannot be default initialized" },
				1
			);
		}


		// ============================ Mutability errors ============================
		{
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					let x: i64;
				}
			)",
				{ "Immutable variables must have an initial value." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					let x: i64 = 1;
					x = 123;
				}
			)",
				{ "Left side of assignment can't be immutable." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
    				var dyn_const_arr: List[const i32];
    				for (x in dyn_const_arr) { x = 2; }
				}
			)",
				{ "Left side of assignment can't be immutable." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
    				var dyn_arr: List[i32];
    				for (x: const i32 in dyn_arr) { x = 2; }
				}
			)",
				{ "Left side of assignment can't be immutable." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
    				var dyn_arr: List[i32];
					for (let x in dyn_arr) { x = 123; }
				}
			)",
				{ "Left side of assignment can't be immutable." },
				1
			);
		}

		// ============================ Static Arrays ============================
		{
			checkForErrorOnCompileModule(
				R"(
				fun main(n: i64) = {
					var arr: i32[n];
				}
			)",
				{ "Expression cannot be evaluated at compile-time." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				const ARR_TYPE = i32[10.5];
			)",
				{ "Type `f32` cannot be converted to type `const i64`." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				const ARR_TYPE = i32[-2];
			)",
				{ "Static array size must be a non-negative integral value." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() = {
					var arr: i32[5];
					var x = arr[1, 2];
				}
			)",
				{ "Array index/size must be exactly one expression" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() = {
					var arr: i32[5];
					arr["index"] = 1;
				}
			)",
				{ "Type `const slice char` cannot be converted to type `const i64`." },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				const NOT_A_TYPE = 10;
				const ARR = NOT_A_TYPE[5];
			)",
				{ "Index operator base must be indexable." },
				1
			);
		}

		// ============================ Dynamic Arrays ============================
		{
			checkForErrorOnCompileModule(
				R"(
				fun main() = {
					var l1: List[i64];
					var l2: List[f64] = l1;
				}
			)",
				{ "Type `List[i64]` cannot be converted to type `List[f64]`" },
				1
			);
		}

		// ========================= Not-yet-implemented errors =========================
		{
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					while (true) {
						break;
					}
				}
			)",
				{ "Feature not implemented", "break" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					while (true) {
						continue;
					}
				}
			)",
				{ "Feature not implemented", "continue" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					defer 1;
				}
			)",
				{ "Feature not implemented", "defer" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					while (true) {
						redo;
					}
				}
			)",
				{ "Feature not implemented", "redo" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					for (i in 0) { }
				}
			)",
				{ "Feature not implemented", "for", "for non-array type: i32" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					fun foo() = 0;
				}
			)",
				{ "Feature not implemented", "Nested", "function" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					var a: i64 = 0;
					a += 1;
					return a;
				}
			)",
				{ "Feature not implemented" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class A { x: i64 = 0; }
				const a = A();

				fun main() -> i64 = {
					return 0;
				}
			)",
				{ "Feature not implemented", "Compile time" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class A { fun foo() = 0; }

				fun main() -> i64 = {
					return 0;
				}
			)",
				{ "Feature not implemented", "zero-sized classes" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					var a: List[i32];
					var b = a;
					return 0;
				};
			)",
				{ "Cannot implicitly copy a value of non-trivially-copyable type `List[i32]`" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					var dyn_matrix: List[List[i64]];
					for (row in dyn_matrix) {} # `row` creates a copy.
				}
			)",
				{ "Cannot implicitly copy a value of non-trivially-copyable type `List[i64]`" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class U { list: List[i32]; }
				class T { u: U; }

				fun main() -> i64 = {
					var a: T;
					var b = a;
					return 0;
				};
			)",
				{ "Cannot implicitly copy a value of non-trivially-copyable type `Class T`" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun foo() -> List[i32] = {
				    var a: List[i32];
				    return a;
				}
				fun main() -> i64 = {
				    var list = foo();
				    return 0;
				}
			)",
				// `return a` implicitly copies the owned local `a`; `var list = foo()` moves the
			    // temporary and is fine. @TODO: #858 `return` should implicitly move owned locals.
				{ "Cannot implicitly copy a value of non-trivially-copyable type `List[i32]`",
			      "return a" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun foo(list: List[i32]) -> i32 = {
				    return 1;
				}
				fun main() -> i64 = {
					var list: List[i32];
				    foo(list);
				    return 0;
				}
			)",
				{ "Cannot implicitly copy a value of non-trivially-copyable type `List[i32]`" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun foo(list: ref List[i32]) -> i32 = {
				    var list_copy: List[i32] = list;
				    return list[0];
				}
				fun main() -> i64 = {
				    var list: List[i32];
				    foo(&list);
				    return 0;
				}
			)",
				{ "Cannot implicitly copy a value of non-trivially-copyable type `List[i32]`" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class U { list: List[i32]; }
				class T { u: U }
				fun main() -> i64 = {
					var a: T;
				    var b = a.u.list; 
					return 0;
				}
			)",
				{ "Cannot implicitly copy a value of non-trivially-copyable type `List[i32]`" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				class U { list: List[i32]; }
				class T { u: U }
				fun main() -> i64 = {
					var list: List[i32];
					var a: T = T(U(&list));
					return 0;
				}
			)",
				{ "Cannot implicitly copy a value of non-trivially-copyable type `List[i32]`" },
				1
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
    				var nested: List[List[i32]];
    				var inner: List[i32];
    				nested.push(inner);    
    				return 0;
				}
			)",
				{ "Cannot implicitly copy a value of non-trivially-copyable type `List[i32]`" },
				1
			);
		}

		// ============================ Other errors ============================
		{
			checkForErrorOnCompileModule(
				R"(fun a() = 100000000000000000000000;)", { "Numeric literal value is too large" }, 1
			);
			checkForErrorOnCompileModule(
				R"(fun a() = 1000i8;)",
				{ "Literal doesn't fit in the declared signed integer type." },
				1
			);
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					let z = "This is an unknown escape sequence: \c";
					return 0;
				}
			)",
				{ "Escape sequence `\\c` is not recognised." },
				1
			);
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					let formatString = f"This is an unknown escape sequence: \c";
					return 0;
				}
			)",
				{ "Escape sequence `\\c` is not recognised." },
				1
			);
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					let x = 1;
					let formatString = f"This is one: {notX}.\c";
					return 0;
				}
			)",
				// @TODO: #2817 Enable errors once they are all reported.
				{ "Symbol 'notX' not found" /*, "Escape sequence `\\c` is not recognised."*/ },
				1 /*2*/
			);

			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					var n = 42;
                    if (true) {
                        var n = 24;
						n + 1;
                    }
				}
			)",
				{ "Variable name is ambiguous, because it has been defined multiple times.",
			      "Found declaration:" },
				1
			);

			// We don't see any query errors here, because they are logged by the PST:
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					if () {}
					return 0;
				}
			)",
				{},
				0
			);

			// Check for multiple errors, note that we only see 1 error, because the other one is
			// logged by the PST.
			checkForErrorOnCompileModule(
				R"(
				fun main() -> i64 = {
					if () {}
					return 0;
				}

				fun foo() -> i64 = {
					return "a";
				}
			)",
				{},
				1
			);

			checkForErrorOnCompileModule(
				R"(
				import foo;

				let x = foo.z;
			)",
				{ "Module not found." },
				1
			);

			// Blocks are having correct scopes
			checkForErrorOnCompileModule(
				R"(
				block globals {
					var x = 0;
				}
				fun main() = {
					{
						var x = 20;
					};
					block inner {
						var x = 30;
					}
					x;
				}
			)",
				{ "Symbol", "not found" },
				1
			);
		}
	}

	void testCopyabilityErrors() {
		const std::string_view msg
			= "Cannot implicitly copy a value of non-trivially-copyable type `List[i32]`";

		checkForErrorOnCompileModule(
			R"( fun main() -> i64 = {
				var a: List[i32];
				var b: List[i32] = a;
				return 0;
			} )",
			{ msg },
			1
		);

		checkForErrorOnCompileModule(
			R"( fun main() -> i64 = {
				var a: List[i32];
				var b = a;
				return 0;
			} )",
			{ msg },
			1
		);

		checkForErrorOnCompileModule(
			R"( class H { l: List[i32]; }
			fun main() -> i64 = {
				var h: H;
				var b: List[i32] = h.l;
				return 0;
			} )",
			{ msg },
			1
		);

		checkForErrorOnCompileModule(
			R"( fun main() -> i64 = {
				var arr: List[i32][2];
				var b: List[i32] = arr[0];
				return 0;
			} )",
			{ msg },
			1
		);

		checkForErrorOnCompileModule(
			R"( fun main() -> i64 = {
				var a: List[i32];
				var b: box List[i32] = a;
				return 0;
			} )",
			{ msg },
			1
		);

		checkForErrorOnCompileModule(
			R"( fun f(r: ref List[i32]) -> i32 = {
				var b: List[i32] = r;
				return 0;
			})",
			{ msg },
			1
		);

		checkForErrorOnCompileModule(
			R"( fun f(bl: box List[i32]) -> i32 = {
				var x: List[i32] = bl;
				return 0;
			})",
			{ msg },
			1
		);

		checkForErrorOnCompileModule(
			R"( fun foo(x: List[i32]) -> i32 = 0;
			fun main() -> i64 = {
				var a: List[i32];
				foo(a);
				return 0;
			} )",
			{ msg },
			1
		);

		checkForErrorOnCompileModule(
			R"( fun main() -> i64 = {
				var t: (i32, List[i32]);
				var b: (i32, List[i32]) = t;
				return 0;
			} )",
			{ "Cannot implicitly copy a value of non-trivially-copyable type "
		      "`Tuple(i32, List[i32])`" },
			1
		);
	}

	/**
	 * Test error logging related to errors in expanded statements or inside the expanded code.
	 */
	void testErrorLoggingExpandStatements() {
		// @TODO: #2213 Update the values in the test cases below.

		// ======= PARSE ERRORS IN EXPAND STATEMENTS =======
		checkForErrorOnCompileModule(
			R"(
				expand "fun foo";
			)",
			{ "Code expanded from here." },
			1
		);

		// Here we check for the parse error msg and two `expanded from` notes.
		checkForErrorOnCompileModule(
			R"(
				expand " expand \" fun a \"  ";
			)",
			{ "Opening bracket ( of a function parameter list expected after here.",
		      "Code expanded from here.",
		      "Code expanded from here." },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				expand " namespace N { fun a }  ";
			)",
			{ "Code expanded from here." },
			1
		);


		// ======= ERRORS RELATED TO EXPANDED CODE =======

		checkForErrorOnCompileModule(
			R"(
				expand "fun bar() = 10;";
				expand "fun bar(x: i64 = 0) = 10;";

				fun main() -> i64 = {
					bar(); # ambiguous call, both overloads match
					return 0;
				}
			)",
			{},
			1
		);

		checkForErrorOnCompileModule(
			R"(
				expand "fun foo(x: i64) = 10 + y;"; # error: `y` is not defined
			)",
			{ "y", "not found" },
			1
		);

		// ======= ERRORS IN EXPANSION EXPRESSION =======

		checkForErrorOnCompileModule(
			R"(
				expand 1;
			)",
			{ "str or String", "i32" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				expand y;
			)",
			{ "y", "not found" },
			1
		);

		// ======= ERRORS IN EXPANDED CODE DOES NOT PREVENT OTHER DIAGNOSTICS =======

		checkForErrorOnCompileModule(
			R"(
				expand "fun foo(x: i64) -> i64 = 10 + y;";

				fun bar() = {
					return foo(1, 1);
				}

			)",
			{},
			2
		);

		checkForErrorOnCompileModule(
			R"(	
				namespace N { expand y; }

				fun foo() = z;
			)",
			{ "y", "z", "not found" },
			2
		);

		checkForErrorOnCompileModule(
			R"(
				class T {
					x: i64 = 1;

					fun m1() = {
						expand "return y";
					}

					fun m2() = {
						expand "return z";
					}
				}

				fun main() = {
					return w;
				}

			)",
			{ "not found" },
			3  // The order of the errors is not deterministic, so we just check the count here.
		);
		// Here I test two things, one if the `code expanded from here`
		// is generated and second if the error position is correct,
		// the position of the `exact candidate` should be the position of the 'expand'.
		checkForErrorOnCompileModule(
			R"(
				const str_a = "expand \"fun foo() -> i64 = 1 + 1;\"; ";
				const str_b = "fun foo() -> i32 = 1 + 1;";
				namespace N {
					expand str_a;
					expand str_b;
				}

				fun foo() = N.foo();
			)",
			{
				"Call failed",
				"Found exact candidate.",
				".dmf:5:6",
				"Code expanded from here.",
				".dmf:5:6",
				"Code expanded from here.",
				".dmf:5:6",
				"Found exact candidate.",
				".dmf:6:6",
				"Code expanded from here.",
				".dmf:6:6",
			},
			1
		);
	}

	/**
	 * Test error logging related to errors on cyclic compilation.
	 */
	void testErrorLoggingCyclicErrors() {
		checkForErrorOnCompileModule(
			R"(
				const a = b;
				const b = a;
			)",
			{ "cycle" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun foo() = {
					return bar();
				}

				fun bar() = {
					return foo();
				}
			)",
			{ "cycle" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				const X = xWithAdded(10);
				fun xWithAdded(v: i64) = X + v;
				const Y = xWithAdded(10);
			)",
			{ "cycle" },
			1
		);
	}

	void testPointerCastErrors() {
		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var x: i32 = 10;
					var p = x as ptr i32;
				}
			)",
			{ "Invalid cast from type", "i32", "ptr i32" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var x: i32 = 10;
					var r: ref i32 = &x;
					var m = r as manyptr i32;
				}
			)",
			{ "Invalid cast from type", "ref i32", "manyptr i32" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var b: box i32 = 10;
					var m = b as manyptr i32;
				}
			)",
			{ "Invalid cast from type", "box i32", "manyptr i32" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var x: i32 = 10;
					var p: ptr i32 = &x as ptr i32;
					var m = p as manyptr i32;
				}
			)",
			{ "Invalid cast from type", "ptr i32", "manyptr i32" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var x: i32 = 10;
					var p: ptr i32 = &x as ptr i32;
					var q = p as ptr i64;
				}
			)",
			{ "Invalid cast from type", "ptr i32", "ptr i64" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var x: i32 = 10;
					var p = &x as ptr i64;
				}
			)",
			{ "Invalid cast from type", "ref i32", "ptr i64" },
			1
		);
		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var x: box i32 = 10;
					var p = x as ptr i64;
				}
			)",
			{ "Invalid cast from type", "box i32", "ptr i64" },
			1
		);
		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var x = 10;
					var y = *x;
				}
			)",
			{ "Tried to dereference a non-pointer type" },
			1
		);
	}

	void testErrorBadExpr() {
		using namespace compiler::helios;

		auto [_, root_scope]
			= test_utils::getModule(fs::File(path("test_modules/error_generating/bad_expr")));


		// Stuff in this fails on the HOUT creation level instead of during the evaluation.
		// @TODO: #1287 write a test that checks failing compile-time evaluation of comparison chain.

		ASSERT_TRUE(query::entryPoint<QueryConstValueOf>(
						test_utils::getChain("InvalidExpr", root_scope).back()
		)
		                .hasFailed());

		try {
			test_utils::getConstValueAs<i64>("InvalidSym", root_scope);
			CORE_PANIC("Should throw.");
		} catch (query::internal::QueryFailedException& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			test_utils::getConstValueAs<i64>("C", root_scope);
			CORE_PANIC("Should throw.");
		} catch (query::internal::QueryFailedException& err) {
			// Since this branch was chosen, everything worked well.
		}

		// This fails on the HOUT creation level instead of during the evaluation.
		// @TODO: #1287 write a test that checks failing compile-time evaluation of comparison chain.
		try {
			test_utils::getConstValueAs<bool>("InvalidCompMiddle", root_scope);
			CORE_PANIC("Should throw.");
		} catch (query::internal::QueryFailedException& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			test_utils::getConstValueAs<bool>("InvalidCompFirst", root_scope);
			CORE_PANIC("Should throw.");
		} catch (query::internal::QueryFailedException& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			test_utils::getConstValueAs<f32>("INVALID_ADD", root_scope);
			CORE_PANIC("Should throw.");
		} catch (query::internal::QueryFailedException& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			test_utils::getConstValueAs<bool>("CHAIN_MIXED_TYPES_TRUE", root_scope);
			CORE_PANIC("Should throw.");
		} catch (query::internal::QueryFailedException& err) {
			// Since this branch was chosen, everything worked well.
		}
	}

	void testDiagnosticErrorsCorrectness() {
		using namespace helios::code;
		using namespace helios;
		using dia_int::testDiagnosticMessage;

		std::stringstream ss;

		query::utils::withContextDo([&](query::Context& ctx) {
			using enum tsh::IntegralAbstractType::Signedness;
			const auto int32_type
				= tsh::getIntegralType(ctx, 32, tsh::IntegralAbstractType::Signedness::Signed);
			auto st = tsh::SymbolType{
				int32_type,
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};

			// UndefinedBinaryOperatorError
			testDiagnosticMessage<UndefinedBinaryOperatorError>(
				ss,
				dia_int::StablePosition::fakePosition(),
				"+",
				makeBox<InteractiveType>(ctx, st),
				makeBox<InteractiveType>(ctx, st)
			);

			// UndefinedUnaryOperatorError
			testDiagnosticMessage<UndefinedUnaryOperatorError>(
				ss, dia_int::StablePosition::fakePosition(), "-", makeBox<InteractiveType>(ctx, st)
			);

			// InvalidNumericLiteralError
			testDiagnosticMessage<InvalidNumericLiteralError>(
				ss, dia_int::StablePosition::fakePosition()
			);

			// NumericLiteralTooLargeError
			testDiagnosticMessage<NumericLiteralTooLargeError>(
				ss, dia_int::StablePosition::fakePosition()
			);

			// LiteralDoesNotFitError
			testDiagnosticMessage<LiteralDoesNotFitError>(
				ss, dia_int::StablePosition::fakePosition(), "signed integer"
			);

			// SingleStmtFunctionMustBeExprError
			testDiagnosticMessage<SingleStmtFunctionMustBeExprError>(
				ss, dia_int::StablePosition::fakePosition()
			);

			// ImmutableVariableNoInitError
			testDiagnosticMessage<ImmutableVariableNoInitError>(
				ss, dia_int::StablePosition::fakePosition()
			);
		});
	}

	void testDuplicatedDefinitions() {
		// Duplicated function.
		checkForErrorOnCompileModule(
			R"(
                fun a() -> i64 = { return 1; }
                fun a() -> i64 = { return 2; }
            )",
			{ "Symbol 'a' is already defined.", "Previous declaration here." },
			1
		);

		// Duplicated class.
		checkForErrorOnCompileModule(
			R"(
                class T { x: i64 = 0; }
                class T { x: i64 = 0; }
            )",
			{ "Symbol 'T' is already defined.", "Previous declaration here." },
			1
		);

		// Duplicated global variable.
		checkForErrorOnCompileModule(
			R"(
                var a: i64 = 123;
                var a: i64 = 12;
            )",
			{ "Symbol 'a' is already defined.", "Previous declaration here." },
			1
		);
	}

	void testBackendDependentAttributeErrors() {
		// ==================== Attribute format errors ====================

		checkForErrorOnCompileModule(
			R"(@unknown_attr fun a() -> i32 = 0;)", { "is not recognized" }, 1
		);

		checkForErrorOnCompileModule(
			R"(@foo.bar fun a() -> i32 = 0;)", { "Attribute is not supported" }, 1
		);

		// @backend_dependent on `fun` instead of `fundecl`
		checkForErrorOnCompileModule(
			R"(@backend_dependent fun a() -> i32 = 0;)",
			{ "Attribute is not supported on this type of statement" },
			1
		);

		// Mutually exclusive @dvm_only_impl and @native_only_impl on same function
		// (also triggers missing @backend_dependent fundecl, so 2 errors total)
		checkForErrorOnCompileModule(
			R"(@dvm_only_impl @native_only_impl fun func() -> i32 = { return 10; })",
			{ "exclusive" },
			2
		);

		// ==================== Missing counterpart errors ====================

		// @backend_dependent fundecl with no @dvm_only_impl implementation
		checkForErrorOnCompileModule(
			R"(
                @backend_dependent
                fundecl func() -> i32;

                @native_only_impl
                fun func() -> i32 = { return 20; }
            )",
			{ "dvm_only_impl" },
			1
		);

		// @dvm_only_impl with no corresponding @backend_dependent fundecl
		checkForErrorOnCompileModule(
			R"(
                @dvm_only_impl
                fun func() -> i32 = { return 10; }
            )",
			{ "backend_dependent" },
			1
		);

		// ==================== Signature mismatch errors ====================

		// Return type mismatch between fundecl and dvm impl
		checkForErrorOnCompileModule(
			R"(
                @backend_dependent
                fundecl func() -> i32;

                @dvm_only_impl
                fun func() -> i64 = { return 10; }

                @native_only_impl
                fun func() -> i32 = { return 20; }
            )",
			{ "does not match" },
			1
		);

		// Parameter type mismatch between fundecl and dvm impl
		checkForErrorOnCompileModule(
			R"(
                @backend_dependent
                fundecl func(x: i32) -> i32;

                @dvm_only_impl
                fun func(x: i64) -> i32 = { return 0; }

                @native_only_impl
                fun func(x: i32) -> i32 = { return x; }
            )",
			{ "does not match" },
			1
		);

		// Parameter name mismatch between fundecl and dvm impl
		checkForErrorOnCompileModule(
			R"(
                @backend_dependent
                fundecl func(x: i32) -> i32;

                @dvm_only_impl
                fun func(y: i32) -> i32 = { return y; }

                @native_only_impl
                fun func(x: i32) -> i32 = { return x; }
            )",
			{ "does not match" },
			1
		);

		// ==================== Initial value errors ====================

		// @dvm_only_impl with a parameter that has a default value
		checkForErrorOnCompileModule(
			R"(
                @backend_dependent
                fundecl func(x: i32) -> i32;

                @dvm_only_impl
                fun func(x: i32 = 5) -> i32 = { return x; }

                @native_only_impl
                fun func(x: i32) -> i32 = { return x; }
            )",
			{ "Initial value is not allowed here" },
			1
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
