#include "stable_position.hpp"

#include "lang_parser_element.hpp"

#include <algorithm>

namespace pst {

	dia::SourcePosition StablePosition::getActiveSourcePosition() const {
		auto first_pos = LangElement::getByStableHash(begin_scope_node)
		                     .illegalAccess()
		                     .value()
		                     ->getSourcePosition();

		if (end_scope_node.has_value()) {
			auto last_pos = LangElement::getByStableHash(end_scope_node.value())
			                    .illegalAccess()
			                    .value()
			                    ->getSourcePosition();
			return dia::SourcePosition::merge(first_pos, last_pos);
		} else {
			return first_pos;
		}
	}

	void StablePosition::extendWith(const StablePosition& other) {
		// Get the source position of the 4 hashes we need to compare
		// and store the two that are the furthest apart (the one with the smallest start and the
		// one with the largest end)

		std::vector<HashType> hashes_to_compare{ begin_scope_node };
		if (end_scope_node.has_value()) hashes_to_compare.push_back(end_scope_node.value());

		hashes_to_compare.push_back(other.begin_scope_node);
		if (other.end_scope_node.has_value())
			hashes_to_compare.push_back(other.end_scope_node.value());

		// Get first
		auto min_start_pos = [](HashType hash1, HashType hash2) {
			auto pos1
				= LangElement::getByStableHash(hash1).illegalAccess().value()->getSourcePosition();
			auto pos2
				= LangElement::getByStableHash(hash2).illegalAccess().value()->getSourcePosition();
			CORE_ASSERT(
				pos1.getSource() == pos2.getSource(),
				"Cannot compare positions from different sources"
			);
			return pos1.getStart() < pos2.getStart();
		};
		auto max_end_pos = [](HashType hash1, HashType hash2) {
			auto pos1
				= LangElement::getByStableHash(hash1).illegalAccess().value()->getSourcePosition();
			auto pos2
				= LangElement::getByStableHash(hash2).illegalAccess().value()->getSourcePosition();
			CORE_ASSERT(
				pos1.getSource() == pos2.getSource(),
				"Cannot compare positions from different sources"
			);
			return pos1.getEnd() < pos2.getEnd();
		};
		auto min_start_hash = *std::ranges::min_element(hashes_to_compare, min_start_pos);
		auto max_end_hash   = *std::ranges::max_element(hashes_to_compare, max_end_pos);

		if (min_start_hash == max_end_hash) {
			// both positions are the same, we can just keep one of the hashes
			begin_scope_node = min_start_hash;
			end_scope_node   = std::nullopt;
		} else {
			begin_scope_node = min_start_hash;
			end_scope_node   = max_end_hash;
		}
	}

	StablePosition StablePosition::extendedWith(const StablePosition& other) const {
		StablePosition copy = *this;
		copy.extendWith(other);
		return copy;
	}
}
