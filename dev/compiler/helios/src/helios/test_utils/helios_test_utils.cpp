#include "helios_test_utils.hpp"

#include <query_framework/query_entry_point.hpp>

namespace compiler::helios::test_utils {
	std::pair<frontend::ModuleID, ScopeID> getModule(const fs::FilePath& path) {
		auto module = query::entryPoint<frontend::QueryModuleTree>(path);

		// This is what extendQueryRootScopeOfMainModuleFile is doing:
		// (there is currently no way to call query extension without context)
		auto main_source_file = query::entryPoint<frontend::QueryMainSourceFile>(module);
		auto main_source_pst  = query::entryPoint<frontend::QueryFilePST>(main_source_file);
		auto main_file_root_scope
			= query::entryPoint<QueryPrimaryCodeScopeFor>({ main_source_pst->getRootElement() });

		return { module, main_file_root_scope };
	}

	std::vector<SymID> getChain(const std::string_view chain, ScopeID scope) {
		auto       symbols = base::strSplit(chain, ".");
		SymbolList result;
		bool       first_symbol = true;
		for (auto&& sym: symbols) {
			auto symbol      = first_symbol ? query::entryPoint<QueryLookupInScopeAndParents>(
                              { scope, base::StrID(sym.c_str()), true }
                          )
			                                : query::entryPoint<QueryLookupInSymbol>(
                                           { result.back(), base::StrID(sym.c_str()), false }

                                       );
			auto symbol_path = symbol->getAsSingle().valueOrThrow();
			for (auto&& elem: symbol_path) {
				auto dealiased = query::entryPoint<QueryDealias>(elem)->valueOrThrow();
				result.insert(result.end(), dealiased.begin(), dealiased.end());
			}
			first_symbol = false;
		}
		return result;
	}

	int getValue(const std::string_view chain, ScopeID scope) {
		return query::entryPoint<QueryConstValueOf>(getChain(chain, scope).back())->valueOrThrow();
	}

	tsh::TypeInfo getTypeOf(const std::string_view chain, ScopeID scope) {
		return query::entryPoint<QueryTypeOfSymbol>(getChain(chain, scope).back())->valueOrThrow();
	}

	tsh::TypeInfo getTypeFromDefinition(const std::string_view chain, ScopeID scope) {
		return query::entryPoint<QueryTypeFromDefinition>(getChain(chain, scope).back())
		    ->valueOrThrow();
	}
}
