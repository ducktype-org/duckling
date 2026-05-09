/**
 * @file log_helpers.hpp
 * @brief Shared logging helpers for driver diagnostics.
 */
#pragma once

#include <functional>
#include <string_view>

namespace compiler::driver::diagnostics {

	/**
	 * @brief Returns a reporter that forwards diagnostics to the global logger.
	 */
	std::function<void(std::string_view, std::string_view, bool)> makeGlobalLoggerReporter();

}  // namespace compiler::driver::diagnostics
