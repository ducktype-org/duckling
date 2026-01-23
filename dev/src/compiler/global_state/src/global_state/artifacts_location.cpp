
#include "artifacts_location.hpp"

namespace global_state {

	namespace {
		constinit MBox<artifacts::ArtifactCollection> root_collection;
	}

	Ref<artifacts::ArtifactCollection> getRootCollection() {
		return root_collection.refMut().toOpt().value();
	}

	bool hasRootCollection() { return root_collection.toOpt().has_value(); }

	namespace setters {
		/**
		 * This should only be called by the driver.
		 */
		void setRootCollection(Box<artifacts::ArtifactCollection> collection) {
			CORE_ASSERT(root_collection.toOpt().empty(), "Root collection is already set!");
			root_collection = std::move(collection);
		}
	}
}
