// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once


#if defined(BUILD_TYPE_DEV)
	/**
     * @brief Macro to include code only in DEV builds.
     * @note if-constexpr does not always work, see:
     * https://www.reddit.com/r/cpp/comments/139b5wt/code_in_ifconstexpr_branch_not_taken_causing/
     */
	#define IF_BUILD_TYPE_DEV(code) code

	/**
     * @brief Macro to include code only in RELEASE builds.
     * @note if-constexpr does not always work, see:
     * https://www.reddit.com/r/cpp/comments/139b5wt/code_in_ifconstexpr_branch_not_taken_causing/
     */
	#define IF_BUILD_TYPE_RELEASE(code)

#elif defined(BUILD_TYPE_RELEASE)
	#define IF_BUILD_TYPE_DEV(code)
	#define IF_BUILD_TYPE_RELEASE(code) code
#else
	#error "Unknown BUILD_TYPE"
#endif

namespace base {
#if defined(BUILD_TYPE_DEV)
	constexpr bool IS_BUILD_TYPE_DEV     = true;
	constexpr bool IS_BUILD_TYPE_RELEASE = false;
#elif defined(BUILD_TYPE_RELEASE)
	constexpr bool IS_BUILD_TYPE_DEV     = false;
	constexpr bool IS_BUILD_TYPE_RELEASE = true;
#else
	#error "Unknown BUILD_TYPE"
#endif
}
