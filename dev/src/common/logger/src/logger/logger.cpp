#include "logger.hpp"
#include <set>
#include <iostream>

namespace logger {
    namespace {
        std::set<LogCategories> enabled_categories;
    }

    namespace internal {
        bool isCategoryEnabled(LogCategories category) {
            return enabled_categories.find(category) != enabled_categories.end();
        }

        void logMessage(std::string_view message) {
            // In the future this could be directed to a file or other streams.
            std::cout << message;
        }
    }

    void enableCategory(LogCategories category) {
        enabled_categories.insert(category);
    }

}

