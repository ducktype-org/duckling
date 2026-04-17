
#include "macros.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios/tsh/queries.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <diagnostic_interactive/placeholder.hpp>


#include <base/str/str_utils.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryMacroExpansion, query::QResult<pst::PST<pst::Stmt>>) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			auto expand = key.element.unlock(ctx);

			auto expand_hout = getHoutOfExprWithExpectedType(
								   ctx,
								   expand->getValue().unlock(ctx)->getExpr(),
								   tsh::SymbolType<>::withDefaults(tsh::getStringType())
			)
			                       .valueOrThrow();
			auto expand_ctv
				= ctx.query<QueryEvaluateHOUTExpression>({ expand_hout.ref() }).valueOrThrow();

			if (expand_ctv.has<base::StrID>()) {
				auto expand_str = expand_ctv.get<base::StrID>().value();

				auto pst = pst::PST<pst::Stmt>::fromExpand(
					expand->getSourcePosition().unlock(ctx),
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

				if (not pst.getLogger()->good()) {
					// Note that PST diagnostics are logged eagerly, so if the PST did not parse correctly, 
					// the diagnostics should already be logged at this point.
					ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						"Macro expansion produced code with parsing errors (see other diagnostics for details)",
						expand->getSourcePosition()
					));

					return query::Failed();
				}

				if (pst.getRootElement().unlockOpt(ctx).has_value()) {
					pst.setAdditionalRootData(pst::AdditionalRootData{
						pst::AdditionalRootData::MacroExpansionParent{ .expand_element = expand } });
				}

				return pst;
			} else {
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"The expresion in expand statements did not evaluate to a string value.",
					expand->getValue().unlock(ctx)->getExpr().unlock(ctx)->getSourcePosition()
				));
				return query::Failed();
			}
		}

		static auto extractResult(CRef<query::QResult<pst::PST<pst::Stmt>>> p_result) -> QResult {
			if (p_result->hasFailed())
				return query::Failed{};
			else {
				Ref pst_ref = &p_result->valueOrPanic();
				CORE_ASSERT(pst_ref->getLogger()->good(), "PST from macro expansion should have been checked for errors in provide()");
				return { pst_ref->getRootElement() };
			}			
		}


		QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA(extractResult)
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMacroExpansion);
}
