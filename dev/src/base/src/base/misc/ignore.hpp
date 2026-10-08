// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace base {

	/**
	 * Type that can be constructed from any type and any type can be assigned to it, but nothing
	 * happens. Used to ignore values intentionally.
	 */
	struct Ignore final {
		Ignore() = default;

		template<typename T>
		Ignore(const T&) {}

		template<typename T>
		Ignore& operator=(const T&) {
			return *this;
		}
	};

}
