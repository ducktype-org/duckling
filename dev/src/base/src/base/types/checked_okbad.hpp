// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/types/ok_bad.hpp>

namespace base {
	/**
	 * Utility type wrapping the base::OkBad,
	 * in a way that forces the value to be checked.
	 *
	 * If status method is never called, the destructor will panic.
	 *
	 * @note This is mostly useful as a return type for functions that can fail, to force the caller
	 * to check the result.
	 */
	struct CheckedOkBad final {
	private:
		base::OkBad result;
		bool        checked = false;

	public:
		CheckedOkBad(base::OkBad result);
		CheckedOkBad(const CheckedOkBad&) = delete;
		CheckedOkBad(CheckedOkBad&&)      = delete;

		~CheckedOkBad();

		[[nodiscard]]
		base::OkBad status();
	};

}
