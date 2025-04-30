
#include "lookup_chain.hpp"

#include <helios_private/lookup/interface.hpp>

namespace compiler::helios {


	errors::HResult<SymbolList, errors::Failed> lookupChain(
		query::Context& ctx, const LookupChainKey& key
	) {
		CORE_ASSERT(!key.names.empty(), "lookupDotted received zero names");

		bool       first_symbol = true;
		SymbolList result;
		for (auto pointed: key.names) {
			if (first_symbol) {
				UNPACK_RESULT_MOVE(auto lookup =,
				                   HInterface::ofScopeWithParents(key.begin_scope)
				                       .typicalSimpleLookup(pointed.position, ctx, pointed.value, key.params););
				result.insert(result.end(), lookup.begin(), lookup.end());
			} else {
				UNPACK_RESULT_MOVE(auto lookup =,
				                   HInterface::ofSymbol(result.back())
				                       .typicalSimpleLookup(pointed.position, ctx, pointed.value, key.params););
				result.insert(result.end(), lookup.begin(), lookup.end());
			}
			first_symbol = false;
		}
		return result;
	}
}
