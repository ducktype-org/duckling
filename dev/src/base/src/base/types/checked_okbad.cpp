// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "checked_okbad.hpp"

#include <base/except/exceptions.hpp>

namespace base {
	CheckedOkBad::CheckedOkBad(base::OkBad result): result(result) {}

	CheckedOkBad::~CheckedOkBad() {
		CORE_ASSERT_NOEXCEPT(checked, "CheckedOkBad status was not checked, use status method!");
	}

	[[nodiscard]]
	base::OkBad CheckedOkBad::status() {
		checked = true;
		return result;
	}
}
