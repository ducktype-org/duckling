#include "logger.hpp"
#include <set>
#include <iostream>

namespace logger {
    namespace {
        std::set<LogCategories> enabled_categories;
    }

    void enableCategory(LogCategories category) {
        enabled_categories.insert(category);
    }

    void logMessage(std::string_view message) {
        // In the future this could be directed to a file or other streams.
        std::cout << message;
    }
}

