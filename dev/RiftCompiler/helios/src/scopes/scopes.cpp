#include "scopes.hpp"
#include "../hout/hout.hpp"
#include <base/string_id.hpp>
#include <query_framework/query_impl.hpp>

namespace compiler::helios {

	using SymbolList = std::vector<SymbolRef>;

	struct ScopeData {
		// adapted from hir:
		
		// created on startup:
		ScopeRef parent;
		base::StrId name; ///< for debug


		// cache entries:
		// in the future we might need separation for: direct symbols, expanded symbols
		// in this system scope is no longer closed/open as we think of it as a pure-value object
		// any lookup in the scope requires calculation of symbols witch itself is done only once!
		base::Optional<query::AddACD<SymbolList> > symbols;



		// This delete is important, to prevent any copy of scope data:
		ScopeData(const ScopeData&)            = delete;
		ScopeData& operator=(const ScopeData&) = delete;

	};


	namespace {
		// stable list of ScopeData
	}

	// query impl of:
	// - lookup in scope:                   ScopeRef, StrID -> LookupResult
	// - lookup in scope and scope parents: ScopeRef, StrID -> LookupResult


	// impl of simple getters ("non-query query"):
	// get name
	// debug print
	// etc 

	// if somewhere then here it is needed to handle cycles somehow
}


