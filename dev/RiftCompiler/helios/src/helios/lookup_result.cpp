#include "lookup_result.hpp"
#include "base/exceptions.hpp"

#include <bits/ranges_algo.h>

namespace compiler::helios {
	auto LookupResult::isEmpty() const -> bool {
		if (!leaves.empty()) return false;
		for (auto&& child: children)
			if (!child.inner.isEmpty()) return false;
		return true;
	}

	u64 LookupResult::symbolCount() const {
		u64 res = leaves.size();
		for (auto&& child: children)
			res += child.inner.symbolCount();
		return res;
	}

	SymID LookupResult::getSingle() const {
		RIFT_ASSERT(symbolCount() == 1, "getSingle called on non single symbol");
		if (leaves.size() == 1) return leaves[0];
		else {
			for (auto&& child: children) {
				if (child.inner.symbolCount() == 1) {
					return child.inner.getSingle();
				}
			}
		}
		RIFT_PANIC("Something went wrong in LookupResult::getSingle()");
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

	NestedResult LookupResult::toNode(SymID node) const {
		return { node, { leaves, children } };
	}
}
