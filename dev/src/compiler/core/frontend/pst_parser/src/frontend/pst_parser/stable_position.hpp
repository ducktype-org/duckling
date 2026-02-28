#pragma once

#include "lang_parser_element.hpp"

#include <diagnostic/source_position.hpp>

namespace pst {

	/**
	 * @brief Class that represents a position of the PST element
	 * that is stable across reparses.
	 * This is different from the normal SourcePosition where
	 * the position can become invalid after reparses.
	 *
	 * It can store the scope position starting from some
	 * PST element to some other PST element.
	 */
	class StablePosition {
		using HashType = LangElement::HashType;

		/**
		 * @brief Starting location node hash
		 */
		HashType first_node_hash;

		/**
		 * @brief Ending location node hash.
		 * If it is empty, it means that the position is calculated from position of a single node.
		 */
		base::Optional<LangElement::HashType> last_node_hash;

		StablePosition(
			LangElement::HashType                 first_node_hash,
			base::Optional<LangElement::HashType> last_node_hash
		):
			  first_node_hash(first_node_hash),
			  last_node_hash(last_node_hash) {}

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
