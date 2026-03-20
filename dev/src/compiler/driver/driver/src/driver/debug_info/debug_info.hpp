/**
 * @file debug_info_source_pos.hpp
 * @author Wojciech Rzepliński
 * @brief This file defines the query for resolving the positions in the debug info from
 * PstHashPosition to FilePosition.
 */
#pragma once

#include <debug_info/debug_info.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::driver {

	constexpr std::string_view DEBUG_INFO_FINAL_EXTENSION  = ".di.json";
	constexpr std::string_view DEBUG_INFO_STABLE_EXTENSION = ".stable.di.json";

	struct KeyOf_DebugInfoCalculatePositions final {
		debug_info::DebugInfo stable_debug_info;

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
	 */
	DECLARE_QUERY(
		DebugInfoCalculatePositions,
		KeyOf_DebugInfoCalculatePositions,
		query::QResult<artifacts::FileArtifact>,
		({
			.used_hashes             = query::UsedHashes::StableHash,
			.can_be_loaded_from_disk = true,
			.preserve_in_graph       = true,
		})
	);
}
