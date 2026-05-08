/**
 * @file log_helpers.hpp
 * @brief Shared logging helpers for driver diagnostics.
 */
#pragma once

#include <string_id/string_id.hpp>

#include <functional>
#include <string_view>

namespace compiler::driver::diagnostics {

	/**
	 * @brief Returns a reporter that forwards diagnostics to the global logger.
	 */
	std::function<void(std::string_view, std::string_view, bool)> makeGlobalLoggerReporter();

	/**
	 * @brief Report a missing package name referenced by a task.
	 */
	void reportMissingPackageInTask(
		base::StrID package_name,
		const std::function<void(std::string_view, std::string_view, bool)>& report
	);

}  // namespace compiler::driver::diagnostics
