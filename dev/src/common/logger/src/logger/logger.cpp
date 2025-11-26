#include "logger.hpp"

#include <base/pointers/ref.hpp>

#include <iostream>
#include <set>

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

/**
 * This macro is made to ensure a compilation error when
 * category name is changed (e.g. via automatic rename) but
 * the function below is not updated accordingly.
 */
#define HANDLE_CATEGORY_NAME(NAME) \
	else if (category_name == #NAME) enableCategory(LogCategories::NAME);

	void enableCategoryByStringName(std::string_view category_name) {
		if (false) {}
		HANDLE_CATEGORY_NAME(General)
		HANDLE_CATEGORY_NAME(Lexer)
		HANDLE_CATEGORY_NAME(Printer)
		HANDLE_CATEGORY_NAME(Artifacts)
		HANDLE_CATEGORY_NAME(Query)
		HANDLE_CATEGORY_NAME(Command)
		HANDLE_CATEGORY_NAME(Diagnostics)
		HANDLE_CATEGORY_NAME(Compiler)
		HANDLE_CATEGORY_NAME(Backend)
		HANDLE_CATEGORY_NAME(Linker)
		HANDLE_CATEGORY_NAME(DVM)
		HANDLE_CATEGORY_NAME(DVMDetails)
		else std::cerr << "Warning: Unknown log category name: " << category_name << '\n';
	}
}
