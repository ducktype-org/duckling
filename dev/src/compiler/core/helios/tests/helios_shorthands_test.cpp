/**
 * @file helios_shorthands_test.cpp
 * @brief Tests for the HOUT construction shorthands (@ref shorthands.hpp).
 */

#include <helios/hout/elements.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>

#include <base/pointers/box.hpp>

#include <filesystem/file.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <sstream>
#include <string>

using namespace compiler;
using namespace compiler::helios::code;
using namespace compiler::helios::code::shorthands;
using namespace compiler::helios::test_utils;

namespace {
	template<typename T>
	std::string dprint(const Box<T>& expr) {
		std::ostringstream out;
		expr->debugPrint(out);
		return out.str();
	}
}

class HeliosShorthandsTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosShorthandsTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testLiterals);
		TESTER_ADD_TEST(testComposites);
		TESTER_ADD_TEST(testTypedNumericLiteral);
		TESTER_ADD_TEST(testGeneratedOrigins);
		TESTER_ADD_TEST(testWithOrigin);
		TESTER_ADD_TEST(testReusable);
		TESTER_ADD_TEST(testCall);
		TESTER_ADD_TEST(testGuardSaveRestore);
	}

	/** Leaf builders produce the expected nodes and derive types from `ctx`. */
	void testLiterals() {
		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			ASSERT_EQUAL(dprint(litUnit()), std::string("()"));
			ASSERT_EQUAL(dprint(litBool(true)), std::string("true"));
			ASSERT_EQUAL(dprint(litBool(false)), std::string("false"));
			ASSERT_EQUAL(dprint(litChar('x')), std::string("'x'"));
			ASSERT_EQUAL(dprint(litNum(40)), std::string("40"));

			// A bare integer literal is minimized to i32 (its type came from `ctx`).
			const auto forty = litNum(40);
			ASSERT_EQUAL(forty->expression_type.getType().getKind(), tsh::Kind::Integral);
		});
	}

	/** Composite builders wire children through and pick the operator. */
	void testComposites() {
		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			ASSERT_EQUAL(
				dprint(binOp(litNum(40), BuiltinBinary::IntegerAdd, litNum(2))),
				std::string("40 + 2")
			);
			ASSERT_EQUAL(
				dprint(unOp(BuiltinUnary::BooleanNot, litBool(true))), std::string("not true")
			);

			// Nesting composes as expected: (40 + 2) * 3.
			using enum BuiltinBinary;
			const auto nested
				= binOp(binOp(litNum(40), IntegerAdd, litNum(2)), IntegerMul, litNum(3));
			ASSERT_EQUAL(dprint(nested), std::string("40 + 2 * 3"));
			ASSERT_EQUAL(nested->expression_type.getType().getKind(), tsh::Kind::Integral);
		});
	}

	/**
	 * The `litNum(value, type)` overload builds a literal of a requested type. Verified through the
	 * `char + integral -> char` promotion rule: a `char` plus a `u8` literal is typed `char`, which
	 * only holds if the `u8` literal actually carried the requested `u8` type.
	 */
	void testTypedNumericLiteral() {
		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			const auto u8_type
				= tsh::getIntegralType(ctx, 8, tsh::IntegralAbstractType::Signedness::Unsigned);
			const auto sum = binOp(litChar('a'), BuiltinBinary::IntegerAdd, litNum(1, u8_type));
			ASSERT_EQUAL(sum->expression_type.getType().getKind(), tsh::Kind::Char);
		});
	}

	/** Trees built purely from shorthands carry generated origins. */
	void testGeneratedOrigins() {
		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			const auto leaf = litNum(1);
			ASSERT_TRUE(leaf->origin.isGenerated());

			const auto composite = binOp(litNum(1), BuiltinBinary::IntegerAdd, litNum(2));
			ASSERT_TRUE(composite->origin.isGenerated());
		});
	}

	/**
	 * `withOrigin` changes the origin of an expression, so that a specific origin can be applied
	 * over the default `generatedOrigin()`. Verified with a *real*,
	 * non-generated PST origin (from `square`'s declaration) so the override is observable — a node
	 * that starts generated/positionless ends up non-generated and carrying a source position.
	 */
	void testWithOrigin() {
		const auto [module, scope] = getModule(fs::File(path("test_modules/with_origin")));
		const auto dummy_symbol    = getChain("dummy", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			// Create a compiler-generated HOUT Expression,
			// which is an IdentifierExpr pointing to the dummy variable.
			auto dummy_ident = ident(dummy_symbol);

			// Get a genuine, positioned (non-generated) origin taken from dummy's PST declaration.
			const auto pst_origin
				= pstOrigin(compiler::helios::maybeSymbolPst(dummy_symbol).value().unlock(ctx));
			ASSERT_TRUE(!pst_origin.isGenerated());
			ASSERT_TRUE(pst_origin.getStablePosition().has_value());

			// Check that origin is generated before override.
			ASSERT_TRUE(dummy_ident->origin.isGenerated());

			// Override the origin (this isn't strictly correct, but OK for the test).
			dummy_ident = withOrigin(pst_origin, std::move(dummy_ident));

			// The HOUT Expr now carries the specified origin.
			ASSERT_TRUE(!dummy_ident->origin.isGenerated());
			ASSERT_TRUE(dummy_ident->origin.getStablePosition().has_value());
		});
	}

	/** `reusable` marks the first use; `nextUse` shares the inner for subsequent uses. */
	void testReusable() {
		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			const auto first  = reusable(litNum(5));
			const auto second = first->nextUse();

			ASSERT_EQUAL(dprint(first), std::string("[tmp](5)"));
			ASSERT_EQUAL(dprint(second), std::string("[reuse](5)"));
		});
	}

	/**
	 * Builds a call to `square(a: i64) -> i64`. This is the path that exercises `ident` and a
	 * composite whose type is resolved *by a query during construction*: building `ident(square)`
	 * runs QueryTypeOfSymbol (yielding the function type), and the call's own type is that
	 * function's i64 result. It therefore validates the ambient guard across a query-triggering
	 * construction against a real symbol.
	 */
	void testCall() {
		const auto [module, scope] = getModule(fs::File(path("test_modules/function_calls")));
		const auto square_symbol   = getChain("square", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto square_call = call(ident(square_symbol), litNum(5, i64_type));

			// The call's type is square's i64 return type.
			ASSERT_EQUAL(square_call->expression_type.getType().getKind(), tsh::Kind::Integral);

			const auto* call_expr = dynamic_cast<const CallExpr*>(square_call.get());
			ASSERT_TRUE(call_expr != nullptr);
			ASSERT_EQUAL(call_expr->arguments.size(), 1UL);
			ASSERT_EQUAL(
				square_symbol,
				compiler::helios::getIdentifierExprSymID(call_expr->callee.ref()).value()
			);
		});
	}

	/**
	 * The guard restores the previous ambient context on destruction rather than nulling it, so a
	 * nested guard does not break an ongoing outer construction. If the destructor nulled the
	 * context, the `litNum` after the inner scope would dereference a null MRef and panic.
	 */
	void testGuardSaveRestore() {
		query::utils::withContextDo([&](query::Context& ctx) {
			ASSERT_TRUE(shorthands::internal::active_ctx == nullptr);

			Shorthand outer{ ctx };
			ASSERT_TRUE(&*shorthands::internal::active_ctx == &ctx);

			const auto before = litNum(1);
			{
				Shorthand inner{ ctx };
				ASSERT_TRUE(&*shorthands::internal::active_ctx == &ctx);
				const auto within = litNum(2);
			}

			// Restored to the outer guard's context, not nulled.
			ASSERT_TRUE(&*shorthands::internal::active_ctx == &ctx);
			const auto after = litNum(3);
			ASSERT_EQUAL(dprint(after), std::string("3"));
		});

		// Once every guard is gone, the ambient context is null again.
		ASSERT_TRUE(shorthands::internal::active_ctx == nullptr);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
