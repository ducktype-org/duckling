// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
