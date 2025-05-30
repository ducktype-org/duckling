
#include "lookup_chain.hpp"

#include <helios_private/lookup/interface.hpp>

namespace compiler::helios {


	errors::HResult<SymbolList, errors::Failed> lookupChain(
		query::Context& ctx, const LookupChainKey& key
	) {
		CORE_ASSERT(!key.names.empty(), "lookupChain received zero names");

		bool       first_symbol = true;
		SymbolList result;
		for (auto pointed: key.names) {
			auto lookup_interface = first_symbol ? HInterface::ofScopeWithParents(key.begin_scope)
			                                     : HInterface::ofSymbol(result.back());

			UNPACK_RESULT_MOVE(auto lookup =,
			                   lookup_interface.lookupExpectUnique(
								   pointed.position, ctx, pointed.value, key.params
							   ););
			result.appendList(lookup);
			first_symbol = false;
		}
		return result;
	}
}
