#include "logger.hpp"

#include <base/pointers/ref.hpp>

#include <iostream>
#include <vector>

namespace logger {
	namespace {
		Ref<std::vector<DevLogCategories>> getEnabledCategories() {
			static std::vector<DevLogCategories> enabled_categories;
			return &enabled_categories;
		}
	}

	bool isCategoryEnabled(DevLogCategories category) {
		for (const auto& enabled_category: *getEnabledCategories())
			if (enabled_category == category) return true;
		return false;
	}

	namespace internal {
		void logMessage(std::string_view message) {
			// In the future this could be directed to a file or other streams.
			std::cout << message;
		}
	}

	void enableDevCategory(DevLogCategories category) {
		for (const auto& enabled_category: *getEnabledCategories())
			if (enabled_category == category) return;
		getEnabledCategories()->push_back(category);
	}

/**
 * This macro is made to ensure a compilation error when
 * category name is changed (e.g. via automatic rename) but
 * the function below is not updated accordingly.
 */
#define HANDLE_CATEGORY_NAME(NAME) \
	else if (category_name == #NAME) enableDevCategory(DevLogCategories::NAME);

	void enableDevCategoryByStringName(std::string_view category_name) {
		if (false) {}
		HANDLE_CATEGORY_NAME(Lexer)
		HANDLE_CATEGORY_NAME(Printer)
		HANDLE_CATEGORY_NAME(Artifacts)
		HANDLE_CATEGORY_NAME(Query)
		HANDLE_CATEGORY_NAME(QueryStacktraces)
		HANDLE_CATEGORY_NAME(Command)
		HANDLE_CATEGORY_NAME(Diagnostics)
		HANDLE_CATEGORY_NAME(Compiler)
		HANDLE_CATEGORY_NAME(Parser)
		HANDLE_CATEGORY_NAME(Backend)
		HANDLE_CATEGORY_NAME(Linker)
		HANDLE_CATEGORY_NAME(DVM)
		HANDLE_CATEGORY_NAME(DVMDetails)
		HANDLE_CATEGORY_NAME(REPL)
		else std::cerr << "Warning: Unknown log category name: " << category_name << '\n';
	}
}
