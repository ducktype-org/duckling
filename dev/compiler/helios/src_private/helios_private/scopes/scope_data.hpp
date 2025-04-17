#pragma once

#include <base/optional.hpp>
#include <pst_parser/access.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios_private/lookup_utils/lookup_result.hpp>

#include <query_framework/query_impl.hpp> // @TODO #404 relax it to just cache entry

namespace compiler::helios {

    struct ScopeData final {
		// created on startup:
		std::optional<ScopeID> parent;

		// base::StrID name; ///< for debug
		bool is_root = false;

		/**
		 * @brief PST element for which the scope was created.
		 * Empty for root scope.
		 */
		base::Optional<pst::AccessLocked<pst::LangElement>> related_pst_element;

		/**
		 * @brief Module, the scope was defined in
		 */
		frontend::ModuleID parent_module;

		// cache entry:
		// in the future we might need separation for: direct symbols, expanded symbols
		// in this system scope is no longer closed/open as we think of it as a pure-value object
		// any lookup in the scope requires calculation of symbols witch itself is done only once!
		base::Optional<query::CacheEntry<SymbolList>> symbols;

		u64 depth;

		// We would like the function bellow to be deleted to prevent any copy of scope data.
		// Unfortunately that would break the aggregate initialization which is super cool.
		// ScopeData is local to this file only, so we just need to be careful.
		// ScopeData(const ScopeData&)            = delete;
		// ScopeData& operator=(const ScopeData&) = delete;
	};

}

