#pragma once

#include "lang_parser_element.hpp"
#include <base/ref.hpp>
#include <concepts>

namespace pst {
	/**
	 * This can be used as a key of a query taking just single PST element.
	 * @todo this is a perfect template for explicit instantiations, to speed up compilation
	 */
	template<std::derived_from<LangElement> T = LangElement>
	struct GenericPSTQueryKey {
		/**
		 * @brief Element for which the query is run.
		 */
		CRef<T> element;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return element->getID().asInt();
		}

		bool operator==(const GenericPSTQueryKey&) const = default;
	};
}
