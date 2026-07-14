/**
 * @file helios_shorthands_test.cpp
 * @brief Tests for the HOUT construction shorthands (@ref shorthands.hpp).
 */

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/hout/elements.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>

#include <base/pointers/box.hpp>

#include <filesystem/file.hpp>
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
		TESTER_ADD_TEST(testReusable);
		TESTER_ADD_TEST(testCall);
		TESTER_ADD_TEST(testConversionExprs);
		TESTER_ADD_TEST(testMiscExprs);
		TESTER_ADD_TEST(testAccess);
		TESTER_ADD_TEST(testStatements);
		TESTER_ADD_TEST(testGeneratedOrigins);
		TESTER_ADD_TEST(testWithOrigin);
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

			// A string literal is a char slice.
			const auto hello = litStr(base::StrID("hello"));
			ASSERT_EQUAL(dprint(hello), std::string("hello"));
			ASSERT_EQUAL(hello->value.str(), std::string("hello"));
			ASSERT_EQUAL(hello->expression_type.getType().getKind(), tsh::Kind::Slice);

			// A type literal is itself a `meta` value, wrapping the `i64` type it carries.
			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto i64_literal = litType(i64_type);
			ASSERT_EQUAL(i64_literal->expression_type.getType().getKind(), tsh::Kind::Meta);
			ASSERT_EQUAL(i64_literal->value_type.getType().getKind(), tsh::Kind::Integral);
		});
	}

	/** Composite builders produce the expected nodes. */
	void testComposites() {
		query::utils::withContextDo([&](query::Context& ctx) {
			using enum BuiltinBinary;
			Shorthand  sh{ ctx };
			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto bool_type = tsh::getBoolType();

			ASSERT_EQUAL(
				dprint(binOp(litNum(40), BuiltinBinary::IntegerAdd, litNum(2))),
				std::string("40 + 2")
			);
			ASSERT_EQUAL(
				dprint(unOp(BuiltinUnary::BooleanNot, litBool(true))), std::string("not true")
			);

			// Nesting composes as expected: (40 + 2) * 3.
			const auto nested
				= binOp(binOp(litNum(40), IntegerAdd, litNum(2)), IntegerMul, litNum(3));
			ASSERT_EQUAL(dprint(nested), std::string("40 + 2 * 3"));
			ASSERT_EQUAL(nested->expression_type.getType().getKind(), tsh::Kind::Integral);

			// A ternary takes the type of its `then` branch.
			const auto tern = ternary(litBool(true), litNum(1), litNum(2));
			ASSERT_EQUAL(dprint(tern), std::string("if true then 1 else 2"));
			ASSERT_EQUAL(tern->expression_type.getType().getKind(), tsh::Kind::Integral);

			// A sequence yields its last expression's value and type.
			const auto sequence = seq(litNum(1), litNum(2), litBool(true));
			ASSERT_EQUAL(dprint(sequence), std::string("1, 2, true"));
			ASSERT_EQUAL(sequence->expression_type.getType().getKind(), tsh::Kind::Bool);

			// The pack and vector overloads are equivalent.
			std::vector<Box<Expr>> elements;
			elements.emplace_back(litNum(1));
			elements.emplace_back(litNum(2));
			ASSERT_EQUAL(dprint(seq(std::move(elements))), dprint(seq(litNum(1), litNum(2))));

			// A chain comparison is the boolean AND of its comparisons.
			// note: Usually this includes a ReusableExpr, but that's not required.
			const auto chain = chainCmp(
				binOp(litNum(1), IntegerLt, litNum(2)), binOp(litNum(2), IntegerLt, litNum(3))
			);
			ASSERT_EQUAL(dprint(chain), std::string("1 < 2 and 2 < 3"));
			ASSERT_EQUAL(chain->expression_type.getType().getKind(), tsh::Kind::Bool);

			// A variant type constructor is a `meta` value over its subtypes.
			const auto variant_ctor = variant(litType(i64_type), litType(bool_type));
			ASSERT_EQUAL(variant_ctor->expression_type.getType().getKind(), tsh::Kind::Meta);
			const auto* variant_expr
				= dynamic_cast<const VariantTypeConstructorExpr*>(variant_ctor.get());
			ASSERT_TRUE(variant_expr != nullptr);
			ASSERT_EQUAL(variant_expr->subtypes.size(), 2UL);

			// A tuple value has a tuple type built from its element types.
			const auto pair = tuple(litNum(1), litBool(true));
			ASSERT_EQUAL(dprint(pair), std::string("(1, true)"));
			ASSERT_EQUAL(pair->expression_type.getType().getKind(), tsh::Kind::Tuple);

			// Indexing a type value builds a static-array type (`i64[4]`), itself a `meta` value.
			const auto array_type = index(litType(i64_type), litNum(4));
			ASSERT_EQUAL(array_type->expression_type.getType().getKind(), tsh::Kind::Meta);
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
	 * `call` builds the expected CallExpr.
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

	/** `cast`, `refOf`, `deref` and `moveOf` reshape an operand's type as expected. */
	void testConversionExprs() {
		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			const auto i32_type
				= tsh::getIntegralType(ctx, 32, tsh::IntegralAbstractType::Signedness::Signed);
			const auto i64_sym_type = tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed)
			);

			// A cast carries the requested target type.
			const auto casted = cast(litNum(5), i64_sym_type);
			ASSERT_EQUAL(casted->expression_type.getType().getKind(), tsh::Kind::Integral);
			ASSERT_EQUAL(
				casted->expression_type.getType().as<tsh::IntegralAbstractType>().getSize(), Bits(32)
			);
			ASSERT_EQUAL(dprint(casted), std::string("cast 5 to i64"));

			// `refof x` has reference type; dereferencing it recovers the (direct) pointee.
			const auto ref = refOf(litNum(5));
			ASSERT_EQUAL(dprint(ref), std::string("refof(5)"));
			ASSERT_EQUAL(ref->expression_type.getSymbolType().getRefKind(), tsh::ReferenceKind::Ref);

			const auto derefed = deref(refOf(litNum(5)));
			ASSERT_EQUAL(dprint(derefed), std::string("deref(refof(5))"));
			ASSERT_EQUAL(
				derefed->expression_type.getSymbolType().getRefKind(), tsh::ReferenceKind::Direct
			);
			ASSERT_EQUAL(derefed->expression_type.getType().getKind(), tsh::Kind::Integral);

			// `move x` preserves the operand type as a temporary.
			const auto moved = moveOf(litNum(5));
			ASSERT_EQUAL(dprint(moved), std::string("move(5)"));
			ASSERT_EQUAL(moved->expression_type.getType().getKind(), tsh::Kind::Integral);
			ASSERT_EQUAL(
				moved->expression_type.getValueCategory().getCategory(),
				tsh::PrimaryCategory::Temporary
			);
		});
	}

	/** `defaultValue`, `liftToType`, and the list push/pop builders. */
	void testMiscExprs() {
		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);

			// A default value carries the given type.
			const auto zero = defaultValue(i64_type);
			ASSERT_EQUAL(zero->expression_type.getType().getKind(), tsh::Kind::Integral);
			ASSERT_EQUAL_PRINT(dprint(zero), std::string("default_value(i64)"));
			ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(zero.get()) != nullptr);

			// Lifting a value to a type produces a `meta` value.
			const auto lifted = liftToType(litUnit());
			ASSERT_EQUAL(dprint(lifted), std::string("lift[to=type](())"));
			ASSERT_EQUAL(lifted->expression_type.getType().getKind(), tsh::Kind::Meta);

			// listPush / listPop are structural wrappers evaluating to unit; like the underlying
			// nodes they do not type-check their operands, so plain literals exercise the wiring.
			// `1` is not a list, but whatever, these are going away anyway soon.
			const auto push = listPush(litNum(1), litNum(2));
			ASSERT_EQUAL(dprint(push), std::string("list_push(1, 2)"));
			ASSERT_EQUAL(push->expression_type.getType().getKind(), tsh::Kind::Unit);

			const auto pop = listPop(litNum(1), litNum(2));
			ASSERT_EQUAL(dprint(pop), std::string("list_pop(1, 2)"));
			ASSERT_EQUAL(pop->expression_type.getType().getKind(), tsh::Kind::Unit);
		});
	}

	/**
	 * `access` builds a field access `base.field`. Its result type comes from a query on the field
	 * symbol, so a module supplying a real struct field (`Point.x`) is loaded.
	 */
	void testAccess() {
		const auto [module, scope] = getModule(fs::File(path("test_modules/shorthands/access")));
		const auto point_var       = getChain("point", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			// Resolve the `x` field of `point`'s struct type through its type interface.
			const auto x_field = ctx.query<helios::QueryTypeOfSymbol>(point_var)
			                         ->valueOrThrow()
			                         .getType()
			                         .getInterface(ctx)
			                         ->getElementsWithName(base::StrID("x"))
			                         .back()
			                         .getSymbol();

			const auto field_access = access(ident(point_var), x_field);

			const auto* access_expr = dynamic_cast<const AccessExpr*>(field_access.get());
			ASSERT_TRUE(access_expr != nullptr);
			// The access wires `point` as the base and `x` as the accessed field.
			ASSERT_EQUAL(
				compiler::helios::getIdentifierExprSymID(access_expr->base.ref()).value(), point_var
			);
			ASSERT_TRUE(access_expr->field == x_field);
			// The access's type is the field's type (`i64`).
			ASSERT_EQUAL(field_access->expression_type.getType().getKind(), tsh::Kind::Integral);
		});
	}

	/**
	 * Statement builders wrap their operands and compose into `CodeBlock`s (including nested bodies).
	 * The builders don't type-check, they just assemble the tree.
	 * `var` needs a real symbol and a type, so a module is loaded to supply them.
	 */
	void testStatements() {
		const auto [module, scope] = getModule(fs::File(path("test_modules/dummy")));
		const auto dummy_symbol    = getChain("dummy", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			Shorthand sh{ ctx };

			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto i64_sym_type = litNum(0, i64_type)->expression_type.getSymbolType();

			// Simple statements wrap their expression operand.
			const auto decl = var(dummy_symbol, i64_sym_type, litNum(0, i64_type));
			ASSERT_TRUE(decl->initial_value.isBox());
			ASSERT_TRUE(decl->helios_symbol == dummy_symbol);

			const auto assignment = assign(ident(dummy_symbol), litNum(1, i64_type));
			ASSERT_TRUE(assignment->location_expr.isBox());
			ASSERT_TRUE(assignment->new_value_expr.isBox());

			ASSERT_TRUE(ret(litNum(1))->value.isBox());
			ASSERT_TRUE(expr(litBool(true))->expr.isBox());
			ASSERT_TRUE(ret()->origin.isGenerated());

			// Body arguments accept a braced list of statements directly.
			const auto loop = whileStmt(litBool(true), { expr(litNum(0)), ret() });
			ASSERT_TRUE(loop->condition.isBox());
			ASSERT_EQUAL(loop->body.statements.size(), 2UL);

			// if with and without else.
			const auto if_no_else = ifStmt(litBool(true), { ret() });
			ASSERT_EQUAL(if_no_else->then_body.statements.size(), 1UL);
			ASSERT_EQUAL(if_no_else->else_body.statements.size(), 0UL);

			const auto if_else = ifStmt(litBool(false), { ret() }, { ret(), ret() });
			ASSERT_EQUAL(if_else->then_body.statements.size(), 1UL);
			ASSERT_EQUAL(if_else->else_body.statements.size(), 2UL);

			// block(...) wraps a body as a BlockStmt.
			ASSERT_EQUAL(block({ ret(), ret() })->body.statements.size(), 2UL);

			// An empty braced list is a valid (empty) body.
			ASSERT_EQUAL(ifStmt(litBool(true), {})->then_body.statements.size(), 0UL);

			// A standalone CodeBlock (e.g. a whole function body) via StmtPack::toCodeBlock.
			const auto body = StmtPack{ ret(litNum(1)), expr(litNum(2)) }.toCodeBlock();
			ASSERT_EQUAL(body.statements.size(), 2UL);

			// Box<Derived> -> Box<Stmt> accumulation, wrapped through StmtPack's vector constructor.
			std::vector<Box<Stmt>> collected;
			collected.emplace_back(ret(litNum(7)));
			ASSERT_EQUAL(StmtPack{ std::move(collected) }.toCodeBlock().statements.size(), 1UL);
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
		const auto [module, scope] = getModule(fs::File(path("test_modules/dummy")));
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

			// Override the origin (this isn't strictly correct, in terms of constructing valid
			// HOUT, but OK for the test).
			dummy_ident = withOrigin(pst_origin, std::move(dummy_ident));

			// The HOUT Expr now carries the specified origin.
			ASSERT_TRUE(!dummy_ident->origin.isGenerated());
			ASSERT_TRUE(dummy_ident->origin.getStablePosition().has_value());
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
