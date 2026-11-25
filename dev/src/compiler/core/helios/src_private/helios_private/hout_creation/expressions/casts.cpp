// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "casts.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/binary_operator.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>

#include <base/except/exceptions.hpp>

#include <diagnostic/stable_position.hpp>

#include <algorithm>
#include <vector>

namespace compiler::helios::code {

	namespace {
		using tsh::Kind;
		using tsh::ReferenceKind;

		/// The result of one cast layer: the converted expression, or empty when the layer has no
		/// rule for the cast. A layer moves from its `value` only when it returns a result.
		using CastResult = base::Optional<Box<Expr>>;

		/**
		 * @brief Checks whether the type is a primitive scalar which may take part in an
		 * explicit `as` conversion (integers, floats, `char` and `byte`).
		 */
		bool isScalarCastableType(const tsh::AbstractType type) {
			switch (type.getKind()) {
			case Kind::Integral:
			case Kind::Float:
			case Kind::Byte:
			case Kind::Char:
				return true;
			default:
				return false;
			}
		}

		/// One allowed pointer cast. The target side is always a `Direct` value.
		struct CastPattern {
			/// The cast must have this reference kind on the source side.
			tsh::ReferenceKind from_ref_kind;
			/// If specified, the cast must have this kind on the source side. If not
			/// specified, any kind matches.
			base::Optional<tsh::Kind> from_kind;
			/// The cast must have this kind on the target side.
			tsh::Kind to_kind;
			/// If true, the cast is only valid if the symbol pointee types are the same.
			bool same_pointee_type;
		};

		/// Checks whether any of @p patterns allows the cast from @p from to @p to.
		bool matchesAny(
			const std::vector<CastPattern>& patterns,
			const tsh::SymbolType<>&        from,
			const tsh::SymbolType<>&        to
		) {
			if (to.getRefKind() != ReferenceKind::Direct) return false;
			return std::ranges::any_of(patterns, [&](const CastPattern& pattern) {
				if (pattern.from_ref_kind != from.getRefKind()) return false;
				if (pattern.from_kind
				        .map([&](Kind kind) { return kind != from.getType().getKind(); })
				        .copyValueOr(false))
					return false;
				if (pattern.to_kind != to.getType().getKind()) return false;
				// To check the pointee we have to be sure that we have ref or pointer.
				return !pattern.same_pointee_type
				    || from.getPointeeSymbolType() == to.getPointeeSymbolType();
			});
		}

		/**
		 * @brief Casts of a `ref`/`box` itself into a pointer to the same place.
		 *
		 * The value behind the reference is never read. Returns empty when no rule applies.
		 */
		CastResult tryReferenceCast(
			query::Context&                        ctx,
			Box<Expr>&                             value,
			const tsh::SymbolType<>&               to,
			pst::Access<pst::expr::BinaryOperator> stmt
		) {
			static const std::vector<CastPattern> reference_casts = {
				{ .from_ref_kind     = ReferenceKind::Ref,
				  .from_kind         = {},
				  .to_kind           = Kind::Pointer,
				  .same_pointee_type = true },
				{ .from_ref_kind     = ReferenceKind::Ref,
				  .from_kind         = {},
				  .to_kind           = Kind::CPointer,
				  .same_pointee_type = false },
				{ .from_ref_kind     = ReferenceKind::Box,
				  .from_kind         = {},
				  .to_kind           = Kind::Pointer,
				  .same_pointee_type = true },
			};

			const auto from = value->expression_type.getSymbolType();
			CORE_ASSERT(from.getRefKind() != ReferenceKind::Direct, "Expected a ref or a box");
			if (!matchesAny(reference_casts, from, to)) return {};
			return makeBox<CastExpr>(ctx, pstOrigin(stmt), std::move(value), to);
		}

		/**
		 * @brief Casts of a plain (`Direct`) value: number to `bool`, scalar to scalar and
		 * pointer to pointer or integer.
		 *
		 * `castAs` also applies these to the value read out of a `ref`/`box`. Returns empty when
		 * no rule applies.
		 */
		CastResult tryValueCast(
			query::Context&                        ctx,
			Box<Expr>&                             value,
			const tsh::SymbolType<>&               to,
			pst::Access<pst::expr::BinaryOperator> stmt
		) {
			static const std::vector<CastPattern> pointer_casts = {
				{ .from_ref_kind     = ReferenceKind::Direct,
				  .from_kind         = Kind::Pointer,
				  .to_kind           = Kind::CPointer,
				  .same_pointee_type = false },
				{ .from_ref_kind     = ReferenceKind::Direct,
				  .from_kind         = Kind::ManyPointer,
				  .to_kind           = Kind::CPointer,
				  .same_pointee_type = false },
				{ .from_ref_kind     = ReferenceKind::Direct,
				  .from_kind         = Kind::ManyPointer,
				  .to_kind           = Kind::Pointer,
				  .same_pointee_type = true },
				// From CPointer to Pointer and ManyPointer is temporary. I do not like this syntax
				// "as" to work on some targets and not work on others. I would like to have some
				// syntax, so that the user has to write `native_cptr_cast<ptr T>(original_ctype)`
				// or `@native v as ptr T` to make it explicit that this cast is only supported on
				// native target.
				{ .from_ref_kind     = ReferenceKind::Direct,
				  .from_kind         = Kind::CPointer,
				  .to_kind           = Kind::Pointer,
				  .same_pointee_type = false },
				{ .from_ref_kind     = ReferenceKind::Direct,
				  .from_kind         = Kind::CPointer,
				  .to_kind           = Kind::ManyPointer,
				  .same_pointee_type = false },
				{ .from_ref_kind     = ReferenceKind::Direct,
				  .from_kind         = Kind::CPointer,
				  .to_kind           = Kind::CPointer,
				  .same_pointee_type = false },
				{ .from_ref_kind     = ReferenceKind::Direct,
				  .from_kind         = Kind::CPointer,
				  .to_kind           = Kind::Integral,
				  .same_pointee_type = false },
				// From Pointer to Integral (LLVM only friendly)
				{ .from_ref_kind     = ReferenceKind::Direct,
				  .from_kind         = Kind::Pointer,
				  .to_kind           = Kind::Integral,
				  .same_pointee_type = false },
			};

			const auto from = value->expression_type.getSymbolType();
			CORE_ASSERT(from.getRefKind() == ReferenceKind::Direct, "Expected a plain value");

			// @TODO: #3631 Numeric coercions to bool are temporarily disabled.
			// Casting to bool needs custom handling.
			if ((from.getType().getKind() == Kind::Integral
			     || from.getType().getKind() == Kind::Float)
			    && to.getType().getKind() == Kind::Bool) {
				const tsh::coercions::Coercion to_direct
					= coercions::coercionOf(ctx, value.ref(), from.toDirect());
				CORE_ASSERT(to_direct.isValid(), "Should always be valid");
				return coercions::buildZeroCheck(
					ctx, coercions::applyCoercion(ctx, to_direct, std::move(value))
				);
			}

			if ((isScalarCastableType(from.getType()) && isScalarCastableType(to.getType())
			     && to.getRefKind() == ReferenceKind::Direct)
			    || matchesAny(pointer_casts, from, to))
				return makeBox<CastExpr>(ctx, pstOrigin(stmt), std::move(value), to);
			return {};
		}
	}

	Box<Expr> castAs(
		query::Context&                        ctx,
		Box<Expr>                              value,
		tsh::SymbolType<>                      as_type,
		pst::Access<pst::expr::BinaryOperator> stmt
	) {
		const auto from = value->expression_type.getSymbolType();
		const auto to   = as_type;
		if (from == to) return value;  // trivial cast, always valid

		// Layer 1: anything the implicit coercion covers (from bool or to bool for example). It
		// handles references on its own, e.g. `ref i32 as i64`.
		const tsh::coercions::Coercion coercion = coercions::coercionOf(ctx, value.ref(), to);
		if (coercion.isValid()) {
			auto coerced = coercions::applyCoercion(ctx, coercion, std::move(value));

			// @TODO: #3853 go back to this once defined
			if (coerced->expression_type.getSymbolType() != to)
				return makeBox<CastExpr>(ctx, pstOrigin(stmt), std::move(coerced), to);
			return coerced;
		}

		// Layer 2: only the mutability changes, for a value and a reference alike.
		if (from.getRefKind() == to.getRefKind() && from.getType() == to.getType())
			return makeBox<CastExpr>(ctx, pstOrigin(stmt), std::move(value), to);

		// Layer 3: the explicit rules. They are split by what the source is, so every rule is
		// written for exactly one kind of source and says nothing about the others.
		switch (from.getRefKind()) {
		case ReferenceKind::Direct:
			if (auto cast = tryValueCast(ctx, value, to, stmt)) return std::move(*cast);
			break;
		case ReferenceKind::Ref:
		case ReferenceKind::Box:
			// A cast of the reference itself (`ref T as ptr T`) keeps its meaning...
			if (auto cast = tryReferenceCast(ctx, value, to, stmt)) return std::move(*cast);
			// ...otherwise a `ref`/`box` casts to a plain value like the value it points to:
			// the value is read once, then only the value casts apply to it.
			if (to.getRefKind() == ReferenceKind::Direct) {
				value = makeBox<DerefExpr>(ctx, pstOrigin(stmt), std::move(value));
				if (auto cast = tryValueCast(ctx, value, to, stmt)) return std::move(*cast);
			}
			break;
		}

		ctx.log<dia::PlaceholderError>(
			base::strConcat("Invalid cast from type ", from.toString(), " to type ", to.toString()),
			stmt->getStablePosition()
		);
		query::throwFailed();
		CORE_UNREACHABLE();
	}
}
