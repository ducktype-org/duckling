#include "backend_options.hpp"

namespace global_state {

	namespace {
		compiler::options::BackendOptions backend_options;
	}

	CRef<compiler::options::BackendOptions> getBackendOptions() { return &backend_options; }

	namespace setters {
		void setBackendOptions(const compiler::options::BackendOptions options) {
			backend_options = options;
		}
	}
}
