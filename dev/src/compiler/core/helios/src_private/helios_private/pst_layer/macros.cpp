
#include "macros.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios/tsh/queries.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>

#include <base/str/str_utils.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryMacroExpansion, pst::PST<pst::Stmt>) {
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
					expand->getSourcePosition(),
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

				if (pst.getRootElement().unlockOpt(ctx).has_value()) {
					pst.setAdditionalRootData(pst::AdditionalRootData{
						pst::AdditionalRootData::MacroExpansionParent{ .expand_element = expand } });
				}

				return pst;
			} else {
				CORE_PANIC("Expand argument is not exactly a single string.");
			}
		}

		static auto extractResult(CRef<pst::PST<pst::Stmt>> pst_ref) -> QResult {
			if (pst_ref->getLogger()->good())
				return { pst_ref->getRootElement() };
			else
				return ExpansionError<pst::Stmt>(pst_ref->getRootElement(), pst_ref->getLogger());
		}


		QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA(extractResult)
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMacroExpansion);
}
