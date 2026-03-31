#include "logger.hpp"

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <vector>

namespace logger {
	namespace {
		Ref<std::vector<DevLogCategories>> getEnabledCategories() {
			static std::vector<DevLogCategories> enabled_categories;
			return &enabled_categories;
		}

		Ref<std::ostream>& getCurrentLoggingStream() {
			static Ref<std::ostream> current_stream = &std::cout;
			return current_stream;
		}
	}

	bool isCategoryEnabled(DevLogCategories category) {
		for (const auto& enabled_category: *getEnabledCategories())
			if (enabled_category == category) return true;
		return false;
	}

	namespace internal {
		void logMessage(std::string_view message) { (*getCurrentLoggingStream()) << message; }
	}

	void enableDevCategory(DevLogCategories category) {
		for (const auto& enabled_category: *getEnabledCategories())
			if (enabled_category == category) return;
		getEnabledCategories()->push_back(category);
	}

	void setDevLogOutputStream(Ref<std::ostream> str) { getCurrentLoggingStream() = str; }

	void setDevLogOutputFile(const std::string& path) {
		auto* fs = new std::ofstream(path, std::ios::out);
		setDevLogOutputStream(fs);
	}

	void setDevLogOutputStreamCurrentDate() {
		auto now     = std::chrono::system_clock::now();
		auto now_sec = std::chrono::floor<std::chrono::seconds>(now);

		std::string filename = std::format("log_{:%Y-%m-%d_%H-%M-%S}.txt", now_sec);

		std::filesystem::path log_directory = "logs";

		std::filesystem::create_directories(log_directory);

		std::filesystem::path full_file_path = log_directory / filename;

		setDevLogOutputFile(full_file_path);
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
