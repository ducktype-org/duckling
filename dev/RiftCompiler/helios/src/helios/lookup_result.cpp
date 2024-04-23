#include "lookup_result.hpp"
#include "base/exceptions.hpp"

namespace compiler::helios {
	auto LookupResult::isEmpty() const -> bool {
		if (!leaves.empty()) return false;
		for (auto&& [node, inner]: children)
			if (!inner.isEmpty()) return false;
		return true;
	}

	bool LookupResult::isSingle() const { return symbolCount() == 1; }

	SymbolList LookupResult::getAsSingle() const {
		RIFT_ASSERT(!isEmpty(), "Empty lookup");
		RIFT_ASSERT(isSingle(), "Ambiguity");
		SymbolList path;
		if (!leaves.empty()) {
			RIFT_ASSERT(leaves.size() == 1, "More than one direct lookup succeded");
			return { leaves[0] };
		}

		for (auto&& child: children) {
			auto&& child_path = child.inner.getAsSingle();
			if (!child_path.empty()) {
				SymbolList result;
				result.push_back(child.node);
				result.insert(result.end(), child_path.begin(), child_path.end());
				return result;
			}
		}

		RIFT_PANIC("Error");
	}

	u64 LookupResult::symbolCount() const {
		u64 res = leaves.size();
		for (auto&& child: children) res += child.inner.symbolCount();
		return res;
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
