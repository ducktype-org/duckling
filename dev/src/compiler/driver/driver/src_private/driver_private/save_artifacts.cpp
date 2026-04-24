#include <driver/module_flags/module_flags.hpp>
#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>

#include <base/types/ints.hpp>

#include <query_framework/external/api.hpp>
#include <string_id/string_id.hpp>

#include <vector>

namespace compiler::driver {

	namespace {
		void saveQueryGraph(Ref<artifacts::ArtifactCollection> root) {
			Ref query_collection = root->subCollectionAtOrNew(base::StrID("query"));

			// Save query graph
			auto query_graph_blob
				= query_collection->blobArtifactAtOrNew(base::StrID("query_graph"));

			std::vector<byte> serialized = query::external::optAndSerializeQueryGraph();

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

		if (enable_incremental_compilation) saveQueryGraph(root);

		// Flush all artifacts to disk.
		root->flush();
	}

}  // namespace compiler::driver
