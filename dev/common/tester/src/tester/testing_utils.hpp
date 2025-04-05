#pragma once

#include <base/ints.hpp>

#include <string>

namespace testing_utils {
	inline i32 nextChar(std::string_view s, i32 i) {
		while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t')) i++;

		// Skipping trailing commas
		if (i < s.size() && s[i] == ',') {
			i32 next = nextChar(s, i + 1);

			if (s[next] == ']') return next;
		}

		return i;
	}

	inline bool compareJson(std::string_view s1, std::string_view s2) {
		i32 i = -1;
		i32 j = -1;

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
