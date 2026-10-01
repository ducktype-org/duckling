#include "identifiers.hpp"

#include <lang_definitions/key_spec_op.hpp>
#include <string_id/string_id.hpp>

#include <algorithm>

namespace c_import {

	bool isReservedName(std::string_view name) {
		// `byte` is a primitive type name without being a keyword.
		if (name == "byte") return true;

		const base::StrID id{ name };
		return lang_def::strAsKeyword(id) != lang_def::Keyword::NotAKeyword
		    || lang_def::strAsOperator(id) != lang_def::NamedOperator::NotAnOperator;
	}

	std::string usableName(std::string_view name) {
		std::string result{ name };
		if (isReservedName(name)) result += '_';
		return result;
	}

	bool globMatch(std::string_view pattern, std::string_view text) {
		std::size_t p      = 0;
		std::size_t t      = 0;
		std::size_t star_p = std::string_view::npos;
		std::size_t star_t = 0;
		while (t < text.size()) {
			if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
				++p;
				++t;
			} else if (p < pattern.size() && pattern[p] == '*') {
				star_p = p++;
				star_t = t;
			} else if (star_p != std::string_view::npos) {
				p = star_p + 1;
				t = ++star_t;
			} else {
				return false;
			}
		}
		while (p < pattern.size() && pattern[p] == '*') ++p;
		return p == pattern.size();
	}

	bool passesFilters(
		std::string_view                name,
		const std::vector<std::string>& include,
		const std::vector<std::string>& exclude
	) {
		auto matches = [&](const std::string& pattern) { return globMatch(pattern, name); };
		if (!include.empty() && std::ranges::none_of(include, matches)) return false;
		return std::ranges::none_of(exclude, matches);
	}

}
