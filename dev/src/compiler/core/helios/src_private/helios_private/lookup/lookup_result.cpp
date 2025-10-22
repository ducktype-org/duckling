#include "lookup_result.hpp"

#include <helios/helios_errors.hpp>

#include <base/except/exceptions.hpp>

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

	query::QResult<SymbolList, errors::Ambiguity, errors::SymbolNotFound> LookupResult::getAsSingle(
	) const {
		if (isEmpty()) return query::QError(errors::SymbolNotFound());
		if (!isSingle()) return query::QError(errors::Ambiguity());

		if (!leaves.empty()) return SymbolList{ { leaves[0] } };

		CORE_ASSERT(children.size() == 1, "Invalid state: contains empty children");

		auto&& [node_id, inner] = children.at(0);
		SymbolList child_path   = inner.getAsSingle().expect(
            "This cannot be error, "
			  "because it was asserted above."
        );

		CORE_ASSERT(!inner.isEmpty(), "Invalid state: found an empty child");

		SymbolList result;
		result.pushBack(node_id);
		result.appendList(child_path);
		return result;
	}

	u64 LookupResult::symbolCount() const {
		u64 res = leaves.size();
		for (auto&& [_, inner]: children) res += inner.symbolCount();
		return res;
	}

	void LookupResult::merge(LookupResult other) {
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
		return { node, { .leaves = leaves, .children = children } };
	}

}
