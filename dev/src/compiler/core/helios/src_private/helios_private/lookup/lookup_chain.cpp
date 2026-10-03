
#include "lookup_chain.hpp"

#include <helios_private/lookup/interface.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

namespace compiler::helios {

	query::QResult<SymbolList> lookupChain(query::Context& ctx, const LookupChainKey& key) {
		SymbolList result;
		if (v_matches(key.start, SymID)) result.pushBack(v_get(key.start, SymID));

		std::variant<ScopeID, SymID> last = key.start;

		for (auto pointed_locked: key.names) {
			auto lookup_interface = VARIANT_VISIT(
				last,
				VISIT_CASE_VAL(ScopeID, id, HInterface::ofScopeWithParents(id)),
				VISIT_CASE_VAL(SymID, id, HInterface::ofSymbol(ctx, id))
			);

			auto pointed = pointed_locked.unlock(ctx);

			UNPACK_QRESULT_MOVE(auto lookup =,
			                    lookup_interface.lookupExpectUnique(
									pointed->getStablePosition(), ctx, pointed->unwrap(), key.params
								););
			result.appendList(lookup);

			last = result.back();
		}
		return result;
	}
}
