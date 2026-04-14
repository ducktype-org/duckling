#include "hash_code_position.hpp"

namespace dia_int {

	void HashCodePosition::extendWith(const HashCodePosition& other) {
		end_node = other.end_node.copyValueOr(other.begin_node);
	}

	HashCodePosition HashCodePosition::extendedWith(const HashCodePosition& other) const {
		HashCodePosition result = *this;
		result.extendWith(other);
		return result;
	}
}
