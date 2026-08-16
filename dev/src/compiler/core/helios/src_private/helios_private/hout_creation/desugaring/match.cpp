#include "match.hpp"

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
	using code::shorthands::StmtPack;
	using code::shorthands::withOrigin;

	namespace {
		void logMatchNYI(query::Context& ctx, std::string what, dia::StablePosition position) {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(std::move(what), position));
		}

		/**
		 * @brief Finds the alternative index matched by a case's type constraint.
		 *
		 * An exact match wins, so a variant that really does list `ref T` among its
		 * alternatives is still reachable. Only when that fails is a `ref T` constraint
		 * treated as "bind the `T` alternative by reference".
		 */
		base::Optional<usize> findAlternativeIndex(
			const tsh::VariantAbstractType& variant_type, const tsh::SymbolType<>& constraint
		) {
			const auto& alternatives = variant_type.getUnderlyingTypes();

			for (usize i = 0; i < alternatives.size(); i++)
				if (alternatives[i].getRefKind() == constraint.getRefKind()
				    && alternatives[i].getType() == constraint.getType())
					return i;

			for (usize i = 0; i < alternatives.size(); i++)
				if (alternatives[i].getRefKind() == tsh::ReferenceKind::Direct
				    && alternatives[i].getType() == constraint.getType())
					return i;

			return {};
		}
	}

	namespace {
		/**
		 * @brief Compiles the match subject.
		 *
		 * The subject is left exactly as it was written. The match wants a reference to it, and
		 * `refof` already produces one from any reference kind: it collapses to the pointer that
		 * is already there for a `ref` or `box` subject, and takes the address of a direct one.
		 * Normalizing to a direct value here would only mean dereferencing a subject that the
		 * caller has to take the address of again.
		 *
		 * @return An empty optional when the subject fails to compile.
		 */
		base::Optional<Box<code::Expr>> getSubjectHout(
			query::Context& ctx, pst::Access<pst::expr::MatchExpr> match_expr
		) {
			auto subject_res = ctx.query<QueryHoutOfExpr>(
				{ match_expr->getValueToMatch().unlock(ctx)->getExpr() }
			);
			if (subject_res->hasFailed()) return {};
			return subject_res->valueOrThrow()->clone();
		}

	}

	base::Optional<code::BlockStmt> desugarMatch(
		query::Context&                   ctx,
		pst::Access<pst::expr::MatchExpr> match_expr,
		tsh::SymbolType<>                 expected_type,
		const MatchResultSink&            sink
	) {
		const Shorthand s{ ctx };
		const auto      match_position = match_expr->getStablePosition();

		auto subject_hout_opt = getSubjectHout(ctx, match_expr);
		if (!subject_hout_opt.has_value()) return {};
		Box<code::Expr> subject_hout = std::move(subject_hout_opt.value());

		const auto subject_type = subject_hout->expression_type.getSymbolType();
		if (subject_type.getType().getKind() != tsh::Kind::Variant) {
			logMatchNYI(
				ctx,
				base::strConcat("`match` over non-variant type: ", subject_type.toString(), "."),
				match_position
			);
			return {};
		}
		const tsh::VariantAbstractType variant_type     = subject_type.getType();
		const usize                    num_alternatives = variant_type.getUnderlyingTypes().size();

		// Lower all cases.
		std::vector<code::MatchStmt::Case> cases;
		std::set<usize>                    covered;
		bool                               has_wildcard = false;

		for (auto match_case: match_expr->getCases()) {
			const auto case_position = match_case.unlock(ctx)->getStablePosition();
			auto       flow          = match_case.unlock(ctx)->getPattern().unlock(ctx);

			if (flow->getAsIdentifier().has_value()) {
				logMatchNYI(ctx, "`as` bindings in match patterns.", case_position);
				return {};
			}

			auto branches = match_case.unlock(ctx)->getBranches();
			if (branches.size() != 1 || branches.front().condition.has_value()) {
				logMatchNYI(ctx, "Guarded match case branches (`case ... if ...`).", case_position);
				return {};
			}

			// Resolve the pattern: binding with a constraint, constrained wildcard, or
			// a plain wildcard.
			auto                  pattern    = flow->getPattern().unlock(ctx);
			auto                  constraint = flow->getTypeConstraint();
			base::Optional<usize> alternative_index;
			base::Optional<SymID> binding_sym;

			if (auto binding_opt = pattern.dynamicCast<pst::BindingPattern>()) {
				if (!constraint.has_value()) {
					logMatchNYI(
						ctx,
						"Match pattern bindings without a type constraint (`case x : T`).",
						case_position
					);
					return {};
				}
				binding_sym = ctx.query<QuerySymbolOfSTMT>({ binding_opt.value()->getName() })
				                  .valueOrThrow();
			} else if (!pattern.dynamicCast<pst::WildcardPattern>().has_value()) {
				logMatchNYI(
					ctx,
					base::strConcat("Match pattern kind: ", pattern->elementType(), "."),
					case_position
				);
				return {};
			}

			if (constraint.has_value()) {
				auto constraint_ctv
					= getTypeCTVFromPST(ctx, constraint.value().unlock(ctx)->getExpr().unlock(ctx));
				if (constraint_ctv.hasFailed()) return {};
				const auto constraint_type
					= constraint_ctv.valueOrThrow().get<tsh::SymbolType<>>().value();

				alternative_index = findAlternativeIndex(variant_type, constraint_type);
				if (!alternative_index.has_value()) {
					ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Type `",
							constraint_type.toString(),
							"` is not an alternative of the matched variant `",
							subject_type.toString(),
							"`."
						),
						case_position
					));
					return {};
				}
				if (!covered.insert(alternative_index.value()).second) {
					ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Alternative `",
							constraint_type.toString(),
							"` is matched by more than one case."
						),
						case_position
					));
					return {};
				}
			} else {
				has_wildcard = true;
			}

			// The binding is filled in from the pointer the alternative test already
			// produced. A `ref T` binding aliases the payload, a direct one copies it.
			if (binding_sym.has_value()) {
				const auto binding_type
					= ctx.query<QueryTypeOfSymbol>(binding_sym.value())->valueOrThrow();

				// Copying a payload that owns something would leave the subject owning it
				// too, so both would destroy it. Such alternatives have to be bound by
				// reference instead.
				if (binding_type.getRefKind() != tsh::ReferenceKind::Ref
				    && !binding_type.isTriviallyCopyable(ctx)) {
					ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Alternative `",
							binding_type.toString(),
							"` cannot be bound by value because it is not trivially copyable. "
							"Bind it by reference instead: `case ",
							name(binding_sym.value()).strView(),
							" : ref ",
							binding_type.getType().toString(),
							"`."
						),
						case_position
					));
					return {};
				}
			}

			auto result_holder = branches.front().result.unlock(ctx);
			auto result_coerced
				= getHoutOfExprWithExpectedType(ctx, result_holder->getExpr(), expected_type);
			if (result_coerced.hasFailed()) return {};

			cases.emplace_back(Shorthand::matchCase(
				alternative_index, binding_sym, sink(std::move(result_coerced).valueOrThrow())
			));
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
			return {};
		}

		// The subject is handed over as a reference and evaluated once by the MIR lowering, so
		// no generated local is needed here. Referencing a temporary is not expressible in the
		// surface language yet, but it is well defined and is what a match over a temporary
		// needs, so the desugaring builds it directly.
		const auto match_origin = code::pstOrigin(match_expr);
		StmtPack   outer{
            withOrigin(
                match_origin,
                Shorthand::matchStmt(s.refOf(std::move(subject_hout)), std::move(cases))
            ),
		};

		return code::BlockStmt(match_origin, std::move(outer).toCodeBlock());
	}
}
