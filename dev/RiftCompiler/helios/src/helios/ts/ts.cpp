#include "ts.hpp"

#include "helios/scopes/scopes.hpp"

#include <query_framework/query_impl.hpp>
#include <base/stable_hashmap.hpp>
#include <helios/symbols/symbols.hpp>

namespace compiler::helios::ts {
	struct IMPLEMENT_QUERY(QueryStructSymbolsInScope, std::vector<SymID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<SymID> structs;
			for (auto&& symbols_in_scope = ctx.query<QuerySymbolsInScope>(key);
			     auto&& symbol: symbols_in_scope) {
				if (kind(symbol) == SymbolKind::Struct) structs.push_back(symbol);
			}
			return structs;
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryStructSymbolsInScope);

	struct IMPLEMENT_QUERY(QueryTypeOf, ts::TypeInfo) {}

}
