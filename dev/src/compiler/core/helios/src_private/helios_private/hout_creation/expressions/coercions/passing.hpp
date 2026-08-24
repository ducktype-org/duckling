/**
 * @file passing.hpp
 * @brief How a value is handed over to a new owner: whether the hand over copies, moves or only
 * rebinds a reference, and what exactly gets copied when it does.
 */
#pragma once

#include <helios/hout/elements/expr.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/pointers/box_or_ref.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios {
	/**
	 * @brief How a coercion may hand over a value.
	 */
	enum class PassingMethod {
		ByteCopy,            ///< Trivially copyable, copied by copying its bytes.
		ImplicitMove,        ///< Owned rvalue (a temporary), moved implicitly.
		ExplicitCopyOrMove,  ///< Non-trivial assignable value (lvalue), needs an explicit
		                     ///< `copy`/`move`.
		NotCopyable,  ///< Non-trivial assignable value (lvalue) value with no copy constructor.
	};

	/**
	 * @brief Decides how the given value may passed, based on its value category and the
	 * abilities of its type.
	 */
	PassingMethod passingMethod(query::Context& ctx, const tsh::ExpressionType<>& value);

	/**
	 * @brief Wraps a value in an implicit move when a `return` is about to end the life of the
	 * owned local it returns.
	 */
	[[nodiscard]] Box<code::Expr> moveReturnedLocal(query::Context& ctx, Box<code::Expr> value);

	/**
	 * @brief Whether the reference-kind transition creates a new value (which may require a
	 * copy).
	 */
	bool requiresValueCopy(tsh::ReferenceKind from_kind, tsh::ReferenceKind to_kind);

	/**
	 * @brief Whether the reference-kind coercion reads the copied value out of a `ref`/`box`,
	 * in which case what gets copied is the dereferenced value, not the reference itself.
	 */
	bool readsThroughReference(tsh::ReferenceKind from_kind, tsh::ReferenceKind to_kind);

	/**
	 * @brief The type of the value a copying coercion actually copies.
	 *
	 * A `var a: box T = box_T` copies the box itself, reading a `box T` into a `T` copies the
	 * pointee.
	 */
	tsh::SymbolType<> copiedValueType(const tsh::SymbolType<>& from, const tsh::SymbolType<>& to);

	/**
	 * @brief The value a copying coercion actually copies.
	 */
	tsh::ExpressionType<> valueBeingCopied(
		const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
	);
}
