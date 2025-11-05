#include <driver_private/save_artifacts.hpp>
#include <global_state/artifacts_location.hpp>

#include <base/str/string_id.hpp>
#include <base/types/ints.hpp>

#include <query_framework/internal/context_access.hpp>

#include <vector>

namespace compiler::driver {

	void saveArtifacts() {
		auto root = global_state::getRootCollection();

		auto query_collection = root->subCollectionAtOrNew(base::StrID("query"));

		auto query_graph_blob = query_collection->blobArtifactAtOrNew(base::StrID("query_graph"));

		// Serialize the query graph from the global query state.
		auto              graph_ref = query::internal::ContextAccess::getState()->getGraphMutable();
		std::vector<byte> serialized = graph_ref->serialize();

		if (!serialized.empty())
			query_collection->setBlobData(query_graph_blob, serialized.data(), serialized.size());

		// Flush all artifacts to disk.
		root->flush();
	}

}  // namespace compiler::driver
