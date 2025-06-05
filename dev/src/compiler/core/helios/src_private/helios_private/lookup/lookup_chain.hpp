#pragma once

#include "interface.hpp"

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/utils/symbol_list.hpp>
#include <query_framework/context_fd.hpp>
#include <token_parser_core/common_elements.hpp>

namespace compiler::helios {

	struct LookupChainKey final {
		std::vector<tpc::Identifier> names;
		ScopeID                      begin_scope;
		AdditionalLookupParameters   params;
	};

	/**
	 * @brief Query extension for looking-up chain of names, that is
	 * a list of names that are assumed to form expression of form `name1.name2.name3...`.
	 * It is currently used for looking up symbols in usings/aliases.
	 */
	errors::HResult<SymbolList, errors::Failed> lookupChain(
		query::Context& ctx, const LookupChainKey& key
	);
}
