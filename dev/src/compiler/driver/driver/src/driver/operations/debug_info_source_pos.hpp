#pragma once

#include <artifacts/artifacts.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::driver {

	struct KeyOf_DebugInfoResolvePositions final {
		artifacts::FileArtifact input_artifact;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;

		[[nodiscard]]
		base::Bit256 queryStablePerfectHash() const;
	};

	/**
	 * Query that resolves PST-hash-based debug-info positions into file line/column positions.
	 *
	 * Input key is an existing debug-info artifact. The key hash is based on the artifact
	 * file content, so cache invalidation follows content changes.
	 */
	DECLARE_QUERY(
		DebugInfoResolvePositions,
		KeyOf_DebugInfoResolvePositions,
		query::QResult<artifacts::FileArtifact>,
		({
			.used_hashes             = query::UsedHashes::StableHash,
			.can_be_loaded_from_disk = true,
			.preserve_in_graph       = true,
		})
	);
}

