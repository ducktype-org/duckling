// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file helios_shorthands_test.cpp
 * @brief Tests for the HOUT construction shorthands (@ref shorthands.hpp).
 */

#include <driver/test_utils.hpp>
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
		TESTER_ADD_TEST(testCoerce);
		TESTER_ADD_TEST(testPrepToPassSelf);
		TESTER_ADD_TEST(testGeneratedOrigins);
		TESTER_ADD_TEST(testWithOrigin);
	}

	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("test_modules/function_calls")), "function_calls" },
			{ fs::FilePath(path("test_modules/shorthands/access")), "access" },
			{ fs::FilePath(path("test_modules/shorthands/copy")), "copy" },
			{ fs::FilePath(path("test_modules/shorthands/dummy")), "dummy" }
		};
		auto init_result
			= compiler::driver::test_utils::initializeCompilerForTests(packages, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

	/** Leaf builders produce the expected nodes and derive types from `ctx`. */
	void testLiterals() {
		query::utils::withContextDo([&](query::Context& ctx) {
			using numeric_value::NumericValue;
			const Shorthand s{ ctx };

			ASSERT_EQUAL(dprint(s.litUnit()), std::string("()"));
			ASSERT_EQUAL(dprint(s.litBool(true)), std::string("true"));
			ASSERT_EQUAL(dprint(s.litBool(false)), std::string("false"));
			ASSERT_EQUAL(dprint(s.litChar('x')), std::string("'x'"));
			ASSERT_EQUAL(dprint(s.litNum(40)), std::string("40"));
			ASSERT_EQUAL(dprint(s.litNum(f64(-3.14))), NumericValue(f64(-3.14)).toString());

			// A bare integer literal is minimized to i32 (its type came from `ctx`).
			const auto forty = s.litNum(40);
			ASSERT_EQUAL(forty->expression_type.getType().getKind(), tsh::Kind::Integral);

			// A string literal is a char slice.
			const auto hello = s.litStr(base::StrID("hello"));
			ASSERT_EQUAL(dprint(hello), std::string("hello"));
			ASSERT_EQUAL(hello->value.str(), std::string("hello"));
			ASSERT_EQUAL(hello->expression_type.getType().getKind(), tsh::Kind::Slice);

			// Unlike `litStr` (a char slice), this evaluates to a `String`.
			const auto str_obj = s.litStrObj(base::StrID("hi"));
			ASSERT_EQUAL(str_obj->expression_type.getType().getKind(), tsh::Kind::Class);

			// - It is a call `builtin_stringify_str(<char slice "hi">)`.
			const auto* call_expr = dynamic_cast<const CallExpr*>(str_obj.get());
			ASSERT_TRUE(call_expr != nullptr);
			ASSERT_EQUAL(call_expr->arguments.size(), 1UL);

			// - The sole argument is the char-slice string literal carrying the value.
			const auto* arg
				= dynamic_cast<const LiteralStringExpr*>(call_expr->arguments.at(0).get());
			ASSERT_TRUE(arg != nullptr);
			ASSERT_EQUAL(arg->value.str(), std::string("hi"));
			ASSERT_EQUAL(arg->expression_type.getType().getKind(), tsh::Kind::Slice);

			// A type literal is itself a `meta` value, wrapping the `i64` type it carries.
			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto i64_literal = s.litType(i64_type);
			ASSERT_EQUAL(i64_literal->expression_type.getType().getKind(), tsh::Kind::Meta);
			ASSERT_EQUAL(i64_literal->value_type.getType().getKind(), tsh::Kind::Integral);
		});
	}

	/** Composite builders produce the expected nodes. */
	void testComposites() {
		query::utils::withContextDo([&](query::Context& ctx) {
			using enum BuiltinBinary;
			const Shorthand s{ ctx };
			const auto      i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto bool_type = tsh::getBoolType();

			ASSERT_EQUAL(
				dprint(s.binOp(s.litNum(40), BuiltinBinary::IntegerAdd, s.litNum(2))),
				std::string("40 + 2")
			);
			ASSERT_EQUAL(
				dprint(s.unOp(BuiltinUnary::BooleanNot, s.litBool(true))), std::string("not true")
			);

			// Nesting composes as expected: (40 + 2) * 3.
			const auto nested
				= s.binOp(s.binOp(s.litNum(40), IntegerAdd, s.litNum(2)), IntegerMul, s.litNum(3));
			ASSERT_EQUAL(dprint(nested), std::string("40 + 2 * 3"));
			ASSERT_EQUAL(nested->expression_type.getType().getKind(), tsh::Kind::Integral);

			// A ternary takes the type of its `then` branch.
			const auto tern = s.ternary(s.litBool(true), s.litNum(1), s.litChar('a'));
			ASSERT_EQUAL(dprint(tern), std::string("if true then 1 else 'a'"));
			ASSERT_EQUAL(tern->expression_type.getType().getKind(), tsh::Kind::Integral);

			// A s.sequence yields its last expression's value and type.
			const auto sequence = s.seq(s.litNum(1), s.litNum(2), s.litBool(true));
			ASSERT_EQUAL(dprint(sequence), std::string("1, 2, true"));
			ASSERT_EQUAL(sequence->expression_type.getType().getKind(), tsh::Kind::Bool);

			// The pack and vector overloads are equivalent.
			std::vector<Box<Expr>> elements;
			elements.emplace_back(s.litNum(1));
			elements.emplace_back(s.litNum(2));
			ASSERT_EQUAL(
				dprint(s.seq(std::move(elements))), dprint(s.seq(s.litNum(1), s.litNum(2)))
			);

			// A chain comparison is the boolean AND of its comparisons.
			// note: Usually this includes a ReusableExpr, but that's not required.
			const auto chain = s.chainCmp(
				s.binOp(s.litNum(1), IntegerLt, s.litNum(2)),
				s.binOp(s.litNum(2), IntegerLt, s.litNum(3))
			);
			ASSERT_EQUAL(dprint(chain), std::string("1 < 2 and 2 < 3"));
			ASSERT_EQUAL(chain->expression_type.getType().getKind(), tsh::Kind::Bool);

			// A s.variant type constructor is a `meta` value over its subtypes.
			const auto variant_ctor = s.variant(s.litType(i64_type), s.litType(bool_type));
			ASSERT_EQUAL(variant_ctor->expression_type.getType().getKind(), tsh::Kind::Meta);
			const auto* variant_expr
				= dynamic_cast<const VariantTypeConstructorExpr*>(variant_ctor.get());
			ASSERT_TRUE(variant_expr != nullptr);
			ASSERT_EQUAL(variant_expr->subtypes.size(), 2UL);

			// A tuple value has a tuple type built from its element types.
			const auto pair = s.tuple(s.litNum(1), s.litBool(true));
			ASSERT_EQUAL(dprint(pair), std::string("(1, true)"));
			ASSERT_EQUAL(pair->expression_type.getType().getKind(), tsh::Kind::Tuple);

			// Indexing a type value builds a static-array type (`i64[4]`), itself a `meta` value.
			const auto array_type = s.index(s.litType(i64_type), s.litNum(4));
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
			const Shorthand s{ ctx };

			const auto u8_type
				= tsh::getIntegralType(ctx, 8, tsh::IntegralAbstractType::Signedness::Unsigned);
			const auto sum
				= s.binOp(s.litChar('a'), BuiltinBinary::IntegerAdd, s.litNum(1, u8_type));
			ASSERT_EQUAL(sum->expression_type.getType().getKind(), tsh::Kind::Char);
		});
	}

	/** `reusable` marks the first use; `nextUse` shares the inner for subsequent uses. */
	void testReusable() {
		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			const auto first  = s.reusable(s.litNum(5));
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
			const Shorthand s{ ctx };

			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto square_call = s.call(s.ident(square_symbol), s.litNum(5, i64_type));

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

	/** `cast`, `refOf`, `deref` and `move` reshape an operand's type as expected. */
	void testConversionExprs() {
		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			const auto i32_type
				= tsh::getIntegralType(ctx, 32, tsh::IntegralAbstractType::Signedness::Signed);
			const auto i64_sym_type = tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed)
			);

			// A cast carries the requested target type.
			const auto casted = s.cast(s.litNum(5, i32_type), i64_sym_type);
			ASSERT_EQUAL(casted->expression_type.getType().getKind(), tsh::Kind::Integral);
			ASSERT_EQUAL(
				casted->expression_type.getType().as<tsh::IntegralAbstractType>().getSize(), Bits(64)
			);
			ASSERT_EQUAL(dprint(casted), std::string("cast[to=i64](5)"));

			// `refof x` has reference type; s.dereferencing it recovers the (direct) pointee.
			const auto ref = s.refOf(s.litNum(5));
			ASSERT_EQUAL(dprint(ref), std::string("refof(5)"));
			ASSERT_EQUAL(ref->expression_type.getSymbolType().getRefKind(), tsh::ReferenceKind::Ref);

			const auto derefed = s.deref(s.refOf(s.litNum(5)));
			ASSERT_EQUAL(dprint(derefed), std::string("deref(refof(5))"));
			ASSERT_EQUAL(
				derefed->expression_type.getSymbolType().getRefKind(), tsh::ReferenceKind::Direct
			);
			ASSERT_EQUAL(derefed->expression_type.getType().getKind(), tsh::Kind::Integral);

			// `move x` preserves the operand type as a temporary.
			const auto moved = s.move(s.litNum(5));
			ASSERT_EQUAL(dprint(moved), std::string("move(5)"));
			ASSERT_EQUAL(moved->expression_type.getType().getKind(), tsh::Kind::Integral);
			ASSERT_EQUAL(
				moved->expression_type.getValueCategory().getCategory(),
				tsh::PrimaryCategory::Temporary
			);
		});
	}

	/** `defaultValue`, `liftToType`, `blockExpr`, and the list push/pop builders. */
	void testMiscExprs() {
		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);

			// A default value carries the given type.
			const auto zero = s.defaultValue(i64_type);
			ASSERT_EQUAL(zero->expression_type.getType().getKind(), tsh::Kind::Integral);
			ASSERT_EQUAL(dprint(zero), std::string("default_value(const i64)"));
			ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(zero.get()) != nullptr);

			// A block expression wraps a BlockStmt and yields unit.
			const auto block_expr = s.blockExpr({ s.ret(s.litNum(1)), s.expr(s.litNum(2)) });

			ASSERT_EQUAL(dprint(block_expr), std::string("block({\n    return 1;\n    do 2\n}\n)"));
			ASSERT_EQUAL(block_expr->expression_type.getType().getKind(), tsh::Kind::Unit);
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
			const Shorthand s{ ctx };

			// Resolve the `x` field of `point`'s struct type through its type interface.
			const auto x_field = ctx.query<helios::QueryTypeOfSymbol>(point_var)
			                         ->valueOrThrow()
			                         .getType()
			                         .getInterface(ctx)
			                         ->getElementsWithName(base::StrID("x"))
			                         .back()
			                         .getSymbol();

			const auto field_access = s.access(s.ident(point_var), x_field);

			const auto* access_expr = dynamic_cast<const AccessExpr*>(field_access.get());
			ASSERT_TRUE(access_expr != nullptr);
			// The access wires `point` as the base and `x` as the accessed field.
			ASSERT_EQUAL(
				compiler::helios::getIdentifierExprSymID(access_expr->base.ref()).value(), point_var
			);
			ASSERT_EQUAL(access_expr->field, x_field);
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
		const auto [module, scope] = getModule(fs::File(path("test_modules/shorthands/dummy")));
		const auto dummy_symbol    = getChain("dummy", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto i64_sym_type = tsh::SymbolType<>::withDefaults(i64_type);

			// Simple statements wrap their expression operand.
			const auto decl = s.var(dummy_symbol, i64_sym_type, s.litNum(0, i64_type));
			ASSERT_TRUE(
				dynamic_cast<LiteralNumericExpr*>(decl->initial_value.getBox().get()) != nullptr
			);
			ASSERT_EQUAL(decl->type, i64_sym_type);
			ASSERT_EQUAL(decl->helios_symbol, dummy_symbol);

			const auto decl_no_type = s.var(dummy_symbol, s.litNum(0, i64_type));
			ASSERT_TRUE(
				dynamic_cast<LiteralNumericExpr*>(decl_no_type->initial_value.getBox().get())
				!= nullptr
			);
			ASSERT_EQUAL(decl_no_type->type, i64_sym_type);
			ASSERT_EQUAL(decl_no_type->helios_symbol, dummy_symbol);

			const auto decl_no_value = s.var(dummy_symbol, i64_sym_type);
			ASSERT_TRUE(
				dynamic_cast<DefaultValueExpr*>(decl_no_value->initial_value.getBox().get())
				!= nullptr
			);
			ASSERT_EQUAL(decl_no_value->type, i64_sym_type);
			ASSERT_EQUAL(decl_no_value->helios_symbol, dummy_symbol);

			const auto assignment = s.assign(s.ident(dummy_symbol), s.litNum(1, i64_type));
			ASSERT_TRUE(assignment->location_expr.isBox());
			ASSERT_TRUE(assignment->new_value_expr.isBox());

			ASSERT_TRUE(s.ret(s.litNum(1))->value.isBox());
			ASSERT_TRUE(s.expr(s.litBool(true))->expr.isBox());
			ASSERT_TRUE(s.ret()->origin.isGenerated());

			// Body arguments accept a braced list of statements directly.
			const auto loop = s.whileStmt(s.litBool(true), { s.expr(s.litNum(0)), s.ret() });
			ASSERT_TRUE(loop->condition.isBox());
			ASSERT_EQUAL(loop->body.statements.size(), 2UL);

			// if with and without else.
			const auto if_no_else = s.ifStmt(s.litBool(true), { s.ret() });
			ASSERT_EQUAL(if_no_else->then_body.statements.size(), 1UL);
			ASSERT_EQUAL(if_no_else->else_body.statements.size(), 0UL);

			const auto if_else = s.ifStmt(s.litBool(false), { s.ret() }, { s.ret(), s.ret() });
			ASSERT_EQUAL(if_else->then_body.statements.size(), 1UL);
			ASSERT_EQUAL(if_else->else_body.statements.size(), 2UL);

			// block(...) wraps a body as a BlockStmt.
			ASSERT_EQUAL(s.block({ s.ret(), s.ret() })->body.statements.size(), 2UL);

			// An empty braced list is a valid (empty) body.
			ASSERT_EQUAL(s.ifStmt(s.litBool(true), {})->then_body.statements.size(), 0UL);

			// A standalone CodeBlock (e.g. a whole function body) via StmtPack::toCodeBlock.
			const auto body = StmtPack{ s.ret(s.litNum(1)), s.expr(s.litNum(2)) }.toCodeBlock();
			ASSERT_EQUAL(body.statements.size(), 2UL);

			// Box<Derived> -> Box<Stmt> accumulation, wrapped through StmtPack's vector constructor.
			std::vector<Box<Stmt>> collected;
			collected.emplace_back(s.ret(s.litNum(7)));
			ASSERT_EQUAL(StmtPack{ std::move(collected) }.toCodeBlock().statements.size(), 1UL);
		});
	}

	/**
	 * `coerce` forcefully coerces an expression to a target type. A widening promotion inserts a
	 * cast, while an identity coercion (target == source) is an identity transformation.
	 */
	void testCoerce() {
		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			const auto i32_type
				= tsh::getIntegralType(ctx, 32, tsh::IntegralAbstractType::Signedness::Signed);
			const auto i64_sym_type = tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed)
			);

			// Widening i32 -> i64 inserts a numeric promotion cast.
			const auto promoted = s.coerce(s.litNum(5, i32_type), i64_sym_type);
			ASSERT_TRUE(dynamic_cast<const CastExpr*>(promoted.get()) != nullptr);
			ASSERT_EQUAL(promoted->expression_type.getType().getKind(), tsh::Kind::Integral);
			ASSERT_EQUAL(
				promoted->expression_type.getType().as<tsh::IntegralAbstractType>().getSize(),
				Bits(64)
			);

			// Coercing to the operand's own type is an identity coercion: the operand is returned
			// unwrapped (no cast node is introduced).
			const auto literal   = s.litNum(5, i32_type);
			const auto self_type = literal->expression_type.getSymbolType();
			const auto unchanged = s.coerce(s.litNum(5, i32_type), self_type);
			ASSERT_TRUE(dynamic_cast<const LiteralNumericExpr*>(unchanged.get()) != nullptr);
			ASSERT_EQUAL(dprint(unchanged), std::string("5"));
		});
	}

	/**
	 * `prepToPassSelf` reshapes an expression so it can be passed as `self` to a method of its
	 * type: complex types are passed by reference, simple types by value. It is idempotent — an
	 * operand already in the right shape is returned untouched. `ident`/access on a real class
	 * field needs a loaded module (`Point`).
	 */
	void testPrepToPassSelf() {
		const auto [module, scope] = getModule(fs::File(path("test_modules/shorthands/access")));
		const auto point_var       = getChain("point", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			const auto i64_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);

			// Simple + direct: methods on simple types take a copy, so it is passed as-is.
			const auto simple_direct = s.prepToPassSelf(s.litNum(5, i64_type));
			ASSERT_TRUE(dynamic_cast<const LiteralNumericExpr*>(simple_direct.get()) != nullptr);

			// Simple + reference: dereffed back down to a value.
			const auto simple_ref = s.prepToPassSelf(s.refOf(s.litNum(5, i64_type)));
			ASSERT_TRUE(dynamic_cast<const DerefExpr*>(simple_ref.get()) != nullptr);

			// Complex (a class) + direct: reffed up so it can be passed as `self`.
			auto       point_ident = s.ident(point_var);
			const auto obj_direct  = s.prepToPassSelf(std::move(point_ident));
			ASSERT_TRUE(dynamic_cast<const RefOfExpr*>(obj_direct.get()) != nullptr);

			// Complex + already a reference: passed as-is, not wrapped in a second `refof`.
			const auto  obj_ref  = s.prepToPassSelf(s.refOf(s.ident(point_var)));
			const auto* ref_expr = dynamic_cast<const RefOfExpr*>(obj_ref.get());
			ASSERT_TRUE(ref_expr != nullptr);
			ASSERT_TRUE(dynamic_cast<const IdentifierExpr*>(ref_expr->inner.get()) != nullptr);
		});
	}

	/** Trees built purely from shorthands carry generated origins. */
	void testGeneratedOrigins() {
		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			const auto leaf = s.litNum(1);
			ASSERT_TRUE(leaf->origin.isGenerated());

			const auto composite = s.binOp(s.litNum(1), BuiltinBinary::IntegerAdd, s.litNum(2));
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
		const auto [module, scope] = getModule(fs::File(path("test_modules/shorthands/dummy")));
		const auto dummy_symbol    = getChain("dummy", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			const Shorthand s{ ctx };

			// Create a compiler-generated HOUT Expression,
			// which is an IdentifierExpr pointing to the dummy variable.
			auto dummy_ident = s.ident(dummy_symbol);

			// Get a genuine, positioned (non-generated) origin taken from dummy's PST declaration.
			const auto pst_origin
				= pstOrigin(compiler::helios::maybeSymbolPst(dummy_symbol).value().unlock(ctx));
			ASSERT_TRUE(!pst_origin.isGenerated());
			ASSERT_HAS_VALUE(pst_origin.getStablePosition());

			// Check that origin is generated before override.
			ASSERT_TRUE(dummy_ident->origin.isGenerated());

			// Override the origin (this isn't strictly correct, in terms of constructing valid
			// HOUT, but OK for the test).
			dummy_ident = withOrigin(pst_origin, std::move(dummy_ident));

			// The HOUT Expr now carries the specified origin.
			ASSERT_TRUE(!dummy_ident->origin.isGenerated());
			ASSERT_HAS_VALUE(dummy_ident->origin.getStablePosition());
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
