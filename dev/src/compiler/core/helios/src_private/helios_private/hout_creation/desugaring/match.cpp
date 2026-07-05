#include "match.hpp"

#include <diagnostic_interactive/placeholder.hpp>
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
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>

#include <set>

namespace compiler::helios::desugaring {
	namespace {
		void logMatchNYI(query::Context& ctx, std::string what, dia_int::StablePosition position) {
			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(std::move(what), position));
		}

		/**
		 * @brief Finds the alternative index of @p alternative_type in @p variant_type.
		 */
		base::Optional<usize> findAlternativeIndex(
			const tsh::VariantAbstractType& variant_type, const tsh::SymbolType<>& alternative_type
		) {
			const auto& alternatives = variant_type.getUnderlyingTypes();
			for (usize i = 0; i < alternatives.size(); i++)
				if (alternatives[i].getRefKind() == tsh::ReferenceKind::Direct
				    && alternatives[i].getType() == alternative_type.getType())
					return i;
			return {};
		}
	}

	namespace {
		/**
		 * @brief Compiles the match subject and normalizes it to a direct value.
		 * @return An empty optional when the subject fails to compile.
		 */
		base::Optional<Box<code::Expr>> getSubjectHout(
			query::Context& ctx, pst::Access<pst::expr::MatchExpr> match_expr
		) {
			auto subject_res = ctx.query<QueryHoutOfExpr>(
				{ match_expr->getValueToMatch().unlock(ctx)->getExpr() }
			);
			if (subject_res->hasFailed()) return {};
			Box<code::Expr> subject_hout = subject_res->valueOrThrow()->clone();

			if (subject_hout->expression_type.getSymbolType().getRefKind()
			    != tsh::ReferenceKind::Direct)
				subject_hout = makeBox<code::DerefExpr>(
					ctx, code::generatedOrigin(), std::move(subject_hout)
				);
			return subject_hout;
		}
	}

	base::Optional<SymID> getMatchSubjectSymbol(
		query::Context& ctx, pst::Access<pst::expr::MatchExpr> match_expr
	) {
		auto subject_hout = getSubjectHout(ctx, match_expr);
		if (!subject_hout.has_value()) return {};

		const auto subject_local_type
			= subject_hout.value()->expression_type.getSymbolType().withMutability(
				tsh::Mutability::Immutable
			);
		const ScopeID match_scope = ctx.query<QueryPrimaryCodeScopeFor>({ match_expr });

		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = base::StrID(base::strConcat("__match_subject_", match_expr->getID().asInt())),
			.generated_symbol_data
			= defgen::GeneratedSymbolData{ defgen::GeneratedSymbolData::ControlFlowLocal{
				.owning_scope = match_scope,
				.role         = base::StrID("__match_subject"),
				.type         = subject_local_type,
			} },
		});
	}

	base::Optional<code::BlockStmt> desugarMatch(
		query::Context&                   ctx,
		pst::Access<pst::expr::MatchExpr> match_expr,
		tsh::SymbolType<>                 expected_type,
		const MatchResultSink&            sink
	) {
		const auto gen            = code::generatedOrigin();
		const auto match_position = match_expr->getStablePosition();

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

		// Generated local holding the subject for the duration of the match.
		const auto subject_local_type = subject_type.withMutability(tsh::Mutability::Immutable);
		const auto subject_sym_opt    = getMatchSubjectSymbol(ctx, match_expr);
		if (!subject_sym_opt.has_value()) return {};
		const SymID subject_sym = subject_sym_opt.value();
		auto subject_ident = [&] { return makeBox<code::IdentifierExpr>(ctx, gen, subject_sym); };

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
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
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
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
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

			// Build the case body: the optional binding followed by the sunk result.
			code::CodeBlock body{};
			if (binding_sym.has_value()) {
				const auto binding_type
					= ctx.query<QueryTypeOfSymbol>(binding_sym.value())->valueOrThrow();
				body.statements.emplace_back(makeBox<code::VariableStmt>(
					gen,
					makeBox<code::VariantProjectExpr>(
						ctx, gen, subject_ident(), alternative_index.value()
					),
					binding_type,
					binding_sym.value()
				));
			}

			auto result_holder = branches.front().result.unlock(ctx);
			auto result_coerced
				= getHoutOfExprWithExpectedType(ctx, result_holder->getExpr(), expected_type);
			if (result_coerced.hasFailed()) return {};

			body.statements.emplace_back(sink(std::move(result_coerced).valueOrThrow()));

			cases.emplace_back(code::MatchStmt::Case{
				.alternative_index = alternative_index,
				.body              = std::move(body),
			});
		}

		if (!has_wildcard && covered.size() < num_alternatives) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
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

		// { var __match_subject = <subject>; match(__match_subject) { ... } }
		code::CodeBlock outer{};
		outer.statements.emplace_back(makeBox<code::VariableStmt>(
			code::pstOrigin(match_expr), std::move(subject_hout), subject_local_type, subject_sym
		));
		outer.statements.emplace_back(
			makeBox<code::MatchStmt>(code::pstOrigin(match_expr), subject_ident(), std::move(cases))
		);

		return code::BlockStmt(code::pstOrigin(match_expr), std::move(outer));
	}
}
