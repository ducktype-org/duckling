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

namespace compiler::helios::code {

	/**
	 * @brief Checks whether the type is a primitive scalar which may take part in an
	 * explicit `as` conversion (integers, floats, `char` and `byte`).
	 */
	static bool isScalarCastableType(const tsh::AbstractType type) {
		switch (type.getKind()) {
		case tsh::Kind::Integral:
		case tsh::Kind::Float:
		case tsh::Kind::Byte:
		case tsh::Kind::Char:
			return true;
		default:
			return false;
		}
	}

	Box<Expr> castAs(
		query::Context&                        ctx,
		Box<Expr>                              value,
		tsh::SymbolType<>                      as_type,
		pst::Access<pst::expr::BinaryOperator> stmt
	) {
		auto from = value->expression_type.getSymbolType();
		auto to   = as_type;
		if (from == to) return value;  // trivial cast, always valid

		using tsh::Kind;
		using tsh::Mutability;
		using tsh::ReferenceKind;

		auto cast_expr = [&](Box<Expr> val) {
			return makeBox<CastExpr>(ctx, pstOrigin(stmt), std::move(val), to);
		};

		// We first check if we can cover the case by the coercion logic (from bool or to bool for
		// example).
		auto coercion = canCoerce(ctx, value->expression_type, to).valueOrThrow();
		if (coercion.isValid()) return coercion.coerce(ctx, std::move(value));

		// @TODO: #3631 Numeric coercions to bool are temporarily disabled.
		// Casting to bool needs custom handling.
		// We use coercion logic here because it's surprisingly complex and handles references
		// (e.g. ref i64 -> bool). @TODO: #3625 we might want to abstract it to a common helper.
		if (auto value_type = value->expression_type.getType();
		    (value_type.getKind() == Kind::Integral || value_type.getKind() == Kind::Float)
		    && to.getType().getKind() == Kind::Bool) {
			auto coercion_numeric_to_bool = Coercion::valid(from, to, false);
			return coercion_numeric_to_bool.coerce(ctx, std::move(value));
		}

		if (isScalarCastableType(from.getType()) && isScalarCastableType(to.getType())
		    && from.getRefKind() == ReferenceKind::Direct
		    && to.getRefKind() == ReferenceKind::Direct)
			return cast_expr(std::move(value));

		struct CastPattern {
			/// If specified, the cast must have this reference kind on the source side. If
			/// not specified, any reference kind matches.
			base::Optional<tsh::ReferenceKind> from_ref_kind;
			/// If specified, the cast must have this kind on the source side. If not
			/// specified, any kind matches.
			base::Optional<tsh::Kind> from_kind;
			/// If specified, the cast must have this reference kind on the target side. If
			/// not specified, any reference kind matches.
			base::Optional<tsh::ReferenceKind> to_ref_kind;
			/// If specified, the cast must have this kind on the target side. If not
			/// specified, any kind matches.
			base::Optional<tsh::Kind> to_kind;
			/// If true, the cast is only valid if the symbol pointee types are the same.
			bool same_pointee_type;
		};

		static const std::vector<CastPattern> valid_pointer_casts = {
			{
				.from_ref_kind     = ReferenceKind::Ref,
				.from_kind         = {},
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = tsh::Kind::Pointer,
				.same_pointee_type = true,
			},
			{
				.from_ref_kind     = ReferenceKind::Ref,
				.from_kind         = {},
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = tsh::Kind::CPointer,
				.same_pointee_type = false,
			},
			{
				.from_ref_kind     = ReferenceKind::Box,
				.from_kind         = {},
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = tsh::Kind::Pointer,
				.same_pointee_type = true,
			},
			{
				.from_ref_kind     = ReferenceKind::Direct,
				.from_kind         = Kind::Pointer,
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = Kind::CPointer,
				.same_pointee_type = false,
			},
			{
				.from_ref_kind     = ReferenceKind::Direct,
				.from_kind         = Kind::ManyPointer,
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = Kind::CPointer,
				.same_pointee_type = false,
			},
			{
				.from_ref_kind     = ReferenceKind::Direct,
				.from_kind         = Kind::ManyPointer,
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = Kind::Pointer,
				.same_pointee_type = true,
			},
			// From CPointer to Pointer is temporary
			{
				.from_ref_kind     = ReferenceKind::Direct,
				.from_kind         = Kind::CPointer,
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = Kind::Pointer,
				.same_pointee_type = false,
			},
			// From CPointer to ManypPointer is temporary
			{
				.from_ref_kind     = ReferenceKind::Direct,
				.from_kind         = Kind::CPointer,
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = Kind::ManyPointer,
				.same_pointee_type = false,
			},
			// From CPointer to CPointer
			{
				.from_ref_kind     = ReferenceKind::Direct,
				.from_kind         = Kind::CPointer,
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = Kind::CPointer,
				.same_pointee_type = false,
			},
			// From CPointer to Integral
			{
				.from_ref_kind     = ReferenceKind::Direct,
				.from_kind         = Kind::CPointer,
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = Kind::Integral,
				.same_pointee_type = false,
			},
			// From Pointer to Integral (LLVM only friendly)
			{
				.from_ref_kind     = ReferenceKind::Direct,
				.from_kind         = Kind::Pointer,
				.to_ref_kind       = ReferenceKind::Direct,
				.to_kind           = Kind::Integral,
				.same_pointee_type = false,
			},
		};

		bool found_match = false;
		for (auto& pattern: valid_pointer_casts) {
			auto equals = [](auto expected) {
				return [v = std::move(expected)](auto other) { return other == v; };
			};
			bool from_ref_match
				= pattern.from_ref_kind.map(equals(from.getRefKind())).copyValueOr(true);
			bool from_kind_match
				= pattern.from_kind.map(equals(from.getType().getKind())).copyValueOr(true);
			bool to_ref_match = pattern.to_ref_kind.map(equals(to.getRefKind())).copyValueOr(true);
			bool to_kind_match
				= pattern.to_kind.map(equals(to.getType().getKind())).copyValueOr(true);
			if (!from_ref_match || !from_kind_match || !to_ref_match || !to_kind_match) continue;
			// To check the pointee we have to be sure that we have ref or pointer.
			bool pointee_match = pattern.same_pointee_type
			                       ? from.getPointeeSymbolType() == to.getPointeeSymbolType()
			                       : true;
			if (from_ref_match && from_kind_match && to_ref_match && to_kind_match
			    && pointee_match) {
				found_match = true;
				break;
			}
		}
		// Temporarily allow casts from CPointer to Pointer and ManyPointer.
		if (found_match && from.getRefKind() == ReferenceKind::Direct
		    && from.getType().getKind() == tsh::Kind::CPointer) {
			// I do not like this syntax "as" to work on some targets and not work on
			// others. I would like to have some syntax, so that the user has to write
			// `native_cptr_cast<ptr T>(original_ctype)` or `@native v as ptr T`
			// to make it explicit that this cast is only supported on native target.
			return cast_expr(std::move(value));
		}
		if (found_match) return cast_expr(std::move(value));

		ctx.logInt(makeBox<dia::PlaceholderError>(
			base::strConcat("Invalid cast from type ", from.toString(), " to type ", to.toString()),
			stmt->getStablePosition()
		));
		query::throwFailed();
		CORE_UNREACHABLE();
	}
}
