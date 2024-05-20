#pragma once
#include <query_framework/query_int.hpp>

#include "../hout/hout.hpp"
#include "../lookup_result.hpp"

namespace compiler::helios::ts {
	DECLARE_QUERY(QueryStructSymbolsInScope, ScopeID, const std::vector<SymID>&);
	// DECLARE_QUERY(QueryRegisterStruct, SymID, TypeID);
}
