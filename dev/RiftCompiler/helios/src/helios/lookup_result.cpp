#include "lookup_result.hpp"
#include <base/exceptions.hpp>

namespace compiler::helios {

	NestedResult::NestedResult(SymID node, LookupResult inner): node(node), inner(std::move(inner)) {}

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

		if (!leaves.empty()) return { leaves[0] };

		for (const auto& [node_id, inner]: children) {
			if (auto&& child_path = inner.getAsSingle(); !child_path.empty()) {
				SymbolList result;
				result.push_back(node_id);
				result.insert(result.end(), child_path.begin(), child_path.end());
				return result;
			}
		}

		// (Thic cannot happen, but for sanity.)
		RIFT_PANIC("isSingle, but haven\'t found any symbols");
	}

	u64 LookupResult::symbolCount() const {
		u64 res = leaves.size();
		for (auto&& [_, inner]: children) res += inner.symbolCount();
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
