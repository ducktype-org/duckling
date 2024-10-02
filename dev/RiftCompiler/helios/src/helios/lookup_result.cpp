#include "lookup_result.hpp"
#include "helios_errors.hpp"
#include <base/exceptions.hpp>

namespace compiler::helios {

	NestedResult::NestedResult(SymID node, LookupResult inner):
		  node(node),
		  inner(std::move(inner)) {}

	auto LookupResult::isEmpty() const -> bool {
		if (!leaves.empty()) return false;
		for (auto&& [node, inner]: children)
			if (!inner.isEmpty()) return false;
		return true;
	}

	bool LookupResult::isSingle() const { return symbolCount() == 1; }

	errors::HResult<SymbolList, errors::Ambiguity, errors::SymbolNotFound>
		LookupResult::getAsSingle() const {
		if (isEmpty()) return errors::HError(errors::SymbolNotFound());
		if (!isSingle()) return errors::HError(errors::Ambiguity());

		if (!leaves.empty()) return SymbolList{ leaves[0] };

		RIFT_ASSERT(children.size() != 1, "Invalid state: contains empty children");

		auto&& [node_id, inner] = children[0];
		SymbolList child_path   = inner.getAsSingle().expect(
            "This cannot be error, "
			  "because it was asserted above."
        );

		RIFT_ASSERT(!inner.isEmpty(), "Invalid state: found an empty child");

		SymbolList result;
		result.push_back(node_id);
		result.insert(result.end(), child_path.begin(), child_path.end());
		return result;
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
