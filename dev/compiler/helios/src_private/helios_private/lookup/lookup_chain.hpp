#pragma once

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/utils/symbol_list.hpp>
#include "interface.hpp"
#include <query_framework/context_fd.hpp>
#include <token_parser_core/common_elements.hpp>

namespace compiler::helios {

	struct LookupChainKey final {
		std::vector<tpc::Identifier> names;
		ScopeID                      begin_scope;
		AdditionalLookupParameters   params;
	};

	/**
	 * @brief Query extension for looking-up chain of names
	 */
	errors::HResult<SymbolList, errors::Failed> lookupChain(
		query::Context& ctx, const LookupChainKey& key
	);
}
