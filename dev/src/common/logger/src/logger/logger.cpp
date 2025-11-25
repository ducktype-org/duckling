#include "logger.hpp"

#include <iostream>
#include <set>
#include <base/pointers/ref.hpp>

namespace logger {
	namespace {
		Ref<std::set<LogCategories>> getEnabledCategories() {
			static std::set<LogCategories> enabled_categories = { LogCategories::General };
			return &enabled_categories;
		}
	}

	namespace internal {
		bool isCategoryEnabled(LogCategories category) {
			return getEnabledCategories()->find(category) != getEnabledCategories()->end();
		}

		void logMessage(std::string_view message) {
			// In the future this could be directed to a file or other streams.
			std::cout << message;
		}
	}

	void enableCategory(LogCategories category) { getEnabledCategories()->insert(category); }
}
