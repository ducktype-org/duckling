
#include "macros.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <tsh/queries.hpp>
#include <tsh/symbol_type.hpp>

#include <base/str/str_utils.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryMacroExpansion, pst::PST<pst::Stmt>) {
		// static inline concurrent::ConHashMap<KHash, query::CacheEntry<pst::PST<pst::Stmt>>> cache;

		static auto provide(Context& ctx, const QKey& key) -> PResult {
			// In the future calculate resulting string in comp time
			auto expand       = key.element.unlock(ctx);
			// auto value_holder = expand->getValue().unlock(ctx);

			auto expand_hout = getHoutOfExprWithExpectedType(
				ctx,
				 expand->getValue().unlock(ctx)->getExpr(),
				  tsh::SymbolType<>::withDefaults(tsh::getStringType())
			).valueOrThrow();
			auto expand_ctv = ctx.query<QueryEvaluateHOUTExpression>({ expand_hout.ref() }).valueOrThrow();

			

			// auto value = value_holder->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();


			if (expand_ctv.has<base::StrID>()) {
				auto expand_str = expand_ctv.get<base::StrID>().value();

				std::cerr << "\n================\n\n";
				std::cerr << "Macro expansion for: " << expand_str.str() << "\n";
				std::cerr << "\n";


				auto pst = pst::PST<pst::Stmt>::fromExpand(
					expand->getSourcePosition(),
					expand_str.str(), // PR: strView here leads to read of more data, investigate
					makeBox<pst::LangParserContext>(expand->getContext())
				);

				if (pst.getRootElement().unlockOpt(ctx).has_value()) {
					pst.setAdditionalRootData(pst::AdditionalRootData{
						pst::AdditionalRootData::MacroExpansionParent{ .expand_element = expand } });
				}
				// std::cerr<< "\n================\n\n";
				// std::cerr << "Macro expansion for: "<< "\n";
				// expand->dprint(std::cerr);
				// std::cerr << "\nExpanded to:\n";
				// pst.dprint(std::cerr);
				// std::cerr << "\n================\n\n";
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

		// static auto load(KHash key) -> LoadResult {
		// 	if (const auto& value = cache.atMaybe(key))
		// 		return QResWithACD{ extractResult(value.value()->data), (value.value())->acd };
		// 	return {};
		// }

		// static auto store(KHash key, PResult res, query::ACD acd) -> QResult {
		// 	cache.put(key, { .data = std::move(res), .acd = acd });
		// 	return extractResult(cache.at(key)->data);
		// }

		// static auto erase(KHash key) -> bool { return cache.erase(key); }


		QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA(extractResult)
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMacroExpansion);
}
