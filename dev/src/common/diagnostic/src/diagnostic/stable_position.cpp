#include "stable_position.hpp"

#include <diagnostic/source_position.hpp>

namespace dia {

	void StablePosition::extendWithSubsequentPos(const StablePosition& other) {
		end_node = other.end_node.copyValueOr(other.begin_node);
	}

	StablePosition StablePosition::extendedWithSubsequentPos(const StablePosition& other) const {
		StablePosition result = *this;
		result.extendWithSubsequentPos(other);
		return result;
	}

	namespace {
		dia::SourcePosition convertToSourcePosFake(const StablePosition&) {
			return dia::SourcePosition::fakePosition();
		}

		dia::SourcePosition convertToSourcePosFakeWithContext(query::Context&, const StablePosition&) {
			return dia::SourcePosition::fakePosition();
		}
	}

	StablePosition StablePosition::fakePosition() {
		return {
			convertToSourcePosFakeWithContext,
			convertToSourcePosFake,
			base::Bit256(1, 2, 3, 4),
			{},
		};
	}
}
