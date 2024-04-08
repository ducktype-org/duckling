#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>

#include "../pst_ref.hpp"
#include "../hout/hout.hpp"
#include "../lookup_result.hpp"


namespace compiler::helios {

	/**
	 * @brief Construct a new declare query object
	 * 
	 */
	DECLARE_QUERY(QuerySymbolOfSTMT, PstRef<pst::Stmt>, SymID);

	// Such functions can probably be just functions:
	base::StrId name(SymID);

	DECLARE_QUERY(QueryLookupIn, SymID, LookupResult);


}