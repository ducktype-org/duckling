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

#include <set>

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

		class MatchSubject final {
		public:
			tsh::SymbolType<> whole_type;
			/// True if the reference kind of the subject is Direct.
			bool                     passed_by_value;
			tsh::VariantAbstractType variant;
			usize                    num_alternatives;
		};

		/**
		 * @brief Validate if the match subject is correct.
		 * @return Boolean flag with true if subject it is a direct and takes ownership, otherwise
		 * false.
		 */
		query::QResult<MatchSubject> validateMatchSubject(
			query::Context& ctx, CRef<code::Expr> expr, dia::StablePosition pos
		) {
			auto type = expr->expression_type.getSymbolType();
			if (type.getType().getKind() != tsh::Kind::Variant
			    or type.getRefKind() == tsh::ReferenceKind::Box) {
				logError(
					ctx,
					pos,
					"`Boxes and non-variant types are invalid to be passed to match. Got `",
					type.toString(),
					"`."
				);
				return query::Failed();
			}
			UNPACK_QRESULT(auto coercion =, canCoerce(ctx, expr->expression_type, type));
			if (coercion.isInvalid()) {
				logCoercionFailure(ctx, coercion, pos, {});
				return query::Failed();
			}

			bool passed_by_value                  = type.getRefKind() == tsh::ReferenceKind::Direct;
			tsh::VariantAbstractType variant_type = type.getType();
			usize                    num_alternatives = variant_type.getUnderlyingTypes().size();
			return { type, passed_by_value, variant_type, num_alternatives };
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
			if (match_subject.whole_type.getRefKind() == tsh::ReferenceKind::Box) {
				logError(ctx, pos, "Box type in the match case is invalid.");
				return query::Failed();
			}

			const auto& alternatives = match_subject.variant.getUnderlyingTypes();

			base::Optional<usize> found_index;

			for (usize i = 0; i < alternatives.size(); i++) {
				// If the subject is passed by value, the whole type has to be exactly the same.
				if (match_subject.passed_by_value) {
					if (alternatives[i] == constraint) found_index = i;
				} else {
					if (alternatives[i].getType() == constraint.getType()) found_index = i;
				}
			}

			if (found_index.empty()) {
				if (match_subject.passed_by_value) {
					logError(
						ctx,
						pos,
						"Failed to find the alternative of the variant for `",
						constraint.toString(),
						"`. The type has to match exactly the variant alternative."
					);
				} else {
					logError(
						ctx,
						pos,
						"Failed to find the alternative of the variant for `",
						constraint.toString()
					);
				}

				return query::Failed();
			}

			auto index = found_index.value();

			// Check if we can do the coercion from the alternative into the constraint type.
			// If the subject was passed by ref, then the cases also coerce from ref types.
			// If the subject was passed by value, then we have to materialize the case by value
			// - where the ownership will move from the variant into the case variable.
			// @TODO: #1488 sorry Piotrek, we will have to check const-ness here someday
			auto source_type = match_subject.variant.getUnderlyingTypes()[index].withMutability(
				match_subject.whole_type.getMutability()
			);
			tsh::ExpressionType<> expr_type{
				match_subject.passed_by_value
					? source_type
					: source_type.withReferenceKind(tsh::ReferenceKind::Ref),
				tsh::ValueCategory{ tsh::PrimaryCategory::Temporary }
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

		UNPACK_QRESULT_MOVE(
			auto subject_hout =, code::subExprFromPST(ctx, subject_pst->getExpr())
		);
		UNPACK_QRESULT(
			auto subject =, validateMatchSubject(ctx, subject_hout.ref(), subject_pst->getStablePosition())
		);

		// Lower all cases.
		std::vector<code::MatchExpr::Case> cases;
		base::Optional<tsh::SymbolType<>>  common_type;
		std::set<usize>                    covered;
		bool                               has_wildcard = false;

		for (auto match_case: match_expr->getCases()) {
			const auto case_position = match_case.unlock(ctx)->getStablePosition();
			auto       flow          = match_case.unlock(ctx)->getPattern().unlock(ctx);

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
				if (!covered.insert(alternative_index.value()).second) {
					logError(
						ctx,
						case_position,
						"Alternative `",
						constraint_type.value().toString(),
						"` is matched by more than one case."

					);
					return query::Failed();
				}
			} else {
				has_wildcard = true;
				if (subject.passed_by_value) {
					logError(
						ctx,
						case_position,
						"When the value expression is passed to the match, it is invalid to use "
					    "the wildcard pattern in cases"
					);
					return query::Failed();
				}
			}
			auto result_holder = branches.front().result.unlock(ctx);
			UNPACK_QRESULT_CREF_TO_BOX(
				auto result =, ctx.query<QueryHoutOfExpr>({ result_holder->getExpr() })
			);

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

			cases.emplace_back(Shorthand::matchCase(alternative_index, binding_sym, result->clone())
			);
		}

		if (!has_wildcard && covered.size() < subject.num_alternatives) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				base::strConcat(
					"`match` is not exhaustive: it covers ",
					covered.size(),
					" of ",
					subject.num_alternatives,
					" alternatives and has no wildcard (`case _`)."
				),
				match_position
			));
			return query::Failed();
		}

		// The subject is handed over as a reference and evaluated once by the MIR lowering.
		// Referencing a temporary is not expressible in the surface language yet, but it is
		// well defined and is what a match over a temporary needs, so it is built directly.
		return Box<code::Expr>(withOrigin(
			code::pstOrigin(match_expr),
			s.matchExpr(std::move(subject_hout), std::move(cases))
		));
	}
}
