#pragma once

#include <vector>
#include "tester/tester.hpp"
#include "helios/scopes/scopes.hpp"
#include "helios/symbols/symbols.hpp"

inline std::pair<compiler::frontend::ModuleId, compiler::helios::ScopeID>
	getModule(fs::FilePath path) {
	auto module = query::entryPoint<compiler::frontend::QueryModuleTree>(path);

	// This is what extendQueryRootScopeOfMainModuleFile is doing:
	// (there is currently no way to call query extension without context)
	auto  main_source_file = query::entryPoint<compiler::frontend::QueryMainSourceFile>(module);
	auto& main_source_pst  = query::entryPoint<compiler::frontend::QueryFilePST>(main_source_file);
	auto  main_file_root_scope = query::entryPoint<compiler::helios::QueryPrimaryCodeScopeFor>(
        { main_source_pst.getRootElement() }
    );

	return { module, main_file_root_scope };
}

inline std::vector<compiler::helios::SymID>
	getChain(std::string chain, compiler::helios::ScopeID scope) {
	auto                         symbols = base::strSplit(chain, ".");
	compiler::helios::SymbolList result;
	bool                         first_symbol = true;
	for (auto&& sym: symbols) {
		auto symbol = first_symbol
		                ? query::entryPoint<compiler::helios::QueryLookupInScopeAndParents>(
							{ scope, base::StrId(sym.c_str()), true }
						)
		                : query::entryPoint<compiler::helios::QueryLookupInSymbol>(
							{ result.back(), base::StrId(sym.c_str()), false }
						);
		for (auto&& symbol_path = symbol.getAsSingle(); auto&& elem: symbol_path) {
			auto dealiased = query::entryPoint<compiler::helios::QueryDealias>(elem);
			result.insert(result.end(), dealiased.begin(), dealiased.end());
		}
		first_symbol = false;
	}
	return result;
}

inline int getValue(std::string name, compiler::helios::ScopeID scope) {
	return query::entryPoint<compiler::helios::QueryConstValueOf>(getChain(name, scope).back());
}

inline ts::TypeInfo getTypeOf(std::string name, compiler::helios::ScopeID scope) {
	return query::entryPoint<compiler::helios::QueryTypeOfSymbol>(getChain(name, scope).back());
}

inline ts::TypeInfo getTypeFromDefinition(std::string name, compiler::helios::ScopeID scope) {
	return query::entryPoint<compiler::helios::QueryTypeFromDefinition>(getChain(name, scope).back()
	);
}
