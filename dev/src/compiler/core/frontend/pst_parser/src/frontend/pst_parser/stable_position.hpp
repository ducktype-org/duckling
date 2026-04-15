#pragma once

#include <base/types/bit256.hpp>

#include <diagnostic/source_position.hpp>

namespace pst {
	class LangElement;

	/**
	 * @brief Class that represents a position in the source code that
	 * that is stable across re-parses.
	 * This is different from the normal SourcePosition where
	 * the position can become invalid after re-parses.
	 */
	class StablePosition {
	public:
		using HashType = base::Bit256;

	private:
		StablePosition(HashType begin_scope_node, base::Optional<HashType> end_scope_node):
			  begin_scope_node(begin_scope_node),
			  end_scope_node(end_scope_node) {}

		friend LangElement;

	public:
		/**
		 * @brief Get the source position from
		 * the most recently parsed nodes.
		 */
		[[nodiscard]] dia::SourcePosition getActiveSourcePosition() const;

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
		 * @brief Node hash that defines one end of the position range.
		 *
		 * @note We don't know that this node comes before or after
		 * the @p end_scope_node, but the resulting position is always defined
		 * as the inclusive range between these two nodes.
		 */
		HashType begin_scope_node;

		/**
		 * @brief Node hash that defines the other end of the position range.
		 * If not set, the position is defined as the position of the @p begin_scope_node only.
		 */
		base::Optional<HashType> end_scope_node;
	};

}
