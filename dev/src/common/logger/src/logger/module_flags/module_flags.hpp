// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace logger {
	/**
	 * If set, dev logs are enabled.
	 * If not set, all dev logs are suppressed.
	 *
	 * @note Dev logs are disabled by default.
	 * This flag exist despite of DevLogCategories enum for easy global enabling/disabling and
	 * performance reasons.
	 *
	 * @note When enabled, specific categories can be enabled via enableDevCategory function to
	 * actually enable logging for those categories.
	 */
	extern constinit bool enable_dev_logs;

	/**
	 * If set, user logs are enabled.
	 * If not set, all user logs are suppressed.
	 * @note User logs are enabled by default.
	 */
	extern constinit bool enable_user_logs;
}
