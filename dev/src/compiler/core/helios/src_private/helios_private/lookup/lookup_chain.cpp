
#include "lookup_chain.hpp"

#include <helios_private/lookup/interface.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>

namespace compiler::helios {

	HInterface getSymbolInterface(query::Context& ctx, SymID sym) {
		switch (kind(sym)) {
		case SymbolKind::Module:
			return HInterface::ofModule(sym);
		case SymbolKind::Namespace:
			return HInterface::ofNamespace(sym);
		default:
			ctx.log<dia::NotYetImplementedCodeError>(
				"Interface of the ", base::enumToStr(kind(sym))
			);
			query::throwFailed();
		}
	}

	query::QResult<SymbolList> lookupChain(query::Context& ctx, const LookupChainKey& key) {
		CORE_ASSERT(!key.names.empty(), "lookupChain received zero names");

		SymbolList                   result;
		std::variant<ScopeID, SymID> last = key.start;

		for (auto pointed_locked: key.names) {
			auto lookup_interface = VARIANT_VISIT(
				last,
				VISIT_CASE_VAL(ScopeID, id, HInterface::ofScopeWithParents(id)),
				VISIT_CASE_VAL(SymID, id, getSymbolInterface(ctx, id))
			);

			auto pointed = pointed_locked.unlock(ctx);

			UNPACK_QRESULT_MOVE(auto lookup =,
			                    lookup_interface.lookupExpectUnique(
									pointed->getStablePosition(), ctx, pointed->unwrap(), key.params
								););
			result.appendList(lookup);

			last = result.back();
		}
		CORE_ASSERT(not result.empty(), "Lookup chain invalid call if empty list.");
		return result;
	}
}
