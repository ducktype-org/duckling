#include "lookup_result.hpp"


namespace compiler::helios {
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

	NestedResult LookupResult::toNode(SymID node) const {
		return { node, { leaves, children } };
	}
}
