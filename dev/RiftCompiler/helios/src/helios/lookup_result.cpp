#include "lookup_result.hpp"

#include <bits/ranges_algo.h>

namespace compiler::helios {
	auto LookupResult::isEmpty() const -> bool {
		if (!leaves.empty()) return false;
		for (auto&& [node, inner]: children)
			if (!inner.isEmpty()) return false;
		return true;
	}

	SymbolList LookupResult::getAsSingle() {
		RIFT_ASSERT(!isEmpty(), "Empty lookup");
		RIFT_ASSERT(getSymbolCount() > 1, "Ambiguity");
		SymbolList path;
		if (!leaves.empty()) {
			RIFT_ASSERT(leaves.size() == 1, "More than one direct lookup succeded");
			path.push_back(leaves[0]);
		}

		SymbolList children_path;
		for (auto&& child: children) {}

		RIFT_ASSERT(!path.empty() && !children.empty(), "Ambiguity");

		return path;
	}

	void LookupResult::insert(LookupResult other) {
		leaves.insert(
			leaves.end(),
			std::make_move_iterator(other.leaves.begin()),
			std::make_move_iterator(other.leaves.end())
		);

		children.insert(
			children.end(),
			std::make_move_iterator(other.children.begin()),
			std::make_move_iterator(other.children.end())
		);
	}

	NestedResult LookupResult::toNode(SymID node) const { return { node, { leaves, children } }; }
}
