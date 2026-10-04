#include "debug_artifacts.hpp"

#include <global_state/artifacts_location.hpp>

namespace compiler::driver {
	Ref<artifacts::ArtifactCollection> getDebugArtifactCollection() {
		return global_state::getRootCollection()->subCollectionAtOrNew(
			base::StrID("duck_debug_artifacts")
		);
	}
}
