
#include "macros.hpp"

#include "ctv/ctv.hpp"
#include "helios/tsh/queries/types.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios/tsh/queries.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryMacroExpansion, query::QResult<pst::PST<pst::Stmt>>) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			auto expand = key.element.unlock(ctx);

			// `expand` accepts either a `str` (char slice) or a `String`, so we can't use
			// `getHoutOfExprWithExpectedType` (single expected type). Build the HOUT directly and try
			// both coercions.
			auto expand_expr       = expand->getValue().unlock(ctx)->getExpr();
			auto expr_hout_qresult = ctx.query<QueryHoutOfExpr>({ expand_expr });
			if (expr_hout_qresult->hasFailed()) return query::Failed();
			CRef<code::Expr> expr_hout = expr_hout_qresult->valueOrThrow().ref();

			auto str_coercion = canCoerce(
				ctx,
				expr_hout->expression_type,
				tsh::SymbolType<>::withDefaultsConst(tsh::getCharSliceType(ctx))
			);
			auto string_coercion = canCoerce(
				ctx,
				expr_hout->expression_type,
				tsh::SymbolType<>::withDefaultsConst(tsh::getStringType(ctx))
			);
			if (str_coercion.hasFailed() || string_coercion.hasFailed()) return query::Failed();

			const auto& str_result    = str_coercion.valueOrThrow();
			const auto& string_result = string_coercion.valueOrThrow();
			if (str_result.isInvalid() && string_result.isInvalid()) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					base::strConcat(
						"expand argument has to be either str or String, but got ",
						expr_hout->expression_type.getSymbolType().toString()
					),
					expand_expr.unlock(ctx)->getStablePosition()
				));
				return query::Failed();
			}

			const auto& coercion
				= str_result.isValid() ? str_result.getCoercion() : string_result.getCoercion();
			auto expand_hout = coercion.coerceFromRef(ctx, expr_hout);
			auto expand_ctv
				= ctx.query<QueryEvaluateHOUTExpression>({ expand_hout.ref() }).valueOrThrow();

			auto get_ctv_string_content
				= [](ctv::CompileTimeValue& ctv) -> base::Optional<base::StrID> {
				v_if_matches(ctv.getStorage(), base::StrID, val) return *val;
				v_if_matches(ctv.getStorage(), ctv::StringClassValue, val) return val->value;
				return {};
			};
			if(auto exapnd_str_opt = get_ctv_string_content(expand_ctv)) {
				auto expand_str = exapnd_str_opt.value();
				auto pst = pst::PST<pst::Stmt>::fromExpand(
					expand->getStablePosition(),
					// @TODO: #2471 change to strView, once it is fixed
					expand_str.str(),
					makeBox<pst::LangParserContext>(expand->getContext()),

					// This is a little weird, we create a path context hash by hashing the string
				    // representation of the expand argument bit256 hash.
				    // Note that this is generally correct since hash(hash) keeps all the necessary
				    // properties we need, and the expand argument hash includes the bits related to
				    // the expand path.
					hashing::ComponentHash({}, expand->getHash().toStringHex())
				);
				bool parse_errors = pst.hasErrors();

				ctx.moveDiagnosticsFrom(*pst.getLoggerMut());
				if (parse_errors) return query::Failed();

				if (pst.getRootElement().unlockOpt(ctx).has_value()) {
					pst.setAdditionalRootData(
						pst::AdditionalRootData{ pst::AdditionalRootData::MacroExpansionParent{
							.expand_element = expand } }
					);
				}

				return pst;
			}
			else {
				ctx.logInt(
					makeBox<dia_int::PlaceholderError>(
						"The expression in expand statements did not evaluate to a string value.",
						expand->getValue().unlock(ctx)->getExpr().unlock(ctx)->getStablePosition()
					)
				);
				return query::Failed();
			}
		}

		static auto extractResult(CRef<query::QResult<pst::PST<pst::Stmt>>> p_result) -> QResult {
			if (p_result->hasFailed())
				return query::Failed{};
			else {
				Ref pst_ref = &p_result->valueOrPanic();
				CORE_ASSERT(
					pst_ref->getLogger()->good(),
					"PST from macro expansion should have been checked for errors in provide()"
				);
				return { pst_ref->getRootElement() };
			}
		}


		QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA(extractResult)
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMacroExpansion);
}
