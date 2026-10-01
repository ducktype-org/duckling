/**
 * @file rules.cpp
 * @brief Every rule of what coerces into what, and `QuerySymbolTypeCoercion`, which answers with
 * them.
 */

#include "../types.hpp"
#include "best_coercion.hpp"
#include "passing.hpp"
#include "queries.hpp"
#include "reference_coercion.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <algorithm>
#include <ranges>
#include <utility>
#include <vector>

namespace compiler::tsh::coercions {
	namespace {
		SymbolTypeCoercion refused(
			const SymbolType<>&               source,
			const SymbolType<>&               target,
			CoercionFailure                   reason,
			std::vector<CoercionError::Cause> causes = {}
		) {
			return SymbolTypeCoercion::refusedCoercion(
				source,
				CoercionError{
					.reason = std::move(reason),
					.source = source,
					.target = target,
					.causes = std::move(causes),
				}
			);
		}

		SymbolTypeCoercion accepted(CoercionNode node) {
			const SymbolType<> source = node.source;
			return SymbolTypeCoercion::successfulCoercion(source, std::move(node));
		}

		base::CRef<SymbolTypeCoercion> planPart(
			query::Context& ctx, const SymbolType<>& source, const SymbolType<>& target
		) {
			return ctx.query<QuerySymbolTypeCoercion>({ source, target });
		}

		/**
		 * A node about coercing @p source into @p target that has not done anything yet.
		 */
		CoercionNode startNode(const SymbolType<>& source, const SymbolType<>& target) {
			return CoercionNode{
				.source = source,
				.target = target,
				.steps  = {},
				.rank   = CoercionRank::identity(),
			};
		}

		/**
		 * @brief Inserts a `HandOver` step into @p node if the value needs it.
		 */
		void handOver(query::Context& ctx, CoercionNode& node) {
			const SymbolType<> value = node.current();

			// @TODO: #1711  A function type does not know whether it is trivially copyable yet and
			// throws NYI. Remove this edge case.
			if (value.getType().getKind() != Kind::Function && isCopiedByBytes(ctx, value)) return;

			node.append({ .kind = coercion_step::HandOver{}, .result = value });
		}

		/**
		 * @brief The value is taken and turned into a value of @p target by @p conversion.
		 */
		SymbolTypeCoercion converted(
			query::Context&     ctx,
			const SymbolType<>& source,
			const SymbolType<>& target,
			CoercionStep::Kind  conversion
		) {
			CoercionNode node = startNode(source, target);
			handOver(ctx, node);
			node.append({ .kind = std::move(conversion), .result = target });
			return accepted(std::move(node));
		}

		/**
		 * @brief A `void` is retyped into whatever the location is.
		 */
		SymbolTypeCoercion retypeVoid(const SymbolType<>& source, const SymbolType<>& target) {
			CoercionNode node = startNode(source, target);

			// `void` into `void`/`const void` is an identity and should rank with identity, not
			// `RetypeVoid` which is the worst rank.
			const bool is_identity = source.getType() == target.getType()
			                      && source.getRefKind() == target.getRefKind();

			if (not is_identity)
				node.append({ .kind = coercion_step::RetypeVoid{}, .result = target });
			return accepted(std::move(node));
		}

		/**
		 * @brief The location keeps a reference kind.
		 *
		 * Nothing converts behind a reference. Only mutability may differ, and only in the
		 * direction that promises less about the value.
		 */
		SymbolTypeCoercion keepReference(
			query::Context& ctx, const SymbolType<>& source, const SymbolType<>& target
		) {
			// Refuse implicit `ref`/`box`.
			if (not referenceCoercionRule(source.getRefKind(), target.getRefKind()).isLegal())
				return refused(source, target, coercion_error::IncompatibleTypes{});
			// Refuse coercions with different underlying types.
			if (source.getType() != target.getType())
				return refused(source, target, coercion_error::IncompatibleTypes{});

			// @TODO: #1488 Take `points_to_source` into consideration.
			if (source.getMutability() == Mutability::Immutable
			    && target.getMutability() == Mutability::Mutable)
				return refused(source, target, coercion_error::MutabilityMismatch{});

			// A `box` is copied or moved, a `ref` is only rebound.
			CoercionNode node = startNode(source, target);
			handOver(ctx, node);
			if (source.getMutability() != target.getMutability())
				node.append({ .kind = coercion_step::MutabilityChange{}, .result = target });
			return accepted(std::move(node));
		}

		/**
		 * @brief The value is packed as an alternative of a variant.
		 *
		 * Every variant alternative is asked about how well the value matches this alternative.
		 * Then either the best one is picked, ambiguity is reported or no alternative matched.
		 */
		SymbolTypeCoercion packIntoVariant(
			query::Context& ctx, const SymbolType<>& source, const SymbolType<>& target
		) {
			const VariantAbstractType        variant{ target.getType() };
			const std::vector<SymbolType<>>& alternatives = variant.getUnderlyingTypes();

			std::vector<Candidate>            candidates;
			std::vector<CoercionError::Cause> refusals;

			for (const auto& [index, alternative]:
			     std::views::zip(std::views::iota(0u), alternatives)) {
				const base::CRef<SymbolTypeCoercion> into = planPart(ctx, source, alternative);

				if (into->isRefused()) {
					refusals.push_back({
						.at    = { .in = CoercionPath::In::Alternative, .index = index },
						.error = into->getError(),
					});
					continue;
				}

				candidates.push_back({ .id = index, .rank = into->getRank() });
			}

			const CandidateChoice choice = bestCoercion(candidates);

			variant_match(choice) {
				variant_case_novalue(candidate_choice::NoCandidate) {
					return refused(
						source, target, coercion_error::IncompatibleTypes{}, std::move(refusals)
					);
				}
				variant_case(candidate_choice::Tied, tied) {
					return refused(
						source,
						target,
						coercion_error::AmbiguousCoercion{ .candidates = tied.candidates }
					);
				}
				variant_case(candidate_choice::Chosen, chosen) {
					// The value is coerced into the alternative and then packed.
					CoercionNode node = planPart(ctx, source, alternatives[chosen.id])->getRoot();
					node.target       = target;
					node.append({
						.kind   = coercion_step::VariantPack{ .alternative = chosen.id },
						.result = target,
					});
					return accepted(std::move(node));
				}
			}

			CORE_UNREACHABLE();
		}

		/**
		 * @brief The source is a `ref/`box` while the target is not. The pointee must be first read
		 * out and then coerced.
		 */
		SymbolTypeCoercion readThroughReference(
			query::Context& ctx, const SymbolType<>& source, const SymbolType<>& target
		) {
			// Check if we can coerce the derefed type.
			const SymbolType<>                   pointee = source.getPointeeSymbolType();
			const base::CRef<SymbolTypeCoercion> rest    = planPart(ctx, pointee, target);

			if (rest->isRefused()) {
				CoercionError error = rest->getError();
				error.source        = source;
				return SymbolTypeCoercion::refusedCoercion(source, std::move(error));
			}

			// Now create the coercion plan.
			CoercionNode node = startNode(source, target);
			node.append({ .kind = coercion_step::Deref{}, .result = pointee });
			for (const CoercionStep& step: rest->getRoot().steps) node.append(step);
			return accepted(std::move(node));
		}

		/**
		 * @brief A composite that is rebuilt out of its parts, each coerced on its own.
		 */
		SymbolTypeCoercion coerceElementwise(
			query::Context&                  ctx,
			const SymbolType<>&              source,
			const SymbolType<>&              target,
			const std::vector<SymbolType<>>& source_parts,
			const std::vector<SymbolType<>>& target_parts
		) {
			std::vector<CoercionNode>         parts;
			std::vector<CoercionError::Cause> refusals;
			parts.reserve(source_parts.size());

			for (const auto& [index, from, into]:
			     std::views::zip(std::views::iota(0u), source_parts, target_parts)) {
				const base::CRef<SymbolTypeCoercion> part = planPart(ctx, from, into);

				if (part->isRefused()) {
					refusals.push_back({
						.at    = { .in = CoercionPath::In::Component, .index = index },
						.error = part->getError(),
					});
					continue;
				}

				parts.push_back(part->getRoot());
			}

			if (not refusals.empty())
				return refused(
					source, target, coercion_error::SubPartRefused{}, std::move(refusals)
				);

			CoercionNode node = startNode(source, target);

			// OPT: A composite none of whose parts changes at runtime, like `(const i32, box i32)`
			// into `(i32, box i32)`, is only seen as the other type. It is handed over and
			// its mutability changed as a whole, rather then rebuilt for nothing.
			const auto only_changes_mutability = [](const CoercionNode& part) {
				return std::ranges::all_of(part.steps, [](const CoercionStep& step) {
					return v_matches(
						step.kind, coercion_step::HandOver, coercion_step::MutabilityChange
					);
				});
			};

			if (std::ranges::all_of(parts, only_changes_mutability)) {
				handOver(ctx, node);
				node.append({ .kind = coercion_step::MutabilityChange{}, .result = target });
				return accepted(std::move(node));
			}

			node.append({
				.kind   = coercion_step::Elementwise{ .parts = std::move(parts) },
				.result = target,
			});
			return accepted(std::move(node));
		}

		/// A tuple is rebuilt into a tuple of as many components, and a tuple of types is read as a
		/// tuple type.
		SymbolTypeCoercion tupleRule(
			query::Context& ctx, const SymbolType<>& source, const SymbolType<>& target
		) {
			const TupleAbstractType          source_tuple{ source.getType() };
			const std::vector<SymbolType<>>& components = source_tuple.getComponents();

			if (target.getType().getKind() == Kind::Meta) {
				for (const SymbolType<>& component: components)
					if (planPart(ctx, component, target)->isRefused())
						return refused(source, target, coercion_error::IncompatibleTypes{});
				return converted(ctx, source, target, coercion_step::LiftToType{});
			}

			if (target.getType().getKind() != Kind::Tuple)
				return refused(source, target, coercion_error::IncompatibleTypes{});

			const TupleAbstractType          target_tuple{ target.getType() };
			const std::vector<SymbolType<>>& targets = target_tuple.getComponents();
			if (targets.size() != components.size())
				return refused(source, target, coercion_error::IncompatibleTypes{});

			return coerceElementwise(ctx, source, target, components, targets);
		}

		/// A function converts to another one when the result and every parameter do.
		SymbolTypeCoercion functionRule(const SymbolType<>& source, const SymbolType<>& target) {
			if (target.getType().getKind() != Kind::Function)
				return refused(source, target, coercion_error::IncompatibleTypes{});

			const FunctionAbstractType from = source.getType();
			const FunctionAbstractType to   = target.getType();

			if ((not from.isPure() && to.isPure()) || (not from.isFree() && to.isFree())
			    || from.getParameterTypes().size() != to.getParameterTypes().size())
				return refused(source, target, coercion_error::IncompatibleTypes{});

			return refused(source, target, coercion_error::NotImplemented{});
		}

		/// An integer widens and never drops the sign.
		bool integerWidens(const AbstractType source, const AbstractType target) {
			using enum IntegralAbstractType::Signedness;

			// @TODO: #3631 An integer is not checked against zero implicitly for now, because
			// overload resolution picks that check in confusing places, like `1u64 == 2i64`. A
			// cast still does it.
			if (target.getKind() != Kind::Integral) return false;

			const IntegralAbstractType from = source;
			const IntegralAbstractType to   = target;

			if (to.getSize() <= from.getSize()) return false;
			return not(from.getSignedness() == Signed && to.getSignedness() == Unsigned);
		}

		/**
		 * @brief A classical value conversion.
		 */
		SymbolTypeCoercion convertValue(
			query::Context& ctx, const SymbolType<>& source, const SymbolType<>& target
		) {
			if (source.getType() == target.getType()) {
				CoercionNode node = startNode(source, target);
				handOver(ctx, node);
				return accepted(std::move(node));
			}

			const AbstractType from = source.getType();
			const AbstractType into = target.getType();

			switch (from.getKind()) {
			case Kind::Unit:
				// A unit value is read as the unit type.
				if (into.getKind() == Kind::Meta)
					return converted(ctx, source, target, coercion_step::LiftToType{});
				break;

			case Kind::Byte:
			case Kind::Char:
				// A byte is checked against the null byte, a character against '\0'.
				if (into.getKind() == Kind::Bool)
					return converted(ctx, source, target, coercion_step::ZeroCheck{});
				break;

			case Kind::Bool:
				// A boolean is widened into an integer.
				if (into.getKind() == Kind::Integral)
					return converted(ctx, source, target, coercion_step::Numeric{});
				break;

			case Kind::Integral:
				// Only a promotion into a wider integer is implicit and only the one that keeps the
				// sign.
				if (integerWidens(from, into))
					return converted(ctx, source, target, coercion_step::Numeric{});
				break;

			case Kind::Float:
				// Only a promotion into a wider float is implicit.
				if (into.getKind() == Kind::Float
				    && FloatAbstractType{ into }.getSize() > FloatAbstractType{ from }.getSize())
					return converted(ctx, source, target, coercion_step::Numeric{});
				break;

			case Kind::RawPointer:
				if (into.getKind() == Kind::Bool)
					return converted(ctx, source, target, coercion_step::ZeroCheck{});

				if (into.getKind() == Kind::RawPointer && RawPointerAbstractType{ from }.isMutable())
					return converted(ctx, source, target, coercion_step::MutabilityChange{});
				break;

			case Kind::Tuple:
				return tupleRule(ctx, source, target);

			case Kind::Function:
				return functionRule(source, target);

			case Kind::Void:
				CORE_PANIC("A void value should have been retyped earlier.");
			case Kind::Pointer:
			case Kind::ManyPointer:
			case Kind::CPointer:
			case Kind::Variant:
			case Kind::Slice:
			case Kind::StaticArray:
			case Kind::Class:
			case Kind::TypeTemplate:
			case Kind::Namespace:
			case Kind::Module:
			case Kind::Import:
			case Kind::Meta:
				// These can convert to nothing at this level.
				break;

			case Kind::COUNT:
				CORE_UNREACHABLE();
			}

			// @TODO: #3656 The user conversions should be resolved here.
			return refused(source, target, coercion_error::IncompatibleTypes{});
		}

		/**
		 * @brief The main type coercion logic.
		 */
		SymbolTypeCoercion planCoercion(
			query::Context& ctx, const SymbolType<>& source, const SymbolType<>& target
		) {
			// First check if we're retyping a `void` type.
			if (source.getType().getKind() == Kind::Void) return retypeVoid(source, target);

			// Secondly, validate coercions which keep the "pointer like" behaviour.
			if (target.getRefKind() != ReferenceKind::Direct)
				return keepReference(ctx, source, target);

			// Thirdly, we packing a value into a variant.
			if (target.getType().getKind() == Kind::Variant && source.getType() != target.getType())
				return packIntoVariant(ctx, source, target);

			// Fourthly, the classical coercions.
			if (source.getRefKind() != ReferenceKind::Direct)
				// Now we know that target is Direct.
				return readThroughReference(ctx, source, target);

			return convertValue(ctx, source, target);
		}
	}

	struct IMPLEMENT_QUERY(QuerySymbolTypeCoercion, SymbolTypeCoercion) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			return planCoercion(ctx, key.source, key.target);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolTypeCoercion);
}
