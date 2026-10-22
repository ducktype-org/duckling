// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file defer.hpp
 * @brief Defer is a macro that postpones execution of expression till the end of scope.
 *
 * @note Multiple defer statements cannot be used in the same line.
 *
 * ### Usage:
 * @include defer_example.cpp
 *
 * @example defer_example.cpp
 */

#pragma once

#include <base/preproc/diagnostics.hpp>

#include <utility>

namespace internal {
	template<typename ActionT>
	class DeferHelper final {
		ActionT action;

	public:
		DeferHelper(ActionT&& action): action(std::move(action)) {}

		~DeferHelper() noexcept { action(); }
	};
}

/**
 * @brief Jai/Duckling-like defer
 *
 * Defer takes any expression or code block and executes it after the block "ends"
 * during destruction of local variables.
 * Code inside defer should never throw.
 *
 * Multiple defers will execute in the reverse order of creation/scheduling:
 * defer (a);
 * defer (b);
 * here `b` will execute before `a`.
 * see: "Destruction sequence" in https://en.cppreference.com/w/cpp/language/destructor
 *
 * See tests/test.cpp for examples.
 */
#define defer(code)                                                         \
	PUSH_DIAGNOSTIC _Pragma("GCC diagnostic ignored \"-Wc++26-extensions\"" \
	)::internal::DeferHelper _{ [&]() noexcept -> void { code; } };         \
	POP_DIAGNOSTIC
#if __cplusplus >= 202'600L
	#warning "Remove the pragmas above when upgrading to C++26"
#endif
