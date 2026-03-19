#include "logger.hpp"

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <iostream>
#include <fstream>
#include <vector>

namespace logger {
	namespace {
		Ref<std::vector<DevLogCategories>> getEnabledCategories() {
			static std::vector<DevLogCategories> enabled_categories;
			return &enabled_categories;
		}

		Ref<std::ostream> current_stream = &std::cout;

		Ref<std::ostream> getOutputStream() { return current_stream; }


	}

	bool isCategoryEnabled(DevLogCategories category) {
		for (const auto& enabled_category: *getEnabledCategories())
			if (enabled_category == category) return true;
		return false;
	}

	namespace internal {
		void logMessage(std::string_view message) {
			// In the future this could be directed to a file or other streams.
			(*getOutputStream()) << message;
		}
	}

	void enableDevCategory(DevLogCategories category) {
		for (const auto& enabled_category: *getEnabledCategories())
			if (enabled_category == category) return;
		getEnabledCategories()->push_back(category);
	}

	void setDevLogOutputStream(Ref<std::ostream> str) { current_stream = str; }

	void devLogOutputFile(const std::string& path) {
        // This introduces memory-leak.
        // That is intentional, although not sure this is correct.
        // Motivation is that ostream shouldn't be deleted until very very late in the program,
        // such that anything that logs in the destructors can do that safely.
        // OS should reclaim resources and close file descriptors anyway.
		auto* fs = new std::ofstream(path, std::ios::out);
        setDevLogOutputStream(fs);
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
		HANDLE_CATEGORY_NAME(NYIStacktraces)
		HANDLE_CATEGORY_NAME(Command)
		HANDLE_CATEGORY_NAME(Diagnostics)
		HANDLE_CATEGORY_NAME(Compiler)
		HANDLE_CATEGORY_NAME(Parser)
		HANDLE_CATEGORY_NAME(Backend)
		HANDLE_CATEGORY_NAME(Linker)
		HANDLE_CATEGORY_NAME(DVM)
		HANDLE_CATEGORY_NAME(DVMDetails)
		HANDLE_CATEGORY_NAME(Incremental)
		HANDLE_CATEGORY_NAME(REPL)
		else std::cerr << "Warning: Unknown log category name: " << category_name << '\n';
	}
}
