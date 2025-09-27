#pragma once

#include <vector>

namespace base {
    /**
     * @note #1323 remove it, change usage to std::ranges::to
     */
    template<class Element, class T>
    auto makeVectorFromView(T view) {
        std::vector<Element> output;
        for (const auto& item: view) {
            output.emplace_back(item);
        }
        return output;
    }
}
