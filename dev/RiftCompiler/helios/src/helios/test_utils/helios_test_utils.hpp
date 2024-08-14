#pragma once

#include <vector>
#include "tester/tester.hpp"
#include "helios/scopes/scopes.hpp"
#include "helios/symbols/symbols.hpp"

namespace compiler::helios::test_utils {
	std::pair<frontend::ModuleId, ScopeID> getModule(const fs::FilePath& path);

	std::vector<SymID> getChain(const std::string& chain, ScopeID scope);

	int getValue(const std::string& name, ScopeID scope);

	ts::TypeInfo getTypeOf(const std::string& name, ScopeID scope);

	ts::TypeInfo getTypeFromDefinition(const std::string& name, ScopeID scope);
}
