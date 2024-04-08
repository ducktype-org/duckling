#include "symbols.hpp"
#include <query_framework/query_impl.hpp>
#include <stable_container.hpp>

namespace compiler::helios {

	// scopes will look more or the less the same!
	// imports are just symbols that will require lookup inside different module that will build itself from different PST. Simple!
	// We will just need query for import lookup cached by (globally) unique PstID

	struct SymbolData {
		// all the stuff from original HIR
	};

	namespace {
		// global table:
		base::StableIntList<SymbolData> symbol_table;
	}


	struct ImplementationOf_QuerySymbolOfSTMT:
		public query::QueryImplementation<QuerySymbolOfSTMT, SymID>
	{
		static auto provide(Context& ctx, QKey key) -> PResult {
			// generate new symbol
		}

		static auto load([[maybe_unused]] QKey key) -> LoadResult { return {}; }

		static auto store([[maybe_unused]] QKey key, PResult res, [[maybe_unused]] query::ACD acd) {

		}
	};



}