#include "hash_source_position.hpp"

namespace dia_int {

	void HashSourcePosition::extendWith(const HashSourcePosition& other) {
		end_node = other.end_node.copyValueOr(other.begin_node);
	}

	HashSourcePosition HashSourcePosition::extendedWith(const HashSourcePosition& other) const {
		HashSourcePosition result = *this;
		result.extendWith(other);
		return result;
	}

	namespace {
		dia::SourcePosition convertToSourcePosFake(const HashSourcePosition&) {
			return dia::SourcePosition::fakePosition();
		}
	}

	HashSourcePosition HashSourcePosition::fakePosition() {
		return { convertToSourcePosFake, base::Bit256(1, 2, 3, 4) };
	}
}
