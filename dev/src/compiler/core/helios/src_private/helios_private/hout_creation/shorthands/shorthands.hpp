#pragma once

/**
 * @file shorthands.hpp
 * @brief DSL-like helpers for constructing HOUT Expr and Stmt trees.
 *
 * Regular, handwritten HOUT objects often take up way too many lines. This small library
 * alleviates this problem by storing and deducing some data (query context and element origin)
 * and makes HOUT more legible and easier to write.
 *
 * Usage:
 * @code
 *   using namespace compiler::helios::code::shorthands;  // imports Shorthand, withOrigin, StmtPack
 *
 *   Shorthand s{ctx};
 *   Box<Expr> e = s.binOp(s.litNum(40), BuiltinBinary::IntegerAdd, s.litNum(2));
 * @endcode
 */

#include <ctv/numeric_value.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/lang_primitives.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/hout_creation/definition_generation/copy_constructors.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/vector_utils.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>

#include <type_traits>
#include <utility>
#include <vector>

namespace compiler::helios::code::shorthands {

	/*******************
	 *   AMBIENT CTX   *
	 *******************/

	namespace internal {
		/**
		 * @brief Arithmetic types that denote a numeric literal.
		 */
		template<typename T>
		concept NumericLiteralValue
			= base::IS_VARIANT_MEMBER_V<T, numeric_value::NumericValue::Storage>;
	}

	/***********************
	 *   ORIGIN OVERRIDE   *
	 ***********************/

	/**
	 * @brief Overrides the origin of an already-built expression (or statement) and returns it.
	 *
	 * Builders always use `generatedOrigin()`, which is correct for generated-only HOUT (the
	 * main use case of this library). When a node must carry a specific origin instead — e.g. when
	 * lowering PST, where diagnostics must point back at the source — wrap the builder call:
	 * @code
	 *   pst_origin = multiplePstOriginOrdered({lhs_pst, rhs_pst})
	 *   withOrigin(pst_origin, s.binOp(a, BuiltinBinary::IntegerLt, b));
	 * @endcode
	 */
	template<typename E>
	requires(std::is_base_of_v<Expr, E> || std::is_base_of_v<Stmt, E>)
	Box<E> withOrigin(ElementOrigin origin, Box<E> expr) {
		expr->origin = origin;
		return expr;
	}

	/**
	 * @brief The body argument of `ifStmt` / `whileStmt` / `block`.
	 *
	 * Besides a `CodeBlock` or a `std::vector<Box<Stmt>>`, it accepts a braced list of
	 * statements — which is the point: it lets you write `ifStmt(cond, {s1, s2})`. A braced
	 * list cannot go through `std::initializer_list` here (its elements are const, and `Box` is
	 * move-only), so the enabling piece is the variadic constructor, which brace-initialization
	 * falls back to when no `initializer_list` constructor applies.
	 */
	class StmtPack final {
		std::vector<Box<Stmt>> statements;

	public:
		StmtPack() = default;

		/** @brief From a braced list / pack of statements: enables `{ s1, s2, ... }`. */
		template<typename... Stmts>
		requires(sizeof...(Stmts) >= 1 && (std::is_constructible_v<Box<Stmt>, Stmts &&> && ...))
		explicit(false) StmtPack(Stmts&&... stmts):
			  statements(base::packToVector<Box<Stmt>>(std::forward<Stmts>(stmts)...)) {}

		explicit(false) StmtPack(std::vector<Box<Stmt>> statements):
			  statements(std::move(statements)) {}

		explicit(false) StmtPack(CodeBlock block): statements(std::move(block.statements)) {}

		/** @brief Consumes the body, yielding its `CodeBlock`. */
		CodeBlock toCodeBlock() && { return CodeBlock{ std::move(statements) }; }
	};

	class Shorthand final {
		Ref<query::Context> ctx;

	public:
		explicit Shorthand(query::Context& ctx): ctx(&ctx) {}

		Shorthand(const Shorthand&)            = delete;
		Shorthand(Shorthand&&)                 = delete;
		Shorthand& operator=(const Shorthand&) = delete;
		Shorthand& operator=(Shorthand&&)      = delete;

		/****************
		 *   LITERALS   *
		 ****************/

		/** @brief The unit literal `()`. */
		[[nodiscard]]
		Box<LiteralUnitExpr> litUnit() const {
			return makeBox<LiteralUnitExpr>(*ctx, generatedOrigin());
		}

		/** @brief A numeric literal from an explicit `numeric_value::NumericValue`. */
		[[nodiscard]]
		Box<LiteralNumericExpr> litNum(numeric_value::NumericValue value) const {
			return makeBox<LiteralNumericExpr>(*ctx, generatedOrigin(), value);
		}

		/**
		 * @brief Convenience numeric literal from a plain C++ arithmetic value.
		 * @note The type is deduced by `NumericValue::createMinimized` — the smallest type that
		 * fits (currently `i32`/`u32`/`i64`/`u64` for integrals). If you need a specific type (e.g.
		 * `u64`), use the `(value, type)` overload or pass a pre-typed @ref
		 * numeric_value::NumericValue.
		 */
		template<internal::NumericLiteralValue T>
		[[nodiscard]]
		Box<LiteralNumericExpr> litNum(T value) const {
			return litNum(numeric_value::NumericValue::createMinimized(value));
		}

		/** @brief A numeric literal of a specific type. Panics if `value` does not fit `type`. */
		template<internal::NumericLiteralValue T>
		[[nodiscard]]
		Box<LiteralNumericExpr> litNum(T value, const tsh::AbstractType& type) const {
			return litNum(numeric_value::NumericValue::createOfType(type, value)
			                  .expect("litNum: value does not fit the requested type"));
		}

		/** @brief A boolean literal. */
		[[nodiscard]]
		Box<LiteralBoolExpr> litBool(bool value) const {
			return makeBox<LiteralBoolExpr>(*ctx, generatedOrigin(), value);
		}

		/** @brief A character literal. */
		[[nodiscard]]
		Box<LiteralCharExpr> litChar(char value) const {
			return makeBox<LiteralCharExpr>(*ctx, generatedOrigin(), value);
		}

		/** @brief A string literal, as char slice. */
		[[nodiscard]]
		Box<LiteralStringExpr> litStr(base::StrID value) const {
			return makeBox<LiteralStringExpr>(*ctx, generatedOrigin(), value);
		}

		/** @brief A string literal, as String. */
		[[nodiscard]]
		Box<Expr> litStrObj(base::StrID value) const {
			const SymID callee_sym
				= ctx->query<QueryLanguagePrimitiveSymID>({ LanguagePrimitive::StringifyStr })
			          ->valueOrThrow();
			return call(ident(callee_sym), litStr(value));
		}

		/** @brief A type literal (e.g. `i32`, `bool`), carrying `type` as its value. */
		[[nodiscard]]
		Box<LiteralTypeExpr> litType(tsh::AbstractType type) const {
			return makeBox<LiteralTypeExpr>(*ctx, generatedOrigin(), type);
		}

		/** @brief An identifier expression referring to `symbol`. */
		[[nodiscard]]
		Box<IdentifierExpr> ident(SymID symbol) const {
			return makeBox<IdentifierExpr>(*ctx, generatedOrigin(), symbol);
		}

		/*****************
		 *   REUSABLES   *
		 *****************/

		/**
		 * @brief Wraps `inner` so it can be evaluated once and its result reused
		 * (e.g. `b` in the chain comparison `a < b < c`).
		 *
		 * @note You are likely to need to save the result of this function in a variable,
		 * as you will need to invoke the `->nextUse()` method.
		 */
		[[nodiscard]]
		Box<ReusableExpr> reusable(Box<Expr> inner) const {
			return makeBox<ReusableExpr>(*ctx, std::move(inner));
		}

		/*************************
		 *   REGULAR COMPOSITE   *
		 *************************/

		/**
		 * @brief Defines the variadic pack overload of a builder that already has a
		 * `std::vector<Box<Expr>>` overload, forwarding the pack through `base::packToVector`.
		 *
		 * `min_arity` is the least number of expressions the builder accepts.
		 * (`call` does not benefit here — its leading `callee` parameter gives it a different shape.)
		 */
#define HOUT_EXPR_PACK_OVERLOAD(builder, ReturnType, min_arity)                                  \
	template<typename... Exprs>                                                                  \
	requires(                                                                                    \
		sizeof...(Exprs) >= (min_arity) && (std::is_constructible_v<Box<Expr>, Exprs &&> && ...) \
	) [[nodiscard]]                                                                              \
	Box<ReturnType> builder(Exprs&&... exprs) const {                                            \
		return builder(base::packToVector<Box<Expr>>(std::forward<Exprs>(exprs)...));            \
	}

		/** @brief A binary operator `lhs op rhs`. */
		[[nodiscard]]
		Box<BinaryOperatorExpr> binOp(Box<Expr> lhs, BuiltinBinary op, Box<Expr> rhs) const {
			return makeBox<BinaryOperatorExpr>(
				*ctx, generatedOrigin(), op, std::move(lhs), std::move(rhs)
			);
		}

		/** @brief A unary operator `op operand`. */
		[[nodiscard]]
		Box<UnaryOperatorExpr> unOp(BuiltinUnary op, Box<Expr> operand) const {
			return makeBox<UnaryOperatorExpr>(*ctx, generatedOrigin(), op, std::move(operand));
		}

		/** @brief A ternary `if condition then if_true else if_false`. */
		[[nodiscard]]
		Box<TernaryOperatorExpr> ternary(Box<Expr> condition, Box<Expr> if_true, Box<Expr> if_false)
			const {
			return makeBox<TernaryOperatorExpr>(
				*ctx, generatedOrigin(), std::move(condition), std::move(if_true), std::move(if_false)
			);
		}

		/** @brief A tuple value `(elements, ...)`. */
		[[nodiscard]]
		Box<TupleExpr> tuple(std::vector<Box<Expr>> elements) const {
			return makeBox<TupleExpr>(*ctx, generatedOrigin(), std::move(elements));
		}

		/** @brief A tuple value from a pack of elements. */
		HOUT_EXPR_PACK_OVERLOAD(tuple, TupleExpr, 2)

		/** @brief A variant type constructor `(subtypes | ...)`. */
		[[nodiscard]]
		Box<VariantTypeConstructorExpr> variant(std::vector<Box<Expr>> subtypes) const {
			return makeBox<VariantTypeConstructorExpr>(*ctx, generatedOrigin(), std::move(subtypes));
		}

		/** @brief A variant type constructor from a pack of subtypes. */
		HOUT_EXPR_PACK_OVERLOAD(variant, VariantTypeConstructorExpr, 2)

		/**
		 * @brief An aggregate value (struct/class/tuple) built field-by-field, in place.
		 * @p values holds one expression per field, in declaration order.
		 */
		[[nodiscard]]
		Box<CreateAggregateExpr> createAggregate(tsh::AbstractType type, std::vector<Box<Expr>> values)
			const {
			return makeBox<CreateAggregateExpr>(*ctx, generatedOrigin(), type, std::move(values));
		}

		/** @brief A fixed-size static array value built element-by-element, in place. */
		[[nodiscard]]
		Box<CreateArrayExpr> createArray(tsh::SymbolType<> element_type, std::vector<Box<Expr>> values)
			const {
			return makeBox<CreateArrayExpr>(
				*ctx, generatedOrigin(), element_type, std::move(values)
			);
		}

		/** @brief A field access `base.field`. */
		[[nodiscard]]
		Box<AccessExpr> access(Box<Expr> base, SymID field) const {
			return makeBox<AccessExpr>(*ctx, generatedOrigin(), std::move(base), field);
		}

		/** @brief An index expression `base[idx]`. */
		[[nodiscard]]
		Box<IndexExpr> index(Box<Expr> base, Box<Expr> idx) const {
			return makeBox<IndexExpr>(*ctx, generatedOrigin(), std::move(base), std::move(idx));
		}

		/** @brief A call `callee(arguments...)`. */
		[[nodiscard]]
		Box<CallExpr> call(Box<Expr> callee, std::vector<Box<Expr>> arguments) const {
			return makeBox<CallExpr>(
				*ctx, generatedOrigin(), std::move(callee), std::move(arguments)
			);
		}

		/** @brief A call from a callee and a pack of arguments. */
		template<typename... Args>
		requires(std::is_constructible_v<Box<Expr>, Args &&> && ...) [[nodiscard]]
		Box<CallExpr> call(Box<Expr> callee, Args&&... arguments) const {
			return call(
				std::move(callee), base::packToVector<Box<Expr>>(std::forward<Args>(arguments)...)
			);
		}

		/** @brief A sequence `expressions, ...` (comma operator); the last one is the result. */
		[[nodiscard]]
		Box<SequenceExpr> seq(std::vector<Box<Expr>> expressions) const {
			CORE_ASSERT(
				!expressions.empty(), "seq(): a SequenceExpr requires at least one expression"
			);
			return makeBox<SequenceExpr>(*ctx, generatedOrigin(), std::move(expressions));
		}

		/** @brief A sequence from a pack of expressions. */
		HOUT_EXPR_PACK_OVERLOAD(seq, SequenceExpr, 2)

		/** @brief A chain comparison, the logical AND of each comparison (e.g. `a < b < c`). */
		[[nodiscard]]
		Box<ChainComparisonExpr> chainCmp(std::vector<Box<Expr>> comparisons) const {
			CORE_ASSERT(
				!comparisons.empty(),
				"chainCmp(): a ChainComparisonExpr requires at least one comparison"
			);
			return makeBox<ChainComparisonExpr>(*ctx, generatedOrigin(), std::move(comparisons));
		}

		/** @brief A chain comparison from a pack of comparisons. */
		HOUT_EXPR_PACK_OVERLOAD(chainCmp, ChainComparisonExpr, 1)

		/** @brief A cast of `source` to `target_type`. */
		[[nodiscard]]
		Box<CastExpr> cast(Box<Expr> source, tsh::SymbolType<> target_type) const {
			return makeBox<CastExpr>(*ctx, generatedOrigin(), std::move(source), target_type);
		}

		/** @brief A reference creation `refof inner`. */
		[[nodiscard]]
		Box<RefOfExpr> refOf(Box<Expr> inner) const {
			return makeBox<RefOfExpr>(*ctx, generatedOrigin(), std::move(inner));
		}

		/** @brief An explicit move `move inner`. Named `moveOf` to avoid clashing with `std::move`. */
		[[nodiscard]]
		Box<MoveExpr> moveOf(Box<Expr> inner) const {
			return makeBox<MoveExpr>(*ctx, generatedOrigin(), std::move(inner));
		}

		/** @brief A dereference `deref inner`. */
		[[nodiscard]]
		Box<DerefExpr> deref(Box<Expr> inner) const {
			return makeBox<DerefExpr>(*ctx, generatedOrigin(), std::move(inner));
		}

		/*********************
		 *   MISCELLANEOUS   *
		 *********************/

		/** @brief A default value of `type`. */
		[[nodiscard]]
		Box<DefaultValueExpr> defaultValue(tsh::AbstractType type) const {
			return makeBox<DefaultValueExpr>(*ctx, generatedOrigin(), type);
		}

		/** @brief A compile-time lift of `value` to a type. */
		[[nodiscard]]
		Box<LiftToTypeExpr> liftToType(Box<Expr> value) const {
			return makeBox<LiftToTypeExpr>(*ctx, generatedOrigin(), std::move(value));
		}

		/************
		 *   LIST   *
		 ************/

		// @note ListPushExpr / ListPopExpr are the two HOUT nodes whose constructors take no
		// query::Context, so these builders do not read the stored context.

		/** @brief A push `list += element`. */
		[[nodiscard]]
		static Box<ListPushExpr> listPush(Box<Expr> list, Box<Expr> element) {
			return makeBox<ListPushExpr>(generatedOrigin(), std::move(list), std::move(element));
		}

		/** @brief A pop of `count` elements from `list`. */
		[[nodiscard]]
		static Box<ListPopExpr> listPop(Box<Expr> list, Box<Expr> count) {
			return makeBox<ListPopExpr>(generatedOrigin(), std::move(list), std::move(count));
		}

		/******************
		 *   STATEMENTS   *
		 ******************/

		// @note Unlike expressions, Stmt constructors take no query::Context (they store, they
		// don't type-check), so these builders never read the stored context — only *expression*
		// builders do.

		/** @brief A bare block statement `{ body }`. Accepts a braced list: `block({s1, s2})`. */
		[[nodiscard]]
		static Box<BlockStmt> block(StmtPack body) {
			return makeBox<BlockStmt>(generatedOrigin(), std::move(body).toCodeBlock());
		}

		/** @brief A variable declaration `var/let symbol: type = init;`. */
		[[nodiscard]]
		static Box<VariableStmt> var(SymID symbol, tsh::SymbolType<> type, Box<Expr> init) {
			return makeBox<VariableStmt>(generatedOrigin(), std::move(init), type, symbol);
		}

		/** @brief A variable declaration without type `var/let symbol = init;`. */
		[[nodiscard]]
		static Box<VariableStmt> var(SymID symbol, Box<Expr> init) {
			return makeBox<VariableStmt>(
				generatedOrigin(),
				std::move(init),
				tsh::SymbolType<>::withDefaults(init->expression_type.getType()),
				symbol
			);
		}

		/**
		 * @brief A variable declaration without init `var/let symbol: type;`.
		 * @note Unlike the other `var` methods, this one isn't static, as it requires ctx.
		 */
		[[nodiscard]]
		Box<VariableStmt> var(SymID symbol, tsh::SymbolType<> type) const {
			return makeBox<VariableStmt>(
				generatedOrigin(), defaultValue(type.getType()), type, symbol
			);
		}

		/** @brief An assignment `location = value;`. */
		[[nodiscard]]
		static Box<AssignmentStmt> assign(Box<Expr> location, Box<Expr> value) {
			return makeBox<AssignmentStmt>(generatedOrigin(), std::move(location), std::move(value));
		}

		/** @brief A `return value;`. */
		[[nodiscard]]
		static Box<ReturnStmt> ret(Box<Expr> value) {
			return makeBox<ReturnStmt>(generatedOrigin(), std::move(value));
		}

		/** @brief A `return;` (no value). */
		[[nodiscard]]
		static Box<VoidReturnStmt> ret() {
			return makeBox<VoidReturnStmt>(generatedOrigin());
		}

		/** @brief An expression statement `expr;` (result discarded). */
		[[nodiscard]]
		static Box<ExprStmt> expr(Box<Expr> expr) {
			return makeBox<ExprStmt>(generatedOrigin(), std::move(expr));
		}

		/** @brief An `if (condition) { then_body }`.
		 * Accepts a braced list: `ifStmt(cond, {s1, s2})`. */
		[[nodiscard]]
		static Box<IfStmt> ifStmt(Box<Expr> condition, StmtPack then_body) {
			return makeBox<IfStmt>(
				generatedOrigin(), std::move(condition), std::move(then_body).toCodeBlock()
			);
		}

		/** @brief An `if (condition) { then_body } else { else_body }`.
		 * Accepts braced lists: `ifStmt(cond, {s1, s2}, {s3, s4})`.*/
		[[nodiscard]]
		static Box<IfStmt> ifStmt(Box<Expr> condition, StmtPack then_body, StmtPack else_body) {
			return makeBox<IfStmt>(
				generatedOrigin(),
				std::move(condition),
				std::move(then_body).toCodeBlock(),
				std::move(else_body).toCodeBlock()
			);
		}

		/** @brief A `while (condition) { body }`.
		 * Accepts a braced list: `whileStmt(cond, {s1, s2})`. */
		[[nodiscard]]
		static Box<WhileStmt> whileStmt(Box<Expr> condition, StmtPack body) {
			return makeBox<WhileStmt>(
				generatedOrigin(), std::move(condition), std::move(body).toCodeBlock()
			);
		}

		/*****************
		 *   AUXILIARY   *
		 *****************/

		/**
		 * @brief Forcefully coerces an expression to the target type.
		 * @note Assumes the coercion will succeed.
		 */
		[[nodiscard]]
		Box<Expr> coerce(Box<Expr> expr, const tsh::SymbolType<> target_type) const {
			auto coercion = canCoerce(*ctx, expr->expression_type, target_type);
			return coercion.valueOrThrow().coerce(*ctx, std::move(expr));
		}

		/**
		 * @brief Coerces the expression to one which can be passed as self to a method of its type.
		 *
		 * Essentially enforces that the expression is a reference. Unless the type is a simple
		 * type, in which case it is enforced by-value instead.
		 */
		[[nodiscard]]
		Box<Expr> prepToPassSelf(Box<Expr> expr) const {
			const auto new_origin = expr->origin.generatedFrom();
			// If the expr is not a simple type, we must call its method on a reference.
			if (not expr->expression_type.getType().isSimple()
			    and expr->expression_type.getSymbolType().getRefKind() != tsh::ReferenceKind::Ref) {
				expr = withOrigin(new_origin, refOf(std::move(expr)));
			}
			// But also if the accessed field is a reference to a simple type, we must deref it.
			if (expr->expression_type.getType().isSimple()
			    and expr->expression_type.getSymbolType().getRefKind()
			            != tsh::ReferenceKind::Direct) {
				expr = withOrigin(new_origin, deref(std::move(expr)));
			}
			return expr;
		}

		/**
		 * @brief Build a HOUT expression producing a copy of `source`.
		 *
		 * Used by the `copy` operator and by generated copy constructors.
		 * - Trivially-copyable sources are byte-copied.
		 * - `box T` is deep-copied
		 * - other non-trivial types are copied via their copy constructor.
		 */
		[[nodiscard]]
		Box<Expr> copy(Box<Expr> source) const {
			const tsh::SymbolType<> type = source->expression_type.getSymbolType();
			if (type.isTriviallyCopyable(*ctx)) return source;

			// A `box T` is deep-copied. Allocate a new box holding a copy of the pointee
			// `box(<copy of *source>)`. For a trivially-copyable pointee this collapses to
			// `box(*source)`.
			if (type.getRefKind() == tsh::ReferenceKind::Box) {
				// Produce a copy of the underlying type.
				auto pointee_copy = copy(deref(std::move(source)));
				// Now wrap it in a heap allocation.
				return makeBoxAllocCall(*ctx, generatedOrigin(), std::move(pointee_copy));
			}

			// Now we have a direct value which should be copied.
			const auto abstract_type = type.getType();
			CORE_ASSERT(
				abstract_type.getKind() == tsh::Kind::Class
					or abstract_type.getKind() == tsh::Kind::StaticArray
					or abstract_type.getKind() == tsh::Kind::Tuple
					or abstract_type.getKind() == tsh::Kind::DynamicArray,
				"Tried to generate a copy constructor for a type which shouldn't need it"
			);

			const SymID copy_sym = defgen::copyConstructorSymForType(*ctx, abstract_type);
			return call(ident(copy_sym), refOf(std::move(source)));
		}
	};
}

#undef HOUT_EXPR_PACK_OVERLOAD
