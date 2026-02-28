#include "stable_position.hpp"

#include <algorithm>

namespace pst {

	dia::SourcePosition StablePosition::getActiveSourcePosition() const {
		auto first_pos = LangElement::getByStableHash(first_node_hash)
		                     .illegalAccess()
		                     .value()
		                     ->getSourcePosition();

		if (last_node_hash.has_value()) {
			auto last_pos = LangElement::getByStableHash(last_node_hash.value())
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
		// and store the two thet are the furthest apart (the one with the smallest start and the
		// one with the largest end)
		std::vector<LangElement::HashType> hashes_to_compare{ first_node_hash };
		if (last_node_hash.has_value()) hashes_to_compare.push_back(last_node_hash.value());
		hashes_to_compare.push_back(other.first_node_hash);
		if (other.last_node_hash.has_value())
			hashes_to_compare.push_back(other.last_node_hash.value());

		// Get first
		auto min_start_pos = [](StablePosition::HashType hash1, StablePosition::HashType hash2) {
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
		auto max_end_pos = [](StablePosition::HashType hash1, StablePosition::HashType hash2) {
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
			first_node_hash = min_start_hash;
			last_node_hash  = std::nullopt;
		} else {
			first_node_hash = min_start_hash;
			last_node_hash  = max_end_hash;
		}
	}
}
