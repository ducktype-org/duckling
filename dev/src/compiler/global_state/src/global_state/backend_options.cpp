#include "backend_options.hpp"

namespace global_state {

	namespace {
		base::Optional<BackendOptions> backend_options = {};
	}

	CRef<BackendOptions> getBackendOptions() { return &backend_options.value(); }

	namespace setters {
		void setBackendOptions(const BackendOptions options) {
			CORE_ASSERT(backend_options.empty(), "Backend options have already been set.");
			backend_options = options;
		}
	}
}
