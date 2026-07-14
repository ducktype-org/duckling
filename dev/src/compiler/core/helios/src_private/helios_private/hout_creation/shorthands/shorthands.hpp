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
		 * @brief Collects the origins of a list of expressions, preserving order.
		 */
		inline std::vector<ElementOrigin> originsOf(const std::vector<Box<Expr>>& exprs) {
			std::vector<ElementOrigin> origins;
			origins.reserve(exprs.size());
			for (const auto& expr: exprs) origins.emplace_back(expr->origin);
			return origins;
		}

		/**
		 * @brief Moves a pack of `Box<Expr>` into a vector, preserving order.
		 */
		template<typename... Exprs>
		requires(std::is_same_v<std::remove_cvref_t<Exprs>, Box<Expr>> && ...)
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

	/****************
	 *   LITERALS   *
	 ****************/

	/** @brief The unit literal `()`. */
	inline Box<Expr> litUnit() {
		return makeBox<LiteralUnitExpr>(internal::ctx(), generatedOrigin());
	}

	/** @brief A numeric literal from an explicit @ref numeric_value::NumericValue. */
	inline Box<Expr> litNum(numeric_value::NumericValue value) {
		return makeBox<LiteralNumericExpr>(internal::ctx(), generatedOrigin(), std::move(value));
	}

	/**
	 * @brief Convenience numeric literal from a plain C++ arithmetic value.
	 * @note The type is deduced by `NumericValue::createMinimized` — the smallest type that fits
	 * (currently `i32`/`u32`/`i64`/`u64` for integrals). If you need a specific type (e.g. `u64`),
	 * use the `(value, type)` overload or pass a pre-typed @ref numeric_value::NumericValue.
	 */
	template<typename T>
	requires(std::is_arithmetic_v<T>) Box<Expr> litNum(T value) {
		return litNum(numeric_value::NumericValue::createMinimized(value));
	}

	/** @brief A numeric literal of a specific type. Panics if `value` does not fit `type`. */
	template<typename T>
	requires(std::is_arithmetic_v<T>) Box<Expr> litNum(T value, const tsh::AbstractType& type) {
		return litNum(numeric_value::NumericValue::createOfType(type, value)
		                  .expect("litNum: value does not fit the requested type"));
	}

	/** @brief A boolean literal. */
	inline Box<Expr> litBool(bool value) {
		return makeBox<LiteralBoolExpr>(internal::ctx(), generatedOrigin(), value);
	}

	/** @brief A character literal. */
	inline Box<Expr> litChar(char value) {
		return makeBox<LiteralCharExpr>(internal::ctx(), generatedOrigin(), value);
	}

	/** @brief A string literal. */
	inline Box<Expr> litStr(base::StrID value) {
		return makeBox<LiteralStringExpr>(internal::ctx(), generatedOrigin(), value);
	}

	/** @brief A type literal (e.g. `i32`, `bool`), carrying `type` as its value. */
	inline Box<Expr> litType(tsh::AbstractType type) {
		return makeBox<LiteralTypeExpr>(internal::ctx(), generatedOrigin(), std::move(type));
	}

	/** @brief An identifier expression referring to `symbol`. */
	inline Box<Expr> ident(SymID symbol) {
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
	inline Box<Expr> binOp(Box<Expr> lhs, BuiltinBinary op, Box<Expr> rhs) {
		auto origin = elementOriginOrdered(lhs->origin, rhs->origin);
		return makeBox<BinaryOperatorExpr>(
			internal::ctx(), origin, op, std::move(lhs), std::move(rhs)
		);
	}

	/** @brief A unary operator `op operand`. */
	inline Box<Expr> unOp(BuiltinUnary op, Box<Expr> operand) {
		auto origin = operand->origin;
		return makeBox<UnaryOperatorExpr>(internal::ctx(), origin, op, std::move(operand));
	}

	/** @brief A ternary `if condition then if_true else if_false`. */
	inline Box<Expr> ternary(Box<Expr> condition, Box<Expr> if_true, Box<Expr> if_false) {
		auto origin
			= elementOriginOrdered({ condition->origin, if_true->origin, if_false->origin });
		return makeBox<TernaryOperatorExpr>(
			internal::ctx(), origin, std::move(condition), std::move(if_true), std::move(if_false)
		);
	}

	/** @brief A tuple value `(elements, ...)`. */
	inline Box<Expr> tuple(std::vector<Box<Expr>> elements) {
		auto origin = elementOriginOrdered(internal::originsOf(elements));
		return makeBox<TupleExpr>(internal::ctx(), origin, std::move(elements));
	}

	/** @brief A tuple value from a pack of elements. */
	template<typename... Exprs>
	requires(std::is_same_v<std::remove_cvref_t<Exprs>, Box<Expr>> && ...)
	Box<Expr> tuple(Exprs&&... elements) {
		return tuple(internal::packToVector(std::forward<Exprs>(elements)...));
	}

	/** @brief A variant type constructor `(subtypes | ...)`. */
	inline Box<Expr> variant(std::vector<Box<Expr>> subtypes) {
		auto origin = elementOriginOrdered(internal::originsOf(subtypes));
		return makeBox<VariantTypeConstructorExpr>(internal::ctx(), origin, std::move(subtypes));
	}

	/** @brief A variant type constructor from a pack of subtypes. */
	template<typename... Exprs>
	requires(std::is_same_v<std::remove_cvref_t<Exprs>, Box<Expr>> && ...)
	Box<Expr> variant(Exprs&&... subtypes) {
		return variant(internal::packToVector(std::forward<Exprs>(subtypes)...));
	}

	/** @brief A field access `base.field`. */
	inline Box<Expr> access(Box<Expr> base, SymID field) {
		auto origin = base->origin;
		return makeBox<AccessExpr>(internal::ctx(), origin, std::move(base), field);
	}

	/** @brief An index expression `base[idx]`. */
	inline Box<Expr> index(Box<Expr> base, Box<Expr> idx) {
		auto origin = elementOriginOrdered(base->origin, idx->origin);
		return makeBox<IndexExpr>(internal::ctx(), origin, std::move(base), std::move(idx));
	}

	/** @brief A call `callee(arguments...)`. */
	inline Box<Expr> call(Box<Expr> callee, std::vector<Box<Expr>> arguments) {
		std::vector<ElementOrigin> origins;
		origins.reserve(arguments.size() + 1);
		origins.emplace_back(callee->origin);
		for (const auto& arg: arguments) origins.emplace_back(arg->origin);

		auto origin = elementOriginOrdered(origins);
		return makeBox<CallExpr>(internal::ctx(), origin, std::move(callee), std::move(arguments));
	}

	/** @brief A call from a callee and a pack of arguments. */
	template<typename... Args>
	requires(std::is_same_v<std::remove_cvref_t<Args>, Box<Expr>> && ...)
	Box<Expr> call(Box<Expr> callee, Args&&... arguments) {
		return call(std::move(callee), internal::packToVector(std::forward<Args>(arguments)...));
	}

	/** @brief A sequence `expressions, ...` (comma operator); the last one is the result. */
	inline Box<Expr> seq(std::vector<Box<Expr>> expressions) {
		CORE_ASSERT(!expressions.empty(), "seq(): a SequenceExpr requires at least one expression");
		auto origin = elementOriginOrdered(internal::originsOf(expressions));
		return makeBox<SequenceExpr>(internal::ctx(), origin, std::move(expressions));
	}

	/** @brief A sequence from a pack of expressions. */
	template<typename... Exprs>
	requires(std::is_same_v<std::remove_cvref_t<Exprs>, Box<Expr>> && ...)
	Box<Expr> seq(Exprs&&... expressions) {
		return seq(internal::packToVector(std::forward<Exprs>(expressions)...));
	}

	/** @brief A chain comparison, the logical AND of each comparison (e.g. `a < b < c`). */
	inline Box<Expr> chainCmp(std::vector<Box<Expr>> comparisons) {
		CORE_ASSERT(
			!comparisons.empty(),
			"chainCmp(): a ChainComparisonExpr requires at least one comparison"
		);
		auto origin = elementOriginOrdered(internal::originsOf(comparisons));
		return makeBox<ChainComparisonExpr>(internal::ctx(), origin, std::move(comparisons));
	}

	/** @brief A chain comparison from a pack of comparisons. */
	template<typename... Exprs>
	requires(std::is_same_v<std::remove_cvref_t<Exprs>, Box<Expr>> && ...)
	Box<Expr> chainCmp(Exprs&&... comparisons) {
		return chainCmp(internal::packToVector(std::forward<Exprs>(comparisons)...));
	}

	/** @brief A cast of `source` to `target_type`. */
	inline Box<Expr> cast(Box<Expr> source, tsh::SymbolType<> target_type) {
		auto origin = source->origin;
		return makeBox<CastExpr>(internal::ctx(), origin, std::move(source), target_type);
	}

	/** @brief A reference creation `refof inner`. */
	inline Box<Expr> refOf(Box<Expr> inner) {
		auto origin = inner->origin;
		return makeBox<RefOfExpr>(internal::ctx(), origin, std::move(inner));
	}

	/** @brief An explicit move `move inner`. Named `moveOf` to avoid clashing with `std::move`. */
	inline Box<Expr> moveOf(Box<Expr> inner) {
		auto origin = inner->origin;
		return makeBox<MoveExpr>(internal::ctx(), origin, std::move(inner));
	}

	/** @brief A dereference `deref inner`. */
	inline Box<Expr> deref(Box<Expr> inner) {
		auto origin = inner->origin;
		return makeBox<DerefExpr>(internal::ctx(), origin, std::move(inner));
	}

	/*********************
	 *   MISCELLANEOUS   *
	 *********************/

	/** @brief A default value of `type`. */
	inline Box<Expr> defaultValue(tsh::AbstractType type) {
		return makeBox<DefaultValueExpr>(internal::ctx(), generatedOrigin(), std::move(type));
	}

	/** @brief A compile-time lift of `value` to a type. */
	inline Box<Expr> liftToType(Box<Expr> value) {
		auto origin = value->origin;
		return makeBox<LiftToTypeExpr>(internal::ctx(), origin, std::move(value));
	}

	/************
	 *   LIST   *
	 ************/

	// @note ListPushExpr / ListPopExpr are the two HOUT nodes whose constructors take no
	// query::Context, so these builders do not read the ambient context.

	/** @brief A push `list += element`. */
	inline Box<Expr> listPush(Box<Expr> list, Box<Expr> element) {
		auto origin = elementOriginOrdered(list->origin, element->origin);
		return makeBox<ListPushExpr>(origin, std::move(list), std::move(element));
	}

	/** @brief A pop of `count` elements from `list`. */
	inline Box<Expr> listPop(Box<Expr> list, Box<Expr> count) {
		auto origin = elementOriginOrdered(list->origin, count->origin);
		return makeBox<ListPopExpr>(origin, std::move(list), std::move(count));
	}
}
