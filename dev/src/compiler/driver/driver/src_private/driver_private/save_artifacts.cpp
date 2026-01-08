#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>

#include <base/types/ints.hpp>

#include <query_framework/external/api.hpp>
#include <string_id/string_id.hpp>

#include <vector>

namespace compiler::driver {

	void saveArtifacts() {
		auto root = global_state::getRootCollection();

		auto query_collection = root->subCollectionAtOrNew(base::StrID("query"));

		auto query_graph_blob = query_collection->blobArtifactAtOrNew(base::StrID("query_graph"));

		std::vector<byte> serialized = query::external::serializeQueryGraph();

		if (!serialized.empty())
			query_collection->setBlobData(query_graph_blob, serialized.data(), serialized.size());

		// Flush all artifacts to disk.
		root->flush();
	}

}  // namespace compiler::driver
