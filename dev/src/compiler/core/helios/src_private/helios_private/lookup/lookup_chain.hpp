#pragma once

#include "interface.hpp"

#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/scope_id.hpp>
#include <helios/utils/symbol_list.hpp>

#include <query_framework/context/context_fd.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	struct LookupChainKey final {
		std::vector<pst::AccessLocked<pst::IdentifierWrapper>> names;
		ScopeID                                                begin_scope;
		AdditionalLookupParameters                             params;
	};

	/**
	 * @brief Query extension for looking-up chain of names, that is
	 * a list of names that are assumed to form expression of form `name1.name2.name3...`.
	 * It is currently used for looking up symbols in usings/aliases.
	 */
	query::QResult<SymbolList> lookupChain(query::Context& ctx, const LookupChainKey& key);
}
