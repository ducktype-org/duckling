// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_flags.hpp"

#include <base/except/exceptions.hpp>

namespace query {
	constinit bool track_reverse_graph = false;
	constinit bool enable_query_graph  = true;

	void setTrackReverseGraph(bool value) {
		CORE_ASSERT(
			!value || enable_query_graph,
			"Reverse graph tracking can only be enabled when query graph is enabled"
		);
		track_reverse_graph = value;
	}

	bool getTrackReverseGraph() { return track_reverse_graph; }
}
