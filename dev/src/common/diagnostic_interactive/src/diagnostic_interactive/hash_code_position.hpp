#pragma once
#include <base/collections/optional.hpp>
#include <base/types/bit256.hpp>

#include <diagnostic/source_position.hpp>

namespace dia_int {
	class HashCodePosition {
	public:
		using HashType = base::Bit256;

		HashType                 begin_node;
		base::Optional<HashType> end_node;

		using ConvertToSourcePosFunc = dia::SourcePosition (*)(const HashCodePosition&);
		ConvertToSourcePosFunc to_source_position;

		HashCodePosition(
			ConvertToSourcePosFunc   to_source_position_fn,
			HashType                 begin_node,
			base::Optional<HashType> end_node = {}
		):
			  begin_node(begin_node),
			  end_node(end_node),
			  to_source_position(to_source_position_fn) {}

		/**
		 * @brief Inplace extend the position to include the position of another HashCodePosition.
		 */
		void extendWith(const HashCodePosition& other);

		/**
		 * @brief Create a new HashCodePosition that is the extension of this position and another
		 * position.
		 */
		[[nodiscard]] HashCodePosition extendedWith(const HashCodePosition& other) const;

		[[nodiscard]] dia::SourcePosition toSourcePositionIllegalAccess() const {
			return to_source_position(*this);
		}
	};
}
