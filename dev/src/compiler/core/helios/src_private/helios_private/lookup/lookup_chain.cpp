// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lookup_chain.hpp"

#include <helios_private/lookup/interface.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>

namespace compiler::helios {


	query::QResult<SymbolList> lookupChain(query::Context& ctx, const LookupChainKey& key) {
		CORE_ASSERT(!key.names.empty(), "lookupChain received zero names");

		bool       first_symbol = true;
		SymbolList result;
		for (auto pointed_locked: key.names) {
			auto lookup_interface = first_symbol ? HInterface::ofScopeWithParents(key.begin_scope)
			                                     : HInterface::ofSymbol(result.back());

			auto pointed = pointed_locked.unlock(ctx);

			UNPACK_QRESULT_MOVE(auto lookup =,
			                    lookup_interface.lookupExpectUnique(
									pointed->getStablePosition(), ctx, pointed->unwrap(), key.params
								););
			result.appendList(lookup);

			first_symbol = false;
		}
		return result;
	}
}
