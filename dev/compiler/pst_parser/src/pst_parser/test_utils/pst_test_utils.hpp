#pragma once

#include <vector>
#include "../lang_parser_element.hpp"

namespace pst {

    /**
     * @brief Generates vector that contain Refs to all elements in the subtree of the root element (recursively).
     * Order of elements is arbitrary.
     * Should be used for tests only.
     */
    std::vector<CRef<pst::LangElement>> viewAllSubTreeElements(CRef<pst::LangElement> root);

    /**
     * @brief Same as viewAllSubTreeElements,
     * but filters out elements that are subtype of type T.
     * Should be used for tests only. Might be slow, due to dynamic_cast's.
     */
    template<class T>
    std::vector<CRef<T>> viewAllSubTreeElementsFillter(CRef<pst::LangElement> root) {
        auto all = viewAllSubTreeElements(root);
        std::vector<CRef<T>> result;
        for (auto el : all) {
            if (auto casted = dynamic_cast<const T*>(&*el)) {
                result.push_back(casted);
            }
        }
        return result;
    }
}
