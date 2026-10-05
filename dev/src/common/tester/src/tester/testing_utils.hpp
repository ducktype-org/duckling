// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/types/ints.hpp>

#include <string_view>

namespace testing_utils {
	inline usize nextChar(std::string_view s, usize i) {
		while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t')) i++;

		// Skipping trailing commas
		if (i < s.size() && s[i] == ',') {
			usize next = nextChar(s, i + 1);

			if (s[next] == ']') return next;
		}

		return i;
	}

	inline bool compareJson(std::string_view s1, std::string_view s2) {
		usize i = -1ULL;
		usize j = -1ULL;

		while (true) {
			i = nextChar(s1, i + 1);
			j = nextChar(s2, j + 1);

			if (i == s1.size() && j == s2.size()) return true;
			if (i == s1.size() || j == s2.size()) return false;
			if (s1[i] != s2[j]) return false;
		}

		return true;
	}
}
