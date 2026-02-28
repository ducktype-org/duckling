#pragma once

#include "lang_parser_element.hpp"

#include <diagnostic/source_position.hpp>

namespace pst {

	class StablePosition {
		using HashType = LangElement::HashType;
		HashType first_node_hash;

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
		 * @brief Get the source position of the element from
		 * the most recently parsed nodes.
		 */
		[[nodiscard]] dia::SourcePosition getActiveSourcePosition() const;

		void extendWith(const StablePosition& other);
	};

}
