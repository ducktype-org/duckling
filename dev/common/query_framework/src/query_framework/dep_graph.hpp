#pragma once

#include "node_id.hpp"

namespace query::detail {

	// @OPT: pick good type size here
	enum class DependencyStatus { OK, Cycle };

	namespace dep_graph {

		// this interface is not all that smart:
		// @TODO: make it better
		// @TODO: some pretty printing should be supported
		// @TODO: when cycle is detected "dep_graph" somehow "cycle" unwrap should happen, and all
		// queries in the cycle should produce "CycleError" that will propagate into any query
		// depending from them

		void             setEntry(detail::NodeID node, detail::NodeID from);
		DependencyStatus addDependency(detail::NodeID from, detail::NodeID to);
		void             setExit(detail::NodeID node);

		void debugPrint();
		void debugPrintForDrawing();
	}
}

namespace query {
	inline void debugPrintDependencyGraph() { detail::dep_graph::debugPrint(); }

	inline void debugPrintDependencyGraphForDrawing() { detail::dep_graph::debugPrintForDrawing(); }
}
