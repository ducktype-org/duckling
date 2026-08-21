/**
 * @file debug_info_source_pos.hpp
 * @author Wojciech Rzepliński
 * @brief This file defines the query for resolving the positions in the debug info from
 * PstHashPosition to FilePosition.
 */
#pragma once

#include <debug_info/debug_info.hpp>
#include <driver/backend_type.hpp>
#include <frontend/module_tree/module_id.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::driver {

	constexpr std::string_view DEBUG_INFO_STABLE_EXTENSION = ".stable.di";

	/**
	 * The key for the DebugInfoForModule query.
	 */
	struct KeyOf_DebugInfoForModule final {
		frontend::ModuleID module_id;
		BackendType        backend_type;

		[[nodiscard]]
		base::Bit256 queryStablePerfectHash() const;
	};

	/**
	 * Query that changes the PST-hash based positions in the debug info artifact into
	 * file/line/column based positions. It requires the loaded parse syntax tree and based on the
	 * current locations of nodes in the PST, it can resolve the positions in the debug info. This
	 * query is purely for better incremental compilation, as changing only the location of the
	 * nodes and not the nodes themselves should not change the PST hash, so the PST-hash debug info
	 * can be cached on disk, but it should invalidate this query's result.
	 *
	 * @warning If you use this query and compileModule make sure you use `compileModule` with
	 * `build_debug_info=true`, otherwise the module will get compiled twice.
	 */
	DECLARE_QUERY(
		DebugInfoForModule,
		KeyOf_DebugInfoForModule,
		query::QResult<artifacts::FileArtifact>,
		({
			.used_hashes             = query::UsedHashes::StableHash,
			.can_be_loaded_from_disk = true,
			.preserve_in_graph       = true,
		})
	);
}
