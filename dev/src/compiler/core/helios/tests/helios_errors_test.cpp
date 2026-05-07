
#include <diagnostic_interactive/stable_position.hpp>
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

#include <diagnostic/highlight_positions.hpp>
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
		TESTER_ADD_TEST(testErrorLoggingExpandStatements);
		TESTER_ADD_TEST(testErrorLoggingCyclicErrors);
		TESTER_ADD_TEST(testErrorBadExpr);
		TESTER_ADD_TEST(testDiagnosticErrorsCorrectness);
	}

private:
	/**
	 * @brief Helper function that check for HELIOS compilation
	 * errors in a module with given content, when compiling it to HOUT module.
	 *
	 * It creates a virtual file from the `module_content` argument
	 * and creates a module tree from it every function call.
	 *
	 * @TODO: #2213 Add PST errors handling here.
	 *
	 * @param module_content The content of the module main source file.
	 * @param present_phrases List of phrases that should be present in the logged errors.
	 * @param logged_msg_count Expected number of logged error messages.
	 */
	void checkForErrorOnCompileModule(
		std::string_view                     module_content,
		const std::vector<std::string_view>& present_phrases,
		u64                                  logged_msg_count
	) {
		frontend::ModuleID module_id
			= frontend::createModuleTreeFromContents(module_content, "test_package");
		query::utils::withContextDo([&](query::Context& ctx) {
			auto result = ctx.query<helios::QueryModuleHOUT>(module_id);
			assertTrue(result->hasFailed(), "Expected HOUT query to fail for module content.");
			auto logger = query::Context::dumpToOneLoggerAndClear();

			// @TODO: #2213 we should do something smarted here, and see if the sum of pst and
			// query errors is ok:
			assertTrue(
				logger->hasErrors() or logged_msg_count == 0, "Expected errors to be logged."
			);

			std::stringstream logged_messages;
			logger->terminalPrint(logged_messages);
			std::cerr << "Logged messages:\n" << logged_messages.str() << "\n";
			auto msg_count = logger->messageCount();
			assertEqual(
				msg_count,
				logged_msg_count,
				"Expected logged message count to be " + std::to_string(logged_msg_count)
					+ ", but got " + std::to_string(msg_count)
			);
			for (const auto& phrase: present_phrases) {
				std::string logged_str = logged_messages.str();
				assertTrue(
					logged_str.find(phrase.data()) != std::string::npos,
					"Expected logged messages to contain phrase: " + std::string(phrase)
				);
			}
		});
	}

	/**
	 * Generic error logging tests.
	 * Add additional test cases for more specific categories.
	 */
	void testErrorLogging() {
		// ============================ No operator found ============================
		checkForErrorOnCompileModule(
			R"(fun a() = true + false;)", { "Call failed due to ambiguous overload resolution" }, 1
		);
		checkForErrorOnCompileModule(
			R"(fun a() = 'c' + 1i64;)",
			{
				"Call failed due to ambiguous overload resolution",
				"type const Function (const u8, const char) -> (const char).",
				"given argument type `char` cannot be converted to the expected type `const u8`.",
				"type const Function (const char, const u8) -> (const char).",
				"given argument type `i64` cannot be converted to the expected type `const u8`.",
			},
			1
		);
		checkForErrorOnCompileModule(R"(fun a() = -true;)", { "No builtin unary operator" }, 1);

		// ============================ Function calls ============================
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
					builtin_output_i64();
				}
			)",
			{ "call is missing a required argument", "declaration is not available." },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun a() = {
					b(1,2,3);
				}
			)",
			{ "no matching functions" },
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

		// =========================== Method call errors ===========================

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
			{ " Call failed due to ambiguous overload resolution." },
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
			{ "Found a combination of function and non-function callables in a call expression." },
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
			{ "Found a combination of function and non-function callables in a call expression." },
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

		// ============================ Typecheck errors ============================
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
			{ "Type `string` cannot be converted to type `const i64`." },
			1
		);


		checkForErrorOnCompileModule(
			R"(
				const unitType: type = ();

				fun foo(u: ()) -> () = {
				    builtin_output_i64(1);
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

		checkForErrorOnCompileModule(
			R"(
				const a: f64 = 1.0 / 0.0;
				fun main() -> i64 = 0;
			)",
			{ "Division", "zero" },
			1
		);

		// ========================== Default initialization errors ==========================
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

		// ============================ Other errors ============================
		checkForErrorOnCompileModule(
			R"(fun a() = 100000000000000000000000;)", { "Numeric literal value is too large" }, 1
		);
		checkForErrorOnCompileModule(
			R"(fun a() = 1000i8;)", { "Literal doesn't fit in the declared signed integer type." }, 1
		);
		checkForErrorOnCompileModule(
			R"(
				fun main() -> i64 = {
					builtin_output_string("This is an unknown escape sequence: \c");
					return 0;
				}
			)",
			{ "unknown escape sequence" },
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
					var n = 42;
                    if (true) {
                        var n = 24;
                        builtin_output_i64(n);
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

		// Check for multiple errors, note that we only see 1 error, because the other one is logged
		// by the PST.
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

		// ============================ Static Arrays ============================
		checkForErrorOnCompileModule(
			R"(
				fun main(n: u64) = {
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
			{ "Type `f32` cannot be converted to type `const u64`." },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				const ARR_TYPE = i32[-2];
			)",
			{ "Value cannot be converted to type `const u64` at compile-time." },
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
			{ "Type `string` cannot be converted to type `const i64`." },
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

		// ============================ Dynamic Arrays ============================
		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var l: List[i64];
					l += 1.5;
				}
			)",
			{ "Type `f32` cannot be converted to type `i64`" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var l: List[i64];
					l -= "sth";
				}
			)",
			{ "Type `string` cannot be converted to type `u64`" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun main() = {
					var x = 10;
					var length = len x;
				}
			)",
			{ "No builtin unary operator `len` for type `i32`" },
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
				fun main() = {
					var l1: List[i64];
					var l2: List[f64] = l1;
				}
			)",
			{ "Type `List[i64]` cannot be converted to type `List[f64]`" },
			1
		);


		// ========================= Not-yet-implemented errors =========================

		// Note: just remove the tests when the features
		// are implemented.

		checkForErrorOnCompileModule(
			R"(
				fun main() -> i64 = {
					var x: i64 = 0;
					x++;
					return 0;
				}
			)",
			{ "Feature not implemented", "Suffix" },
			1
		);

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
			{ "Feature not implemented", "for" },
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
				class A { a: i64 = 1; }
				fun main() -> i64 = {
					var a: (i32, A);
					return 0;
				}
			)",
			{ "Feature not implemented", "Generating default constructors for", "tuple types" },
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
			{ "Copy constructor for non-trivially-copyable type `List[i32]`" },
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
			{ "Copy constructor for non-trivially-copyable type `Class T`" },
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
			{ "Copy constructor for non-trivially-copyable type `List[i32]`", "return a", "foo()" },
			2
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
			{ "Copy constructor for non-trivially-copyable type `List[i32]`" },
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
			{ "Copy constructor for non-trivially-copyable type `List[i32]`" },
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
			{ "Copy constructor for non-trivially-copyable type `List[i32]`" },
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
			{ "Copy constructor for non-trivially-copyable type `List[i32]`",
		      "This was caused by the need" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				fun main() -> i64 = {
    				var nested: List[List[i32]];
    				var inner: List[i32];
    				nested += inner;    
    				return 0;
				}
			)",
			{ "Copy constructor for non-trivially-copyable type `List[i32]`" },
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
			{ "Macro", "expansion" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				expand " expand \" fun a \"  ";
			)",
			{ "Macro", "expansion" },
			1
		);

		checkForErrorOnCompileModule(
			R"(
				expand " namespace N { fun a }  ";
			)",
			{ "Macro", "expansion" },
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
			{ "i32", "string" },
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
			{ "y", "z", "w", "not found" },
			3
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
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
