// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "coercions.hpp"

#include <ctv/numeric_value.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/coercions/coercion_node.hpp>
#include <helios/tsh/coercions/passing.hpp>
#include <helios/tsh/coercions/queries.hpp>
#include <helios/tsh/coercions/value_source.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_errors.hpp>

#include <ranges>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace compiler::helios::coercions {
	namespace {
		using namespace code;
		using tsh::coercions::CoercionNode;
		using tsh::coercions::CoercionStep;
		using tsh::coercions::ValueSource;

		namespace step = tsh::coercions::coercion_step;

		[[noreturn]] void failNotImplemented(
			query::Context& ctx, std::string what, const Expr& position
		) {
			ctx.logInt(
				makeBox<dia::NotYetImplementedCodeError>(std::move(what), position.getPosition())
			);
			query::throwFailed();
		}

		/**
		 * @brief Creates a `ValueSource` of @p expr. Contains the `ExpressionType`s of the value
		 * and its sub-`ExpressionType`s if the value needs it (i.e. is a tuple literal, everything
		 * else is a single value).
		 */
		ValueSource valueSourceOf(const CRef<Expr> expr) {
			const auto* literal = dynamic_cast<const TupleExpr*>(expr.get());
			if (literal == nullptr || expr->expression_type.getType().getKind() != tsh::Kind::Tuple)
				return ValueSource::singleValueSource(expr->expression_type);

			std::vector<ValueSource> elements;
			elements.reserve(literal->elements.size());
			for (const auto& element: literal->elements)
				elements.push_back(valueSourceOf(element.ref()));

			return ValueSource::nestedValueSource(expr->expression_type, std::move(elements));
		}

		Box<Expr> applyNode(query::Context& ctx, const CoercionNode& node, Box<Expr> from);

		/**
		 * @brief Whether performing @p part on a component read out of an existing tuple would
		 * take something the tuple owns, which is a partial move which is not yet supported.
		 */
		bool takesOwnedComponent(query::Context& ctx, const CoercionNode& part) {
			// Reading the pointee out of a `ref`/`box` never takes the reference itself.
			if (part.derefs()) return false;

			// @TODO: #1711 A function type does not know whether it is trivially copyable yet.
			if (part.source.getType().getKind() == tsh::Kind::Function) return false;

			return not tsh::coercions::isCopiedByBytes(ctx, part.source);
		}

		/**
		 * @brief Rebuilds a tuple out of its components, each coerced on its own by @p parts.
		 */
		Box<Expr> applyElementwise(
			query::Context& ctx, const std::vector<CoercionNode>& parts, Box<Expr> whole
		) {
			const auto origin = whole->origin.generatedFrom();

			// Case 1: We have a tuple literal. We go over the components, coerce them and create a
			// new tuple.
			if (auto* literal = dynamic_cast<TupleExpr*>(whole.get())) {
				CORE_ASSERT(
					literal->elements.size() == parts.size(),
					"A tuple literal is coerced by exactly one part per element."
				);

				std::vector<Box<Expr>> elements;
				elements.reserve(parts.size());
				for (auto&& [part, element]: std::views::zip(parts, literal->elements))
					elements.push_back(applyNode(ctx, part, std::move(element)));

				return makeBox<TupleExpr>(ctx, origin, std::move(elements));
			}

			// Case 2: We have a tuple value. We have to extract all of its components and create a
			// new tuple. The first read of a component evaluates the tuple, every read after it
			// reuses the stored value.
			for (const CoercionNode& part: parts)
				if (takesOwnedComponent(ctx, part))
					failNotImplemented(
						ctx,
						base::strConcat(
							"Taking a component of type `",
							part.source.toString(),
							"` out of an existing tuple is not supported yet."
						),
						*whole
					);

			const auto source_tuple = whole->expression_type.getType().as<tsh::TupleAbstractType>();
			const std::vector<SymID>& fields
				= ctx.query<QueryTupleTypeData>(source_tuple)->valueOrThrow().members;

			Box<ReusableExpr>      tuple = makeBox<ReusableExpr>(ctx, std::move(whole));
			std::vector<Box<Expr>> reads
				= std::views::iota(usize{ 1 }, parts.size())
			    | std::views::transform([&](usize) -> Box<Expr> { return tuple->nextUse(); })
			    | std::ranges::to<std::vector>();
			reads.insert(reads.begin(), std::move(tuple));

			std::vector<Box<Expr>> components;
			components.reserve(parts.size());

			for (auto&& [part, read, field]: std::views::zip(parts, reads, fields)) {
				auto component = makeBox<AccessExpr>(ctx, origin, std::move(read), field);
				components.push_back(applyNode(ctx, part, std::move(component)));
			}

			return makeBox<TupleExpr>(ctx, origin, std::move(components));
		}

		Box<Expr> applyStep(
			query::Context& ctx, const step::Deref&, const tsh::SymbolType<>&, Box<Expr> from
		) {
			const auto origin = from->origin.generatedFrom();
			return makeBox<DerefExpr>(ctx, origin, std::move(from));
		}

		Box<Expr> applyStep(query::Context&, const step::HandOver&, const tsh::SymbolType<>&, Box<Expr>) {
			CORE_PANIC("HandOver node should never appear in an `ExpressionType` coercion");
		}

		Box<Expr> applyStep(
			query::Context& ctx, const step::ImplicitMove&, const tsh::SymbolType<>&, Box<Expr> from
		) {
			const auto origin = from->origin.generatedFrom();
			return makeBox<MoveExpr>(ctx, origin, std::move(from), MoveExpr::MoveKind::Implicit);
		}

		Box<Expr> applyStep(
			query::Context&, const step::MutabilityChange&, const tsh::SymbolType<>&, Box<Expr> from
		) {
			// Only what may be done with the value changes, the value itself does not.
			return from;
		}

		Box<Expr> applyStep(
			query::Context& ctx, const step::Numeric&, const tsh::SymbolType<>& result, Box<Expr> from
		) {
			const auto origin = from->origin.generatedFrom();
			return makeBox<CastExpr>(ctx, origin, std::move(from), result);
		}

		Box<Expr> applyStep(
			query::Context& ctx, const step::ZeroCheck&, const tsh::SymbolType<>&, Box<Expr> from
		) {
			return buildZeroCheck(ctx, std::move(from));
		}

		Box<Expr> applyStep(
			query::Context& ctx, const step::LiftToType&, const tsh::SymbolType<>&, Box<Expr> from
		) {
			const auto origin = from->origin.generatedFrom();
			return makeBox<LiftToTypeExpr>(ctx, origin, std::move(from));
		}

		Box<Expr> applyStep(
			query::Context& ctx,
			const step::RetypeVoid&,
			const tsh::SymbolType<>& result,
			Box<Expr>                from
		) {
			const auto origin = from->origin.generatedFrom();
			return makeBox<CastExpr>(ctx, origin, std::move(from), result);
		}

		Box<Expr> applyStep(
			query::Context&          ctx,
			const step::Elementwise& elementwise,
			const tsh::SymbolType<>&,
			Box<Expr> from
		) {
			return applyElementwise(ctx, elementwise.parts, std::move(from));
		}

		Box<Expr> applyStep(
			query::Context&          ctx,
			const step::VariantPack& packing,
			const tsh::SymbolType<>& result,
			Box<Expr>                from
		) {
			const auto origin = from->origin.generatedFrom();
			return makeBox<VariantConstructExpr>(
				ctx, origin, std::move(from), result, packing.alternative
			);
		}

		Box<Expr> applyStep(
			query::Context& ctx, const step::UserConversion&, const tsh::SymbolType<>&, Box<Expr> from
		) {
			// @TODO: #3656 Call the conversion function once anything decides on one.
			failNotImplemented(ctx, "User defined conversions are not supported yet.", *from);
		}

		/**
		 * @brief Performs one step @p coercion_step of a coercion.
		 */
		Box<Expr> applyStep(query::Context& ctx, const CoercionStep& coercion_step, Box<Expr> from) {
			return VISIT(coercion_step.kind, kind, {
				return applyStep(ctx, kind, coercion_step.result, std::move(from));
			});
		}

		/**
		 * @brief Performs every step of @p node on @p from, in order.
		 */
		Box<Expr> applyNode(query::Context& ctx, const CoercionNode& node, Box<Expr> from) {
			CORE_ASSERT(
				from->expression_type.getSymbolType() == node.source,
				base::strConcat(
					"A coercion node of `",
					node.source.toString(),
					"` is carried out on an expression of type `",
					from->expression_type.getSymbolType().toString(),
					"`."
				)
			);

			for (const CoercionStep& coercion_step: node.steps)
				from = applyStep(ctx, coercion_step, std::move(from));
			return from;
		}
	}

	Coercion coercionOf(
		query::Context&          ctx,
		const CRef<Expr>         from,
		const tsh::SymbolType<>& target,
		const HandOverKind       kind
	) {
		const ValueSource source = [&] {
			ValueSource value = valueSourceOf(from);

			// A value built out of parts is a temporary, so there is nothing to consume in it as a
			// whole. Its parts are handed over into it either way.
			if (kind == HandOverKind::Regular || value.isBuiltOutOfParts()) return value;
			return ValueSource::singleValueSource(tsh::coercions::consumed(from->expression_type));
		}();

		return tsh::coercions::coercionOf(ctx, source, target);
	}

	Coercion coercionToMeta(query::Context& ctx, const CRef<Expr> from) {
		return coercionOf(
			ctx,
			from,
			tsh::SymbolType<>{
				tsh::getMetaType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			}
		);
	}

	query::QResult<Coercion> validCoercionOf(
		query::Context&                     ctx,
		const CRef<Expr>                    from,
		const tsh::SymbolType<>&            target,
		const dia::StablePosition           source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		const CoercionErrorOverrides&       error_overrides,
		const HandOverKind                  kind
	) {
		Coercion coercion = coercionOf(ctx, from, target, kind);
		if (coercion.isValid()) return coercion;

		logCoercionFailure(ctx, coercion, source_position, coercion_expects_pos, error_overrides);
		return query::Failed();
	}

	query::QResult<Coercion> validCoercionOf(
		query::Context&                     ctx,
		const ValueSource&                  from,
		const tsh::SymbolType<>&            target,
		const dia::StablePosition           source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		const CoercionErrorOverrides&       error_overrides
	) {
		Coercion coercion = coercionOf(ctx, from, target);
		if (coercion.isValid()) return coercion;

		logCoercionFailure(ctx, coercion, source_position, coercion_expects_pos, error_overrides);
		return query::Failed();
	}

	Box<Expr> applyCoercion(query::Context& ctx, const Coercion& coercion, Box<Expr> from) {
		const auto describe = [&] {
			std::ostringstream out;
			coercion.debugPrint(out);
			return out.str();
		};

		CORE_ASSERT(
			coercion.isValid(), base::strConcat("Carrying out a refused coercion: ", describe())
		);
		CORE_ASSERT(
			coercion.getSource().getSymbolType() == from->expression_type.getSymbolType(),
			base::strConcat(
				"Carrying out a coercion on an expression it was not decided for: ", describe()
			)
		);

		return applyNode(ctx, coercion.getRoot(), std::move(from));
	}

	BoxOrCRef<Expr> applyCoercionFromRef(
		query::Context& ctx, const Coercion& coercion, CRef<Expr> from
	) {
		if (coercion.isNoOp()) return from;
		return applyCoercion(ctx, coercion, from->clone());
	}

	Box<Expr> buildZeroCheck(query::Context& ctx, Box<Expr> value) {
		const auto              origin = value->origin.generatedFrom();
		const tsh::AbstractType type   = value->expression_type.getType();

		switch (type.getKind()) {
		case tsh::Kind::Integral:
		case tsh::Kind::Float: {
			auto zero = numeric_value::NumericValue::createOfType(type).expect(
				"A zero fits into every numeric type."
			);
			const BuiltinBinary not_equal = type.getKind() == tsh::Kind::Float
			                                  ? BuiltinBinary::FloatNeq
			                                  : BuiltinBinary::IntegerNeq;

			return makeBox<BinaryOperatorExpr>(
				ctx,
				origin,
				not_equal,
				std::move(value),
				makeBox<LiteralNumericExpr>(ctx, generatedOrigin(), zero)
			);
		}
		case tsh::Kind::Byte:
		case tsh::Kind::Char: {
			// There is no literal of either to compare against, so they are compared as the
			// unsigned integer of the same width. Both are 8 bits wide.
			const auto as_integer = tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(ctx, 8, tsh::IntegralAbstractType::Signedness::Unsigned)
			);
			return buildZeroCheck(ctx, makeBox<CastExpr>(ctx, origin, std::move(value), as_integer));
		}
		default:
			failNotImplemented(
				ctx,
				base::strConcat(
					"Checking a value of type `",
					value->expression_type.getSymbolType().toString(),
					"` against zero is not supported yet."
				),
				*value
			);
		}

		CORE_UNREACHABLE();
	}

	query::QResult<Box<Expr>> coerceFromBox(
		query::Context&                     ctx,
		Box<Expr>                           expr,
		const tsh::SymbolType<>&            expected_type,
		const dia::StablePosition           source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		const CoercionErrorOverrides&       error_overrides
	) {
		UNPACK_QRESULT(
			const Coercion coercion =,
			validCoercionOf(
				ctx, expr.ref(), expected_type, source_position, coercion_expects_pos, error_overrides
			)
		);
		return applyCoercion(ctx, coercion, std::move(expr));
	}
}
