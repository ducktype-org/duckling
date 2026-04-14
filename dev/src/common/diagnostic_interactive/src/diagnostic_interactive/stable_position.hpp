#pragma once
#include <base/collections/optional.hpp>
#include <base/types/bit256.hpp>

#include <diagnostic/source_position.hpp>

namespace dia_int {
	class StablePosition {
	public:
		using HashType = base::Bit256;

		HashType                 begin_node;
		base::Optional<HashType> end_node;

		using ConvertToSourcePosFunc = dia::SourcePosition (*)(const StablePosition&);
		ConvertToSourcePosFunc to_source_position;

		StablePosition(
			ConvertToSourcePosFunc   to_source_position_fn,
			HashType                 begin_node,
			base::Optional<HashType> end_node = {}
		):
			  begin_node(begin_node),
			  end_node(end_node),
			  to_source_position(to_source_position_fn) {}

		/**
		 * @brief Inplace extend the position to include the position of another StablePosition.
		 */
		void extendWith(const StablePosition& other);

		/**
		 * @brief Create a new StablePosition that is the extension of this position and another
		 * position.
		 */
		[[nodiscard]] StablePosition extendedWith(const StablePosition& other) const;

		/**
		 * @brief Get the active source position corresponding to this stable position.
		 */
		[[nodiscard]] dia::SourcePosition getActiveSourcePositionIllegalAccess() const {
			return to_source_position(*this);
		}

		static StablePosition fakePosition();
	};
}
