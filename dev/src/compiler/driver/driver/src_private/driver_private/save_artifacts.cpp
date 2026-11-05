// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <driver/module_flags/module_flags.hpp>
#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <artifacts/artifacts.hpp>
#include <logger/logger.hpp>
#include <query_framework/external/api.hpp>
#include <string_id/string_id.hpp>

#include <vector>

namespace compiler::driver {

	namespace {
		/**
		 * @brief How long a query artifact may go unused before the cache removes it.
		 *
		 * Far longer than any build takes, so that no build - this one or another sharing the
		 * folder - loses a file it is still using.
		 */
		constexpr timer::Duration UNUSED_QUERY_ARTIFACT_MAX_AGE = timer::Duration::fromDays(7);

		/**
		 * @brief How old a temporary file left by a killed write must be to be removed.
		 *
		 * Such a file is always garbage, and can be large (an executable, an archive), so it goes
		 * sooner than unused artifacts - but no single write takes a day, so a write still in
		 * progress in another build sharing the folder keeps its file.
		 */
		constexpr timer::Duration STALE_TEMPORARY_FILE_MAX_AGE = timer::Duration::fromDays(1);

		/**
		 * @brief Removes the query artifacts that no build has written or reused for
		 * `UNUSED_QUERY_ARTIFACT_MAX_AGE`.
		 *
		 * The cleanup used to remove the artifacts of the queries missing from this build's
		 * graph. With several builds sharing the folder - different targets, built in parallel -
		 * that removed the other builds' artifacts, even while they were using them. Going by when
		 * a file was last used needs no knowledge of what the other builds are doing. It runs at
		 * the end of the build, when everything this build used has already been marked.
		 */
		void removeUnusedQueryArtifacts(Ref<artifacts::ArtifactCollection> root) {
			if_opt_some(root->subCollectionAtMaybe(base::StrID("query")), query_collection) {
				const usize removed
					= query_collection->removeFileArtifactsUnusedFor(UNUSED_QUERY_ARTIFACT_MAX_AGE);
				CORE_DEV_LOG(Artifacts, "Removed ", removed, " unused query artifact(s)\n");
			}
		}

		void saveQueryStateData(Ref<artifacts::ArtifactCollection> root) {
			Ref query_collection = root->subCollectionAtOrNew(base::StrID("query"));

			// Save query graph
			auto query_graph_blob
				= query_collection->blobArtifactAtOrNew(base::StrID("query_graph"));

			std::vector<byte> serialized = query::external::optAndSerializeQueryGraph();

			if (!serialized.empty()) query_graph_blob.setData(serialized.data(), serialized.size());

			// Save metadata
			auto metadata_blob
				= query_collection->blobArtifactAtOrNew(base::StrID("query_metadata"));

			std::vector<byte> metadata_serialized = query::external::serializeMetadata();

			if (!metadata_serialized.empty())
				metadata_blob.setData(metadata_serialized.data(), metadata_serialized.size());
		}
	}

	void removeStaleTemporaryFiles(Ref<artifacts::ArtifactCollection> collection) {
		const usize removed
			= collection->removeTemporaryFilesOlderThan(STALE_TEMPORARY_FILE_MAX_AGE);
		CORE_DEV_LOG(Artifacts, "Removed ", removed, " stale temporary file(s)\n");
	}

	void saveArtifacts() {
		Ref root = global_state::getRootCollection();

		if (enable_incremental_compilation) saveQueryStateData(root);
		// Without incremental compilation the query artifacts are still written, so they are
		// cleaned up either way.
		removeUnusedQueryArtifacts(root);
		removeStaleTemporaryFiles(root);

		// Flush all artifacts to disk.
		root->flush();
	}

}  // namespace compiler::driver
