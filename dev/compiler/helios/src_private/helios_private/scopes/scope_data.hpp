#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/utils/symbol_list.hpp>
#include <pst_parser/access.hpp>
#include <query_framework/query_impl.hpp>  // @TODO #404 relax it to just cache entry

#include <base/optional.hpp>

namespace compiler::helios {

	/**
	 * Structure holding all data directly stored for each created scope.
	 */
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

		/**
		 * Scope depth, i.e. distance to the root scope.
		 * It is currently unused but might be useful in the future.
		 */
		u64 depth;

		// We would like the function bellow to be deleted to prevent any copy of scope data.
		// Unfortunately that would break the aggregate initialization which is super cool.
		// ScopeData is local to this file only, so we just need to be careful.
		// ScopeData(const ScopeData&)            = delete;
		// ScopeData& operator=(const ScopeData&) = delete;
	};

}
