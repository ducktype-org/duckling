#include "match.hpp"

#include "helios/tsh/symbol_type.hpp"
#include "helios_private/hout_creation/expressions/coercions/coercions.hpp"
#include "helios_private/hout_creation/expressions/hout_of_subexpr.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/match_case.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/patterns/patterns.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>

#include "diagnostic/stable_position.hpp"
#include <diagnostic/placeholder.hpp>

#include <algorithm>
#include <vector>

namespace compiler::helios::desugaring {
	using code::shorthands::Shorthand;
	using code::shorthands::withOrigin;

	namespace {
		template<typename... MsgParts>
		void logNYI(query::Context& ctx, dia::StablePosition position, MsgParts&&... what) {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				base::strConcat(std::forward<MsgParts>(what)...), position
			));
		}

		template<typename... MsgParts>
		void logError(query::Context& ctx, dia::StablePosition position, MsgParts&&... what) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				base::strConcat(std::forward<MsgParts>(what)...), position
			));
		}

		template<typename... MsgParts>
		void logWarning(query::Context& ctx, dia::StablePosition position, MsgParts&&... what) {
			ctx.logInt(makeBox<dia::PlaceholderWarning>(
				base::strConcat(std::forward<MsgParts>(what)...), position
			));
		}

		class MatchSubject final {
		public:
			Box<code::Expr>          expr;
			tsh::SymbolType<>        whole_type;
			bool                     moves_ownership;
			tsh::VariantAbstractType variant;
			usize                    num_alternatives;
		};

		/**
		 * @brief Validate if the match subject is correct.
		 * @return The validated subject: the (possibly coerced) expression, its type, its variant
		 * type and whether the match takes the ownership of it.
		 */
		query::QResult<MatchSubject> validateMatchSubject(
			query::Context& ctx, Box<code::Expr> expr, dia::StablePosition pos
		) {
			auto type = expr->expression_type.getSymbolType();
			if (type.getRefKind() == tsh::ReferenceKind::Box) {
				logError(
					ctx, pos, "`match` cannot look through a box. Got `", type.toString(), "`."
				);
				return query::Failed();
			}
			if (type.getType().getKind() != tsh::Kind::Variant) {
				logNYI(ctx, pos, "`match` on a non-variant type. Got `", type.toString(), "`.");
				return query::Failed();
			}
			// The coercion into the subject's own type looks like a no-op, but it is the check that
			// makes `match (v)` on an owning variant report "use `copy` or `move`" - it is the
			// implicit copy of a `Direct` place that is rejected in there.
			UNPACK_QRESULT_MOVE(auto return_expr =, coerceFromBox(ctx, std::move(expr), type, pos));

			bool moves_ownership                  = type.getRefKind() == tsh::ReferenceKind::Direct;
			tsh::VariantAbstractType variant_type = type.getType();
			usize                    num_alternatives = variant_type.getUnderlyingTypes().size();
			return {
				std::move(return_expr), type, moves_ownership, variant_type, num_alternatives,
			};
		}

		/**
		 * @brief Validate the match case type constraint.
		 * @return The alternative index of the type.
		 */
		query::QResult<usize> validateMatchCaseConstraint(
			query::Context&          ctx,
			const MatchSubject&      match_subject,
			const tsh::SymbolType<>& constraint,
			dia::StablePosition      pos
		) {
			if (not match_subject.moves_ownership
			    and constraint.getRefKind() == tsh::ReferenceKind::Box) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"This `match` only borrows its subject, so a case cannot take a `box` payload "
					"out of it.",
					pos,
					"Bind it with `ref` instead, or hand the variant over with `move`."
				));
				return query::Failed();
			}

			const auto& alternatives = match_subject.variant.getUnderlyingTypes();

			base::Optional<usize> found_index;

			for (usize i = 0; i < alternatives.size(); i++) {
				// If the subject moves ownership, the case type has to be exactly the same.
				if (match_subject.moves_ownership) {
					if (alternatives[i] == constraint) {
						found_index = i;
						break;
					}
				} else if (alternatives[i].getType() == constraint.getType()) {
					found_index = i;
					break;
				}
			}

			if (found_index.empty()) {
				if (match_subject.moves_ownership) {
					auto error_msg = makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Failed to find the alternative of the variant for `",
							constraint.toString(),
							"`. The type has to match exactly the variant alternative."
						),
						pos
					);
					error_msg->addAttachedMessage(makeBox<dia::PlaceholderNote>(
						"You can also pass the expression by reference by adding `&` prefix.",
						match_subject.expr->origin.getStablePosition()
					));
					ctx.logInt(std::move(error_msg));
				} else {
					logError(
						ctx,
						pos,
						"Failed to find the alternative of the variant for `",
						constraint.toString(),
						"`."
					);
				}

				return query::Failed();
			}

			auto index = found_index.value();

			// Note: we don't do here an actual coercion of some expression (it is done in MIR
			// by adding `derefs` manually) but we check here if the coercion done in MIR is
			// valid, so we must construct a proxy expression type.
			// @TODO: #1488 sorry Piotrek, we will have to check const-ness here someday, maybe
			// it will be automatic by using a coercion.
			auto source_type
				= alternatives[index].withMutability(match_subject.whole_type.getMutability());
			// If the subject moves ownership we keep the original ref kind of the variant
			// alternative. If not, then the cases coerce from ref type (subject is also a
			// reference) and we check here if adding "deref" will be valid. The "derefs" are
			// added in MIR.
			if (not match_subject.moves_ownership)
				source_type = source_type.withReferenceKind(tsh::ReferenceKind::Ref);

			tsh::ExpressionType<> expr_type{
				source_type, match_subject.expr->expression_type.getValueCategory()
			};

			UNPACK_QRESULT(auto coercion =, canCoerce(ctx, expr_type, constraint));
			if (coercion.isInvalid()) {
				logCoercionFailure(ctx, coercion, pos, {});
				return query::Failed();
			}
			return index;
		}
	}

	query::QResult<Box<code::Expr>> desugarMatch(
		query::Context& ctx, pst::Access<pst::expr::MatchExpr> match_expr
	) {
		const Shorthand s{ ctx };
		const auto      match_position = match_expr->getStablePosition();
		auto            subject_pst    = match_expr->getValueToMatch().unlock(ctx);

		UNPACK_QRESULT_MOVE(auto subject_hout =, code::subExprFromPST(ctx, subject_pst->getExpr()));
		UNPACK_QRESULT_MOVE(
			auto subject =,
			validateMatchSubject(ctx, std::move(subject_hout), subject_pst->getStablePosition())
		);

		// Lower all cases.
		std::vector<code::MatchExpr::Case>  cases;
		base::Optional<tsh::SymbolType<>>   common_type;
		std::vector<bool>                   covered(subject.num_alternatives, false);
		bool                                has_wildcard = false;
		base::Optional<dia::StablePosition> wildcard_position;

		for (auto match_case: match_expr->getCases()) {
			const auto case_position = match_case.unlock(ctx)->getStablePosition();
			auto       flow          = match_case.unlock(ctx)->getPattern().unlock(ctx);

			// The wildcard always matches, so the lowering enters it unconditionally and never
			// reaches whatever is listed after it.
			if_opt_some(wildcard_position, position) {
				logWarning(
					ctx,
					case_position,
					"This `case` is never entered, because the wildcard `case _` above it always "
					"matches. The wildcard has to be the last case."
				);
				ctx.logInt(makeBox<dia::PlaceholderNote>("The wildcard case is here.", position));
			}

			if (flow->getAsIdentifier().has_value()) {
				logNYI(ctx, case_position, "`as` bindings in match patterns.");
				return query::Failed();
			}

			auto branches = match_case.unlock(ctx)->getBranches();
			if (branches.size() != 1 || branches.front().condition.has_value()) {
				logNYI(ctx, case_position, "Guarded match case branches (`case ... if ...`).");
				return query::Failed();
			}

			// Resolve the pattern: binding with a constraint, constrained wildcard, or
			// a plain wildcard.
			auto pattern    = flow->getPattern().unlock(ctx);
			auto constraint = flow->getTypeConstraint();

			base::Optional<usize>             alternative_index;
			base::Optional<SymID>             binding_sym;
			base::Optional<tsh::SymbolType<>> constraint_type;

			if (auto binding_opt = pattern.dynamicCast<pst::BindingPattern>()) {
				if (!constraint.has_value()) {
					logNYI(
						ctx,
						case_position,
						"Match pattern bindings without a type constraint (`case x : T`)."
					);
					return query::Failed();
				}
				binding_sym = ctx.query<QuerySymbolOfSTMT>({ binding_opt.value()->getName() })
				                  .valueOrThrow();
			} else if (not pattern.dynamicCast<pst::WildcardPattern>().has_value()) {
				logNYI(
					ctx,
					case_position,
					base::strConcat("Match pattern kind: ", pattern->elementType(), ".")
				);
				return query::Failed();
			}


			if (constraint.has_value()) {
				auto pst_expr = constraint.value().unlock(ctx)->getExpr().unlock(ctx);
				UNPACK_QRESULT(auto constraint_ctv =, getTypeCTVFromPST(ctx, pst_expr));
				constraint_type = constraint_ctv.get<tsh::SymbolType<>>().value();

				UNPACK_QRESULT(
					alternative_index =,
					validateMatchCaseConstraint(
						ctx, subject, constraint_type.value(), pst_expr->getStablePosition()
					)
				);
				if (covered[alternative_index.value()]) {
					logError(
						ctx,
						case_position,
						"Alternative `",
						constraint_type.value().toString(),
						"` is matched by more than one case."

					);
					return query::Failed();
				}
				covered[alternative_index.value()] = true;
			} else {
				has_wildcard                     = true;
				wildcard_position                = case_position;
				auto not_covered_with_destructor = [&] -> base::Optional<tsh::SymbolType<>> {
					auto& alternatives = subject.variant.getUnderlyingTypes();
					for (usize i{ 0 }; i < subject.num_alternatives; i++) {
						if (covered[i]) continue;

						if (not alternatives[i].isTriviallyDestructible(ctx))
							return alternatives[i];
					}
					return {};
				};
				if (subject.moves_ownership) {
					if_opt_some(not_covered_with_destructor(), alternative) {
						ctx.logInt(makeBox<dia::PlaceholderError>(
							base::strConcat(
								"A bare `case _` binds nothing, so the payload it covers would "
								"never be destroyed, and the alternative `",
								alternative.toString(),
								"` it covers has a destructor."
							),
							case_position,
							base::strConcat(
								"Give it its own case, or constrain this one with `case _ : ",
								alternative.toString(),
								"`."
							)
						));
						return query::Failed();
					}
				}
			}
			auto result_holder = branches.front().result.unlock(ctx);
			UNPACK_QRESULT_MOVE(auto result =, code::subExprFromPST(ctx, result_holder->getExpr()));

			// A match yields one value, so every case has to agree on its type. There is no
			// common-type inference, so anything else is an error the user has to resolve.
			const auto result_type = result->expression_type.getSymbolType();
			if (!common_type.has_value()) common_type = result_type;
			if (common_type.value() != result_type) {
				logError(
					ctx,
					case_position,
					"All `match` cases have to be of the same type, but this one is `",
					result_type.toString(),
					"` while an earlier one is `",
					common_type.value().toString(),
					"`."
				);
				return query::Failed();
			}

			UNPACK_QRESULT_MOVE(
				result =,
				coerceFromBox(
					ctx, std::move(result), result_type, result_holder->getStablePosition()
				)
			);

			cases.emplace_back(Shorthand::matchCase(
				alternative_index, constraint_type, binding_sym, std::move(result)
			));
		}

		const auto covered_count = static_cast<usize>(std::ranges::count(covered, true));
		if (!has_wildcard && covered_count < subject.num_alternatives) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				base::strConcat(
					"`match` is not exhaustive: it covers ",
					covered_count,
					" of ",
					subject.num_alternatives,
					" alternatives and has no wildcard (`case _`)."
				),
				match_position
			));
			return query::Failed();
		}

		return Box<code::Expr>(withOrigin(
			code::pstOrigin(match_expr), s.matchExpr(std::move(subject.expr), std::move(cases))
		));
	}
}
