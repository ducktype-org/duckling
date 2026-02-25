
#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/expressions/errors.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/types.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
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
			ctx.int_logger.clear();
			auto result = ctx.query<helios::QueryModuleHOUT>(module_id);
			assertTrue(result->hasFailed(), "Expected HOUT query to fail for module content.");
			assertTrue(ctx.int_logger.hasErrors(), "Expected errors to be logged.");

			std::stringstream logged_messages;
			ctx.int_logger.terminalPrint(logged_messages);
			std::cerr << "Logged messages:\n" << logged_messages.str() << "\n";
			auto msg_count = ctx.int_logger.messageCount();
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

	void testErrorLogging() {
		// ============================ No operator found ============================
		checkForErrorOnCompileModule(
			R"(fun a() = true + false;)", { "No builtin binary operator" }, 1
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


		// ========================== Comp time errors ==========================

		checkForErrorOnCompileModule(
			R"(
				const a: f64 = 1.0 / 0.0;
				fun main() -> i64 = 0;
			)",
			{ "Division", "zero" },
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


		// =========================== Not-yet-implemented errors ==========================
		// Note: just remove the tests when the features are implemented.

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

		try {
			test_utils::getConstValueAs<bool>("INVALID_MODULO", root_scope);
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
				dia::SourcePosition::fakePosition(),
				"+",
				makeBox<InteractiveType>(ctx, st),
				makeBox<InteractiveType>(ctx, st)
			);

			// UndefinedUnaryOperatorError
			testDiagnosticMessage<UndefinedUnaryOperatorError>(
				ss, dia::SourcePosition::fakePosition(), "-", makeBox<InteractiveType>(ctx, st)
			);

			// InvalidNumericLiteralError
			testDiagnosticMessage<InvalidNumericLiteralError>(
				ss, dia::SourcePosition::fakePosition()
			);

			// NumericLiteralTooLargeError
			testDiagnosticMessage<NumericLiteralTooLargeError>(
				ss, dia::SourcePosition::fakePosition()
			);

			// LiteralDoesNotFitError
			testDiagnosticMessage<LiteralDoesNotFitError>(
				ss, dia::SourcePosition::fakePosition(), "signed integer"
			);

			// SingleStmtFunctionMustBeExprError
			testDiagnosticMessage<SingleStmtFunctionMustBeExprError>(
				ss, dia::SourcePosition::fakePosition()
			);

			// ImmutableVariableNoInitError
			testDiagnosticMessage<ImmutableVariableNoInitError>(
				ss, dia::SourcePosition::fakePosition()
			);
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
