#include "stable_position.hpp"

namespace dia_int {

	void StablePosition::extendWith(const StablePosition& other) {
		end_node = other.end_node.copyValueOr(other.begin_node);
	}

	StablePosition StablePosition::extendedWith(const StablePosition& other) const {
		StablePosition result = *this;
		result.extendWith(other);
		return result;
	}

	namespace {
		dia::SourcePosition convertToSourcePosFake(const StablePosition&) {
			return dia::SourcePosition::fakePosition();
		}
	}

	StablePosition StablePosition::fakePosition() {
		return { convertToSourcePosFake, base::Bit256(1, 2, 3, 4) };
	}
}
