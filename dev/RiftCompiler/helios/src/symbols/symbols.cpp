#include "symbols.hpp"
#include <query_framework/query_impl.hpp>
#include <base/stable_container.hpp>

namespace compiler::helios {

	// scopes will look more or the less the same!
	// imports are just symbols that will require lookup inside different module that will build itself from different PST. Simple!
	// We will just need query for import lookup cached by (globally) unique PstID


	struct SymbolData {
		// adapted from hir:

		// created on startup:
		ScopeRef    scope;
		base::StrId name;
		bool        anonymous;
		bool wildcard = false;
		bool is_alias = false;
		bool dependent = false;
		SymbolKind kind;
		// pst link? -- what about down casting...


		// cached: linked_lookup_scope ?
		// cached: type
		// cached: value?
		



	};

	// do we want internal inheritance?
	// query: lookupIn
	// query: dealias
	

	namespace {
		// global table:
		base::StableIntList<SymbolData> symbol_table;
	}


	struct ImplementationOf_QuerySymbolOfSTMT:
		public query::QueryImplementation<QuerySymbolOfSTMT, SymID>
	{
		static auto provide(Context& ctx, QKey key) -> PResult {
			// generate new symbol
			throw "TODO";
		}

		static auto load([[maybe_unused]] QKey key) -> LoadResult { return {}; }

		static auto store([[maybe_unused]] QKey key, PResult res, [[maybe_unused]] query::ACD acd) {
			throw "TODO";
		}
	};

	// if somewhere then here it is needed to handle cycles somehow

}