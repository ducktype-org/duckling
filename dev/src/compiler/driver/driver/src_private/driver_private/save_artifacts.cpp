#include <driver/module_flags/module_flags.hpp>
#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>

#include <base/types/ints.hpp>

#include <logger/logger.hpp>
#include <query_framework/external/api.hpp>
#include <string_id/string_id.hpp>

#include <vector>

namespace compiler::driver {

	namespace {
		void saveQueryStateData(Ref<artifacts::ArtifactCollection> root) {
			Ref query_collection = root->subCollectionAtOrNew(base::StrID("query"));

			// Save query graph
			auto query_graph_blob
				= query_collection->blobArtifactAtOrNew(base::StrID("query_graph"));

			std::vector<byte> serialized = query::external::optAndSerializeQueryGraph();

			// Reclaim on-disk caches orphaned by this compilation (nodes dropped from the graph,
			// e.g. because their query hash changed) before the graph blob is written and flushed.
			const usize deleted_disk_caches = query::external::deleteOrphanedDiskCaches();
			CORE_DEV_LOG(
				Artifacts, "Removed ", deleted_disk_caches, " orphaned on-disk query cache(s)\n"
			);

			if (!serialized.empty())
				query_collection->setBlobData(
					query_graph_blob, serialized.data(), serialized.size()
				);

			// Save metadata
			auto metadata_blob
				= query_collection->blobArtifactAtOrNew(base::StrID("query_metadata"));

			std::vector<byte> metadata_serialized = query::external::serializeMetadata();

			if (!metadata_serialized.empty())
				query_collection->setBlobData(
					metadata_blob, metadata_serialized.data(), metadata_serialized.size()
				);
		}
	}

	void saveArtifacts() {
		Ref root = global_state::getRootCollection();

		if (enable_incremental_compilation) saveQueryStateData(root);

		// Flush all artifacts to disk.
		root->flush();
	}

}  // namespace compiler::driver
