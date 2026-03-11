#pragma once

#include "lang_parser_element.hpp"

#include <diagnostic/source_position.hpp>

namespace pst {

	/**
	 * @brief Class that represents a position in the source code that
	 * that is stable across re-parses.
	 * This is different from the normal SourcePosition where
	 * the position can become invalid after re-parses.
	 */
	class StablePosition {
		/**
		 * @brief Node hash that is defines one end of the position range.
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
	};

}
