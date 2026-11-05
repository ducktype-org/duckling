// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/except/exceptions.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace base {
	/**
	 * @brief Unwraps a `std::expected` whose failure the caller treats as a broken invariant,
	 * panicking with the failure's reason.
	 *
	 * Operations that can fail for reasons outside the program - a full disk, a missing file -
	 * report the failure in a `std::expected` and leave the decision to the caller. Where a
	 * failure really is a bug, or does not matter (e.g. scratch space a test needs), this is the
	 * one way to give up on it, and it is visible at the call site.
	 *
	 * @note Like every `CORE_PANIC`, this panics only in dev builds; in other builds a failure
	 * here is undefined behaviour. Do not use it where the environment can plausibly fail.
	 *
	 * @param result The result to unwrap.
	 * @param what What was being done, for the panic message.
	 */
	template<class T>
	T valueOrPanic(
		std::expected<T, std::string> result, std::string_view what = "unexpected failure"
	) {
		if (!result.has_value()) CORE_PANIC(what, ": ", result.error());
		if constexpr (!std::is_void_v<T>) return std::move(result).value();
	}
}
