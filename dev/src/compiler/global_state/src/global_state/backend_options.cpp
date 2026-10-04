// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
