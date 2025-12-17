#include "lookup_result.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

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
		if (isEmpty()) return errors::SymbolNotFound();
		if (!isSingle()) return errors::Ambiguity();

		// if leaves has exactly one symbol,
		// it must work (as symbolCount() == 1):
		if (!leaves.empty()) return SymbolList{ { leaves[0] } };

		CORE_ASSERT(children.size() == 1, "Invalid state: contains empty children");

		auto&& [node_id, inner] = children.at(0);
		CORE_ASSERT(!inner.isEmpty(), "Invalid state: found an empty child");
		
		UNPACK_QRESULT(auto& child_path =, inner.getAsSingle());
		
		variant_match(child_path) {
			variant_case(SymbolList, symbols) {
				SymbolList result;
				result.pushBack(node_id);
				result.appendList(symbols);
				return result;
			}
			variant_case(errors::Ambiguity, _) {
				return errors::Ambiguity();
			}
			variant_case(errors::SymbolNotFound, _) {
				return errors::SymbolNotFound();
			}
			variant_default { CORE_PANIC("Invalid state");}
		}
		CORE_UNREACHABLE();		
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
