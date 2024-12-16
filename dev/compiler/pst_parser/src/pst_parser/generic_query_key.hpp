#pragma once

#include "lang_parser_element.hpp"
#include <base/ref.hpp>
#include <concepts>

namespace pst {
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
