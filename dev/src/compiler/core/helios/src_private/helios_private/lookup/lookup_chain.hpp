// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		/**
		 * @brief Scope where to start lookup or symbol where to start lookup.
		 */
		std::variant<ScopeID, SymID> start;
		AdditionalLookupParameters   params;
	};

	/**
	 * @brief Query extension for looking-up chain of names, that is
	 * a list of names that are assumed to form expression of form `name1.name2.name3...`.
	 * It is currently used for looking up symbols in usings/imports.
	 */
	query::QResult<SymbolList> lookupChain(query::Context& ctx, const LookupChainKey& key);
}
