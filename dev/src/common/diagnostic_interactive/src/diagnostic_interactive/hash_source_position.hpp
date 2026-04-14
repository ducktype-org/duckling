#pragma once
#include <base/collections/optional.hpp>
#include <base/types/bit256.hpp>

#include <diagnostic/source_position.hpp>

namespace dia_int {
	class HashSourcePosition {
	public:
		using HashType = base::Bit256;

		HashType                 begin_node;
		base::Optional<HashType> end_node;

		using ConvertToSourcePosFunc = dia::SourcePosition (*)(const HashSourcePosition&);
		ConvertToSourcePosFunc to_source_position;

		HashSourcePosition(
			ConvertToSourcePosFunc   to_source_position_fn,
			HashType                 begin_node,
			base::Optional<HashType> end_node = {}
		):
			  begin_node(begin_node),
			  end_node(end_node),
			  to_source_position(to_source_position_fn) {}

		/**
		 * @brief Inplace extend the position to include the position of another HashSourcePosition.
		 */
		void extendWith(const HashSourcePosition& other);

		/**
		 * @brief Create a new HashSourcePosition that is the extension of this position and another
		 * position.
		 */
		[[nodiscard]] HashSourcePosition extendedWith(const HashSourcePosition& other) const;

		[[nodiscard]] dia::SourcePosition toSourcePositionIllegalAccess() const {
			return to_source_position(*this);
		}

		static HashSourcePosition fakePosition();
	};
}
