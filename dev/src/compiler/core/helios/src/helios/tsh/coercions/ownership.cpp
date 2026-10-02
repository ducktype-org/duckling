/**
 * @file ownership.cpp
 * @brief The value layer, `coercionOf`, which decides every `HandOver` of a plan for one value.
 *
 * The plan already says what happens to the value and where it is taken, so this layer never asks
 * what a step does. It only follows which value each step is about: a `Deref` reads the pointee
 * out, an `Elementwise` hands each part the value it is about, and every other step builds a new
 * value that belongs to the coercion.
 */

#include "../types.hpp"
#include "../value_category.hpp"
#include "passing.hpp"
#include "queries.hpp"

#include <helios/tsh/coercions/coercion_error.hpp>
#include <helios/tsh/coercions/coercion_node.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>

#include <ranges>
#include <utility>
#include <variant>
#include <vector>

namespace compiler::tsh::coercions {
	namespace {
		/// A node decided for one value, or why it may not be.
		using DecidedNode = std::variant<CoercionNode, CoercionError>;
		/// The parts of a node decided for the parts of one value, or why they may not be.
		using DecidedParts = std::variant<std::vector<CoercionNode>, CoercionError>;

		CoercionError refusalOf(
			const CoercionNode&               node,
			CoercionFailure                   reason,
			std::vector<CoercionError::Cause> causes = {}
		) {
			return CoercionError{
				.reason = std::move(reason),
				.source = node.source,
				.target = node.target,
				.causes = std::move(causes),
			};
		}

		/**
		 * @brief The value @p step produced, created by a coercion so it's a temporary.
		 */
		ValueSource producedBy(const CoercionStep& step) {
			return ValueSource::singleValueSource({
				step.result,
				ValueCategory(PrimaryCategory::Temporary),
			});
		}

		DecidedNode decideNode(query::Context& ctx, const CoercionNode& planned, ValueSource value);

		/**
		 * @brief Enriches @p planned_parts with ownership decisions base on @p value, or the
		 * refusal of @p whole naming every part that refused.
		 */
		DecidedParts decideParts(
			query::Context&                  ctx,
			const CoercionNode&              whole,
			const std::vector<CoercionNode>& planned_parts,
			const ValueSource&               value
		) {
			std::vector<CoercionNode>         parts;
			std::vector<CoercionError::Cause> refusals;
			parts.reserve(planned_parts.size());

			for (const auto& [index, planned]:
			     std::views::zip(std::views::iota(usize{ 0 }), planned_parts)) {
				DecidedNode part = decideNode(ctx, planned, value.partAt(index));

				v_if_matches(part, CoercionError, refusal) {
					refusals.push_back({
						.at    = { .in = CoercionPath::In::Component, .index = index },
						.error = std::move(*refusal),
					});
					continue;
				}

				parts.push_back(std::move(v_get(part, CoercionNode)));
			}

			if (not refusals.empty())
				return refusalOf(whole, coercion_error::SubPartRefused{}, std::move(refusals));
			return parts;
		}

		/**
		 * @brief The plan for handing each part of a value of @p type over into its own place.
		 */
		std::vector<CoercionNode> planPartsInPlace(query::Context& ctx, const SymbolType<>& type) {
			std::vector<CoercionNode> plans;

			const TupleAbstractType tuple{ type.getType() };
			for (const SymbolType<>& slot: tuple.getComponents()) {
				const base::CRef<SymbolTypeCoercion> plan
					= ctx.query<QuerySymbolTypeCoercion>({ slot, slot });
				CORE_ASSERT(plan->isValid(), "A type always coerces into itself.");
				plans.push_back(plan->getRoot());
			}

			return plans;
		}

		/**
		 * @brief Decides the `HandOver` @p step of @p planned for @p value, into @p decided.
		 * This is where the user may have to write `copy` or `move` themselves.
		 */
		base::Optional<CoercionError> decideHandOver(
			query::Context&     ctx,
			const CoercionNode& planned,
			const CoercionStep& step,
			const ValueSource&  value,
			CoercionNode&       decided
		) {
			CORE_ASSERT(
				value.getValue().getSymbolType() == step.result,
				"A value is handed over as what it is at that point."
			);

			// A value built out of parts, like a tuple literal, is not a value yet: each part is
			// handed over into its own place in it. Handing it over whole would copy a local `box`
			// in a tuple literal by its bytes.
			if (value.isBuiltOutOfParts()) {
				DecidedParts parts
					= decideParts(ctx, planned, planPartsInPlace(ctx, step.result), value);

				v_if_matches(parts, CoercionError, refusal) { return std::move(*refusal); }

				std::vector<CoercionNode> handed_over
					= std::move(v_get(parts, std::vector<CoercionNode>));
				decided.append({
					.kind   = coercion_step::Elementwise{ .parts = std::move(handed_over) },
					.result = step.result,
				});
				return {};
			}

			switch (passingMethod(ctx, value.getValue())) {
			case PassingMethod::ByteCopy:
				return {};
			case PassingMethod::ImplicitMove:
				decided.append({ .kind = coercion_step::ImplicitMove{}, .result = step.result });
				return {};
			case PassingMethod::ExplicitCopyOrMove:
				return refusalOf(planned, coercion_error::RequiresExplicitCopyMove{});
			case PassingMethod::NotCopyable:
				return refusalOf(planned, coercion_error::TypeNotCopyable{});
			}

			CORE_UNREACHABLE();
		}

		/**
		 * @brief Enriches @p planned_node with ownership information based on @p value
		 * Changes every `HandOver` step of the @p planned_node into an `ImplicitMove` or a refusal.
		 */
		DecidedNode decideNode(
			query::Context& ctx, const CoercionNode& planned_node, ValueSource value
		) {
			CoercionNode decided{
				.source = planned_node.source,
				.target = planned_node.target,
				.steps  = {},
				.rank   = CoercionRank::identity(),
			};
			decided.steps.reserve(planned_node.steps.size());

			for (const CoercionStep& step: planned_node.steps) {
				// A `Deref` only reads the pointee out, so the steps after it are about that.
				if (v_matches(step.kind, coercion_step::Deref)) {
					decided.append(step);
					value = value.dereferenced();
					continue;
				}

				if (v_matches(step.kind, coercion_step::HandOver)) {
					if (base::Optional<CoercionError> refusal
					    = decideHandOver(ctx, planned_node, step, value, decided);
					    refusal.has_value())
						return std::move(refusal.value());
					value = producedBy(step);
					continue;
				}

				// An `Elementwise` takes the value apart, so its parts are handed over instead.
				v_if_matches(step.kind, coercion_step::Elementwise, elementwise) {
					DecidedParts parts = decideParts(ctx, planned_node, elementwise->parts, value);

					v_if_matches(parts, CoercionError, refusal) { return std::move(*refusal); }

					std::vector<CoercionNode> decided_parts
						= std::move(v_get(parts, std::vector<CoercionNode>));
					decided.append({
						.kind   = coercion_step::Elementwise{ .parts = std::move(decided_parts) },
						.result = step.result,
					});
					value = producedBy(step);
					continue;
				}

				// Every other step builds a new value out of the one it was given.
				decided.append(step);
				value = producedBy(step);
			}

			return decided;
		}
	}

	Coercion coercionOf(query::Context& ctx, const ValueSource& source, const SymbolType<>& target) {
		const ExpressionType<>&   value = source.getValue();
		const SymbolTypeCoercion& plan
			= *ctx.query<QuerySymbolTypeCoercion>({ value.getSymbolType(), target });

		// If the `SymbolTypeCoercion` failed we return it's error so we don't override it with the
		// ownership errors.
		if (plan.isRefused()) return Coercion::refusedCoercion(value, plan.getError());

		DecidedNode decided = decideNode(ctx, plan.getRoot(), source);

		variant_match(decided) {
			variant_case(CoercionNode, success) return Coercion::successfulCoercion(
				value, std::move(v_get(decided, CoercionNode))
			);
			variant_case(CoercionError, error) return Coercion::refusedCoercion(
				value, std::move(error)
			);
		}
		CORE_UNREACHABLE();
	}
}
