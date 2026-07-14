#pragma once

/**
 * @file shorthands.hpp
 * @brief DSL-like helpers for constructing HOUT @ref compiler::helios::code::Expr trees.
 *
 * Regular, handwritten HOUT objects often take up way too many lines. This small library
 * alleviates this problem by storing and deducing some data (query context and element origin)
 * and makes HOUT more legible and easier to write.
 *
 * Usage:
 * @code
 *   using namespace compiler::helios::code::shorthands;  // or rename to `sh::`
 *
 *   Shorthand sh{ctx};  // RAII-captures `ctx` for the enclosing scope.
 *   Box<Expr> e = binOp(litNum(40), BuiltinBinary::IntegerAdd, litNum(2));
 *   // Captured reference to `ctx` disappears when `sh` goes out of scope.
 * @endcode
 *
 * @warning A builder must only be called while a @ref Shorthand guard is alive on the current
 * thread; otherwise the ambient context is null, and the call panics (see @ref Shorthand).
 */

#include <ctv/numeric_value.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/except/exceptions.hpp>
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
		 * @brief The query context which the shorthand builders read from, for the current thread.
		 */
		inline thread_local base::MRef<query::Context> active_ctx{};

		/**
		 * @brief The active context, or a panic (via `MRef::operator*`) if no @ref Shorthand guard
		 * is currently in scope on this thread.
		 */
		inline query::Context& ctx() { return *active_ctx; }

		/**
		 * @brief Moves a pack of `Box<Expr>` into a vector, preserving order.
		 */
		template<typename... Exprs>
		requires(std::is_constructible_v<Box<Expr>, Exprs &&> && ...)
		std::vector<Box<Expr>> packToVector(Exprs&&... exprs) {
			std::vector<Box<Expr>> result;
			result.reserve(sizeof...(exprs));
			(result.emplace_back(std::forward<Exprs>(exprs)), ...);
			return result;
		}
	}

	/**
	 * @brief RAII guard that makes `ctx` the ambient context for the shorthand builders, for as
	 * long as the guard is alive on the current thread.
	 *
	 * @par Why save/restore rather than set/null?
	 * A guard looks local, but building a single expression can transitively construct *another*
	 * guard on the same thread. HOUT node constructors compute their own type, and that often
	 * issues a query; for example, `IdentifierExpr` runs `QueryTypeOfSymbol`.
	 *
	 * Now consider building a call to a function whose return type the user left implicit.
	 * Resolving that type forces the callee's own body/HOUT to be generated and examined, which may
	 * itself use this library — establishing a nested guard, typically with a *different* `Context`
	 * (each query node gets its own; `Context` is non-copyable).
	 */
	class Shorthand final {
		MRef<query::Context> previous{};

	public:
		explicit Shorthand(query::Context& ctx): previous(internal::active_ctx) {
			internal::active_ctx = &ctx;
		}

		~Shorthand() { internal::active_ctx = previous; }

		Shorthand(const Shorthand&)            = delete;
		Shorthand(Shorthand&&)                 = delete;
		Shorthand& operator=(const Shorthand&) = delete;
		Shorthand& operator=(Shorthand&&)      = delete;
	};

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
	 *   withOrigin(pst_origin, binOp(a, BuiltinBinary::IntegerLt, b));
	 * @endcode
	 */
	template<typename E>
	requires(std::is_base_of_v<Expr, E> || std::is_base_of_v<Stmt, E>)
	Box<E> withOrigin(ElementOrigin origin, Box<E> expr) {
		expr->origin = std::move(origin);
		return expr;
	}

	/****************
	 *   LITERALS   *
	 ****************/

	/** @brief The unit literal `()`. */
	inline Box<LiteralUnitExpr> litUnit() {
		return makeBox<LiteralUnitExpr>(internal::ctx(), generatedOrigin());
	}

	/** @brief A numeric literal from an explicit `numeric_value::NumericValue`. */
	inline Box<LiteralNumericExpr> litNum(numeric_value::NumericValue value) {
		return makeBox<LiteralNumericExpr>(internal::ctx(), generatedOrigin(), std::move(value));
	}

	/**
	 * @brief Convenience numeric literal from a plain C++ arithmetic value.
	 * @note The type is deduced by `NumericValue::createMinimized` — the smallest type that fits
	 * (currently `i32`/`u32`/`i64`/`u64` for integrals). If you need a specific type (e.g. `u64`),
	 * use the `(value, type)` overload or pass a pre-typed @ref numeric_value::NumericValue.
	 */
	template<typename T>
	requires(std::is_arithmetic_v<T>) Box<LiteralNumericExpr> litNum(T value) {
		return litNum(numeric_value::NumericValue::createMinimized(value));
	}

	/** @brief A numeric literal of a specific type. Panics if `value` does not fit `type`. */
	template<typename T>
	requires(std::is_arithmetic_v<T>)
	Box<LiteralNumericExpr> litNum(T value, const tsh::AbstractType& type) {
		return litNum(numeric_value::NumericValue::createOfType(type, value)
		                  .expect("litNum: value does not fit the requested type"));
	}

	/** @brief A boolean literal. */
	inline Box<LiteralBoolExpr> litBool(bool value) {
		return makeBox<LiteralBoolExpr>(internal::ctx(), generatedOrigin(), value);
	}

	/** @brief A character literal. */
	inline Box<LiteralCharExpr> litChar(char value) {
		return makeBox<LiteralCharExpr>(internal::ctx(), generatedOrigin(), value);
	}

	/** @brief A string literal. */
	inline Box<LiteralStringExpr> litStr(base::StrID value) {
		return makeBox<LiteralStringExpr>(internal::ctx(), generatedOrigin(), value);
	}

	/** @brief A type literal (e.g. `i32`, `bool`), carrying `type` as its value. */
	inline Box<LiteralTypeExpr> litType(tsh::AbstractType type) {
		return makeBox<LiteralTypeExpr>(internal::ctx(), generatedOrigin(), std::move(type));
	}

	/** @brief An identifier expression referring to `symbol`. */
	inline Box<IdentifierExpr> ident(SymID symbol) {
		return makeBox<IdentifierExpr>(internal::ctx(), generatedOrigin(), symbol);
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
	inline Box<ReusableExpr> reusable(Box<Expr> inner) {
		return makeBox<ReusableExpr>(internal::ctx(), std::move(inner));
	}

	/*************************
	 *   REGULAR COMPOSITE   *
	 *************************/

	/** @brief A binary operator `lhs op rhs`. */
	inline Box<BinaryOperatorExpr> binOp(Box<Expr> lhs, BuiltinBinary op, Box<Expr> rhs) {
		return makeBox<BinaryOperatorExpr>(
			internal::ctx(), generatedOrigin(), op, std::move(lhs), std::move(rhs)
		);
	}

	/** @brief A unary operator `op operand`. */
	inline Box<UnaryOperatorExpr> unOp(BuiltinUnary op, Box<Expr> operand) {
		return makeBox<UnaryOperatorExpr>(
			internal::ctx(), generatedOrigin(), op, std::move(operand)
		);
	}

	/** @brief A ternary `if condition then if_true else if_false`. */
	inline Box<TernaryOperatorExpr> ternary(
		Box<Expr> condition, Box<Expr> if_true, Box<Expr> if_false
	) {
		return makeBox<TernaryOperatorExpr>(
			internal::ctx(),
			generatedOrigin(),
			std::move(condition),
			std::move(if_true),
			std::move(if_false)
		);
	}

	/** @brief A tuple value `(elements, ...)`. */
	inline Box<TupleExpr> tuple(std::vector<Box<Expr>> elements) {
		return makeBox<TupleExpr>(internal::ctx(), generatedOrigin(), std::move(elements));
	}

	/** @brief A tuple value from a pack of elements. */
	template<typename... Exprs>
	requires(std::is_constructible_v<Box<Expr>, Exprs &&> && ...)
	Box<TupleExpr> tuple(Exprs&&... elements) {
		return tuple(internal::packToVector(std::forward<Exprs>(elements)...));
	}

	/** @brief A variant type constructor `(subtypes | ...)`. */
	inline Box<VariantTypeConstructorExpr> variant(std::vector<Box<Expr>> subtypes) {
		return makeBox<VariantTypeConstructorExpr>(
			internal::ctx(), generatedOrigin(), std::move(subtypes)
		);
	}

	/** @brief A variant type constructor from a pack of subtypes. */
	template<typename... Exprs>
	requires(std::is_constructible_v<Box<Expr>, Exprs &&> && ...)
	Box<VariantTypeConstructorExpr> variant(Exprs&&... subtypes) {
		return variant(internal::packToVector(std::forward<Exprs>(subtypes)...));
	}

	/** @brief A field access `base.field`. */
	inline Box<AccessExpr> access(Box<Expr> base, SymID field) {
		return makeBox<AccessExpr>(internal::ctx(), generatedOrigin(), std::move(base), field);
	}

	/** @brief An index expression `base[idx]`. */
	inline Box<IndexExpr> index(Box<Expr> base, Box<Expr> idx) {
		return makeBox<IndexExpr>(
			internal::ctx(), generatedOrigin(), std::move(base), std::move(idx)
		);
	}

	/** @brief A call `callee(arguments...)`. */
	inline Box<CallExpr> call(Box<Expr> callee, std::vector<Box<Expr>> arguments) {
		return makeBox<CallExpr>(
			internal::ctx(), generatedOrigin(), std::move(callee), std::move(arguments)
		);
	}

	/** @brief A call from a callee and a pack of arguments. */
	template<typename... Args>
	requires(std::is_constructible_v<Box<Expr>, Args &&> && ...)
	Box<CallExpr> call(Box<Expr> callee, Args&&... arguments) {
		return call(std::move(callee), internal::packToVector(std::forward<Args>(arguments)...));
	}

	/** @brief A sequence `expressions, ...` (comma operator); the last one is the result. */
	inline Box<SequenceExpr> seq(std::vector<Box<Expr>> expressions) {
		CORE_ASSERT(!expressions.empty(), "seq(): a SequenceExpr requires at least one expression");
		return makeBox<SequenceExpr>(internal::ctx(), generatedOrigin(), std::move(expressions));
	}

	/** @brief A sequence from a pack of expressions. */
	template<typename... Exprs>
	requires(std::is_constructible_v<Box<Expr>, Exprs &&> && ...)
	Box<SequenceExpr> seq(Exprs&&... expressions) {
		return seq(internal::packToVector(std::forward<Exprs>(expressions)...));
	}

	/** @brief A chain comparison, the logical AND of each comparison (e.g. `a < b < c`). */
	inline Box<ChainComparisonExpr> chainCmp(std::vector<Box<Expr>> comparisons) {
		CORE_ASSERT(
			!comparisons.empty(),
			"chainCmp(): a ChainComparisonExpr requires at least one comparison"
		);
		return makeBox<ChainComparisonExpr>(
			internal::ctx(), generatedOrigin(), std::move(comparisons)
		);
	}

	/** @brief A chain comparison from a pack of comparisons. */
	template<typename... Exprs>
	requires(std::is_constructible_v<Box<Expr>, Exprs &&> && ...)
	Box<ChainComparisonExpr> chainCmp(Exprs&&... comparisons) {
		return chainCmp(internal::packToVector(std::forward<Exprs>(comparisons)...));
	}

	/** @brief A cast of `source` to `target_type`. */
	inline Box<CastExpr> cast(Box<Expr> source, tsh::SymbolType<> target_type) {
		return makeBox<CastExpr>(internal::ctx(), generatedOrigin(), std::move(source), target_type);
	}

	/** @brief A reference creation `refof inner`. */
	inline Box<RefOfExpr> refOf(Box<Expr> inner) {
		return makeBox<RefOfExpr>(internal::ctx(), generatedOrigin(), std::move(inner));
	}

	/** @brief An explicit move `move inner`. Named `moveOf` to avoid clashing with `std::move`. */
	inline Box<MoveExpr> moveOf(Box<Expr> inner) {
		return makeBox<MoveExpr>(internal::ctx(), generatedOrigin(), std::move(inner));
	}

	/** @brief A dereference `deref inner`. */
	inline Box<DerefExpr> deref(Box<Expr> inner) {
		return makeBox<DerefExpr>(internal::ctx(), generatedOrigin(), std::move(inner));
	}

	/*********************
	 *   MISCELLANEOUS   *
	 *********************/

	/** @brief A default value of `type`. */
	inline Box<DefaultValueExpr> defaultValue(tsh::AbstractType type) {
		return makeBox<DefaultValueExpr>(internal::ctx(), generatedOrigin(), std::move(type));
	}

	/** @brief A compile-time lift of `value` to a type. */
	inline Box<LiftToTypeExpr> liftToType(Box<Expr> value) {
		return makeBox<LiftToTypeExpr>(internal::ctx(), generatedOrigin(), std::move(value));
	}

	/************
	 *   LIST   *
	 ************/

	// @note ListPushExpr / ListPopExpr are the two HOUT nodes whose constructors take no
	// query::Context, so these builders do not read the ambient context.

	/** @brief A push `list += element`. */
	inline Box<ListPushExpr> listPush(Box<Expr> list, Box<Expr> element) {
		return makeBox<ListPushExpr>(generatedOrigin(), std::move(list), std::move(element));
	}

	/** @brief A pop of `count` elements from `list`. */
	inline Box<ListPopExpr> listPop(Box<Expr> list, Box<Expr> count) {
		return makeBox<ListPopExpr>(generatedOrigin(), std::move(list), std::move(count));
	}

	/******************
	 *   STATEMENTS   *
	 ******************/

	// @note Unlike expressions, Stmt constructors take no query::Context (they store, they don't
	// type-check), so these builders never read the ambient context — only *expression* builders do.
	// A guard is still typically in scope because functions and statements contain expressions anyway.

	/**
	 * @brief The body argument of `ifStmt` / `whileStmt` / `block`.
	 *
	 * Besides a `CodeBlock` or a `std::vector<Box<Stmt>>`, it accepts a braced list of statements —
	 * which is the point: it lets you write `ifStmt(cond, {s1, s2})`.
	 * A braced list cannot go through `std::initializer_list` here (its elements are const, and
	 * `Box` is move-only), so the enabling piece is the variadic constructor, which
	 * brace-initialization falls back to when no `initializer_list` constructor applies.
	 */
	class StmtPack final {
		std::vector<Box<Stmt>> statements;

	public:
		StmtPack() = default;

		/** @brief From a braced list / pack of statements: enables `{ s1, s2, ... }`. */
		template<typename... Stmts>
		requires(sizeof...(Stmts) >= 1 && (std::is_constructible_v<Box<Stmt>, Stmts &&> && ...))
		StmtPack(Stmts&&... stmts) {
			statements.reserve(sizeof...(stmts));
			(statements.emplace_back(std::forward<Stmts>(stmts)), ...);
		}

		StmtPack(std::vector<Box<Stmt>> statements): statements(std::move(statements)) {}

		StmtPack(CodeBlock block): statements(std::move(block.statements)) {}

		/** @brief Consumes the body, yielding its `CodeBlock`. */
		CodeBlock toCodeBlock() && { return CodeBlock{ std::move(statements) }; }
	};

	/** @brief A bare block statement `{ body }`. Accepts a braced list: `block({s1, s2})`. */
	inline Box<BlockStmt> block(StmtPack body) {
		return makeBox<BlockStmt>(generatedOrigin(), std::move(body).toCodeBlock());
	}

	/** @brief A variable declaration `var/let symbol: type = init;`. */
	inline Box<VariableStmt> var(SymID symbol, tsh::SymbolType<> type, Box<Expr> init) {
		return makeBox<VariableStmt>(generatedOrigin(), std::move(init), type, symbol);
	}

	/** @brief An assignment `location = value;`. */
	inline Box<AssignmentStmt> assign(Box<Expr> location, Box<Expr> value) {
		return makeBox<AssignmentStmt>(generatedOrigin(), std::move(location), std::move(value));
	}

	/** @brief A `return value;`. */
	inline Box<ReturnStmt> ret(Box<Expr> value) {
		return makeBox<ReturnStmt>(generatedOrigin(), std::move(value));
	}

	/** @brief A `return;` (no value). */
	inline Box<VoidReturnStmt> ret() { return makeBox<VoidReturnStmt>(generatedOrigin()); }

	/** @brief An expression statement `expr;` (result discarded). */
	inline Box<ExprStmt> expr(Box<Expr> expr) {
		return makeBox<ExprStmt>(generatedOrigin(), std::move(expr));
	}

	/** @brief An `if (condition) { then_body }`. Accepts a braced list: `ifStmt(cond, {s1, s2})`. */
	inline Box<IfStmt> ifStmt(Box<Expr> condition, StmtPack then_body) {
		return makeBox<IfStmt>(
			generatedOrigin(), std::move(condition), std::move(then_body).toCodeBlock()
		);
	}

	/** @brief An `if (condition) { then_body } else { else_body }`.
	 * Accepts braced lists: `ifStmt(cond, {s1, s2}, {s3, s4})`.*/
	inline Box<IfStmt> ifStmt(Box<Expr> condition, StmtPack then_body, StmtPack else_body) {
		return makeBox<IfStmt>(
			generatedOrigin(),
			std::move(condition),
			std::move(then_body).toCodeBlock(),
			std::move(else_body).toCodeBlock()
		);
	}

	/** @brief A `while (condition) { body }`. Accepts a braced list: `whileStmt(cond, {s1, s2})`. */
	inline Box<WhileStmt> whileStmt(Box<Expr> condition, StmtPack body) {
		return makeBox<WhileStmt>(
			generatedOrigin(), std::move(condition), std::move(body).toCodeBlock()
		);
	}
}
