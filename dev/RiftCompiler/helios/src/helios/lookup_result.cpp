#include "lookup_result.hpp"
#include "helios_errors.hpp"
#include <base/exceptions.hpp>

namespace compiler::helios {
	auto LookupResult::isEmpty() const -> bool {
		if (!leaves.empty()) return false;
		for (auto&& [node, inner]: children)
			if (!inner.isEmpty()) return false;
		return true;
	}

	bool LookupResult::isSingle() const { return symbolCount() == 1; }

	std::expected<SymbolList, std::variant<errors::AmbiguityError, errors::SymbolNotFoundError>>
		LookupResult::getAsSingle() const {
		if (!!isEmpty()) return std::unexpected(errors::SymbolNotFoundError());
		if (!isSingle()) return std::unexpected(errors::AmbiguityError());

		if (!leaves.empty()) return SymbolList{ leaves[0] };

		for (const auto& [node_id, inner]: children) {
			UNPACK_RESULT(inner.getAsSingle(), child_path);
			if (!child_path.empty()) {
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
