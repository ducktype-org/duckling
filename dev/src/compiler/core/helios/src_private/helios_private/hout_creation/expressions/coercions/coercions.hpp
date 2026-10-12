// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file coercions.hpp
 * @brief Build an expression which performs a coercion which was decided by the type system.
 */
#pragma once

#include "errors.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/tsh/coercions/coercion.hpp>
#include <helios/tsh/expression_type.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/pointers/box_or_ref.hpp>

#include <diagnostic/stable_position.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

#include <cstdint>

namespace compiler::helios::coercions {
	using tsh::coercions::Coercion;

	/**
	 * @brief Whether this is the last use of the returned value (i.e. coercion of a value in a
	 * return statement).
	 */
	enum class HandOverKind : uint8_t {
		Regular,
		/// This is the last use of the value, so it's moved implicitly.
		LastUse,
	};

	/**
	 * @brief The coercion of an expression @p from into @p target type.
	 */
	[[nodiscard]] Coercion coercionOf(
		query::Context&          ctx,
		CRef<code::Expr>         from,
		const tsh::SymbolType<>& target,
		HandOverKind             kind = HandOverKind::Regular
	);

	/**
	 * @brief The coercion of @p from into the meta type.
	 */
	[[nodiscard]] Coercion coercionToMeta(query::Context& ctx, CRef<code::Expr> from);

	/**
	 * @brief The coercion of @p from into @p target type, logging why it was
	 * refused if it was, see `logCoercionFailure`.
	 *
	 * @return The valid coercion, or a failed result when the coercion was refused.
	 */
	query::QResult<Coercion> validCoercionOf(
		query::Context&                     ctx,
		CRef<code::Expr>                    from,
		const tsh::SymbolType<>&            target,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos = {},
		const CoercionErrorOverrides&       error_overrides      = {},
		HandOverKind                        kind                 = HandOverKind::Regular
	);

	/**
	 * @brief Builds the expression that performs the @p coercion on @p from.
	 *
	 * The coercion has to valid and created for the value @p from produces.
	 */
	[[nodiscard]] Box<code::Expr> applyCoercion(
		query::Context& ctx, const Coercion& coercion, Box<code::Expr> from
	);

	/**
	 * @brief Same as above, but for `CRef`.
	 */
	[[nodiscard]] BoxOrCRef<code::Expr> applyCoercionFromRef(
		query::Context& ctx, const Coercion& coercion, CRef<code::Expr> from
	);

	/**
	 * @brief Compares a value against zero of its own type, which turns it into a `bool`.
	 *
	 * @TODO: #3631 This is currently used by the explicit `as bool` casts of numbers. Remove it
	 * from the public interface.
	 */
	[[nodiscard]] Box<code::Expr> buildZeroCheck(query::Context& ctx, Box<code::Expr> value);

	/**
	 * @brief Coerces @p expr to @p expected_type, logging the failure if there is one.
	 *
	 * Logs errors via @p error_overrides if ones are provided.
	 * @return The coerced expression, or a failed result when the coercion was refused.
	 */
	query::QResult<Box<code::Expr>> coerceFromBox(
		query::Context&                     ctx,
		Box<code::Expr>                     expr,
		const tsh::SymbolType<>&            expected_type,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos = {},
		const CoercionErrorOverrides&       error_overrides      = {}
	);
}
