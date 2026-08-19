#include "match.hpp"

#include "helios/tsh/mutability.hpp"

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

#include <diagnostic/placeholder.hpp>

#include <set>

namespace compiler::helios::desugaring {
	using code::shorthands::Shorthand;
	using code::shorthands::withOrigin;

	namespace {
		void logMatchNYI(query::Context& ctx, std::string what, dia::StablePosition position) {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(std::move(what), position));
		}

		/**
		 * @brief Finds the alternative index matched by a case's type constraint.
		 *
		 * Only the underlying type decides which alternative is meant; the constraint's
		 * reference kind picks how the payload is bound, not what is matched. A variant may
		 * not list one type twice, so this stays unambiguous.
		 */
		base::Optional<usize> findAlternativeIndex(
			const tsh::VariantAbstractType& variant_type, const tsh::SymbolType<>& constraint
		) {
			const auto& alternatives = variant_type.getUnderlyingTypes();

			for (usize i = 0; i < alternatives.size(); i++)
				if (alternatives[i].getType() == constraint.getType()) return i;

			return {};
		}
	}

	query::QResult<Box<code::Expr>> desugarMatch(
		query::Context& ctx, pst::Access<pst::expr::MatchExpr> match_expr
	) {
		const Shorthand s{ ctx };
		const auto      match_position = match_expr->getStablePosition();

		UNPACK_QRESULT_CREF_TO_BOX(
			auto subject_hout =,
			ctx.query<QueryHoutOfExpr>({ match_expr->getValueToMatch().unlock(ctx)->getExpr() })
		);

		const auto subject_type = subject_hout->expression_type.getSymbolType();
		if (subject_type.getType().getKind() != tsh::Kind::Variant) {
			logMatchNYI(
				ctx,
				base::strConcat("`match` over non-variant type: ", subject_type.toString(), "."),
				match_position
			);
			return query::Failed();
		}
		const tsh::VariantAbstractType variant_type     = subject_type.getType();
		const usize                    num_alternatives = variant_type.getUnderlyingTypes().size();

		// Lower all cases.
		std::vector<code::MatchExpr::Case> cases;
		base::Optional<tsh::SymbolType<>>  common_type;
		std::set<usize>                    covered;
		bool                               has_wildcard = false;

		for (auto match_case: match_expr->getCases()) {
			const auto case_position = match_case.unlock(ctx)->getStablePosition();
			auto       flow          = match_case.unlock(ctx)->getPattern().unlock(ctx);

			if (flow->getAsIdentifier().has_value()) {
				logMatchNYI(ctx, "`as` bindings in match patterns.", case_position);
				return query::Failed();
			}

			auto branches = match_case.unlock(ctx)->getBranches();
			if (branches.size() != 1 || branches.front().condition.has_value()) {
				logMatchNYI(ctx, "Guarded match case branches (`case ... if ...`).", case_position);
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
					logMatchNYI(
						ctx,
						"Match pattern bindings without a type constraint (`case x : T`).",
						case_position
					);
					return query::Failed();
				}
				binding_sym = ctx.query<QuerySymbolOfSTMT>({ binding_opt.value()->getName() })
				                  .valueOrThrow();
			} else if (!pattern.dynamicCast<pst::WildcardPattern>().has_value()) {
				logMatchNYI(
					ctx,
					base::strConcat("Match pattern kind: ", pattern->elementType(), "."),
					case_position
				);
				return query::Failed();
			}

			if (constraint.has_value()) {
				UNPACK_QRESULT(
					auto constraint_ctv =,
					getTypeCTVFromPST(ctx, constraint.value().unlock(ctx)->getExpr().unlock(ctx))
				);
				constraint_type = constraint_ctv.get<tsh::SymbolType<>>().value();

				alternative_index = findAlternativeIndex(variant_type, constraint_type.value());
				if (!alternative_index.has_value()) {
					ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Type `",
							constraint_type.value().toString(),
							"` is not an alternative of the matched variant `",
							subject_type.toString(),
							"`."
						),
						case_position
					));
					return query::Failed();
				}
				if (!covered.insert(alternative_index.value()).second) {
					ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Alternative `",
							constraint_type.value().toString(),
							"` is matched by more than one case."
						),
						case_position
					));
					return query::Failed();
				}

				if (constraint_type->getRefKind() == tsh::ReferenceKind::Box)
					logMatchNYI(ctx, "Box types in cases.", case_position);
			} else {
				has_wildcard = true;
			}


			if (binding_sym.has_value()) {
				if (constraint_type->getRefKind() != tsh::ReferenceKind::Ref
				    and not constraint_type->isTriviallyCopyable(ctx)) {
					ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Alternative `",
							constraint_type->toString(),
							"` cannot be bound by value because it is not trivially copyable. "
							"Bind it by reference instead: `case ",
							name(binding_sym.value()).strView(),
							" : ref ",
							constraint_type->getType().toString(),
							"`."
						),
						case_position
					));
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
				ctx.logInt(makeBox<dia::PlaceholderError>(
					base::strConcat(
						"All `match` cases have to be of the same type, but this one is `",
						result_type.toString(),
						"` while an earlier one is `",
						common_type.value().toString(),
						"`."
					),
					case_position
				));
				return query::Failed();
			}

			cases.emplace_back(Shorthand::matchCase(alternative_index, binding_sym, result->clone())
			);
		}

		if (!has_wildcard && covered.size() < num_alternatives) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				base::strConcat(
					"`match` is not exhaustive: it covers ",
					covered.size(),
					" of ",
					num_alternatives,
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
			s.matchExpr(s.refOf(subject_hout->clone()), std::move(cases))
		));
	}
}
